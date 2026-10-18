# Audit: M2-R6 oracle-mode-a (the reference recording replayed in hwi-engine)

Adversarial audit of the producer report `docs/m2-r6-oracle-mode-a.md` and its diff against `af50963` (the motion-interp
checkpoint; no WIP snapshot of this port exists, the whole port is in the working tree). Every check below was rerun by
the auditor; every engine run went through `tools/m2_harness.sh` (plain harness, silent, headless, `--bin` pointing at the
auditor's clean builds). Ghidra was not run; the exe was read through `tools/m2_fdec.py`, `tools/m2_fdis.py`,
`tools/m2_exedata.py` and the r6 disassembly `original/scratch-m2/m2r2-mesh-tools/exe.dis`.

## 1. Clean build

`original/scratch-m2/build-audit-r6-oracle-mode-a/{debug,release}`, set up from scratch with `tools/build.sh`'s
arguments (`--buildtype=debug -Db_sanitize=address,undefined`; `-Doptimization=2 -Ddebug=false -Db_sanitize=none`),
`-Werror` in build.ninja: both rc 0, **0 warnings** in either compile log. `hwi-engine` debug 87,127,488 B, release
5,362,248 B. `tools/m2_layout32.sh`: OK (45 ILP32 ship specifics). `python3 -I tools/m2_save32_gen.py --check`: current.

## 2. Runs (all `tools/m2_harness.sh`, runs under `original/scratch-m2/m2r2-harness/runs/`)

| run | binary | result |
|---|---|---|
| `aud-r6a-replay-a`, `-b` (`--mode replay`, default oracle01) | audit debug | `oracle replay done`, `ticks=9601 packets=4800 ships=81 randSyncErr=4037 univSyncErr=4703 blobSyncErr=4746 pktSyncErr=0`, `savecat32: ... 1842 chunks converted, 47 ships, 0 size problems`, ASan 0, rc 0, 33.2 s each; **byte-identical** (cmp), 2,630,923 B, sha256 `c55fd4b2d2e9afd0`, and byte-identical to the producer's `r6a-final2-replay-a` |
| `aud-r6a-rel-replay` | audit release | replay done, same counters, rc 0, 7.0 s, sha256 `a53c3c229eaa459a` = the producer's `r6a-final2-rel-replay`; against `aud-r6a-replay-a`: every simulation field IDENTICAL over 9601 ticks / 4800 packets, `rng_all` first differs at tick 1952 (as reported) |
| `aud-r6a-mission-a`, `-b` (`tools/oracle/mission_test.sh --ticks 6000 --no-perturb-run`) | audit debug | MISSION TEST PASS, byte-identical, sha256 `bcb0ff6a537477d7`, ASan 0 both; `hwo_compare` against the motion-interp audit's `aud-mi-fix-mission-on-a`: **IDENTICAL**, 6001 ticks (header build identity 3aa4cce1 vs af50963a only) |
| `aud-r6a-fe` (timeout 120) | audit debug | Main_game_screen, ASan 0 |
| `aud-r6a-sp` (timeout 360) | audit debug | universe running (univUpdateCounter > 0), ASan 0, no unsuppressed leak |

## 3. Numbers of the report, rechecked

* **Inputs** (sha256 first 16 hex, sizes): oracle01 1,181,728 `f9756e29ab39846b`, oracle01.pkts 384,726 `7121f1ce914e2583`,
  oracle01-rec.hwo 3,365,447 `ae4a58729273dda1`, oracle01-play-a.hwo `f1b38c21781a5d61`, `01 - Hiigara` 1,116,837
  `73f52d0766d309fc`; the RecordedGames copies in original/game and in the harness data dir hash the same. Correct.
* **original/game not written**: `find original/game -newermt '2026-10-17 15:00'` is empty (the recording pair is dated
  13:51 / 14:01 on 10-17, before the port).
* **Tick alignment**: the replay's first T is tick 1 (`playback-start ... tick=1`); packet k is processed at tick 2k+1
  (packet 46 at 93, 67 at 135, 89 at 179 in the compare output); the GOG stream's `record-stop` E record is at tick 9600
  and its T records run to 12281 (`ticks only in REF: 2681 (first 0, last 12281)`). Correct.
* **hwo_compare against oracle01-rec.hwo** (the report's four command lines rerun on `aud-r6a-replay-a`): full range ->
  first tick 1 `ships_rot` (`13f60ac3b9dc240e` vs `fa135abf2b7b795b`), `fpcw` not compared, first divergent packet 46
  (tick 93) blobcheck 1785610478 vs 1785610479, CAND sync errors 4037/4703/4746, first frames 1393/195/109; with the field
  list -> tick 93 `players` (2499/12 vs 2500/12); without `players` -> **tick 136 `ships_health`** `b3990c7b2addc54d` vs
  `5393328381129e2c`, last equal 135; against `oracle01-play-a.hwo` -> tick 136 `ships_health`, first packet 89 (tick 179)
  univcheck 1234478809 vs 1234478826, REF blobSyncErr 4746 only (first 109). Per field: ships_health 136, rng6 161,
  ships_vel 177, ships_pos 178, univcheck 179, resources 243, ships_misc 871, rng10 1376, bullets 1917, ships_n 2048. All
  as reported. GOG record vs GOG playback, same field list: tick 93 `players`, packet 46: the stock game's own
  record/playback difference, as reported.
* **Ship and record counts**: GOG `offset-check ... walked=47 live=47` at tick 1 and our tick-1 T `ships=47`; the save
  probe: 3,377 chunks, 1,532 asteroids (`BASIC|SPACEOBJ|objtype2`), 59 blobs, 47 ship chunks, minimum ship 0x644.
  Correct.
* **Packets** (an independent parser, `scratchpad/aud/pkts.py`, written from clProcessSyncPacket's offsets): 4,800 PACK
  records, no HEAD, frames 0..4799, every command parsed to the packet's exact end; histogram 1:52, 2:9, 3:1, 4:2, 5:28,
  6:26, 7:40, 8:15, 10:41 (subs 1:2, 2:1, 10:29, 11:9), 13:21, 19:15, 20:10,881, 24:4; the first extbuildupdate is frame
  62, `00000000610b363a0200000018002f00`; the header word +0x14 (`catExtra`) is 0 in all 4,800. Correct.
* **Owning function and evidence**: `cat_clExternalBuildUpdate` 0x0044dfc0 disassembled: mode 2 and the mode-0 branch
  are `fld [progress]; fsub [ship+0x3e8]; fmul [static+0xa4]; fadd [ship+0x170]; fst [ship+0x170]`, one rounding at the
  `fst` (the GOG header's control word 0x027f = 53-bit precision). From the producer's debug log (`r6a-replay-dbg4`,
  ship 47's operands per update) the auditor recomputed every step float-by-float and in double-then-round: they agree
  up to the update from 137.5 (0x43098000), where the tree gives 0x43160000 and the x87 0x43160001. The hash claim was
  checked exactly: the ships_health FNV-1a-64 was unwound by ship 47's last four bytes (multiplication by the inverse of
  the FNV prime) and re-run with 0x43160001: the result is `b3990c7b2addc54d`, the GOG value (ship 47 is the last ship in
  ShipList, so nothing else enters). The evidence is sound and the attribution correct.
* **Packet 46 explanation**: `cmBuildTaskFunction` 0x00454940 opens with `gameIsRunning && !0x00a2dccc && !0x00a7518c`
  (playPackets): confirmed.
* **ships_rot**: tools/oracle/dll/hook_sites.h has `CAT_SHIP_coordsys 0x80u`; missileShoot 0x0048aac0 multiplies by
  `(matrix *)(ship + 0x98)`. The DLL defect claim stands (the tree's own rotation hash value was not recomputed).
* **Pre-fix nondeterminism**: `r6a-fin-replay-a` vs `-b`: first difference tick 477 `rng_all`, every other field
  IDENTICAL: as reported.
* **Delta map**: 32 entries carry `[review: M2-R6 oracle-mode-a]`, 16 of them newly reviewed, counts.reviewed 2814 (the
  base file said 2797 while 2798 entries were reviewed; the producer's count is the true one).
* **Report errors** (two; fixed in the report, marked `[audit: ...]`): "15 places marked (real file)" in
  docs/formats/savegame.md: there are 13; "oracle01 is a two-human recording": the save's name chunk is
  `[Monty_Crabapple, Computer 1]` and the playback-start event says `multiplayer=0`: a skirmish against the computer
  (docs/formats/savegame.md itself lists the computer player's AIPLAYER chunk). The conclusion it supported (the replay
  does not need the AI state) still holds for another reason, now given.
* **Hygiene**: no game-derived file tracked or added (`git status`/`git ls-files`: only src/, docs/, tools/ and
  ghidra/delta_map.json; SaveCat32Layouts.c is generated from the tree's own headers); `fsqrt` is still
  `(real32)sqrtf((real32)(x))` (FastMath.h:15, decision 23).

## 4. Fifteen bodies compared with the exe

| # | exe function | tree | verdict |
|---|---|---|---|
| 1 | PreLoadGame 0x004fdfc0 | LoadPreGameInfo + PreLoadGame | same order: sp, SongNumber, numPlayers word, the 0x90 names chunk, race `!= 0` / two colours per player, then `tpGameCreated` (0xAC dwords = 0x2B0) or mission + 0x00894194, universeLoadEverythingNeeded unless the tutorial; no harvesting switch. The tree converts only the first 0xC8 bytes of tpGameCreated (CAT-TODO stage 5, one line) |
| 2 | LoadGame 0x004fe0e0 | LoadGame | same order (dmgStopEffect, registries, the 0x27-dword state block, cat_rndFogLayerTextureLoad, objects, blobs, the 3-entry table at 0x00892660 = 0x00a6b204 / 0x00a6b20c / 0x00883960, LoadUniverse ... cat_loadGameMiscChunks, timer, sp chunks, selections, background, lighting, sm, ran, two numbers, Determ, fixes, cat_loadShipRecordTableFix, effect list, close, the Sect max-health loop with infoFlags bit 2 = IF_InfoLoaded); cat_kasAutoSaveOnLoad is a one-line stage-6 CAT-TODO |
| 3 | LoadUniverse 0x00503910 | LoadUniverse | same: the lists, attack memory as RetreatList, cat_loadUniverseList5e288, cat_univRequestBulletEffectsRecreate, cat_loadShipRecordTable, resource volumes, two grow selections, stars, players (research info, cat_saveLoadPlayerFOW), player 8 zeroed, worlds, cat_loadShipCounters, 0x00a6ed74 |
| 4 | FixShip 0x00500950 (with LoadShip's +0x594 clear) | FixShip | same fields in the exe's order (+0x54, +0x32c, +0x1c4, +0x294, +0x344, +0x348, +0x4e8, +0x4ec, +0x4fc bindings, +0x370 command only, clamp +0x1bc with the host list, attack targets, +0x378..+0x380, +0x494, +0x3ec / +0x510, +0x608, inside ships, mad, +0x4ac, +0x4b0, harvest, +0x400, +0x420, Fix callback, droids, cat_univSetShipMaxHealth, the sp Somtaaw factor 0x008941a4) |
| 5 | cat_loadGameMiscChunks 0x005ba1e0 (+ cat_spLoadMiscInfo 0x005b9e90) | Mimic.c | rows 0x00a76310, counts 0x00a77ab0, base 0x00a762e8 in that order; the INFO copy per player (two counts, 0x54 types at stride 8), the object player's three `.index` words (0x00a77814 = row 672 +4), its two counts and the byte 0x00a77ad4: same |
| 6 | recPackPlayInit 0x004cc0e0 | NetCheck.c (not edited) | not on the replay path; latent difference noted below |
| 7 | recPackPlayGetNextPacket 0x004cbf00 | NetCheck.c | identical (HEAD skip, PACK check, size, body) |
| 8 | recPackPlayInGameInit 0x004cc010 | NetCheck.c | playPackets and recordplayPacketsInGame set, file opened, netcheck state cleared (the tree's netcheckInit): same |
| 9 | clWaitSyncPacket 0x00451280 | CommandNetwork.c | the playback branch: next packet, clProcessSyncPacket, memFree, PACKET_READY: same |
| 10 | clProcessSyncPacket 0x00450910 (header) | CommandNetwork.c | frame check only when not playing back, counter, record, netcheckShow, `numberOfCommands` at +0x18, commands from +0x1C: same; the tree static-asserts both |
| 11 | LoadRecordedGame 0x00487cf0 (disassembly) | gpLoadRecordedGameGivenName | VerifySaveFile (two messages: -2 -> 0x00bb3f00, else 0x00bb3efc = strings 568 / 567, the tree's Version / Invalid pair), feScreenDisappear, recPackPlayInGameInit, utyLoadMultiPlayerGameGivenFilename, then 0x00a7519c = 0x00a751a0 = 1, 0x00a75190 = 0 and set when a name at 0x00c39652 + 0x12 i (i = 1 .. numPlayers-1) differs from sprintf("%s %i", string 614 = strComputerName, i): same |
| 12 | aiplayerUpdateAll 0x00411240 | AIPlayer.c (not edited) | the computer players run only while 0x00a7518c (playPackets) is clear: the tree's `if (!playPackets)` is the same skip |
| 13 | receive side 1 (move) | clProcessSyncPacket | path type +0, from +4, count +0x10, points +0x14 (the exe passes the packet's pointer), selection +0x104, next +0x106 + 2n: same |
| 14 | receive side 10 (misc) | clProcessSyncPacket | data dword +0, misc number +4, selection +6, subs 0..11 incl. the module bits tables 0x0072a6d0 / 0x0072a708 and the one-ship Beast system test, next +10 + 2n: same |
| 15 | receive side 20 (extbuildupdate) | clProcessSyncPacket | player u16 +0, progress dword +4, mode +8, creator ID +0xC, ship ID +0xE, 16 bytes; cat_clExternalBuildUpdate's mode 0 / 1 / 2 = CAT_EXTBUILD_DONE / CANCEL / PROGRESS: same |

Also checked in passing against the decompiled switch: types 2-9, 11-14, 17-19, 21-27 (body offsets and lengths) all
match the tree's readers; type 16 has no case (the default continues after the command word, as the tree does).

## 5. Diff review (scope, CAT comments, CAT-TODOs, delta map)

* Every changed file is in the producer's list; every change serves the loader, the packet path, the playback, the
  recorded-game words or the oracle replay mode. No out-of-scope change found.
* Cataclysm changes carry `CAT:` / `CAT (M2-R6 oracle-mode-a)` comments with exe addresses; new files (SaveCat32.c/.h,
  SaveCat32Layouts.c) carry the tree's REULA header; HWI-only code is marked `HWI-ORACLE` / `HWI`.
* CAT-TODOs added by the port: 10, every one on one line with the owner first (stage 4 AI, stage 5 recorder x3,
  stage 5 multiplayer, stage 5 LAN, stage 6 level load, stage 6 single player x3: the mission-number mapping,
  cat_kasAutoSaveOnLoad, kasLoad's health factor).
  The closed tags (`M2-R6 save game`, `the user's saves`, `saves`, `save hook`, `stage 3 recorder`, the `stage 5 recorder`
  recorded-game conditions) are gone from the tree (grep).
* **Finding (fixed)**: two functions the delta map calls IDENTICAL were edited without a review entry: `gcStartup`
  0x00487020 (GameChat.c, the region user ID cast widened to `smemsize`, an LP64 CAT-FIX) and `nebLoad_Nebula` 0x004cb7f0
  (Nebulae.c, a dbgFatalf refusal for a cataclysm.exe save with nebulae). The auditor compared both with the exe (the
  exe passes the atom 0x00880f38 as the user ID; its nebula loader copies 0x58 / 0xFC chunks) and added
  `[review: M2-R6 oracle-mode-a audit]` entries; counts.reviewed 2814 -> 2816. The other IDENTICAL functions the diff
  touches either have this port's entry (LoadBlob, LoadDustCloud, Load_CameraCommand, netcheckShow, VerifySaveFile,
  clWaitSyncPacket, recPackPlay*, utyLoadMultiPlayerGameGivenFilename, FixBlob, FixMissile) or an earlier review entry
  (LoadMilitaryParade, Load_CameraStackEntry, LoadResourceVolumeCB: a chunk-conversion line added, no behaviour change);
  clSendProtect / packetBroadcastSync only precede inserted comment blocks.

## 6. Findings

1. **Missing review entries** (gcStartup, nebLoad_Nebula): fixed in ghidra/delta_map.json (section 5).
2. **Report: "15 places marked (real file)"**: 13 (`grep -o '(real file)' docs/formats/savegame.md | wc -l`); fixed.
3. **Report: "oracle01 is a two-human recording"**: wrong (player 1 is "Computer 1"); fixed with the right reason.

Notes for other owners (not defects of this port): the tree's `recPackPlayLoadHeader` (used by recPackPlayInit for
`/playpackets`, not by the replay) still compares a HEAD record with the LP64 `sizeof(tpGameCreated)`, while the exe
(0x004cc0e0) reads 0x2B0 bytes: a recording with a HEAD record would leave tpGameCreated unread (stage 5 recorder).
OracleDump.c's oracleFillTick gained a stray blank line (cosmetic).

## 7. DONE WHEN, re-judged on the plain harness

* replay mode loads oracle01 (no refusal from saveCatLoadHook: `3377 chunks, 560 fixed-size and 47 ship chunks checked,
  0 wrong`), 2 players and 47 ships = the GOG tick-1 count: **met**;
* runs to the end of the packets (tick 9601, packet 4800) with ASan 0 and writes run.hwo: **met**;
* two replays byte-identical: **met** (`c55fd4b2d2e9afd0`);
* hwo_compare against oracle01-rec.hwo run and the report states first divergent tick, fields, first packet and the
  owning function with exe address and evidence: **met** (tick 136 ships_health, packet 46 with the stock explanation,
  cat_clExternalBuildUpdate 0x0044dfc0, evidence reproduced exactly);
* mission stream IDENTICAL to motion-interp's: **met**; fe and sp ASan 0: **met**.

## 8. Precision

Sampled checks: 44 (29 numbers / claims of sections 1-3 and 5, 15 body comparisons). Wrong: 3 (the two report
statements and the missing review entries). Precision = 41 / 44 = 0.93.

Fixes: 3 (two report corrections marked `[audit: ...]`, one delta-map change adding two review entries plus the count).
No source file changed; the clean builds above are of the final source.

Verdict: PASS-WITH-FIXES - every DONE WHEN item reproduced on the plain harness from clean builds (replay 9601 ticks / 4800 packets ASan 0 and byte-identical, first divergence tick 136 ships_health owned by cat_clExternalBuildUpdate 0x0044dfc0 with the x87 hash reproduced exactly, mission IDENTICAL, fe/sp ASan 0); fixed two wrong report statements and two missing delta-map review entries
