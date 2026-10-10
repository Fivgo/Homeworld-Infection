<!-- done: savegame -->
# Save games (`SavedGames\SinglePlayer|MultiPlayer|Training\*`): layout and chunk order

M2-R1 item 2. Sources: Cataclysm `SaveGame` 0x004fdaf0, `PreLoadGame` 0x004fdfc0 (decompile truncated by Ghidra at
`dbgFatalf`, read with `objdump -d` 0x4fdfc0-0x4fe0e0), `LoadGame` 0x004fe0e0, `SaveThisChunk` 0x004fd7c0,
`LoadNextChunk` 0x004fd840, `SaveInfoNumber` 0x004fd940, `SaveUniverse` 0x005034c0, `SaveShip` 0x004ffd70,
`SaveAllSpaceObjs` 0x004ff080, `SaveBlob` 0x004fe7d0, `SaveSinglePlayerGame` 0x0051bee0 and the Cataclysm-only
`cat_saveGamePreGameTable` 0x004fda00, `cat_saveGameMiscChunks` 0x005ba170, `cat_spSaveMiscInfo` 0x005b9df0,
`cat_missionTimerSave` 0x004acbb0, `cat_saveShipRecordTable*` 0x0050f870/0x0050f8a0, `cat_saveShipCounters`
0x00540130, `cat_saveSavePlayerFOW` 0x004ff6b0, `cat_saveUniverseList5e288` 0x005169f0. HW1:
`ORIG@ad1bb82:src/Game/SaveGame.h` (chunk types, version), `SaveGame.c` (`SaveGame`, `LoadGame`, `PreLoadGame`,
`SavePreGameInfo`, `LoadPreGameInfo`, `SaveUniverse`, `SaveShip`, `SaveBackground`, `SaveLighting`),
`singleplayer.c` (`SaveSinglePlayerGame`); struct sizes from a clang `-fdump-record-layouts-complete` of the ORIG
headers (i686-pc-windows-msvc, `HW_Release`).

**No real Cataclysm save exists on this machine** (`original/game/SavedGames/*/` hold only empty `dummy.txt` files) and
producing one needs the GOG build to be played, which this round may not do. The probe is therefore checked with a
synthetic stream (`--selftest`) and against the empty files only. See Open points.

## Summary

* **Same container as HW1, same version number.** A save is a raw `udword 0x10ad0027` followed by HW1's chunk stream
  (`{sdword type; sdword contentsSize; contents}`), written with `fwrite` through `fileStream` (`SaveThisChunk`).
  `PreLoadGame` still rejects any other version (`cmp [esp+8],0x10ad0027` at 0x4fdff3). Because HW1 1.04 uses the same
  number (`SaveGame.h`), a HW1 save passes the version check and then fails on the first struct size: the version does
  not identify the game.
* **Saves are raw memory dumps of Cataclysm's structs**, so every struct growth changes the file: `Ship` 0x63C (HW1
  0x470) + `sizeofShipSpecifics` - 4 (+0x7C `PreciseSelection` if present, + MAD bindings); `Universe` chunk 0x3D9BC
  (HW1 12,448 = 0x30A0), dominated by 8 `Player`s of 0x6530 each (HW1 `Player` 392 B); `CaptainGameInfo` 0x2B0 (HW1
  0x268); `blob` 0x100 (unchanged); `SinglePlayerGameInfo` 0x54 (unchanged).
* **New chunks** (sub-types HW1 never used): `0x80000000` 0x9C bytes = 39 dwords from 0x00c32c00 written right after the
  pre-game block; `0x80004000` pre-game table (three `{ptr,size}` entries at 0x00892660: 0x00a6b204/4, 0x00a6b20c/4,
  0x00883960/4 = 12 bytes; CET calls the last one `SongNumber`); `0x8000B000` 0x17A0 bytes from 0x00a76310,
  `0x8000A000` 0x24 from 0x00a77ab0, `0x80009000` 0x24 from 0x00a762e8 (`cat_saveGameMiscChunks`); plus extra INFO
  numbers (single-player pre-game, `SaveSinglePlayerGame` +10 and `cat_spSaveMiscInfo` per player, mission timer,
  universe tail).
