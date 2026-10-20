<!-- done: recorder-sites, recording-format, subtitle, tutorial-pair, reviews, combat (deferred), gates -->
# M2-R9 producer 4: the recorder's record side (stage 5 reduced) and the test / tooling leftovers

Base commit `9c1801f` (branch `m2`, worktree `.claude/worktrees/m2`). Nothing committed. Every engine run went through
`tools/m2_harness.sh` (silent, headless, offscreen driver). No Ghidra, no GOG game, no oracle DLL, no input injection,
no gamescope. Run directories are `original/scratch-m2/m2r2-harness/runs/<label>` (gitignored scratch).

Subsystems (the done marker): **recorder-sites** (the 4 `CAT-TODO(stage 5 recorder)` sites), **recording-format**
(hwi-engine records a game in cataclysm.exe's own recording format; the replay proof; the format checks; the user's
GOG steps), **subtitle** (`Subtitle.c:1144`), **tutorial-pair** (the Tutorial1 leg on a virtual audio clock),
**reviews** (the 123 stage-4 bodies, a sample of the unflagged rows, every edited function), **combat** (deferred,
measured), **gates**.

## 1. Summary

- **The 4 `CAT-TODO(stage 5 recorder)` sites are closed** (`grep` count 0). The record side is ported at the exe's bodies
  (section 2). With it, hwi-engine records a game the way cataclysm.exe does: a save at the recording's start and a
  `.pkts` file of every sync packet.
