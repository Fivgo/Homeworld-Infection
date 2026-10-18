<!-- done: probe, loader, packets, playback, compare -->
# M2-R6 oracle-mode-a: the user's reference recording replayed in hwi-engine

Status: DONE (M2-R6 oracle-mode-a producer; base commit `af50963`; nothing committed by the producer, the auditor
checkpoints).

## Headline

hwi-engine loads the user's reference recording `oracle01` (the cataclysm.exe 0x10ad0027 multiplayer save: 2 players,
47 ships, 1,532 asteroids, 59 blobs; 1,842 struct chunks converted from the exe's ILP32 layouts, 0 size problems) and
plays all 4,800 packets of `oracle01.pkts` on the lockstep branch to tick 9601 (`oracle: replay done ticks=9601
packets=4800 ships=81`, ASan 0, rc 0, 33 s). Two runs are byte-identical (sha256 `c55fd4b2d2e9afd0`).

**First divergence from the GOG stream** (`oracle01-rec.hwo`, ticks 1..9600, the fields both producers fill
comparably): **tick 136, `ships_health`**, one ship: ship ID 47 (0x2F), the Somtaaw destroyer player 0 builds
externally (builder ship ID 0x18; its `extbuildupdate` commands start at packet frame 62).
Owning function: **`cat_clExternalBuildUpdate` 0x0044dfc0** (tree: `clExternalBuildAdvance`, src/Game/CommandLayer.c).
The exe computes `health += (progress - oldProgress) * maxhealth` on the x87 at 53-bit precision (the GOG header's
control word 0x027f) and rounds once at the `fstp`; the tree rounds every float step. Evidence: the tick-136 health hash
recomputed with ship 47's x87 result 0x43160001 in place of the tree's 0x43160000 (and nothing else changed) equals the
GOG hash `b3990c7b2addc54d` exactly. That is a simulation fix: proposed for M2-R7
(below), not made here.

Two earlier differences are not divergences of this engine:

* **tick 1 `ships_rot`**: the GOG DLL hashes Ship+0x80 (tools/oracle/dll/hook_sites.h `CAT_SHIP_coordsys 0x80`); the
  exe's coordsys is at Ship+0x98 (missileShoot 0x0048aac0 uses `ship + 0x98`), so the DLL hashes the mesh-state and
  effect pointers and the visible-list node. Our tick-1 rotation hash equals the one computed from the save's own
  coordsys matrices (`fa135abf2b7b795b`). Tool defect, proposal below.
* **tick 93 `players`** (player 0 RU 2499 GOG vs 2500 ours) and first divergent packet 46 (tick 93, blobcheck
  1785610478 vs 1785610479): the stock game's own record/playback difference. `cmBuildTaskFunction` 0x00454940 returns
  while `playPackets` (0x00a7518c) is set, so a playback never charges the local player's build; the GOG **playback**
  stream of the same recording (`oracle01-play-20261017-144421/oracle01-play-a.hwo`) leaves the recording at tick 93 in
  the same way, and its blobSyncErr (4746, first frame 109) equals ours. Against that GOG playback stream the first
  divergence is the same tick 136 `ships_health`.

fsqrt experiment: not run. The task allows ONE experiment only when positions or velocities diverge first; they
diverge later (velocities 177, positions 178), health first. Decision 23 stands (sqrtf default, no switch added).

## Inputs (read only; never modified, never copied into git)

| file | bytes | sha256 (16) |
|---|---|---|
| `original/oracle/oracle01-rec-20261017-140428/oracle01` (= `original/game/SavedGames/RecordedGames/oracle01`) | 1,181,728 | `f9756e29ab39846b` |
| `.../oracle01.pkts` (= the RecordedGames copy) | 384,726 | `7121f1ce914e2583` |
| `.../oracle01-rec.hwo` (GOG DLL, record, 12,282 T / 6,140 P / 5 E) | 3,365,447 | `ae4a58729273dda1` |
| `original/oracle/oracle01-play-20261017-144421/oracle01-play-a.hwo` (GOG DLL, playback; cross-check only) | | `f1b38c21781a5d61` |
| `original/game/SavedGames/SinglePlayer/01 - Hiigara` (probe only) | 1,116,837 | `73f52d0766d309fc` |