* **Order changes vs HW1**: the pre-game table chunk sits between the blobs and the universe; `cat_saveGameMiscChunks`
  and the mission timer follow `SaveSinglePlayerGame`; inside `SaveUniverse` the command-layer and render lists are
  written as INFO count + `LINKEDLISTSPACEOBJS` ID arrays, the attack memory is gone and ship-record tables, ship
  counters and per-player fog of war are added.

## On-disk layout

```c
/* little-endian, packed as written by fwrite */
udword   version;                      /* 0x10ad0027, no chunk header */
SaveChunk chunks[];                    /* until EOF */

typedef struct SaveChunk {
/*00*/ sdword type;                    /* flags | sub-type, see below */
/*04*/ sdword contentsSize;            /* >= 0 */
/*08*/ ubyte  contents[contentsSize];
} SaveChunk;

/* type flags (HW1 SaveGame.h, unchanged) */
#define BASIC_STRUCTURE      0x80000000  /* fixed-size struct dump */
#define VARIABLE_STRUCTURE   0x40000000  /* variable-size (ship = Ship + specifics, strings) */
#define INFO_CHUNK           0x20000000  /* one sdword */
#define LINKEDLISTSPACEOBJS  0x10000000  /* sdword count + count x sdword registry IDs */
#define AIPLAYER             0x08000000
/* sub-types: HW1 0x0010 SPACEOBJ (| ObjType in the low nibble) .. 0x1000 PING (SaveGame.h);
   Cataclysm adds 0x4000 (pre-game table), 0x9000, 0xA000, 0xB000 (misc per-player tables). */
```

### Chunk order written by Cataclysm `SaveGame` (read back by `PreLoadGame` + `LoadGame`)

| # | chunk(s) | HW1 | Cataclysm | evidence |
|---|---|---|---|---|
| 0 | raw `udword` version | 0x10ad0027 | 0x10ad0027 | SaveGame `local_8 = 6.8236703e-29` (= 0x10ad0027 as float bits); PreLoadGame cmp |
| 1 | INFO singlePlayerGame | yes | yes | |
| 2 | INFO song number | `musicEventCurrentTrack()` | same | |
| 3 | INFO numPlayers | yes | `numPlayers & 0xFFFF` | |
| 4 | BASIC playerNames 8 x 18 = 0x90 | yes | yes | `SaveStructureOfSize(0x00c39640, 0x90)` |
| 5 | 8 x {INFO race, INFO base colour, INFO detail colour} | yes | yes, Player stride 0x6530 | PreLoadGame loop `add esi,0x6530` to 0xb99e94 |
| 6 | SP: INFO currentMission | yes | yes **+ INFO 0x00894194 (new)** | SaveGame; PreLoadGame calls LoadInfoNumber twice |
| 6' | MP: BASIC `tpGameCreated` | 0x268 | **0x2B0** | `SaveStructureOfSize(&tpGameCreated, 0x2b0)` |
| 7 | `universeSaveEverythingNeeded()` (not in tutorial) | yes | yes | |
| 8 | **BASIC 0x9C: 39 dwords from 0x00c32c00** | - | new | SaveGame inline chunk `0x80000000`, size 0x9C; first chunk read by LoadGame |
| 9 | `SaveAllSpaceObjs`: INFO count, then per object chunk (ship `0x40000010`, bullet, resource, derelict, missile, ...) | yes | yes, bigger structs | SaveAllSpaceObjs, SaveShip (`0x63c` base) |
| 10 | `SaveAllBlobs`: INFO count + BASIC\|BLOB 0x100 each | yes | yes | SaveBlob chunk `0x80000040` size 0x100 |
| 11 | **`0x80004000` pre-game table (12 B)** | - | new | cat_saveGamePreGameTable; LoadGame copies it back |
| 12 | `SaveUniverse` (see below) | yes | changed | |
| 13 | 4 x IDToPtr tables (ship, resource, derelict, missile), sub-type 0x0F00 | yes | yes | SaveIDToPtrTable 0x005406c0 `CreateChunk(0xf00, n*4+4)` |
| 14 | `nebSave_Nebula`, `SaveConsMgr`, `aiplayerSave` | yes | yes (contents not compared) | |
| 15 | `SaveSinglePlayerGame`: BASIC 0x54, inside-ships list, grow selection | yes | yes **+ 10 INFO + `cat_spSaveMiscInfo` (per player 2 + 84 INFO, then 3 + 3) + 1 INFO** | 0x0051bee0, 0x005b9df0 |
| 16 | **`0x8000B000` 0x17A0, `0x8000A000` 0x24, `0x80009000` 0x24** | - | new | cat_saveGameMiscChunks 0x005ba170 |
| 17 | **INFO mission start time (0x00a4b8b4)** | - | new | cat_missionTimerSave |
| 18 | SP: `kasSave`, `objectiveSave`, `tmSave`, `pingSave`, VARIABLE\|STRING `CurrentLevelName` | yes | yes | |
| 19 | tutorial: `tutSaveTutorialGame` | yes | yes | |
| 20 | selections: `selSelected` + hot-key groups | `SaveMaxSelection` | `SaveSelection` (same body in HW1) | |
| 21 | background: INFO theta, INFO phi, STRING `btgLastBackground`; lighting: STRING `lightCurrentLighting` | `SaveBackground`/`SaveLighting` | same, inlined | |
| 22 | `smSave`, `ranSave` | yes | yes | |
| 23 | INFO actorFlagsEnabled, INFO smGhostMode | yes | yes | |
| 24 | `SaveConsMgrDetermOptional` (optional on load) | yes (1.04) | yes | |