- **The recording's save is in cataclysm.exe's own format** (version 0x10ad0027, the exe's ILP32 structs). This needed
  a new writer, the inverse of the M2-R6 loader (`SaveCat32.c`). The tree's other saves keep the native format.
- **Proof on this side**: two recordings made by the skirmish self-test (`hwirec01` on Armed8, `hwirec02` on
  LiveFireExercise2, 4,800 ticks / 2,400 packets each). Each one:
  - replays in hwi-engine's own replay mode twice **byte-identical**;
  - its replay stream is **IDENTICAL to the recording run's stream** over all 4,800 ticks and 2,400 packets, in every
    simulation field, including `ships_rot`, `players` and `real`.
  - Three fields are excluded, each a stock record/playback difference (section 3.3): `rng0`, `rng6` and `rng_all`.
- **The format matches the GOG sessions field by field** (`tools/formats/recording_compare.py`, new):
  - Save: the version, the pre-game block chunk by chunk, every fixed-size chunk type's size, the walk to EOF.
  - `.pkts`: every record header, every packet header, every command parsed with the exe's layouts to the packet's end.
  - **Writer byte check**: both GOG recording saves (10-17, 10-18) were loaded and saved again with the writer
    (`/oracleResave`). Each result has the **same chunk structure, chunk for chunk, and the same byte count** as GOG's
    own file. 1,723 of 3,377 chunks are byte-identical. Every other differing dword is one of these: a stale pointer of
    the saving process, a capacity word the exe's loader overwrites, a function pointer the exe's load rebuilds, or
    state a load resets (section 3.5).
- **The mode-B rung itself** (the GOG game plays our recording back and the DLL's stream must be IDENTICAL to ours) is
  the user's session. The exact steps are in section 3.6.
- **Leftovers**:
  - (b) **Tutorial1 leg is a byte-identical pair** (`7ce11b952c0ef020`, 12/12 lessons PASS). Speech and the mixer now
    run on a virtual audio clock.
  - (c) **`Subtitle.c:1144` reports are gone** from sp, nis and every missiontest run (CAT-FIX where the exe forms the
    out-of-table address).
  - (d) **Review rows**: 123 stage-4 bodies, 31 sampled unflagged rows and 42 notes on edited rows, merged. In the
    sample, 2 bodies were found modified and ported: `RegisterAllSpaceObjsInsideMe` and `RegisterAllSpaceObjs`. Three
    other modified bodies were tagged for stage 5.
  - (a) **The mission self-test's combat is deferred** with a measured reason (section 6).
- **No stream moved**:
  - Headless Mission01 pair `fd82fafbcd9011fc`: IDENTICAL to producer 3's audited `dc2fb8ce71c0d26b` (6,001 ticks).
  - Every mission 01-17 self-test stream, the warp start, loadgame, ai, build and both skirmish streams: IDENTICAL to
    producer 3's audited runs.
  - Both rung replays (10-17, 10-18): IDENTICAL to producer 1's audit runs.
- Builds `--debug` and `--release`: rc 0, 0 warnings. fe ASan 0. sp ASan 0; sp still stops at the known Mission04
  `texreg.c (569)` assertion (owner M2-R10 render).

## 2. File-by-file changes

| file | change |
|---|---|
| `src/Game/UnivUpdate.c` | univFindNearestResource 0x00549c50: the fog-of-war test is skipped while `recordPackets` 0x00a75188 is set (0x00549eac). CAT-TODO closed. |
| `src/Game/UnivUpdate.h` | Prototypes of `univDeleteDeadShips/Resources/Derelicts/Missiles` (0x00548e00 / 0x00549270 / 0x00549820 / 0x00549ac0) and `univRemoveShipReferences` 0x0054b230. |
| `src/Game/SaveGame.c` | **SaveUniverse 0x005034c0**: the four delete passes run after SaveMineFormationList (CAT-TODO closed). **cat_saveSavePlayerFOW 0x004ff6b0**: writes nothing while `recordingStartSave` 0x00a578f8 is set (CAT-TODO closed). **RegisterAllSpaceObjsInsideMe 0x004fe400**: registers recursively (found modified). **RegisterAllSpaceObjs 0x004fe470**: dead ships get univRemoveShipReferences and a mimic's fake asteroid is registered (found modified). **SaveGame**: a recording's save runs the exe-format writer (`saveCat32SaveBegin/End`). **SaveVersionInfo**: 0x10ad0027 in that case. **SavePreGameInfo**: the exe's 0x2B0 tpGameCreated (`saveCat32SaveCaptainGameInfo`). **Every struct-chunk saver** routes its chunk through the writer: blob, attack targets, gun info, ship SP info, ships inside, clamp, ship and its dynamic static info, asteroid and static, cloud system, dust cloud and static, nebula, bullet, derelict, missile, military slot / parade, command to-do, camera stack entry / command, research topic, mine formation, attack atom, resource volume, universe. |
| `src/Game/SaveCat32.c` / `.h` | The writer: `saveCat32ConvertBack` (each generated op inverted: pointer slots keep their low 4 bytes, a real stale pointer whose low half is 0 stays non-NULL, longs and wide characters keep their low bytes), plus `saveCat32SaveStruct`, `saveCat32SaveArray`, `saveCat32SaveShip` (Ship 0x63C + specifics, PreciseSelection 0x7C, the madanim block with the spline curves' pointers turned into the .mad file's offsets), `saveCat32SaveBegin/End` and its log line. |
| `src/Game/MeshAnim.c` / `.h` | The .mad file's keyframe-block offsets kept per loaded header (`madCat32ObjectFileOffset`), for the writer's spline curves. |
| `src/Game/NetCheck.c` / `.h` | **recPackInGameStartCBSafeToStart 0x004cbbb0** in the exe's order (the flags and 0x00a7519c before the save, `recordingStartSave` around it, 0x00a75190). HWI-FIX: every recorder file open and delete is in the user settings path (`recPackDeleteFile`; GoK's default fileOpen path is the data folder and its fileDelete is cwd-relative). `CAT-TODO(stage 5)` at recPackRecordSaveHeader (found modified). |
| `src/Game/CommandNetwork.c` / `.h` | **clSendMove 0x0044eb20** (0x124 + 2n with the path), **clSendMpHyperspace 0x0044ed00**, **clSendFormation 0x0044f690** (with the force-field level) in the exe's layouts, plus the `clPkt*` writers. CAT-TODO closed. `CAT-TODO(stage 5 LAN)` at KeepAliveDropPlayerCB (found modified). |
| `src/Game/CommandWrap.c` | clWrapMove 0x004529e0 / clWrapFormation 0x00452ca0 pass the path / the level (two `CAT-TODO(stage 5)` closed with the send bodies). |
| `src/Game/AIPlayer.c`, `AITeam.c` | SaveRequestShipsCB / SaveTeamWaitingCB / SaveThisAIPlayer / SaveMoveCB through the writer. SaveThisAITeam writes the exe's 0x280-byte team (`aitCat32SaveTeam`, the inverse of `aitCat32LoadTeam`). |
| `src/Game/ConsMgr.c`, `HS.c`, `Blast.c`, `ShipNames.c`, `Nebulae.c`, `Objectives.c`, `Ping.c`, `KAS.c`, `SinglePlayer.c` | Their savers in the exe's layout under the writer: shipsinprogress and carriers, 0x30-byte gates (count × 0x30), 0x74-byte blast records, 0x70-byte ship-name records, nebula chunk / tendril, objective, fleet intelligence, the 0x58 ping head with dword user data, KAS label entries, SinglePlayerGameInfo 0x54. |
| `src/Game/Subtitle.c` | subTitlesUpdate 0x005842f0: CAT-FIX for an actor past the 16-theme table (section 4). |
| `src/Game/MultiplayerGame.c` | `CAT-TODO(stage 5 multiplayer options)` at mgUnitCapsEnabled (found modified, deferred). |
| `src/Game/OracleDump.c` / `.h`, `src/SDL/main.c` | HWI-ORACLE `/oracleRecord <name>` and `/oracleRecordStop <tick>` (the DLL's record mode on this engine), and `/oracleResave <name>` (the writer's byte check). HWI-TEST `/catVirtualAudio`. |
| `src/SDL/sstream.c`, `soundlow.c` / `.h`, `utility.c` | HWI-TEST: the virtual audio clock (section 5). The stream thread's loop body became `isoundstreamstep` (unchanged); no device / thread in virtual mode; `soundVirtualAdvance` from utyTasksDispatch's virtual branch. |
| `tools/m2_harness.sh` | `--record NAME`, `--record-stop N`, the summary's `record:` line. Header updated: replay packets live in the settings path; the tutorial's virtual audio clock. |
| `tools/formats/pkts_probe.py` (new) | Parses a `.pkts` with the exe's layouts (`--selftest`). |
| `tools/formats/recording_compare.py` (new) | Format comparison of recordings; `--chunks` for the writer's byte check, classified by the generated ILP32 tables. |
| `ghidra/delta_map_review.json`, `ghidra/delta_map.json` | 165 new entries plus notes, merged (section 7). |

## 3. The recorder's record side and the mode-B oracle

### 3.1 The four sites (exe functions and addresses)

1. **UnivUpdate.c**: univFindNearestResource 0x00549c50 tests `recordPackets` 0x00a75188 at 0x00549eac, after the
   fogOfWar argument (0x00549ea4) and before the grid (0x00549eb5).
2. **SaveGame.c, the save skip**: recPackInGameStartCBSafeToStart 0x004cbbb0 sets 0x00a578f8 around its SaveGame.
   cat_saveSavePlayerFOW 0x004ff6b0 returns while it is set. Name chosen: `recordingStartSave`.
3. **SaveGame.c, SaveUniverse 0x005034c0**: the four delete passes run in the exe's order after
   SaveMineFormationList. HW1 asserted the delete lists empty.
4. **CommandNetwork.c, the send bodies**:
   - clSendMove 0x0044eb20 receives clWrapMove 0x004529e0's arguments unchanged.
   - Body offsets: +0 path type, +4 from, +0x10 number of points, +0x14 the points, +0x104 the selection. Size
     0x124 + 2n.
   - clSendMpHyperspace 0x0044ed00 uses the same layout with path type 0 and one point.
   - clSendFormation 0x0044f690: +0 formation, +2 force-field level, +4 selection; size 0x24 + 2n. It sends for any
     non-empty selection.
   - The other senders that occur in the recordings were compared with their exe bodies: attack, special, dock,
     launch, misc, RU transfer, autolaunch, alliance, research info, create / build ship, deterministic build, collect,
     protect. HW1's pointer-free structs already give the exe's offsets (TargetID = number + type, NetMiscCommand
     10 + 2n, NetDeterministicBuildCommand 12, ...).

Also ported: recPackInGameStartCBSafeToStart 0x004cbbb0 in the exe's order. The exe also clears 0x80 netcheck dwords
(netcheckInit inlined) and, for a local game, sets captainIndex / sigsPlayerIndex / sigsNumPlayers /
sigsPressedStartGame as HW1 does.

### 3.2 Recording on this engine

`/oracleRecord NAME [/oracleRecordStop N]` (harness `--record NAME --record-stop N`) follows the DLL's record mode
(`tools/oracle/dll/hwi_oracle.c` start_recording / on_tick):

1. At the first universe tick, after the tick's T record, it calls recPackInGameStartCB 0x004cbaf0 with
   `SavedGames/RecordedGames/NAME` and writes the record-start event.
2. universeUpdateTask then calls recPackInGameStartCBSafeToStart. That function saves the game: tick 1, exe format,
   `savecat32: saved ... 0 problems`. From then on the lockstep runs with fake sends.
3. At tick N it calls recPackInGameStopCB 0x004cbd10 and writes the record-stop event.

A single-player game is refused, as the DLL refuses it. The files land in the run's settings dir only.

**The recordings** (final build, plain harness, `--mode skirmish --skirmish-ticks 5000 --record ... --record-stop 4800`):

| run | map | save | .pkts | record run stream | commands |
|---|---|---|---|---|---|
| `r9r-rec01` | Armed8 (a listed map) | `hwirec01` 1,195,596 B, sha256 `4640546c0708c284`, 3,390 chunks, 54 ship chunks | 88,558 B `e852ee9f3a457e3b`, 2,400 packets (frames 0..2399) | `03b6c875f8df84f9` | move 8, buildship 2, createship 1 |
| `r9r-rec02` | LiveFireExercise2 | `hwirec02` 841,152 B `13a17b9f3a2d8664`, 1,892 chunks, 103 ship chunks | 88,798 B `d24604387e083cf5`, 2,400 packets | `633c0256696b42a3` | protect 12, collect 9, move 7, misc 5, attack 4, research info 3, formation 1 |

Both runs: `record stopped ... tick=4800 packets=2400 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0`, skirmish
PASS, rc 0, ASan 0.

A second recording run of `hwirec02` (`r9r-rec02-again`) gave the same record-run stream (`633c0256696b42a3`). Its save
differs only in stale-pointer bytes, which the heap gives each process. Its `.pkts` header pads are allocator bytes
as in the exe (GOG's own files have them in 2,535 of 4,800 packets).

### 3.3 The replay proof

Replay command (each twice, the same `--recording-as`):

```
tools/m2_harness.sh r9r-play02-a --mode replay --recording hwirec02 \
  --recording-src original/scratch-m2/m2r2-harness/runs/r9r-rec02/settings/SavedGames/RecordedGames --recording-as hwirec02 --timeout 900
```

| pair | replay line | run.hwo | a vs b |
|---|---|---|---|
| `r9r-play01-a/-b` | `replay done ticks=4801 packets=2400 ships=57 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0`; `savecat32: loaded ... 1863 chunks converted, 54 ships, 0 size problems` | `5e45a4da19f8eeef` | `cmp` identical |
| `r9r-play02-a/-b` | `replay done ticks=4801 packets=2400 ships=70 ... pktSyncErr=0`; `736 chunks converted, 103 ships, 0 size problems` | `1c9fc262e52800c5` | `cmp` identical |

The recording run against its replay:

```
python3 -I tools/oracle/hwo_compare.py runs/r9r-rec02/run.hwo runs/r9r-play02-a/run.hwo --from-tick 1 --to-tick 4800 \
  --fields rng10,ships_n,ships_misc,ships_pos,ships_vel,ships_health,ships_rot,missiles,bullets,derelicts,resources,univcheck,players,dt,total,real \
  --json runs/r9r-play02-a/compare-rec.json
IDENTICAL: 4800 ticks and 2400 packets compared (ticks 1-4800), no difference
```

The same result holds for rec01 / play01. `fpcw` is IDENTICAL too.

**Exclusions, each with its reason (both recordings):**
- **`rng0`** first differs at tick 2: the replay is 6 draws behind. The computer player draws stream 0 (AIResourceMan.c
  airDumbResearchRequests, `ranRandom(RANDOM_STREAM_0)` at lines 699 / 715 / 724), and a playback never runs the computer players
  (aiplayerUpdateAll). The GOG DLL splits stream 0 for the same reason (`rng0=split`).
- **`rng6`** first differs at tick 321: the AI stream, for the same reason (decision 47's rng6).
- **`rng_all`**: the aggregate of the above.

`players` is comparable here because the human player issues no build (decision 47's players difference needs a local
build). The P records (the sync checksums of all 2,400 packets) are IDENTICAL.

Before the writer existed, the native-format save gave the same identity (`r9r-rec-try2` / `r9r-play-try2`). The exe
format therefore round-trips the tick-1 state exactly through the mode-A loader.

### 3.4 The format matched to the GOG sessions, field by field

`python3 -I tools/formats/recording_compare.py original/oracle/oracle01-rec-20261017-140428/oracle01 original/oracle/oracle01-rec-20261019-143530/oracle01 runs/r9r-rec01/.../hwirec01 runs/r9r-rec02/.../hwirec02`
gives rc 0, `SAME FORMAT` for all three candidates. The full output is `runs/r9r-rec02/recording_compare.txt`.

**Save:**
- Version 0x10ad0027 and a walk to EOF.
- The 29 pre-game chunks, type and size, in order: single-player flag, song, player count, the 0x90 names, 8 × 3 INFO,
  the 0x2B0 tpGameCreated.
- tpGameCreated is 0 after +0xC8 in GOG's and in ours.
- Every chunk type both files have agrees on its fixed sizes: Universe 0x3D9BC, blob 0x100, asteroid 0x1B4, command
  0x330, camera 0x94, topic 0x18, parade 0x104, shipsinprogress 0x1B48, ...
- Ship chunks are at least 0x63C - 4 bytes.
- Section order: Armed8 `hwirec01` against the 10-17 session's (also a 2-player skirmish) has a collapsed-run match
  ratio of 0.959. The differences are the maps' contents (ship mixes; LFE2 has dust clouds and nebulae, 0.394).
- `savegame_probe.py` reads both saves OK.

**.pkts:**
- Records are PACK only (no HEAD, as GOG's in-game recordings).
- Packet type 0x5555, frames consecutive from 0, the extra checksum word 0, numberOfCommands at +0x18.
- Every command is parsed with the exe's layouts to the packet's exact end (`pkts_probe.py`: OK for all three GOG
  sessions and both of ours).
- Per-type body sizes follow the same formulas: move 264 = 0x106 + 2, misc 10 + 2n, attack 2 + 4t + 2 + 2n,
  protect 8, ...

One content difference shows: GOG's two skirmish sessions carry 21 alliance commands at frame 88 (cat_aisSetupAlliances
0x004310a0 with tpGameCreated.aiplayerBigotry >= 2, every computer index pair 1..7). Our skirmish test runs with the
front end's default bigotry and sends none. This is a setting, not a format difference.

[audit: a second content difference the comparison does not check: the sync checksum's lag. In GOG's recordings
(10-17, 10-18) the first 8 packets carry randcheck 0 and every later one the checksum of packet frame - 8
(packetnum = frame - 8); in hwi-engine's (`hwirec01/02`, the audit's `audrec01/02`) only packet 0 has randcheck 0 and
packetnum = frame - 1. Both fill the header with netcheckFillInChecksum 0x004cc2f0 from netcheckShow 0x004cc400's
ring (128 entries), so a GOG playback finds packet frame - 1's entry and this should not raise a sync error; the lag is
how far the captain's packets run ahead of their processing (GOG's real-time sessions vs the skirmish test's virtual
clock). The mode-B session will show it if GOG's netcheck disagrees.]

### 3.5 The writer's byte check against GOG's own saves

`-- /oracleResave gogresave17` on the 10-17 replay (`r9r-r17`) saves the just-loaded game again in the exe format. The
same was done for 10-18 (`r9r-r18`). Then:
`python3 -I tools/formats/recording_compare.py --chunks <GOG oracle01> <runs/r9r-r17/settings/SavedGames/RecordedGames/gogresave17>`.

- **Structure SAME**: 3,377 chunks, every type and size in order. 1,181,728 B (10-17) / 1,181,736 B (10-18), each
  equal to GOG's file.
- 1,723 chunks are byte-identical.
- The differing dwords, classified by the generated ILP32 tables:
  - **pointer slots**: stale pointers of the saving process (list links, cached pointers).
  - **"data"**:
    - blob +0xCC..+0xF0 and the AIPlayer / Universe GrowSelection `maxNumShips` words: capacities. The exe's loaders
      overwrite them (LoadBlob 0x004feb30 sets maxNum = the loaded count).
    - Universe `players[8]` totals: cleared by the exe's LoadUniverse 0x00503910 itself (memset of 0x194C dwords), so a
      save after a load has 0.
  - **ship blocks past 0x638**: the mesh animation's header / curves pointers, the bindings' postCallback 0x004b68d0,
    the per-LOD binding pointers and the mesh-binding contents. The exe's Fix_madBindings 0x004b7000 rebuilds all of
    them (madAnimBindingsDupe).
  - **Every spline curve's three offsets are byte-identical**, so the .mad file-offset reconstruction is exact.
  - **unmapped**: the song INFO (2 vs 0: the replay has no sound); two clamp `clampedObjs` pointers; the ship-name
    table's first pointer (the exe's fix takes the ID); SinglePlayerGameInfo's grow-selection capacity and pointer;
    `smGhostMode` (gameStart sets it during a playback).

### 3.6 The mode-B rung: the user's GOG playback steps

Agents never run the GOG game or the DLL. These are the user's steps (from the main checkout,
`~/Documents/GameDev/Homeworld-Infection`; `tools/oracle/record_reference.sh` there is the same file as in the
worktree, and it installs the newest DLL build, `.claude/worktrees/m2/build/oracle-dll`):

