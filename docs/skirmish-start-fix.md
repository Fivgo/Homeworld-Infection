# Skirmish start fix (branch `skirmish-start`, 2026-10-18)

Starting a skirmish from the front end died in the level load. This change ports the two MissionSphere readers from
the exe, adds a headless skirmish self-test (harness mode `skirmish`), and makes a fatal error or a crash visible in
the release build. The start-position shuffle itself stays deferred to stage 6 (decision 40): the port uses fixed
start positions and draws no random numbers, so the Mission01 oracle stream is unchanged.

## The bug

The user's sanitizer run (`original/play-home/logs/play-20261018-082635.log`, debug build) pressed Start on the
skirmish screen (`StartNewGame` -> `utyNewGameStart` -> `gameStart` -> `levelPreInit`) and stopped with:

```
../../src/Game/LevelLoad.c:2075:39: runtime error: index -1 out of bounds for type 'Player [9]'
../../src/Game/LevelLoad.c (1735): Fatal error - Assertion of (playerNumber < numPlayers) failed.
```

The first is `scriptSetShipsToBeNeeded` taking `&universe.players[-1]`, the second the HW1 assertion in
`scriptPreMissionSphereCB`. The user's release runs (`play-20261018-081815.log`, `play-20261018-101223.log`) end on
`WARNING: Stat file incorrect version` with no message at all.

## Root cause, with the exe evidence

Cataclysm's skirmish maps are MissionMan levels (`; MissionMan script` first line, so the 10-field sphere format). Each
carries a sphere for player -1, an ownerless `ResourceSphere` of race `?`, first, then one sphere for every start
position the map has room for, whatever the number of players chosen:

```
original/game_extracted/cataclysm/multiplayer/armed8/armed8.level
MissionSphere    -1,?,ResourceSphere,16759.0,-85466.0,0.0,30000.0,0.0,Resource.missphere,2000
MissionSphere    0,RACE_SECT,sMothership,...,Mothership_0.missphere,2000
...
MissionSphere    7,RACE_SECT,sMothership,...,Mothership_7.missphere,2000
```

The tree had HW1's readers: an assertion `playerNumber < numPlayers` and `universe.players[playerNumber]` with the raw
number, -1 included. The exe's readers (decompile and disassembly, `tools/m2_fdec.py`, `tools/m2_fdis.py`) differ:

| exe site | what it does |
|---|---|
| `scriptSetMissionSphereCB` 0x004a24d4..0x004a2507 | copies an 8-entry start-position table into a local: the dwords at 0x00ab77e0 when `multiPlayerGame == 0` (`rep movs`), else the 8 bytes at `tpGameCreated` +0xC8 (0x008860f0) |
| 0x004a2509..0x004a2520 | processes the sphere only if `playerNumber < 0` or `table[playerNumber] < numPlayers` (`cmp edx,ecx; jge` to the return): a position no player has is skipped |
| 0x004a2543..0x004a25bb | for `playerNumber >= 0` and not the tutorial: `Bad shiprace` fatal if the race id is 7 or more (`cmp esi,7; jb`, unsigned); a non-MissionMan level skips a sphere of another race; a MissionMan level sets `players[table[p]].race` only when the sphere's race is none of 2, 3, 0, 1 (Kushan, Taiidan, Sect, Beast) |
| 0x004a25c1..0x004a25e6 | the sphere's player: 1 in single player, else `table[p]`, or 8 (the misc player) for -1 |
| 0x004a2633..0x004a2689 | for `playerNumber >= 0` only: `players[table[p]]` gets the starting RUs and `PLAYER_ALIVE`, the game stats the RUs, `universeSwitchToPlayer(table[p])`; -1 touches no player |
| 0x004a26b0..0x004a27d1 | the capture-capital-ship gift inlined for `table[p]` (16-type capital list; the gift is the race's Hive Frigate, type 8 for the Sect and 0x2a for the Beast, with a compiled-out message "Giving Hive Frigate as default") |
| `scriptPreMissionSphereCB` 0x004a28d1..0x004a295b | the same table and skip; the sphere's player is `table[p]`, or -1 kept for the ownerless sphere |
| `scriptSetShipsToBeNeeded` 0x004a2f30 | reads the number on entry and indexes `universe.players` only after `-1 < number` |
| `gameStart` 0x005a9290 | in every non-tutorial new game sets 0x00ab77e0 to 0..7, then 20 swaps of two `ranRandomFn(0) % spScenarios[spCurrentSelected].maxplayers` picks |

**Why the release build died silently.** In the release build the assertions compile out (`DBG_ASSERT 0`,
`src/Game/Debug.h`), so the spheres of absent players and the ownerless one went on into the HW1 code and the
process died of a raw SIGSEGV, which prints nothing. The experiment run `sks-before-armed8-rel` reproduces it: the
same tree with HW1's readers restored, release build, armed8 with 2 players, rc 139, and its log ends on the same
`WARNING: Stat file incorrect version` line as both of the user's release logs. A `dbgFatal` was not the silent part:
its string already reached stdout, flushed, in every build.

## What changed, file by file

- `src/Game/LevelLoad.c`
  - `catMissionSphereStartPositions` (new, static): the readers' local copy of the table. Outside a network game it is
    `catStartPositions`; in a network game it is the identity, `CAT-TODO(stage 5 LAN)` (the tree's `CaptainGameInfo`
    has no +0xC8 table yet).
  - `scriptSetMissionSphereCB` and `scriptPreMissionSphereCB`: ported from the exe as in the table above, with
    `CAT:` comments. One `CAT-FIX`: a sphere number of 8 or more returns, because the exe would read past its local
    table of 8 (no level has one). Single-player and tutorial branches as the exe.
  - `isCapturableCapitalShip`: the exe's 16 types (`bDFGFrigate`, six Kushan and nine Taiidan capital ships) instead of
    HW1's 9 aliases. `GivePlayerCapitalShip`: the race's Hive Frigate, as the exe. One `CAT-FIX`: a race other than
    Sect or Beast gets nothing, where the exe takes an unset local as the type.
  - `scriptSetShipsToBeNeeded`: the player is looked up only after the `>= 0` test. Its other exe deltas are left with
    a one-line `CAT-TODO(stage 6 level load)`.
  - The skirmish self-test (`HWI-TEST`, not in the exe): `catSkirmishSelfTestSet` / `SetMap` / `Requested` /
    `Start` / `Tick`. The judgement now also requires player 0 human and player 1 the computer player.
- `src/Game/LevelLoad.h`: the self-test's prototypes.
- `src/SDL/utility.c`, `utility.h`: `catStartPositions[8]` (the exe's 0x00ab77e0, name chosen). `gameStart` sets
  it to 0..7 where the exe does, on one line marked `CAT-PROVISIONAL` (no shuffle until stage 6). The
  `CAT-TODO(stage 6 level load)` now says the readers are ported and only the 40 stream-0 draws are left. The
  self-test's start comes before the mission run in `utyGameSystemsInit`.
- `src/Game/MultiplayerGame.c`, `.h`: `mgSkirmish` sets one computer player on every entry, as HW1 and the exe
  (0x004b9440); GoK kept the previous count, which is 0 in a fresh options file, so no skirmish could start with the
  default settings.
- `src/Game/ScenPick.c`, `.h`: `catSpScenarioPickForTest` (`HWI-TEST`): what a click on a map line and OK do
  (`spNewItem`'s description game type, `spDonePicking`'s apply and selection). LiveFireExercise2 is commented out in
  `multiPlayerMissions.script` (the front end does not list it, in the exe either), so the test adds its line the way
  `spTitleListLoad` adds one.
- `src/Game/OracleDump.c`: the self-test's judgement after each universe tick, as the other self-tests.
- `src/SDL/main.c`: the `/catSkirmishTest <ticks>` and `/catSkirmishMap <name>` switches; `dbgFatalSignalsInstall()`
  at the start of `main`.
- `src/Game/Debug.c`, `.h` (`HWI-FIX`): `dbgFatal` flushes stdout, then writes the fatal string to stderr before the
  deliberate fault (`dbgAllowInterrupts`, kept as is) or the exit. `dbgFatalSignalsInstall`: in a build without ASan
  (the release build) SIGSEGV, SIGBUS, SIGFPE, SIGILL and SIGABRT print `hwi-engine: fatal signal N (the engine
  crashed; any fatal error message is above)` on stderr, then the default action ends the process as before. The
  debug build keeps ASan's own report.
- `tools/m2_harness.sh`: `--mode skirmish`, `--skirmish-map NAME` (default LiveFireExercise2), `--skirmish-ticks N`
  (default 600), the milestone "skirmish self-test done" and a `skirmishtest:` summary line, documented in the header.
- `ghidra/delta_map_review.json` (decision 55): new rows for `scriptSetMissionSphereCB` 0x004a2420 (MODIFIED),
  `scriptPreMissionSphereCB` 0x004a2820 (IDENTICAL/medium before, now MODIFIED), `scriptSetShipsToBeNeeded` 0x004a2f30
  (MODIFIED), `mgSkirmish` 0x004b9440 (IDENTICAL, the tree's GoK change restored); notes added to `dbgFatal` 0x00462a90
  and `gameStart` 0x005a9290. `delta_map.json` is not merged here.

## Test results

Every run went through `tools/m2_harness.sh` (silent, offscreen, no input) on the plain harness data, from clean builds
of this change (`tools/build.sh --debug --clean` and `--release --clean`: `[316/316]`, 0 warnings under `-Werror`).
Labels are under `original/scratch-m2/m2r2-harness/runs/`.

| label | build | result |
|---|---|---|
| `sks-final-lfe2-a`, `-b` | debug | skirmish LiveFireExercise2, 600 ticks: **PASS**, ASan 0, rc 0; 89 ships; player 0 Sect `sMothership`, player 1 Beast `bMothership` (computer); 20 asteroids; `run.hwo` 147,967 B, both sha256 `ae51552a6c391e1d` (byte-identical) |
| `sks-final-armed8-a`, `-b` | debug | skirmish Armed8 with 2 players: **PASS**, ASan 0, rc 0; 54 ships, 0 ships of players 2..7 (their spheres skipped), 24 ships of the misc player (the resource sphere's crystals), 242 asteroids; both sha256 `34c9f43ad745bbfd` |
| `sks-final-rel-lfe2`, `sks-final-rel-armed8` | release | **PASS** on both maps (the binary the user runs) |
| `sks-final-rel-fatal` | release | isolation data dir (scratchpad, a test map whose spheres name a missing file): `Fatal error - fileOpen: cannot open file MultiPlayer\FatalTest2\NoSuchSphere.missphere` in the log, rc 213 (the fatal exit code) |
| `sks-before-armed8-debug` | debug, experiment | this tree with HW1's readers restored: the user's two reports reproduced (`index -1 out of bounds for type 'Player [9]'`, then the assertion in `scriptPreMissionSphereCB`) |
| `sks-before-armed8-rel` | release, experiment | the same: SIGSEGV, rc 139, log ends on `WARNING: Stat file incorrect version` and now `hwi-engine: fatal signal 11 ...` |
| `sks-final-mission-a`, `-b`, `-perturb` | debug | `tools/oracle/mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000`: **MISSION TEST PASS**, 1,476,370 B, sha256 `6bac08c64992e462` |
| `sks-base-mission-a`, `-b`, `-perturb` | debug, base a3cc7f4 | the same test on a clean build of the base commit (git archive into `original/scratch-m2/build-skirmish-base`): MISSION TEST PASS, sha256 `0ea19f070fede4c0` |
| base vs this change | | `hwo_compare.py` a vs a and b vs b: **IDENTICAL**, 6,001 ticks; the only differing bytes are 17..20 of the 64-byte header (the build's git hash) |
| `sks-final-fe` | debug | `Main_game_screen`, ASan 0 |
| `sks-final-sp` | debug | `--timeout 340`: universe running, `univUpdateCounter` 3,264, ASan 0 |
| `sks-final-dock`, `-build`, `-ui`, `-fescreens` | debug | **PASS** each, ASan 0, rc 0 |

The same runs before the last edit of the judgement (`sks-lfe2-a/b`, `sks-armed8-a/b`, `sks-mission-*`, `sks-dock`,
`sks-build`, `sks-ui`, `sks-fescreens`, `sks-fe`) gave the same results and the same stream hashes. The first sp run
(`sks-sp`) lost its harness process after the engine's run and wrote no summary; it is not counted. The sp and
skirmish runs report a non-fatal UBSan misaligned load in `BTG.c:388`, which earlier rounds' runs already show.

## What the user should expect

- Starting a skirmish from the front end loads the map and runs (both builds; `tools/play.sh --bin
  <worktree>/build/release/hwi-engine`). The skirmish screen starts with one computer opponent each time, as the exe.
- Start positions are fixed: player i starts at the map's position i. The exe shuffles the positions at every new
  game with 40 draws from random stream 0; that comes with stage 6.
- LiveFireExercise2 is not in the front end's map list, as in the exe. Armed8 and the other listed maps are.
- With the capture-capital-ship option on, a player without a capturable capital ship now gets the race's Hive
  Frigate (the exe's gift), not a Taiidan Standard Frigate.
- If the release build crashes, the log (and the terminal) now ends with `hwi-engine: fatal signal N ...`. A fatal
  error's message goes to stderr before the exit.

## What is left

- The shuffle's 40 stream-0 draws in `gameStart` (`CAT-TODO(stage 6 level load)`): they move the Mission01 stream's
  `rng0` when they land.
- A network game's start-position bytes at `tpGameCreated` +0xC8, shuffled by `mgStartGameCB` 0x004bc7a0 and
  `lgStartGame` 0x004c5900 (`CAT-TODO(stage 5 LAN)`).
- `scriptSetShipsToBeNeeded`'s other deltas and `levelPreInit`'s need-capital-ship words (`CAT-TODO(stage 6 level
  load)`).
- The next `delta_map.py merge` folds the new review rows into `ghidra/delta_map.json`.