### Inside `SaveUniverse`

| HW1 (SaveGame.c) | Cataclysm 0x005034c0 |
|---|---|
| BASIC\|UNIVERSE `sizeof(Universe)` (12,448) with players' mothership IDs and research info prefixed | `0x80000020`, 0x3D9BC bytes; `cat_saveShipRecordTablePreFix` first |
| `Save_CommandLayer(&mainCommandLayer)` | INFO todo count + `Save_CommandToDo` per entry |
| `Save_CameraCommand` | same |
| 4 render/space-object lists via `SaveLinkedListOfSpaceObjs` | 4 x `LINKEDLISTSPACEOBJS` ID arrays written inline |
| Ship, Bullet, Resource, Derelict, Impactable, Missile lists; mine formations | same |
| `SaveRetreatList` | INFO count + `SaveRetreatAtom` each |
| `SaveAttackMemory` | gone |
| - | `cat_saveUniverseList5e288`, `cat_saveShipRecordTable` (new) |
| resource volumes, housekeep grow selection | same, plus a second selection (`universe.unk1`) |
| per player `Save_PlayerResearchInfo` | same **+ `cat_saveSavePlayerFOW`** per player |
| - | `cat_saveShipCounters`, INFO 0x00a6ed74 (new) |

## Loader control flow

1. `VerifySaveFile` / `PreLoadGame(name)`: open, read the raw version, `dbgFatalf("Error %d loading game %s")` unless
   0x10ad0027; read the pre-game block (rows 1-7) including the extra single-player number; `universeLoadEverythingNeeded`.
2. `LoadGame`: read the 0x9C state block into 0x00c32c00, `cat_rndFogLayerTextureLoad`, `LoadAllSpaceObjs`,
   `LoadAllBlobs`, the pre-game table chunk (scattered back through the 0x00892660 table), `LoadUniverse`, the ID
   tables, nebulae, construction manager, AI players, `LoadSinglePlayerGame`, `cat_loadGameMiscChunks`,
   `cat_missionTimerLoad`, KAS/objectives/timers/pings/level name, tutorial, selections (`LoadSelectionAndFix`),
   background (`btgSetTheta/Phi`, `btgLoadTextures`), lighting, sensors manager, random, actor flags, ghost mode,
   optional cons-mgr determinism; then `FixAllSpaceObjs`, `FixAllBlobs`, `cat_loadShipRecordTableFix`, reset the effect
   list, close, and a post-pass over the Sect ship types (`FirstShipTypeOfRace[0]..LastShipTypeOfRace[0]`).