1. Copy the recording's two files into the install's recorded games folder (do it for one name at a time, or both):
   ```
   cd ~/Documents/GameDev/Homeworld-Infection
   R=original/scratch-m2/m2r2-harness/runs
   cp $R/r9r-rec01/settings/SavedGames/RecordedGames/hwirec01 $R/r9r-rec01/settings/SavedGames/RecordedGames/hwirec01.pkts original/game/SavedGames/RecordedGames/
   cp $R/r9r-rec02/settings/SavedGames/RecordedGames/hwirec02 $R/r9r-rec02/settings/SavedGames/RecordedGames/hwirec02.pkts original/game/SavedGames/RecordedGames/
   ```
   The expected hashes:
   - `hwirec01`: `4640546c0708c284`, `.pkts` `e852ee9f3a457e3b`
   - `hwirec02`: `13a17b9f3a2d8664`, `.pkts` `d24604387e083cf5`
2. Verify each recording through the oracle DLL. This plays it back twice on the virtual clock, rng0 split:
   ```
   ORACLE_NAME=hwirec01 tools/oracle/record_reference.sh verify
   ORACLE_NAME=hwirec02 tools/oracle/record_reference.sh verify
   ```
   Each run collects `original/oracle/<NAME>-play-<timestamp>/<NAME>-play-a.hwo` and `-b.hwo` and prints a compare of
   a vs b, which should be IDENTICAL.