The harness copies the recording pair into its data dir (`original/scratch-m2/m2r2-harness/data/SavedGames/RecordedGames`,
hashes equal) and links it into each run's settings dir. Not used: `original/oracle/pristine/`, the 12:36 / 13:30
session dirs, the stale `oracle01-play-dbg.hwo`, the `oracle01-campaign-INVALID*` pair.

## Tick alignment

The save is written after tick 1 (`univUpdateCounter` 1 in the Universe chunk), so the replay's first T record is tick
1. Packet k (frame k, 0..4799) is processed by the universe update at tick 2k+1 and the two ticks it releases are 2k+2
and 2k+3 (lockstep, two universe ticks per packet at 16 Hz); the last packet ends at tick 9601. The GOG recording ends
with `record-stop` at tick 9600, its T records go on to 12281 (the game kept running after the recording stopped), so
the comparison range is ticks 1..9600. The P records align by frame: our packet n is GOG's packet n (same tick).

## Comparison commands and outputs

Canonical run `r6a-final2-replay-a` (runs under `original/scratch-m2/m2r2-harness/runs/`):

```
python3 -I tools/oracle/hwo_compare.py original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo \
    <runs>/r6a-final2-replay-a/run.hwo                                                  # full range, rc 1
  HEADER: option bits differ: {determCompPlayer} vs {clock=virtual}
  ticks only in REF: 2681 (first 0, last 12281)
  FIRST DIVERGENT TICK 1 (last equal tick None); 1 field(s) differ, likely origin: ships_rot
  not compared (a file does not fill it): fpcw
  FIRST DIVERGENT PACKET 46 (REF tick 93, CAND tick 93): blobcheck 1785610478 vs 1785610479
  CAND: the game's own sync check counted errors: randSyncErr 4037, univSyncErr 4703, blobSyncErr 4746
        (first frames rand 1393 univ 195 blob 109)

python3 -I tools/oracle/hwo_compare.py <rec.hwo> <run.hwo> --from-tick 1 --to-tick 9600 \
    --json <runs>/r6a-final2-replay-a/compare.json                                       # rc 1, same first tick 1 ships_rot

python3 -I tools/oracle/hwo_compare.py <rec.hwo> <run.hwo> --from-tick 1 --to-tick 9600 \
    --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,players,dt,total,real
  FIRST DIVERGENT TICK 93 (last equal tick 92); players: REF ru/ships 2499/12 12500/11  CAND 2500/12 12500/11

... the same without players, --json <runs>/r6a-final2-replay-a/compare-sim.json
  FIRST DIVERGENT TICK 136 (last equal tick 135); ships_health REF b3990c7b2addc54d CAND 5393328381129e2c

... the same field list against oracle01-play-a.hwo (GOG playback)
  FIRST DIVERGENT TICK 136 ships_health; FIRST DIVERGENT PACKET 89 (tick 179): univcheck 1234478809 vs 1234478826
  REF sync errors: blobSyncErr 4746 only (first frame 109)
```

Exclusions: `ships_rot` (DLL offset defect above), `rng_all` (GOG splits stream 0, header `rng0=split`, and the sound
streams follow the mixer), `fpcw` (not filled by this engine), `players` only for the separate stock-playback reason.