## Diff vs HW1

| Field / key / branch | HW1 (`ad1bb82`) | Cataclysm | Evidence |
|---|---|---|---|
| Version | 0x10ad0027 | 0x10ad0027 (identical) | SaveGame.h; PreLoadGame 0x4fdff3 |
| Chunk header | 8 B `{type, size}` | same | SaveThisChunk/LoadNextChunk; SaveShip writes `0x40000010` + size |
| SP pre-game | currentMission | currentMission + 1 INFO (0x00894194) | SaveGame; PreLoadGame |
| `tpGameCreated` | 0x268 | 0x2B0 | SaveGame |
| State block 0x9C | - | new BASIC chunk after the pre-game block | SaveGame / LoadGame |
| Pre-game table | - | `0x80004000`, 12 B | cat_saveGamePreGameTable 0x004fda00 |
| Misc tables | - | `0x8000B000` 0x17A0, `0x8000A000` 0x24, `0x80009000` 0x24 | cat_saveGameMiscChunks 0x005ba170 |
| SP game info | BASIC 0x54 + lists | same + 11 INFO + cat_spSaveMiscInfo | SaveSinglePlayerGame 0x0051bee0 |
| Mission timer | - | INFO | cat_missionTimerSave 0x004acbb0 |
| Ship chunk | `sizeof(Ship)` 0x470 + specifics - 4 (+PreciseSelection, +MAD) | 0x63C + specifics - 4 (+0x7C, +MAD) | SaveShip 0x004ffd70 |
| Universe chunk | 0x30A0 | 0x3D9BC | SaveUniverse 0x005034c0 |
| `Player` | 0x188 | 0x6530 | PreLoadGame stride |
| Attack memory | saved | not saved | SaveUniverse |
| FOW, ship records, ship counters | - | saved | SaveUniverse callees |
| Damage effects | - | `dmgStopEffect(ship, 3)` on every ship before saving | SaveGame |

## Probe

`tools/formats/savegame_probe.py [-v] FILE...` walks the chunk chain to EOF, decodes the pre-game block in Cataclysm
order, histograms chunk types, counts the Cataclysm-only chunks and checks ship chunk sizes against 0x63C.
`--selftest` builds a synthetic stream in memory.

```
selftest OK sp=1 mission=5 players=2; chunks=34 cat_only={'0x80004000': 1, '0x8000b000': 1} state9C=1 ship_chunks=1 min_ship=0x63c; ...
FAIL not a save (too short)  original/game/SavedGames/SinglePlayer/dummy.txt   (0-byte placeholder)
FAIL not a save (too short)  original/game/SavedGames/MultiPlayer/dummy.txt    (0-byte placeholder)
```

## Open points

* **Real-file check outstanding.** The two-real-files requirement cannot be met without a save produced by the GOG
  build. Next step for the user or a later round: make two saves during the M0 play-test (one single-player mission, one
  skirmish) and run `savegame_probe.py -v` on them.
* Contents of the 0x9C state block (0x00c32c00, also used by the mesh renderer and `cat_meshApplyBulletHit`), the
  three misc tables and the 0x00894194 / 0x00a6ed74 numbers are not identified.
* Per-object chunk layouts other than Ship/blob (bullet, derelict 0x1BC, missile, dust cloud) and `universeSaveEverythingNeeded`,
  `aiplayerSave`, `kasSave` were not compared field by field.
* HW1's `LoadPreGameInfo` disables resource collectors when harvesting is off in a multiplayer game; that branch is not
  visible in Cataclysm's `PreLoadGame` (UNVERIFIED where it went).