3. Compare the GOG playback stream with ours (one line per recording):
   ```
   python3 -I tools/oracle/hwo_compare.py original/oracle/hwirec01-play-<timestamp>/hwirec01-play-a.hwo \
     original/scratch-m2/m2r2-harness/runs/r9r-play01-a/run.hwo --from-tick 1 --to-tick 4800 \
     --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,players,dt,total \
     --json original/oracle/hwirec01-play-<timestamp>/compare-modeB.json
   ```
   Do the same for `hwirec02` with `r9r-play02-a`.

   Exclusions (both sides are playbacks, so `rng6` and `players` stay in):
   - `ships_rot`: the DLL hashes Ship+0x80 (decision 48).
   - `rng0`: the DLL splits stream 0.
   - `rng_all`, and `fpcw` (not filled by this engine).
   - `real`: the vqpc one-step alignment (decision 60).

   The rung is judged on the first divergent tick: IDENTICAL over 4,800 ticks passes. Otherwise the divergence is the
   next rung item, located with `/oracleShipDump` as in docs/m2-r9-oracle-rung-physics.md.

## 4. Subtitle.c:1144 (the theme table)

subTitlesUpdate 0x005842f0 computes the default theme as `0x00abdb20 + actor * 0x20` (0x0058437b). The table has 16
themes: subTexturesReset 0x00583990 walks 0x00abdb29 .. 0x00abdd28. Right after the table, 0x00abdd20 is
subTexturesReset's saved race and 0x00abdd40 its colour table.