Per field, the first divergent tick against the recording (`--fields <one>`): ships_health 136, rng6 161 (the AI stream:
the stock playback skips the computer players, aiplayerUpdateAll), ships_vel 177, ships_pos 178, univcheck 179,
resources 243, ships_misc 871, rng10 1376, bullets 1917, ships_n 2048. The engine's own sync check against the recorded
checksums: randSyncErr 4037 (first frame 1393), univSyncErr 4703 (first 195), blobSyncErr 4746 (first 109, equal to
GOG's own playback), pktSyncErr 0.

## Localisation of the first divergence

* Tick 136, field `ships_health`; last equal tick 135; packet 67 (frame 67) is processed at tick 135; no sync error
  is counted yet at that point (the first is blob, frame 109).
* Ship: ID 47 (0x2F), player 0's externally built destroyer; changing only its health word reproduces the GOG hash.
* Commands: `extbuildupdate` (exe type 20, 16 bytes; e.g. frame 62: `00000000 610b363a 02000000 1800 2f00` = a word 0,
  the progress float 0x3a360b61, 2, builder ID 0x18, ship ID 0x2F), received by `clProcessSyncPacket` 0x00450910 and
  handed to `cat_clExternalBuildUpdate` 0x0044dfc0.
* Owning function and evidence: as in the headline (0x43160001 vs 0x43160000; hash reproduced exactly).
* Proposal M2-R7 (simulation, not done): evaluate the health increment of `clExternalBuildAdvance` as the exe does: in
  double (`(double)(progress - old) * (double)maxhealth + (double)health`) and round once to float, as the x87 does at
  precision control 53 bits (to be checked against the exe's instruction sequence at 0x0044dfc0 before it lands: which
  operands are loaded as float, and whether the subtraction is done on the FPU too).

## Loader and parser deltas (exe addresses)

The Cataclysm save is a raw dump of the exe's ILP32 structs. The loader converts every struct chunk field by field into
this LP64 tree's struct before the unchanged load and fix-up code runs:

* `tools/m2_save32_gen.py` (new) compiles the tree's headers twice with clang `-fdump-record-layouts-complete` (`-m32
  -fshort-wchar`, as the exe, and native), matches the fields and writes `src/Game/SaveCat32Layouts.c`: 120 layout
  tables (copy runs, pointers sign-extended 4 -> 8 because a save holds IDs there, `long` and Windows' 2-byte `wchar_t`
  widened, arrays of sub-layouts) for 39 roots and 45 ship specifics, each with `_Static_assert(sizeof(T) == size64)`.
  Unions: padding members are weak, conflicting 64-bit views are listed (two: CaptainGameInfo's AddrPart IP/etherAddr,
  P1StandardCorvetteSpec's mimic overlay). `--check` verifies the file is current.
* `src/Game/SaveCat32.c` (new): the converter, chunk helpers (one struct, head + array, count read at an exe offset), the
  ship chunk (Ship 0x63C + specifics size32 - 4, + 0x7C PreciseSelection, + the madanim block whose spline curves are
  re-laid out with header-relative offsets) and the log line.
* `SaveGame.c`: `saveCatLoadHook` reads the file (it refused it); PreLoadGame 0x004fdfc0 (difficulty factor 0x00894194,
  race word, tpGameCreated 0x2B0: first 0xC8 bytes converted, the rest CAT-TODO(stage 5 multiplayer)); LoadGame
  0x004fe0e0 and SaveGame 0x004fdaf0 in the exe's order (state block 0x00c32c00, pre-game table, misc chunks, record
  tables, the Sect max-health post-pass); LoadUniverse 0x00503910 / SaveUniverse 0x005034c0 in the exe's order
  (ship-record prefix, blasts `cat_loadUniverseList5e288` 0x00516a10, bullet-effect recreate, ship-record table 0x2BC0,
  second grow selection, ship counters 0x00540130, frame-step INFO 0x00a6ed74); FixShip 0x00500950 rewritten in the exe
  order; asteroid catMimicShip saved as an ID (SaveAsteroid / FixAllSpaceObjs); ClampInfo with its selection when the
  host is -1; FOW grid cleared during a playback (`cat_saveLoadPlayerFOW` 0x004ff7e0); research info ends with
  `cat_playerUpdateUpgradesFromResearch`.
* `cat_loadGameMiscChunks` 0x005ba1e0 / `cat_saveGameMiscChunks` 0x005ba170 and `cat_spLoad/SaveMiscInfo` 0x005b9df0
  (Mimic.c: the disguise tables 0x00a76310 / 0x00a77ab0 / 0x00a762e8), `cat_load/saveShipRecordTable*` 0x0050f870..
  0x0050f8e0 (ShipNames.c), `cat_save/loadShipCounters` (UnivUpdate.c), the blast list (Blast.c), the state block
  (render.c `cat_rndStateBlockGet/Set`), LoadSinglePlayerGame 0x0051bf90 (10 INFOs, misc info, catSpBigGunAllowed).
* AIPlayer.c `aiplayerCat32Load`: walks the exe's AI chunks (aiplayerLoad 0x00412400) and starts the players fresh at the
  saved difficulty (the stock playback never runs them); the state itself is CAT-TODO(stage 4 AI).
* CAT-FIXes: `sizeship` rounded to 8 in LoadShip/SaveShip (the specifics behind an LP64 ship must be 8-aligned);
  `univAddAsteroid` 0x00540a30 sets catMimicShip NULL (the exe leaves allocator garbage that `univFindNearestResource`
  followed at tick 7313: SEGV); GameChat.c region user ID kept 64-bit (feUserRegionDraw SEGV).
* Fixes on the way that were defects of the port, not of the exe: InfoOverlay tooltip reads `mouseCursorOverShipType`
  (0x008ba740; tick-3501 SEGV), the SP-info chunk size mapping (0x48), the ship specifics' 32-bit size.

## Packets: histogram and receive sides

`oracle01.pkts`: 4,800 PACK records, no HEAD, frames 0..4799, every command parsed to its exact end:

| type | name | count | receive side |
|---|---|---|---|
| 1 | move | 52 | ported (exe layout: path type + up to 20 points) |
| 2 | attack | 9 | ported |
| 3 | formation | 1 | ported |
| 4 | dock | 2 | ported |
| 5 | createship | 28 | ported |
| 6 | collectresource | 26 | ported |
| 7 | protect | 40 | ported |
| 8 | buildship | 15 | ported |
| 10 | misc | 41 (sub 1: 2, 2: 1, 10: 29, 11: 9) | ported, all subs 1..11 |
| 13 | alliance | 21 | ported |
| 19 | research | 15 | ported |
| 20 | extbuildupdate | 10,881 | ported |
| 24 | upgrade | 4 | ported |

The other exe types (9, 11, 12, 14..18, 21..23, 25..27) are parsed by the same reader (`clProcessSyncPacket`,
CommandNetwork.c `clPkt*`) from the exe's offsets; they do not occur in oracle01, so they are untested. The send sides of
move / MP hyperspace / formation still build HW1 bodies: CAT-TODO(stage 5 recorder). The sync header is 0x1C bytes
(NetSyncChecksums + `catExtra`, static-asserted; every oracle01 header carries 0 there).

## Playback

`gpLoadRecordedGameGivenName` (GamePick.c) is LoadRecordedGame 0x00487cf0: VerifySaveFile, feScreenDisappear,
recPackPlayInGameInit 0x004cc010, utyLoadMultiPlayerGameGivenFilename 0x005aaa50, the recorded-game words 0x00a7519c = 1
and 0x00a751a0 = 1, 0x00a75190 from the "%s %i" name check. The universe update takes the packets through
recPackPlayGetNextPacket 0x004cbf00 and clWaitSyncPacket 0x00451280 -> clProcessSyncPacket; the computer players are
skipped (aiplayerUpdateAll); CheckPlayerWin returns during a playback. `/oracleReplay <name>` (main.c, OracleDump.c)
starts it once the main screen is up, runs on the virtual clock (one universe tick per frame), writes E `game-load`,
`playback-start`, `playback-end` and a P record per packet, and quits at the first NO_PACKET with
`oracle: replay done ...` rc 0. `/oracleReplay` turns sound and speech off: the mixer thread ends ambience sounds on the
wall clock and SoundEvent.c then draws RANDOM_SOUND_GAME_THREAD (stream 11), which made `rng_all` differ run to run from
about tick 476 (found by the first pair of final runs, `r6a-fin-replay-a/-b`, then localised per stream).

Harness: `tools/m2_harness.sh <label> --mode replay [--recording NAME]` (header documented; default oracle01; the pair is
copied into the data dir's SavedGames/RecordedGames when it differs, never written in original/game).

## Harness runs (all plain harness, silent, headless)

| run | build | result |
|---|---|---|
| `r6a-final2-replay-a`, `-b` | debug ASan | replay done ticks=9601 packets=4800 ships=81, ASan 0, rc 0, 33 s; **byte-identical**, 2,630,923 B, sha256 `c55fd4b2d2e9afd0` |
| `r6a-final2-rel-replay`, `-rel-replay-b` | release | replay done, rc 0, 7 s; byte-identical to each other (`a53c3c229eaa459a`); against debug all sim fields IDENTICAL over 9601 ticks and 4800 packets, `rng_all` differs from tick 1952 (stream 8, battle chatter, a debug-only speech path; not simulation) |
| `r6a-fin-mission-a`, `-b` (mission_test.sh, 6000 ticks, no perturb run) | debug ASan | MISSION TEST PASS, byte-identical, sha256 `bcb0ff6a537477d7`, ASan 0 |
| `r6a-final-fe` | debug ASan | Main_game_screen, ASan 0 (timeout 120, as designed) |
| `r6a-final-sp` | debug ASan | universe running, ASan 0 (timeout 360) |
| `r6a-fin-replay-a`, `-b` | debug ASan | (before the sound-off change) differ in `rng_all` only, tick 477 |
| earlier: `r6a-replay-try1..8`, `r6a-dbg-*` | debug | development and the stream-11 / stream-8 localisation (temporary instrumentation removed) |

**Mission01 stream**: before = motion-interp's final `mi-fin3-on-mission-a` sha256 `794eae2c20c68aac` (build 3aa4cce in the
header); after = `r6a-fin-mission-a` `bcb0ff6a537477d7`; `hwo_compare` mi-fin3-on-mission-a vs r6a-fin-mission-a:
**IDENTICAL**, 6001 ticks, no difference (header build identity 3aa4cce1 vs af50963a only).

Builds: `tools/build.sh --debug` and `--release` rc 0, 0 compiler warnings (-Werror). `tools/m2_layout32.sh` OK;
`python3 -I tools/m2_save32_gen.py --check`: current.

## Probe

`savegame_probe.py -v` reads both real files to EOF (OK; oracle01 3,377 chunks, sp=0, 47 ships, min ship 0x644;
Hiigara 2,354 chunks, sp=1 mission 1, 167 ships, min 0x63C). docs/formats/savegame.md corrected where the files
disagree or were unknown (13 places marked "(real file)" [audit: said 15; `grep -c` counts 13]): the state block, pre-game table and misc tables identified,
the 0x00894194 / 0x00a6ed74 words, the ShipSinglePlayerGameInfo 0x48 per ship, tpGameCreated's 0xC8 + 0x48 split, the
AI, command-layer, camera, ship-record and counter chunks, 27 RNG chunks, the 189 single-player INFOs in a multiplayer
save, the harvesting branch (absent), the open points closed.

## Delta-map review entries

`ghidra/delta_map.json`: 32 entries got `[review: M2-R6 oracle-mode-a] ...` (16 newly reviewed; counts.reviewed 2814):
PreLoadGame, LoadGame, SaveGame, LoadShip, SaveShip, FixShip, FixAllSpaceObjs, FixBlob, FixMissile, VerifySaveFile,
LoadDustCloud, Load_CameraCommand, LoadConsMgr, LoadBlob, LoadUniverse, SaveUniverse, LoadSinglePlayerGame,
SaveSinglePlayerGame, aiplayerLoad, Load_PlayerResearchInfo, Load_CommandToDo, clWaitSyncPacket, clProcessSyncPacket,
recPackPlayGetNextPacket, recPackPlayInGameInit, netcheckShow (16-byte ring entry), utyLoadMultiPlayerGameGivenFilename,
univAddAsteroid (CAT-FIX), univAddMissile, univCreateShip, CheckPlayerWin, FixClampInfo. [audit: two more edited IDENTICAL functions had no review entry and got one, `[review: M2-R6 oracle-mode-a audit]`: gcStartup (the LP64 region-ID cast) and nebLoad_Nebula (the nebula refusal); counts.reviewed 2816.] The Cataclysm-only functions
(cat_*Misc*, cat_*ShipRecordTable*, cat_*ShipCounters, cat_*UniverseList5e288, LoadRecordedGame, cat_clExternalBuildUpdate)
are not in the map (no HW1 counterpart); they are named at their call sites.

## CAT-TODOs

Closed (25 markers, by file): ConsMgr.c 9 and ResCollect.c 1 (`stage 5 recorder`: the exe's recorded-game conditions,
now `catNetworkGame()`), UnivUpdate.c 4 (`stage 3/5 recorder` univAddMissile / univCreateShip / CheckPlayerWin, `saves`
ship counters), SaveGame.c 3 (`M2-R6 save game` misc chunks, the FOW grid cleared on a playback load, `saves` the attack atoms'
save), CommandNetwork.c 2 (`stage 5` receive sides of the move path and the research level), CommandLayer.c 1 (the
halt while a game is recorded), CommandWrap.c 1 (misc 5-10 receive side), Globals.c 1 (`stage 3 recorder` globalsInit), InfoOverlay.c 1 (the
recording-ended word), ShipNames.c 1 and Mimic.c 1 (`the user's saves`). Gun.c's recorder condition was a comment, now
`catNetworkGame()` too.

Added (one line each, owner first): stage 4 AI (AI chunk conversion), stage 5 recorder (move/hyperspace/formation send
bodies; univDeleteDead* in SaveUniverse; the save skip at recording start 0x00a578f8), stage 5 multiplayer
(tpGameCreated after +0xC8), stage 5 LAN (0x00ab7658 set by gameStart), stage 6 level load (nebula chunks), stage 6
single player (mission-number mapping, cat_kasAutoSaveOnLoad, kasLoad's difficulty factor).

## Findings for other owners

* GOG DLL (tools/oracle/dll/hook_sites.h): `CAT_SHIP_coordsys` should be 0x98, not 0x80 (the ships_rot field is not
  comparable until a new recording is made with a fixed DLL). Proposal only; the DLL was not run or rebuilt.
* Latent 64-bit union aliasing in the tree: MCVSpec and P1StandardCorvetteSpec overlay members whose 64-bit offsets
  overlap differently than in the exe (listed by the generator).
* Render: UBSan `signed integer overflow 384 - INT_MIN` at Select.c:1166 (`selCircleCompute`, primGLToScreenY of a
  far-off ship) twice per replay; render-only.
* Release vs debug: stream 8 (RANDOM_BATTLE) draws differ from tick 1952 of the replay (a speech path compiled only in
  one build); simulation fields identical.

## What's left (owners)

* M2-R7 (simulation): the x87 single rounding of `cat_clExternalBuildUpdate`; then rerun this comparison for the next
  divergence (velocities at 177 are next in line).
* Stage 4 AI: the computer players' saved state (the replay does not need it: the stock playback never runs the computer players, aiplayerUpdateAll skips them while playPackets is set) [audit: said "oracle01 is a two-human recording"; its player 1 is "Computer 1" (the save's name chunk, `multiplayer=0` in the playback-start event), a skirmish against the computer].
* Stage 5: the recorder's record side (send bodies, univDeleteDead*, the save skip), LAN `catExtra`, the remaining 0x48
  CaptainGameInfo bytes.
* Stage 6: nebula chunks, single-player tail (KAS/objectives/timers/pings), mission-number mapping.
* Oracle tooling: the DLL coordsys offset; a second recording with nebulae and a single-player save replay.