The only actor past 15 in every run's reports is 0x70 (`index 112 out of bounds for type 'subtheme [16]'`, 315 reports
across runs). That is the location cards' actor: kasfLocationCard 0x00497710 and cat_nisSubtitle 0x004cf740. Both
prefix the text with `#r3#tf`, so subControlsScan replaces the region and the theme before anything reads them. The exe
only forms the out-of-table address; it does not read it.

CAT-FIX: an actor past the table starts from STT_LocationCard. Result: no Subtitle.c report in `r9r-sp`, `r9r-nis`,
`r9r-mt1-a/-b`, `r9r-m2-a` .. `r9r-m17-a` or `r9r-tut2-a/-b`.

## 5. The Tutorial1 leg as a byte-identical pair (the R8 audit's O1)

The leg keeps speech on. On SDL's dummy driver, the speech's subtitles started when the "soundstream" thread happened to
read the next phrase header, and ended when SDL's audio thread had consumed the audio. Both run on the wall clock.

Now `/catMissionTestTutorial` sets `/catVirtualAudio` (HWI-TEST):
- No audio device is opened and no stream thread is started.
- utyTasksDispatch's virtual branch calls `soundVirtualAdvance`. It mixes 22,050 samples per second of task time, in
  256-sample blocks with an exact accumulator, and runs one pass of the stream loop (`isoundstreamstep`, GoK's loop
  body unchanged) before each block.
- soundpause's fade-out wait mixes block by block instead of sleeping.
- A normal run is unchanged.

| run | done line | run.hwo |
|---|---|---|
| `r9r-tut-a` / `-b` (before the reviews' code changes) | `ticks=10444 ... prompts=7/102/158 ... lessons=12/12 complete=12/12 inOrder=1 nextClicks=11 result=PASS` | `7ce11b952c0ef020` both |
| `r9r-tut2-a` / `-b` (final build) | same line | `7ce11b952c0ef020` both |

The leg is now 4,758 ticks shorter than producer 3's wall-clock run (15,202 ticks). The narration takes its real length
in game time (prompt 3 at tick 158, not 1,295). Lessons complete at 1108, 1400, 1740, 1936, 4058, 4390, 4586, 6174,
7968, 8260, 9912 and 10348. The lesson saves are still written.

## 6. The mission self-test's combat (a): deferred, measured

The experiment (reverted, no stream moved) changed Mission01's destroy objectives:
- For 1,600 ticks after an objective appeared, the player's armed ships in space got the command layer's attack order
  on the objective's ships every 64 ticks (clWrapAttack).
- Only then did ShipsDamage run.

Result (`r9r-mt1-fight1`):
- 37 armed ships attacked the 14 Taiidan bombers 25 times between ticks 4784 and 6320 and killed none (14 targets
  each time). ShipsDamage then ended the objective at 6464.
- The 1,664-tick delay made the mission fail later: ExploreRegion appeared at 7056 but never completed;
  `done ... objectives=1/6 result=FAIL` at 29998.

Combat is not cheap here. The next step is to find why the attack order makes no kills (the bombers' reach or state at
that point of the script). **Owner: M2-R10 test.** Decision 77's ShipsDamage stays.

## 7. Delta-map review entries (decision 55)

All entries are in `ghidra/delta_map_review.json`, merged with `python3 -I tools/delta_map.py merge --done
m2r9:recorder-leftovers`. Counts after: IDENTICAL 2773, MODIFIED 1564, REWRITTEN 83, reviewed 3511 (before: 2761 /
1576 / 83 / 3346).

- **The 123 stage-4 IDENTICAL bodies without a review row** (AIAttackMan 3, AIDefenseMan 5, AIEvents 2, AIFleetMan 3,
  AIHandler 9, AIMoves 68, AIOrders 2, AIPlayer 11, AIResourceMan 2, AITeam 18):
  - Each was compared with the exe body: the decompile, plus the disassembly for every field offset against the
    tree's ILP32 layout, the constants and the callees. All are the same.
  - Ghidra's field names are HW1's and often wrong (aiaTimeAttack reads +0xBB4 airEasilyAccesibleRUsInWorld).
  - The notes record ICF folds: aimCreateControlResources' ship-died callback is nullsub 0x0049ae30, the fold of the
    tree's empty aimControlResourcesShipDied; aimCreateSupport / aimCreateSpecial callbacks fold onto
    aimShipDiedGuardShips 0x00406f00 / aimCloseKamikaze 0x0040d950.
  - The notes also record inlined NoAdd bodies and the extra 0 the exe writes to +0x950 in aiplayerInit.
- **Sample of the 179 unflagged MODIFIED / REWRITTEN rows outside TitanInterface.cpp** (31 rows, across 19 files):

| file | rows | result |
|---|---|---|
| SaveGame.c (all 6) | RegisterAllSpaceObjsInsideMe, RegisterAllSpaceObjs, SaveBlob, SaveAllSpaceObjs, SaveMissile, Save_CommandToDo | **2 found modified and ported** (recursion; univRemoveShipReferences on dead ships and the mimic's fake asteroid); SaveBlob → IDENTICAL (inlining); the other 3 same as the tree |
| NetCheck.c (1) | recPackRecordSaveHeader | found modified ("HEAD", 0x2B0, the exe's tpGameCreated), not ported: `CAT-TODO(stage 5)` |
| CommandNetwork.c (1) | KeepAliveDropPlayerCB | found modified (realtimeelapsed), not ported: `CAT-TODO(stage 5 LAN)` |
| MultiplayerGame.c (1) | mgUnitCapsEnabled | REWRITTEN: disables its button; not ported: `CAT-TODO(stage 5 multiplayer options)` |
| ETG.c (3) | etgAdd, etgSub, etgMult | IDENTICAL |
| Particle.c (2) | partSetTumble, partSetDeltaR | IDENTICAL |
| ObjTypes.c (2) | ShipClassToStr, ShipTypeToStr | IDENTICAL (NumToStr inlined) |
| texreg.c (2) | trLIFFileLoad, trColorsEqual | IDENTICAL |
| Select.c (2) | selStartup, selSelectionDimensions | same as the tree (ported deltas) |
| File.c (1) | fileDelete | platform (GoK's cwd-relative delete kept; the recorder deletes in the settings path itself) |
| DefenseFighter.c (2) | DefenseFighterHouseKeep, DefenseFighter_Fix | dormant in the exe; the tree deleted the file in M2-R4 |
| BMP.c, SoundEventPlay.c, TitanNet.c, ddraw.cpp, MultiplayerLANGame.c (1 each) | bmpFileOpen, soundEffect, CheckNetworkVersionCompatibility, hwDeleteWindow, lgShutdownMultiPlayerGameScreens | IDENTICAL (TTimerInit is TTimerDisable's fold) |
| Stats.c (1) | statsGetStatChecksum | same as the tree; a dead loop of 59 static reads below 0x0072a4f0 noted |
| ResCollect.c (1) | clCollectResource | same as the tree |
| StubTransform.c (1) | kniTransFreeVertexLists | the exe frees 10 trails; Transformer stubbed (decision 28) |

- **Every edited function with a row has a note** (42), among them [audit: all but one: recPackRecordInit 0x004cbd30
  (IDENTICAL, its two fileDelete calls became recPackDeleteFile) had none; the audit added it and merged: reviewed
  3512]:
  - Found modified and ported: clSendMove / clSendMpHyperspace / clSendFormation (IDENTICAL → MODIFIED).
  - Ported or extended: clWrapMove / clWrapFormation, recPackInGameStartCBSafeToStart, univFindNearestResource,
    SaveUniverse.
  - subTitlesUpdate (the CAT-FIX).
  - The recorder's HWI path fixes: recPackRecordPacket / Filename / PlayInGameInit / PlayInit.
  - The savers routed through the writer, compared where they had no review: SaveBullet, SaveMilitaryParade,
    Save_CameraCommand, SaveRetreatAtom (the exe's attack-atom saver), nebSave_Nebula.
  - The HWI-TEST sound changes: streamStartThread, soundinit, soundpause, utyTasksDispatch; and madFileLoad /
    madHeaderDelete.

## 8. CAT-TODOs

- **Closed**:
  - the 4 `CAT-TODO(stage 5 recorder)` sites (SaveGame.c 2, UnivUpdate.c 1, CommandNetwork.c 1);
  - 2 `CAT-TODO(stage 5)` in CommandWrap.c (the move path, the formation level), part of the send bodies.
- **Added** (one line each, owner first, the review findings above): `CAT-TODO(stage 5)` recPackRecordSaveHeader,
  `CAT-TODO(stage 5 LAN)` KeepAliveDropPlayerCB, `CAT-TODO(stage 5 multiplayer options)` mgUnitCapsEnabled.
- **Count after** (a Python scan of src/, since `grep` skips some files as binary here): 25 in all.
  - stage 5: 8
  - stage 5 LAN: 6
  - stage 5 multiplayer options: 8
  - stage 5 multiplayer: 1
  - M2-R10 physics: 2
  - recorder 0; none names M2-R8 or M2-R9.

## 9. Builds and gate runs (final build; plain harness)

`tools/build.sh --debug` and `--release`: rc 0, 0 warnings. Checks: `tools/m2_layout32.sh` OK;
`m2_save32_gen.py --check` current; `m2_kas2c_gen.py --check` current; `m2_kasf_table_gen.py --check` current.

| run | milestone | first stop | ASan | stream | against the audited reference |
|---|---|---|---|---|---|
| `r9r-mission-a/-b` (mission_test.sh 6000) | MISSION TEST PASS (perturb 1500 rng10, flipped copy 2000) | none | 0/0 | `fd82fafbcd9011fc` pair | `a9m3-mission-a` (`dc2fb8ce`): IDENTICAL 6,001 ticks |
| `r9r-mt1-a/-b` (Mission01 self-test) | done 6/6 PASS at 11424 | none | 0/0 | `ca7ee113b86a118b` pair | `a9m3-mt1`: IDENTICAL 11,426 ticks |
| `r9r-m2-a` .. `r9r-m17-a` | all PASS (m17: the game's end, credits) | none | 0 | 02 `deda1104`, 03 `3bef1f52`, 04 `c3570ef9`, 05 `f43d7602`, 06 `4f27fa01`, 07 `94ff4dd3`, 08 `a1ac2a5e`, 09 `892dca9a`, 10 `093c6982`, 11 `32b973a0`, 12 `77761ec5`, 13 `cff52c61`, 14 `2999a83a`, 15 `50ca3969`, 16 `e6beebb3`, 17 `91047333` | each IDENTICAL to `a9m2/a9m3-mN` (Mission06: the reference variant) |
| `r9r-tut2-a/-b` | tutorial 12/12 PASS | none | 0/0 | `7ce11b952c0ef020` pair | new (moved: the virtual audio clock; was not a pair) |
| `r9r-warp10-a` | warp start PASS | none | 0 | `55b3d9c6` | `a9m3-warp10-a`: IDENTICAL |
| `r9r-loadgame-a` ("01 - Hiigara", 3600) | PASS | none | 0 | `6a61f8b8` | `a9m3-loadgame`: IDENTICAL |
| `r9r-ai-a/-b` | ai self-test PASS | none | 0/0 | `89e5fb99fb9494b5` pair | `a9m3-ai`: IDENTICAL |
| `r9r-build-a/-b` | build PASS | none | 0/0 | `906bb558a953a8fc` pair | `a9m3-build`: IDENTICAL |
| `r9r-lfe2-a/-b`, `r9r-armed8` | skirmish PASS | none | 0 | `310be1aa` pair, `3176b730` | `a9m3-lfe2`, `a9m3-armed8`: IDENTICAL |
| `r9r-r17` (10-17 replay + `/oracleResave`) | replay done 9601 / 4800 | none | 0 | | `r9a2-r17-a` (producer 1's audit): IDENTICAL 9,601 ticks / 4,800 packets |
| `r9r-r18` (10-18 replay + `/oracleResave`) | replay done 3527 / 1763 | none | 0 | | `r9a2-r18-a`: IDENTICAL |
| `r9r-rec01`, `r9r-rec02`, `r9r-rec02-again` | skirmish PASS + recording | none | 0 | `03b6c875`, `633c0256` (×2) | section 3 |
| `r9r-play01-a/-b`, `r9r-play02-a/-b` | replay done | none | 0 | `5e45a4da` pair, `1c9fc262` pair | section 3.3 |
| `r9r-nis` (n06:26:36) | motion self-test done | none | 0 | | no Subtitle report |
| `r9r-fe` (120 s) | Main_game_screen | running at the timeout | **0** | | |
| `r9r-sp` (340 s) | universe running | `texreg.c (569): Assertion of (reg->nPalettes < 8)` (the known Mission04 stop, M2-R10 render) | **0** | | 40 bytes leaked after the fatal exit, as in producers 2 / 3; no Subtitle report |

**Stream moves by this producer:**
- The Tutorial1 leg, by design: wall-clock speech gave `2dbb97754c1ca05f` / `4215dcdb...` (not a pair); now the
  virtual audio clock gives `7ce11b952c0ef020` (a pair).
  [audit: against producer 3's audited `a9m3-tut` (`2dbb97754c1ca05f`): IDENTICAL in every field through tick 4057, first
  divergent tick 4058 in rng0 (and rng_all; 300 draws ahead), the fifth lesson's completion; the stream ends at 10,445
  instead of 15,203. The narration's earlier prompts move no simulation field.]
- Nothing else moved: every stream above is IDENTICAL to the stream the previous audits recorded.

The writer and SaveUniverse's delete passes run only when a game is saved. RegisterAllSpaceObjs' new behaviour does too
(the end-of-mission KAS saves). They did not move any mission stream: every mission's stream equals producer 3's audited
one over its full length.

## For STATE.md

- Stage 5 reduced: the recorder's record side is done and the 4 `CAT-TODO(stage 5 recorder)` sites are closed.
  hwi-engine records skirmish / multiplayer games in cataclysm.exe's recording format:
  - the save at version 0x10ad0027 with the exe's ILP32 layouts, through the new writer `SaveCat32.c saveCat32Save*`;
  - the `.pkts` file.
- Mode-B oracle: `/oracleRecord <name>` / `/oracleRecordStop <tick>` (harness `--record`, `--record-stop`).
  - The recordings `hwirec01` (Armed8) and `hwirec02` (LFE2) each replay in hwi-engine as a byte-identical pair.
  - Each replay is IDENTICAL to the recording run over 4,800 ticks, except rng0 / rng6 / rng_all (the computer player
    does not run in a playback).
  - The GOG playback (the rung) is the user's session (section 3.6).
- Names chosen:
  - `recordingStartSave` (0x00a578f8);
  - `recPackDeleteFile` (HWI-FIX);
  - `saveCat32Saving`, `saveCat32ConvertBack`, `saveCat32SaveStruct/Array/Ship`, `saveCat32SaveBegin/End`,
    `madCat32ObjectFileOffset`, `aitCat32SaveTeam`;
  - `soundVirtualClock`, `soundVirtualAdvance`, `isoundstreamstep`.
- Defaults chosen:
  - Only a recording's save is written in the exe format; the tree's other saves keep the native format.
  - Recorder files live in the user settings path.
  - A truncated stale pointer whose low half is 0 is written as 4 (non-NULL).
- New tools: `tools/formats/pkts_probe.py`, `tools/formats/recording_compare.py` (`--chunks`); engine switch
  `/oracleResave <name>` (with `/oracleReplay`).
- Tutorial1 leg: a byte-identical pair on the virtual audio clock (HWI-TEST `/catVirtualAudio`, set by
  `/catMissionTestTutorial`). The narration now takes its game-time length; the leg ends at 10,444 ticks.
- Subtitle: CAT-FIX at subTitlesUpdate 0x005842f0. An actor past the 16-theme table (the location cards' 0x70) starts
  from STT_LocationCard. The exe forms the address and never reads it.
- Delta map: 165 new review entries, 31 sampled rows (2 ported, 3 tagged for stage 5), counts 2773 / 1564 / 83,
  reviewed 3511.
- Ported in passing: RegisterAllSpaceObjs 0x004fe470 / RegisterAllSpaceObjsInsideMe 0x004fe400.

## What is left, and owners

- **The mode-B rung**: the user's GOG playback of `hwirec01` / `hwirec02` (section 3.6). Its first divergence, if any,
  goes to M2-R10 physics.
- **The mission self-test's combat** (decision 77): M2-R10 test (section 6).
- **Recording fidelity in stage 5** (deferred, decision 9): a recorded game sends nothing for these, so they do nothing
  in a recorded game (consistently in the record run and the replay):
  - the external-build packets (`cat_clSendExternalBuildUpdatePacket` 0x004523a0, clSendMisc 11);
  - the point / mimic / upgrade / special-target senders.
  These are the remaining `CAT-TODO(stage 5)` lines in CommandWrap.c. The skirmish AI did not use them in these two
  recordings.
- **The /packetRecord debug header** (`CAT-TODO(stage 5)`), **KeepAliveDropPlayerCB** (stage 5 LAN), **mgUnitCapsEnabled**
  (stage 5 multiplayer options).
- **The skirmish test's bigotry setting**: GOG's sessions had computer alliances (21 alliance packets at frame 88). Ours
  runs with the front end default. M2-R10 test, if a mode-B recording should include them.
- **The other 148 unflagged rows** outside the sample (ETG 45, Particle 41, ObjTypes 20, ...): a later audit.
- Unchanged: the sp gate's Mission04 texreg stop (M2-R10 render); Mission06's rare `rng_all` flake (the mixer's
  `RANDOM_SOUND` on the wall clock). The virtual audio clock here is the likely fix if the mission runs adopt it. That
  would move every mission's `rng_all` once. Owner: the oracle / M2-R10.
