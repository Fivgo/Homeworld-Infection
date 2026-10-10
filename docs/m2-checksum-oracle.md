<!-- done: hw1-checksum, cat-checksum, hook-design, driving, recreation-side, risks -->
# M2 checksum oracle: design (M2-R1 item 5)

Status: DESIGN ONLY, nothing built or run. Written 2026-10-10 for M2-R1 (`cycle/M2_BRIEF.md` item 5).

Goal: a behaviour oracle for the recreation. The GOG build (`original/game/cataclysm.exe`, build 12948) writes, once per universe tick, the engine's own sync checksums plus a few diagnostic sub-checksums to a file; `hwi-engine` writes the same stream; a comparison tool reports the first tick where they differ and which sub-checksum moved first.

Sources and labels:
- `ORIG@ad1bb82:` = Relic's untouched 2003 source, the root commit of the Gardens of Kadesh history. The reference clones are shallow and the harness refuses `git` against them from this worktree, so the two directories this doc needs (`src/Game`, `src/Win32`) were downloaded as GitLab archives of commit `ad1bb82c` into scratch (`original/scratch-m2/oracle-ad1bb82-dl/`, untracked, outside git). `src/Game` has 302 files, the count `docs/hw1-port-lessons.md:76` gives for the original tree. Original file names are lower-case (`univupdate.c`, `netcheck.c`).
- `CAT:` = the round-5 named Ghidra export `ghidra/exports/cataclysm_gog_r5/` (functions.csv, symbols.csv, decompiled.c, call_graph.csv, strings.csv).
- `CET:` = `reference/CataclysmEngineTweaks` (read-only), summarized in `docs/cet-knowledge.md`.
- UNVERIFIED / GUESS marks anything not read from a file or command output.

---

## 1. HW1: what the sync checksum is, when it is computed, what it covers

### 1.1 The three numbers in every sync packet

`ORIG:CommandNetwork.h:207-233` (inside `#if SYNC_CHECK`, and `ORIG:switches.h:16` sets `SYNC_CHECK 1`, so it is compiled into release builds):

```c
typedef struct {
    uword  packetnum;   // packetnum this checksum is for
    uword  randcheck;   // random # generated, but if == 0, indicates all checksums should be ignored
    real32 univcheck;   // universe checksum
    udword blobcheck;   // blob checksum   (really the stats/cheat checksum, see below)
} NetSyncChecksums;

typedef struct {
    uword type; uword from; udword frame;
    NetSyncChecksums checksums;     // #if SYNC_CHECK
    uword numberOfCommands;          // commands follow
} HWPacketHeader;
```

| field | computed by | covers |
|---|---|---|
| `randcheck` | `gamerand()` = `ranRandomFn(RAN_Game)` (`ORIG:randy.h:105,114`) | **Draws** the next value of the game RNG stream. It is a side effect, not a read: computing the checksum advances `RAN_Game` by one step per sync packet. A recreation must do the same draw at the same moment or every later random number shifts. |
| `univcheck` | `univGetChecksum(&n)` (`ORIG:univupdate.c:5096-5366`) | `float` sum of `posinfo.position.x`, `.y`, `.z` over (a) every ship in `universe.ShipList` without `SOF_Dead`, (b) every derelict in `DerelictList` without `SOF_Dead`, (c) **every** resource in `ResourceList` (no dead test), accumulated into three `real32` locals `x`, `y`, `z` in list order, returned as `x+y+z`. `n` = number of objects summed. Nothing else (no health, no velocity, no bullets, no missiles) enters the value; the rest of the function only writes the verbose net log. |
| `blobcheck` | `statsGetStatChecksum()` (`ORIG:stats.c:2316-2396`) | `udword` sum over `tpGameCreated.numPlayers` players of `totalships`, `resourceUnits`, `crc32Compute(shiptotals, TOTAL_NUM_SHIPS*4)`, `crc32Compute(shiptotals, NUM_CLASSES*4)` (Relic's code hashes `shiptotals` twice; the second was meant to be `classtotals`), `researchinfo.HasTechnology`, `researchinfo.listoftopics.num`, then `players[i].bounty` over `universe.numPlayers`. A mismatch here calls `SendCheatDetect()` (`ORIG:netcheck.c:916-917` [audit: was ":902-904"; the `if (multiPlayerGame) SendCheatDetect();` is at 916-917]). |

`bobGetChecksum` (`ORIG:blobs.c:1567`, sum of collision-blob centres) exists but has no caller in `ORIG:src/Game`. `univCalcShipChecksum` (`ORIG:univupdate.c:5375-5626`, a `udword` sum of ~150 ship fields reinterpreted as integers with `TreatAsUdword`, `ORIG:TYPES.H:186`) has its whole ship loop commented out; what is left adds 128 per live bullet with an owner. It is used only by the debug-only `GOD_LIKE_SYNC_CHECKING` path (`ORIG:Globals.h:25-29`: defined only `#ifndef HW_Release`).

### 1.2 When it is computed: once per sync packet, i.e. every second universe tick

Constants: `UNIVERSE_UPDATE_RATE 16`, `UNIVERSE_UPDATE_PERIOD (1/16 s)` (`ORIG:universe.h:57-58`); `CAPTAINSERVER_PERIOD 1.0f/8.0f` (`ORIG:CommandNetwork.h:124`).

`universeUpdateTask` (`ORIG:universe.c:3765-4052`) has two branches while `gameIsRunning`:

1. **Lockstep branch** when `multiPlayerGame || playPackets || recordFakeSendPackets` (`:3885`): `clWaitSyncPacket()` (`:3946`) and, if a packet is ready, `netCheck()` (queue statistics only, `ORIG:CommandNetwork.c:2450-2490`), then **two** `univUpdate(UNIVERSE_UPDATE_PERIOD)` calls (`:3957`, `:3972`, with a `taskYield` between them unless catching up). One sync packet = two universe ticks.
2. **Free-running branch** otherwise (single player, ordinary skirmish): one `univUpdate` per task tick (`:4027`), and **no checksum is ever computed**.

Inside `clWaitSyncPacket` -> `clProcessSyncPacket` (`ORIG:CommandNetwork.c:1501-1530`) the order is:

1. packet-number check (`pktSyncErr`);
2. if `recordPackets`: append the packet to the `.pkts` file;
3. **`netcheckShow(packet)`** (`ORIG:netcheck.c:937-985`): compute `univcheck`, `blobcheck`, then `randcheck = gamerand()`; store them in a 128-entry ring `netsyncchecksums[packetnum & 127]`; compare the copy carried in the incoming packet's header (`netcheckCheckChecksum`, `:853-925`): `randSyncErr`, `univSyncErr`, `blobSyncErr` counters plus the first frame of each (`*SyncErrFrame = universe.univUpdateCounter`); write the log line `"Packet:%d Rand:%d Checksum:%f %d CDET:%d\n"` or the binary `binnetlogPacket` record if a net log is open;
4. only then execute the packet's commands.

So the checksum describes the state **after** the previous packet's two ticks and **before** this packet's commands. The sender fills the header with the most recent locally computed entry (`netcheckFillInChecksum`, `ORIG:netcheck.c:832-842`, called when the captain builds a sync packet, `ORIG:CommandNetwork.c:2144-2148`). `randcheck == 0` marks "ignore" (`:857-860`), used before the first checksum exists.

### 1.3 The per-tick order of `univUpdate` (`ORIG:univupdate.c:7422-7738`)

The checksum stream only proves what the tick did; the order matters for placing hooks and for the recreation's dump point:

1. debug autosave (`autoSaveDebug`), GOD sync (debug only);
2. `universe.univUpdateCounter++`, `phystimeelapsed`, `totaltimeelapsed += dt`;
3. quit/win timers; multiplayer resource regeneration (every `regenerateRUrate` frames);
4. collision blobs (`collUpdateCollBlobs` / `collUpdateObjsInCollBlobs`, battle pings);
5. `cloudSetFog`, `nebSetFog`, `cmDeterministicBuildProcess`, `cmBuildTaskFunction`, `rmUpdateResearch` (every `REFRESH_RESEARCH_RATE`);
6. resource injections / lump sums, alliance timeout (multiplayer);
7. first tick only: `univSetupShipsForControl`, `gameStatsCalcShipCostTotals`;
8. `wkTradeUpdate`; `singlePlayerGameUpdate` (SP, runs KAS) or mid-level hyperspace;
9. `collCheckAllBulletMissileCollisions`;
10. `cratesUpdate`, **`aiplayerUpdateAll`** (computer players; skipped entirely when `playPackets`, `ORIG:AIPlayer.c:859`);
11. `clProcess` + `clPostProcess` (command layer), `tacticsGlobalUpdate`, `univUpdateMineWallFormations`;
12. `collCheckAllBumpCollisions`;
13. position/velocity integration: minor objects, bullets, missiles, then derelicts, resources, ships (or the paused variants), effects;
14. delete dead ships/resources/derelicts/missiles; regrow resources;
15. new-level load (SP); bounties.

The natural "end of tick" point is the return of `univUpdate`. That is where the oracle samples (section 3), not inside `netcheckShow`.

### 1.4 Randomness that the checksum depends on

`ORIG:randy.c`: 25 independent streams (`RAN_Game` = 10, `RAN_AIPlayer` = 6, `RAN_Damage` 7, `RAN_Battle` 8, ...; `ORIG:randy.h:34-59` [audit: was ":25-50"; the `RAN_*` defines and `RAN_NumberStreams 25` are at lines 34-59]), each a `{x,y,z,c,n}` subtract-with-borrow generator plus a 69069 LCG (`ranRandomFn`, `ORIG:randy.c:58-104`, integer-only). All streams start from fixed constants (`ranParametersReset`, `:181-190`, called by `ranStartup`). Non-deterministic seeding happens in exactly one sim-relevant place:

- `aiplayerStartup` calls `ranRandomize(RAN_AIPlayer)` (advance by `GetSystemTime().wSecond` steps) **unless** `determCompPlayer` (`ORIG:AIPlayer.c:684-687`, `ORIG:randy.c:198-210`). The `/determCompPlayer` switch exists only in non-release builds (`ORIG:Win32/main.c:1272-1280`, inside `#ifndef HW_Release`). So in a retail HW1 build, two runs of the same AI-vs-AI skirmish diverge as soon as an AI makes a random choice, unless the wall-clock second matches. This is the main reason section 4 does not pick a fresh skirmish.
- All 25 streams are saved and restored with save games (`ranSave`/`ranLoad`, `ORIG:randy.c:268-286`, called from `ORIG:SaveGame.c:654,775`).

### 1.5 Why it is deterministic only on the same binary

Facts:
- `univcheck` is a float sum; a one-ulp change in any coordinate changes it. Float code is not bit-reproducible across compilers or flags.
- Relic built `univupdate.c` (which holds `univGetChecksum` and the integration calls), `physics.c`, `tactics.c`, `etg.c` and 8 other files with **MSVC `cl -Od`**, unoptimised even in release, and most other files with **Intel C `/Ogitb1 -G6`** (`docs/hw1-port-lessons.md:27-35`, `ORIG:Win32/MAKEFILE:62-78,144-149`). Under `-Od` each `x += ...` is stored to a 32-bit memory slot, so the sum rounds to `float` after every addition. Optimised x87 code may keep partial sums in 80-bit registers and round only at a spill, which gives different low bits.
- The x87 precision-control word matters: the MSVC runtime starts a Win32 process at 53-bit precision (standard behaviour, not verified here for this exe or Wine); Direct3D lowers it to 24-bit unless the device is created with `D3DCREATE_FPU_PRESERVE`. CET's D3D9 rGL sets `FPU_PRESERVE` because "Audio and various other things will break in Cataclysm if D3D changes the FPU precision" (`docs/cet-knowledge.md:1294`, `RGL/D3D9 d3dinit.cpp:134-143`). So the precision in effect depends on the renderer. GUESS: the OpenGL path through `rgl.dll` leaves it at 53 bits.
- SSE ("Katmai") is used only for vertex transforms in the renderer (`ORIG:transformer.c:127-176`, `knitransform.cpp`), not in the simulation, so `/enableSSE` should not affect the checksum (inference from the call sites).
- Math primitives: Relic's `fsqrt` is a table-driven bit-manipulation approximation, `fmathSqrtDouble` (`ORIG:fastmath.h:18`, `ORIG:fastmath.c:87`), which is portable if ported bit for bit. `sin`/`cos`/`sqrt` from the C library are used in 29 `ORIG:src/Game` files (15 lines in `physics.c`, 7 in `univupdate.c`), and `matrix.c` has an inline-asm 3x3 multiply (`ORIG:matrix.c:381,482,564`). In the Cataclysm exe the library calls appear inlined as x87 instructions: code at 0x00401000-0x00670000 (mostly game code, below the CRT) holds 133 `fsin`, 142 `fcos`, 16 `fsqrt`, 16 `fpatan`, 6 `fptan` (`objdump` count). x87 `fsin`/`fcos` are not correctly rounded and do their own argument reduction, so their last bits generally differ from a libm result, and a recreation that calls libm `sinf`/`cosf` produces different last bits wherever these feed ship motion.
- The checksum includes nothing that depends on rendering or the frame rate **as long as the run is in the lockstep branch**: ticks are driven by packets, not by wall time. In the free-running branch the tick count per wall second depends on the task scheduler, but the tick contents still do not (each `univUpdate` takes the constant `UNIVERSE_UPDATE_PERIOD`).

Consequence for the oracle: bit-exact equality is a realistic target only between two runs of the same `cataclysm.exe`. Between the GOG exe and `hwi-engine` it is a goal to measure, and section 6 gives the fallback.

---

## 2. Cataclysm: the same machinery, found by name, and the diff

Every HW1 function of section 1 exists in the GOG exe under its HW1 name (round-5 `functions.csv`; addresses are VAs, image base 0x00400000, no relocations per `docs/cet-knowledge.md` section 1). Disassembly quoted below was read with `objdump -d -M intel` on `original/game/cataclysm.exe` (read-only; nothing was executed).

### 2.1 Functions and globals

| HW1 name | Cataclysm VA | size | prototype (applied) | callers (call site) |
|---|---|---|---|---|
| `univGetChecksum` | 0x00549F50 | 732 | `real32 __cdecl univGetChecksum(sdword *numShipsInChecksum)` | `netcheckShow` only (0x004CC40C) |
| `statsGetStatChecksum` | 0x00525170 | 398 | `sdword __cdecl statsGetStatChecksum(void)` | `netcheckShow` only (0x004CC415) |
| `netcheckShow` | 0x004CC400 | 189 | `void __cdecl netcheckShow(HWPacketHeader *packet)` | `clProcessSyncPacket` (0x00450985) |
| `netcheckCheckChecksum` | 0x004CC340 | 190 | `void __cdecl netcheckCheckChecksum(NetSyncChecksums *checksum)` | `netcheckShow` |
| `netcheckFillInChecksum` | 0x004CC2F0 | 67 | `void __cdecl netcheckFillInChecksum(HWPacketHeader *packet)` | `captainServerTask` (0x00451901) |
| `netcheckInit` / `Reset` / `Close` | 0x004CC220 / 0x004CC1D0 / 0x004CC2C0 | | | |
| `clProcessSyncPacket` | 0x00450910 | 2019 | `void __cdecl clProcessSyncPacket(CommandLayer*, ubyte*, udword)` | `clWaitSyncPacket` (2 sites) |
| `clWaitSyncPacket` | 0x00451280 | 755 | `WaitPacketStatus __cdecl clWaitSyncPacket(CommandLayer*)` | `universeUpdateTask` (0x0053E872) |
| `universeUpdateTask` | 0x0053E460 | 1445 | task function | (task) |
| `univUpdate` | 0x0054C3F0 | 2605 | `int __cdecl univUpdate(float phystimeelapsed)` | `universeUpdateTask` 0x0053E8AA, 0x0053E8E0 (lockstep pair), 0x0053E9BA (free-running); debug harness `statsShowFancyFight`, `statsGatherFightStats` |
| `ranRandomFn` | 0x004EF860 | 112 | `uint __cdecl ranRandomFn(int ranIndex)` (no file/line args: release build) | 179 functions, 574 sites |
| `ranStartup` / `ranRandomize` / `ranParametersReset` / `ranSave` / `ranLoad` | 0x004EFA30 / 0x004EF9A0 / 0x004EF960 / 0x004EFB50 / 0x004EFB70 | | | |
| `recPack*` (record/playback, 11 functions [audit: was "12"; `functions.csv` has exactly 11 `recPack*` entries, 0x004CBAF0 `recPackInGameStartCB` through 0x004CC1B0 `recPackPlayClose`]) | 0x004CBAF0-0x004CC1B0 | | | |

Globals (from the decompiled bodies; names are the HW1 meaning, addresses read from the code):

| global | VA | evidence |
|---|---|---|
| `universe` | 0x00B62A00 | `symbols.csv` |
| `universe.phystimeelapsed` / `totaltimeelapsed` / `realtimeelapsed` | 0x00B62ABC / 0x00B62AC0 / 0x00B62AC4 | `univUpdate` 0x0054C54E-0x0054C5F6 |
| `universe.ShipList.head` | 0x00B62AF0 | `univGetChecksum` 0x00549F5C (`LinkedList` = {head, tail, num}, `datatypes.h:1647`) |
| `universe.univUpdateCounter` | 0x00B62BAC | `univUpdate` 0x0054C554-0x0054C574 (`inc edi`) |
| `netsyncchecksums[128]` (16-byte entries) | 0x00C30FC0 | `netcheckShow` 0x004CC443-0x004CC467 |
| `lastchecksumCalculated` | 0x00C30FA4 | `netcheckShow` 0x004CC478 |
| `netsyncchecksumsValid[128]` (dwords) | 0x00C317E0 | `netcheckShow` 0x004CC46D |
| `randSyncErr` / `univSyncErr` / `blobSyncErr` / `pktSyncErr` | 0x00C317D4 / 0x00C317DC / 0x00C319E0 / 0x00C317CC | `netcheckCheckChecksum`, `clProcessSyncPacket` |
| `randSyncErrFrame` / `univSyncErrFrame` / `blobSyncErrFrame` | 0x00C317C0 / 0x00C317C4 / 0x00C30FA0 | `netcheckCheckChecksum` |
| `netlogfile` (FILE*) / `logEnable` / `logFilePath` | 0x00A4CA30 / 0x00A43CC0 / 0x00A43CC4 | `netcheckInit` 0x004CC220 |
| `playPackets` / `recordPackets` / `recordFakeSendPackets` / `recordplayPacketsInGame` | 0x00A7518C / 0x00A75188 / 0x00A75198 / 0x00A75194 | command table, `recPack*` |
| `recordPacketFileName` / `recordPacketSaveFileName` | 0x00B39F00 / 0x00B39F40 | `EnablePacketPlay`, `recPackInGameStartCBSafeToStart` |
| `receivedPacketNumber` | 0x00A2DB80 | `clProcessSyncPacket` |
| `startingGame` / `sigsPressedStartGame` | 0x00A43CB8 / 0x00A43CA8 | `universeUpdateTask` (calls `utyNewGameStart(NULL,NULL)` when the second is set and the first is not) |
| `determCompPlayer` | 0x00A75248 | `aiplayerStartup` 0x00410FA0 |
| `ranStream[27]` (20 bytes each: x, y, z, c, n) | 0x00C2FD80-0x00C2FF9C | `ranRandomFn`, `ranSave` loop bound |

### 2.2 `univGetChecksum`: ships only

Decompiled body (`decompiled.c:297865-297936`) and disassembly agree:

```
for each node in universe.ShipList:            ; head at [0x00B62AF0], node->structptr at +0xC
    if ((ship->flags & 0x800) == 0):           ; flags at +0x14; `test ah,0x8`
        x += ship->posinfo.position.x          ; +0x58   fld [esp+0x20]; fadd [esi+0x58]; fstp [esp+0x20]
        y += ...position.y                     ; +0x5C   (same pattern)
        z += ...position.z                     ; +0x60
        if (netlogfile && logEnable == 2) fprintf "S:%d %d %d %d (%f %f %f) %f %d %d %d %d (%d %d)\n" ...
        count++
*numShipsInChecksum = count
return (z + y) + x                             ; 0x0054A210: fld z; fadd y; fadd x  (no store between)
```

Differences from HW1 (section 1.1):

| | HW1 (`ORIG:univupdate.c:5096`) | Cataclysm (0x00549F50) |
|---|---|---|
| objects summed | live ships + live derelicts + **all** resources | **live ships only** (no derelict or resource loop exists in the body) |
| dead test | `SOF_Dead` = 0x1000 (`ORIG:spaceobj.h:929`) | bit 0x800: the `SOF_*` flag values moved down one bit in Cataclysm (inference from this test; the full enum is R2 work) |
| net log | binary `binnetlog*` records (`BINNETLOG 1`) | text only: `"S:..."`, `"R:%d "`, `"M: %df %dg %dw %dd %ds %di %dt\n"` |
| final sum | `x+y+z` in C | `(z+y)+x` on the x87 stack, rounded to `float` once by the caller's `fstp` (0x004CC411) |
| per-addition rounding | `-Od` build: stored to memory each time | stored to memory each time too (the logging calls force the spill) |

So the Cataclysm universe checksum ignores asteroids, dust clouds and derelicts. A recreation bug that moves only a resource or a derelict is invisible to it; the oracle's sub-checksums (section 3.4) add those lists.

### 2.3 `statsGetStatChecksum` (the "blob" check)

Disassembly 0x00525170-0x00525260 and `decompiled.c:262638-262721`:
- If `universe.totaltimeelapsed < 10.0f` (constant at 0x0072A4F0 = 10.0), it calls `GetShipStaticInfo(i)` for i = 0..58 and adds field +0x410 into a register that is then discarded (`edi`, never read again). No effect on the result; only `GetShipStaticInfo`'s side effects, if any.
- For each of `tpGameCreated.numPlayers` players (uword at 0x008860EE; `Player` stride 0x6530, base `ebx = 0x00B6785C` for player 0, i.e. `universe.players` 0x00B67500 + 0x35C): adds the dwords at player +0x4, +0xC, +0x2C, +0x35C and `crc32Compute(player + 0x30, 0x244)`. 0x244 = 580 = 145 x 4, i.e. one CRC over a 145-entry `sdword` per-ship-type table (HW1: `shiptotals[TOTAL_NUM_SHIPS]`; Cataclysm has 145 ship types). HW1's second CRC is gone. Which fields +0x4/+0xC/+0x2C/+0x35C are (HW1: `totalships`, `resourceUnits`, `HasTechnology`, `listoftopics.num`) is UNVERIFIED.
- Then adds the byte `players[i].bounty` over `universe.numPlayers` (0x00B674F8).
- A mismatch calls `SendCheatDetect()` once (multiplayer only), as in HW1.

### 2.4 `netcheckShow` and the packet layout: one new 4-byte field

`netcheckShow` (0x004CC400) does exactly HW1's sequence: `univGetChecksum`, `statsGetStatChecksum`, `ranRandomFn(10)` (= `gamerand()`, `RAN_Game` = 10 as in HW1), store in the ring, `netcheckCheckChecksum`, then `fprintf(netlogfile, "Packet:%d Rand:%d Checksum:%f %d CDET:%d\n", ...)` when the net log is open. The ring entry is 16 bytes, not HW1's 12:

| offset | field | HW1 |
|---|---|---|
| +0x0 | `uword packetnum` | same |
| +0x2 | `uword randcheck` | same |
| +0x4 | `real32 univcheck` | same |
| +0x8 | `udword blobcheck` (stats checksum) | same |
| +0xC | **new** `udword`: a copy of global 0x00AB7658 | absent |

0x00AB7658 is written only in `gameStart` (0x005A9290, `decompiled.c:379794-379799`; the only write instruction is at 0x005A9F1D [audit: the producer also cited `:379139-379144`, but those lines lie inside the decompiled text of `utyFatalErrorWaitLoop` (index lines 378477-379152), where Ghidra's decompile ran on into `gameStart`'s tail. They are a duplicate of the same code, not a second writer]): when `multiPlayerGame`, it is the sum of 8 dwords at stride 0x30 from 0x00BB3020 (just after `DIRSERVER_PORTS` 0x00BB2FA0). It is constant for a game and is **not compared** in `netcheckCheckChecksum`; what it sums is UNVERIFIED (GUESS: per-player setup data).

Consequently the Cataclysm `HWPacketHeader` is 4 bytes longer: `numberOfCommands` is read at packet +0x18 and commands start at +0x1C (`clProcessSyncPacket`, 0x0045098C `mov ax,[esi+0x18]`; `decompiled.c` `packet + 0x1c`), versus HW1's +0x14/+0x18. Recorded `.pkts` files and LAN packets of the two games are therefore not interchangeable. The `.pkts` header block is `CaptainGameInfo` of **0x2B0** bytes (`recPackPlayInit` checks `size == 0x2b0`), HW1-style `HEAD`/`PACK` magic values (0x44414548 / 0x4B434150) unchanged.

`netcheckCheckChecksum` (0x004CC340) compares only `randcheck`, `univcheck` (as `float !=`) and `blobcheck`, records the first-error frame from `universe.univUpdateCounter`, and marks the entry checked (`|= 2`), the same as HW1. `pktSyncErr` is still counted in `clProcessSyncPacket`, but its `dbgMessagef` was compiled out (`nullsubReturnFalse()`). The on-screen `"PKT SYNC ERR"` string sits in `rndDrawOnScreenSyncStatus` (0x0057A300).

### 2.5 When it runs: unchanged lockstep, two ticks per sync packet

`universeUpdateTask` (0x0053E460, `decompiled.c:287671-287930`) keeps HW1's two branches: free-running when `!multiPlayerGame && !playPackets && !recordFakeSendPackets` (one `univUpdate(0.0625)` at 0x0053E9BA, `kasExecute` for the tutorial), and lockstep otherwise (`clWaitSyncPacket` 0x0053E872, `netCheck` 0x0053E8A0, `univUpdate(0.0625)` at 0x0053E8AA and again at 0x0053E8E0). Turbo playback repeats the packet loop **9** times. [audit: the producer called this a Cataclysm addition, but it is not one. HW1 has `#define UNIVERSE_TURBO_REPEAT 19` (`ORIG:universe.c:96`, under `UNIVERSE_TURBOPAUSE_DEBUG`, which `ORIG:universe.h:40,48` sets to 1), and `repeattimes = UNIVERSE_TURBO_REPEAT>>1` = 9 (`ORIG:universe.c:3935`), so the value is unchanged.] One real Cataclysm addition in the lockstep branch: during playback a `NO_PACKET` result sets a new flag 0x00A6C5C8 (end of recording reached; meaning inferred). The tick period is the HW1 constant 1/16 s (`push 0x3D800000` at the call sites, which CET's `UpdateRateShift` rewrites; `docs/cet-knowledge.md` section 2.2).

`univUpdate` itself (0x0054C3F0) gained, before the HW1 body (`decompiled.c:299650-299700`):
1. an early `return 0` tick gate (`if [0x00A6ED74] != 0: if [0x00A6ED78] < 1 return 0; else [0x00A6ED78]--`), meaning UNVERIFIED (a frame-step/limit counter); on that path the counter does **not** advance, which the hook in section 3 uses to tell real ticks from skipped calls;
2. a timed multiplayer autosave (`/SaveMPGames` flag 0x00A74DAC, `"SavedGames\multiplayer\AS%d"`, every 200 s of game time from the constant 0x0072A440);
3. **a wall-clock read**: `QueryPerformanceCounter` at 0x0054C57F (`FF 15 28 A2 72 00`, `call [IAT 0x0072A228]`) feeds `universe.realtimeelapsed` (0x00B62AC4). The quit/win timers now compare against `realtimeelapsed` instead of HW1's `totaltimeelapsed`, and 21 other functions read it, among them simulation code: `CheckPlayerWin`, `LeechHouseKeep`, `cat_leechPickTarget`, `cat_salvageSpecialTarget`, `cat_univShipFOWUpdate`, `singlePlayerGameUpdate` (9 reads), `mrScuttle` (counted over `decompiled.c` by function). HW1 has no such field (`docs/cet-knowledge.md` section 4.2 marks it "Cataclysm/CET").

### 2.6 Randomness: 27 streams, and a wall-clock-seeded stream 0

`ranRandomFn` (0x004EF860) is HW1's generator unchanged (subtract-with-borrow on x/y/z/c, `-0x12`, LCG `n*0x10DCD + 0x3C6EF373` = 69069 / 1013904243). `ranParametersReset` writes HW1's five constants (0x1F123BB5 = 521288629, 0x159A55E5, 0x189FE9, 1, 0x436CBAE9). Differences:

| | HW1 | Cataclysm |
|---|---|---|
| number of streams | 25 (`RAN_NumberStreams`) | **27** (`ranStartup` loop to 0x00C2FFA0, `ranSave`/`ranLoad` to 0x00C2FF9C, 20 bytes each) |
| `ranStartup` | resets every stream | resets every stream, **then advances stream 0 by `GetSystemTime().wSecond` steps** (0x004EFA30) |
| `aiplayerStartup` | `ranRandomize(RAN_AIPlayer)` unless `determCompPlayer` | same (stream 6, flag 0x00A75248). The flag is not in the command table; it is set only when demo recording starts (`utyGameSystemsInit`, `decompiled.c:382068`). [audit: this is not the end of it in Cataclysm. For a **new non-single-player game** (skirmish or multiplayer; `loadfilename == NULL && singlePlayerGame == 0`), `gameStart` calls `aiplayerStartup` at 0x005A9367 and later calls the new `cat_ranResetGameStreams` (0x004EFB10) at 0x005A9A98 (`decompiled.c:379622-379626`). That function writes the fixed `ranParametersReset` constants into streams 1..26, stream 6 included. So the wall-clock `ranRandomize(6)` is overwritten, and a fresh skirmish starts with streams 1..26 deterministic whatever `determCompPlayer` says. Only stream 0 keeps its wall-clock offset. The `wSecond` seed of stream 6 survives only in a new single-player campaign game (`aiplayerStartup(2,1,1)`, no reset). HW1 has no such reset: `ranParametersReset` is called only from `randy.c`, `nebulae.c` and `AIPlayer.c`] |
| save games | `ranSave`/`ranLoad` all streams | same, all 27 |

Stream use by literal index (`ranRandomFn(N)` over all decompiled bodies): 0 in 27 functions, 1 in 14, 4 in 9 (clouds), 5 in 5 (nebulae), 6 in 34 (AI, KAS `kasfRandom`), 7 in 1 (`dmgRandomDamageLocation`), 8 in 5 (battle chatter), 9 in 2 (NIS static), 10 (`gamerand`) in 62.

The leak: **stream 0 mixes rendering and simulation.** Its callers include renderer functions that run once per drawn frame (`Make3dStar`, `cat_rndRenderObjectList`, `cat_partRenderUnk1System`, `btgColorVertices`, `cat_blastRenderAll`, `cat_rndLightningBoltDraw`, music) and simulation or start-up decisions: `univFindNearestResource` (resource choice for ships with `RepairTech`, `decompiled.c:297805`), `airDumbResearchRequests` (AI research), `cat_leechDrainEffect` (next drain time, ship +0x64C), `droidUpdateArray`, `gameStart` (per-scenario random picks, `:379301`), `mgStartGameCB` / `lgStartGame` (one random bit per player). Stream 0 is seeded from the wall clock at start-up and advanced by frame rate. In HW1, stream 0 (`RAN_ParticleStream`) was meant for effects only. Whether these Cataclysm sim uses ever change a checksummed value in a typical skirmish is UNVERIFIED; the oracle measures it (section 4.4).

### 2.7 Built-in logging that survives in the release exe

The command-line table at 0x008B8870 (24-byte `commandoption` entries, decoded from the exe) still holds, hidden (flag 0): `/logOff`, `/logOn`, `/logOnVerbose` (all write `logEnable` 0x00A43CC0 = 0/1/2), `/logFilePath` (0x0055F4D0), `/packetRecord` (0x0055F5A0, var 0x00A75188), `/packetPlay <file>` (`EnablePacketPlay` 0x0055F550, var 0x00A7518C). New Cataclysm entries: `/aiplayerLog`, `/SaveMPGames` (0x00A74DAC) and `/LoadMPGame <name>` (`cat_mainLoadMPGameSet` 0x0055F500: `gameStart` loads `savedGames\multiplayer\<name>` instead of starting fresh, but only when `multiPlayerGame`). Absent: `/determCompPlayer`, `/debugSync`, `/autosavedebug`, `/demoRecord`, `/demoPlay`.

With `/logOn`, `netcheckInit` opens `netlog.txt` (or `/logFilePath`) and every processed sync packet appends `Packet:%d Rand:%d Checksum:%f %d CDET:%d`. With `/logOnVerbose`, every ship's `S:` line and the per-player `CDET:` and `BOUNT:` lines follow. This is a zero-code oracle, but a weak one: it exists only in lockstep mode (one line per two ticks), prints `univcheck` through `%f` (six decimal places, so two adjacent float values are guaranteed to print differently only when |univcheck| >= 16, where the float spacing exceeds 1e-6), and covers ships only. The hook of section 3 keeps it as a cross-check.

The recorded-game feature is intact: the front-end callbacks `LoadRecordedGame` (code at 0x00487CF0, not yet a Ghidra function; it calls `VerifySaveFile` 0x004FDF40, `recPackPlayInGameInit` 0x004CC010, `utyLoadMultiPlayerGameGivenFilename` 0x005AAA50) and `IG_SaveRecordedGame` (0x00487EF0; calls `recPackInGameStartCB` 0x004CBAF0) are registered in the `gp` callback table at 0x00881138/0x00881140, and the shipped screens still reference them: `feman/multiplayer_game.fib` has a `Recorded_Game` screen ("LOAD RECORDED GAME"), `feman/in_game_esc_menu.fib` links to `Game_record_save` (read with `strings` from `original/game_extracted/cataclysm/feman/`). Whether those buttons are visible in the running game is UNVERIFIED (the game was not launched).

### 2.8 Diff summary

| item | HW1 | Cataclysm | consequence for the oracle |
|---|---|---|---|
| universe checksum coverage | ships + derelicts + resources | ships only | add derelict/resource/missile/bullet sub-checksums |
| `SOF_Dead` | 0x1000 | 0x800 | use the Cataclysm value in the DLL; `hwi-engine` uses its own enum |
| sync-checksum entry | 12 B | 16 B (+ constant udword) | packet header +4 B; `.pkts` not portable between HW1 and Cataclysm |
| stats checksum | 2 CRCs (one buggy) + 4 fields + bounty | 1 CRC (145 types) + 4 fields + bounty; dead 10-s static-info loop | port the Cataclysm form |
| RNG streams | 25 | 27; stream 0 wall-clock seeded | the oracle records all 27 states each tick |
| wall clock in the sim | none (`totaltimeelapsed` only) | `realtimeelapsed` from QPC, read by win/quit timers and ~8 sim functions | virtual clock in oracle runs (hook H3, section 3.2) |
| tick period, lockstep shape | 1/16 s, 2 ticks per packet | same; turbo x9 (same as HW1's 19>>1 [audit]); end-of-playback flag | none |
| net log | binary records | text | none |
| `ship->health` | (HW1 `SpaceObjRotImpTarg.health`) | read/written at **+0x170** in `ApplyDamageToTarget` (0x00546257, 0x0054643E/0x00546448 `fld/fstp [esi+0x170]`) for ships and other targets | CET's table puts `Ship.health` at 0x158 and an asserted `buildProgress` at 0x170 (`docs/cet-knowledge.md` section 4.2); the code says 0x170 is health. Resolve in R2 before the DLL reads health |

---

## 3. The hook: `hwi_oracle.dll` for the GOG build

### 3.1 Shape, and what it borrows from CET

CET is a DLL injected into a suspended `cataclysm.exe` by a small loader, which then patches the exe at fixed VAs with MinHook entry detours, mid-function detours, Keystone instruction overwrites and data pokes (`docs/cet-knowledge.md` sections 2.1, 8; `CET/loader/Main.cpp`, `CET/Main.h:23-36`). That pattern already works on this machine under Proton: `original/game/CataclysmEngineTweaks.log` (2026-10-08 13:54) records "RGL patches applied", "Update rate patches applied", "Patches applied", "Universe reset called".

The oracle reuses the pattern but **not CET itself**:
- CET hooks `univUpdate` (0x0054C3F0) for its autosave, runs the universe at 64 Hz by default (`UpdateRateShift=2`) and changes several simulation paths (`docs/cet-knowledge.md` section 2.2 and 9). Even with `UpdateRateShift=0` its autosave fires on the first tick (section 9, item 1). Oracle runs must be stock 16 Hz behaviour, so CET's DLL stays out. CET's replacement renderer DLLs (`rgl.dll`, `rgld3d.dll` from hw_rgl, already installed in `original/game/`) can stay: they are not hooks, and the D3D9 one creates its device with `FPU_PRESERVE` (`docs/cet-knowledge.md:1294`).
- Two programs, both 32-bit, built on this machine without new packages (plan, not tried): `clang-18 --target=i686-pc-windows-msvc -ffreestanding -nostdlib` plus `lld-link` and an import library for `kernel32.dll` made by `llvm-dlltool-18` from a `.def` file (all three present in `/usr/bin`). No CRT, no MinHook: the five function-entry hook sites below have verified prologues, so a 40-line hand-written 5-byte `jmp` patcher with copied-prologue trampolines is enough. If that proves awkward, `gcc-mingw-w64-i686` plus vendored MinHook is the alternative (a package install, i.e. a user decision).
  - `hwi_oracle_loader.exe`: CET's loader logic (`CREATE_SUSPENDED`, `VirtualAllocEx` + `WriteProcessMemory` + `CreateRemoteThread(LoadLibraryA)`, `ResumeThread`) with two fixes: it checks the remote thread's exit code (CET does not, `docs/cet-knowledge.md` section 8 step 5) and terminates the child if injection failed, and it quotes forwarded arguments. It forwards its own argv to `cataclysm.exe`.
  - `hwi_oracle.dll`: `DllMain(DLL_PROCESS_ATTACH)` checks the image base (as CET does) **and** the exe identity (PE `TimeDateStamp` and `SizeOfImage` of build 12948, values to be read from `docs/binary-fingerprint.md` at build time), then verifies every hook site's original bytes (table 3.2) before writing anything. Any mismatch: log and patch nothing.
- Configuration through one environment variable read in `DllMain`, `HWI_ORACLE="mode=play:oracle01;clock=virtual;rng0=split;out=oracle\\run.hwo"`, so `tools/run_game.sh` needs no new options (Wine passes the Unix environment through). Defaults: `mode=observe`, `clock=real`, `rng0=off`.

Launch (no gamescope, no input):

```
HWI_ORACLE="mode=play:oracle01;clock=virtual" \
  tools/run_game.sh --no-gamescope --virtual-desktop 1024x768 --exe hwi_oracle_loader.exe -- /logOn
```

`--exe` runs any program from the game directory and appends `-- ARGS` (`tools/run_game.sh:54-58,197-212`), the virtual desktop keeps the window bounded without gamescope (docs/CONSTRAINTS.md rule 1). `/logOn` turns on the game's own `netlog.txt` as a cross-check (section 2.7). An install script copies the two binaries into `original/game/` (gitignored), like `tools/install_cet.sh` does for CET.

### 3.2 Hook sites

All addresses VA in `cataclysm.exe` build 12948. "Prologue" is the exact byte sequence the DLL must find before patching (read with `objdump` for this doc). Every site is a function entry except H3 (a call site) and H5 (data). Each entry patch replaces whole instructions (the first 5 bytes end on an instruction boundary at every site) that contain no relative branch, so the trampolines are plain copies.

| id | site | prototype (applied, round 5) | prologue bytes (patch covers the first 5) | action | purpose |
|---|---|---|---|---|---|
| H1 | `univUpdate` 0x0054C3F0 | `int __cdecl univUpdate(float phystimeelapsed)`; returns 0/1 in EAX (callers `test eax,eax`, `docs/cet-knowledge.md` section 9 item 15) | `A1 74 ED A6 00` (`mov eax,[0x00A6ED74]`), then `81 EC D0 00 00 00` | entry detour. Detour: `c0 = *(u32*)0x00B62BAC; r = orig(dt); if (*(u32*)0x00B62BAC != c0) emit_tick(); return r;` The copied instruction uses an absolute address, so the trampoline needs no fix-up | the per-tick record (section 3.4), taken at the end of the tick, on every return path, and skipped on the early-return gate where the counter does not move (section 2.5) |
| H2 | `netcheckShow` 0x004CC400 | `void __cdecl netcheckShow(HWPacketHeader *packet)` | `83 EC 08 53 55` (`sub esp,8; push ebx; push ebp`), then `56 8D 44 24 10 57` [audit: was "then `56 57`"; objdump shows `push esi; lea eax,[esp+0x10]; push edi` at 0x004CC405-0x004CC40A. The patched first 5 bytes are unaffected, but a byte check written from the old text would fail] | entry detour: call original, then read entry `[0x00C30FA4]` of the ring at 0x00C30FC0 and the error counters, `emit_packet()` | the game's own sync checksums, exactly as computed and compared, once per sync packet |
| H3 | `univUpdate` + 0x18F = 0x0054C57F (only with `clock=virtual`) | call site, `BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER*)`, argument pushed at 0x0054C56D | `FF 15 28 A2 72 00` (`call [0x0072A228]`, 6 bytes) | overwrite with `E8 rel32 90` to `vqpc`, a `__stdcall` function that ignores the real counter and returns `realtimeremainder` (u64 at 0x00A6D9A0) + `utyTimerDivisor` (u64 at 0x00AB7520) x `taskFrequency` (float at 0x00BB3260) / 16, the three globals the code after the call uses (0x0054C589-0x0054C5EA). `realtimeelapsed` then grows by exactly `taskFrequency/16` timer periods per tick, i.e. 1/16 s if `taskFrequency` is the stock 240 (inference from HW1's 240 Hz task timer and CET's pooled 240.0f constant, `docs/cet-knowledge.md` section 9 item 13; read the real value at build time) | removes the wall clock from `realtimeelapsed` (section 2.5), so win/quit timers, Leech, salvage and fog-of-war timing follow ticks. The other 28 `QueryPerformanceCounter` code refs (`imports.txt`) are left alone; which of them reach simulation state is an R2 question |
| H4 | `ranRandomFn` 0x004EF860 (only with `rng0=split`) | `uint __cdecl ranRandomFn(int ranIndex)` | `8B 44 24 04 56` (`mov eax,[esp+4]; push esi`), then `8D 04 80` | entry detour: if `ranIndex == 0` and the return address lies inside one of the render-only stream-0 callers (`Make3dStar`, `cat_rndRenderObjectList`, `cat_partRenderUnk1System`, `btgColorVertices`, `cat_blastRenderAll`, `cat_blastDrawLightning`, `cat_rndLightningBoltDraw`, `cat_rndLinkLightningDraw`, `cat_sentinelFieldSegmentDraw`, `musicEventPlay`, `musicEventUpdateVolume`; ranges from `functions.csv`) or in `airDumbResearchRequests` (player AI, which runs while recording but not during playback), draw from a private copy of the generator; otherwise call the original | stops frame-rate-dependent draws, and AI draws that playback skips, from moving the stream-0 values that simulation code reads (section 2.6). It changes only cosmetic randomness, but it is a behaviour change, so it is opt-in and used only if section 4.4 shows stream 0 diverging |
| H5 | data 0x00A75248 `determCompPlayer` (only for a fresh skirmish, `mode=record`) | `bool32` | (data) | write 1 at attach | `aiplayerStartup` then calls `ranParametersReset(6)` instead of `ranRandomize(6)` (0x00410FA0). [audit: this is redundant for a new skirmish or multiplayer game, because `gameStart` resets streams 1..26 afterwards anyway with `cat_ranResetGameStreams` (section 2.6 table). It matters only for a new single-player campaign game] |
| H6 | `taskExecuteAllPending` 0x0052A840 (only with `mode=play` / `mode=load`) | `sdword __cdecl taskExecuteAllPending(sdword ticks)`; called from the main loop `utyTasksDispatch` 0x005ACD10 | `55 8B EC 51 53` (`push ebp; mov ebp,esp; push ecx; push ebx`), then `56 57` | entry detour, one-shot trigger on the main thread (section 4.3), then pass-through | starts the run without input |
| H7 | `gameStart` 0x005A9290 | `void __cdecl gameStart(char *loadfilename)` (NULL for a new game) | `83 EC 0C 53 55` (`sub esp,0xC; push ebx; push ebp`), then `56 57` | entry detour: call original, then write an `E` record (load file name or "new") and a `T` record of the state right after start/load, before the first tick | the tick-0 snapshot that stage 1 of section 5.2 compares |

The DLL never calls `ranRandomFn` (it would advance a stream) and never calls `univGetChecksum` or `statsGetStatChecksum` while the net log is open: those two write to `netlogfile` when `logEnable == 2`. `emit_tick` calls them with `netlogfile` (0x00A4CA30) set to NULL for the duration of the call and restored afterwards. Both are otherwise side-effect free (bodies in section 2), so the tick record carries the game's own values computed by the game's own code.

Threading: `univUpdate` and `netcheckShow` run inside `universeUpdateTask`, a task on the main thread (HW1's stack-switching scheduler, `docs/hw1-port-lessons.md:319`; Cataclysm still uses it: `universeUpdateTask` yields through `(*taskFunctionContinue)()`, `decompiled.c:287930`, and `utyGameSystemsInit` starts `captainServerTask` with `taskStart("CaptServTask", captainServerTask, 0.125, 0)`, `decompiled.c:381909`). The detours therefore run on the main thread, on a task stack: keep them small (no large locals), no locks. Output is written with `WriteFile` on a handle opened with `CreateFileA(..., FILE_APPEND_DATA)`, one record per call, so a crash or a killed process loses at most the record being written.

### 3.3 x87 state

`emit_tick` reads the x87 control word with `fnstcw` and stores it in every record (a change of precision control between runs or mid-run is then visible). The detour itself does no floating-point arithmetic: floats are copied as 32-bit integers.

### 3.4 Record format (`.hwo`, little-endian, shared by both producers)

File header, 64 bytes:

| off | type | field |
|---|---|---|
| 0 | char[8] | magic `HWIORCL1` |
| 8 | u16 | format version (1) |
| 10 | u8 | producer: 1 = GOG DLL, 2 = `hwi-engine` |
| 11 | u8 | run mode: 1 packet playback, 2 save-run, 3 record, 4 observe |
| 12 | u32 | options bitmask: bit0 `clock=virtual`, bit1 `rng0=split`, bit2 `determCompPlayer` |
| 16 | u32 | exe `TimeDateStamp` (GOG) or `hwi-engine` git hash prefix |
| 20 | u32 | exe `SizeOfImage` (GOG) or build flags hash |
| 24 | u16 | x87 control word at attach (GOG) or MXCSR low 16 bits (`hwi-engine`) |
| 26 | u16 | ticks per second (16) |
| 28 | char[36] | run tag (recording name), NUL-padded |

Then a stream of records, each `u8 type, u8 reserved, u16 length (bytes after this 4-byte prefix)`:

**`T` tick record** (written after each `univUpdate` that advanced the counter):

| field | type | source in the GOG build | why |
|---|---|---|---|
| tick | u32 | `universe.univUpdateCounter` 0x00B62BAC | the join key |
| dt, total, real | 3 x f32 bits | 0x00B62ABC, 0x00B62AC0, 0x00B62AC4 | `real` shows wall-clock leakage when `clock=real` |
| fpucw | u16 | `fnstcw` | precision mode |
| univcheck | f32 bits | `univGetChecksum` (netlog muted) | the engine's own checksum, now per tick instead of per packet |
| nships | u16 | its out-parameter | |
| statcheck | u32 | `statsGetStatChecksum` (netlog muted) | the engine's own cheat checksum |
| rng_all | u32 | CRC32 of the 540 bytes at 0x00C2FD80 | any RNG drift |
| rng10, rng6, rng0 | 3 x 20 B | raw `{x,y,z,c,n}` of streams 10 (`gamerand`), 6 (AI), 0 (mixed) | which stream moved first, and by how many draws (a draw count can be recovered by stepping a copy) |
| ships_n, ships_pos, ships_vel, ships_rot | u16 + 3 x u64 | walk `ShipList` (head 0x00B62AF0), skip `flags & 0x800`; FNV-1a-64 over, per ship in list order: `shipID` (u16 at +0x43C), then position (+0x58, 3 x f32 bits) / velocity (+0x64) / `rotinfo.coordsys` (+0x80, 9 x f32) | localises a divergence to motion or rotation; list order is part of determinism, so it is hashed as is |
| ships_misc | u64 | FNV-1a-64 over per-ship `shiptype` (+0x2B0), `shiprace` (+0x2B4), owner `playerIndex` (u16 at `[+0x32C]+0x24`), command order/attributes (u16 at `[+0x370]+0x14`/`+0x16`, -1 if none), and health (+0x170, **only after R2 confirms the offset**, section 2.8) | AI and command-layer divergence |
| missiles_n/h, bullets_n/h, derelicts_n/h, resources_n/h | 4 x (u16 + u64) | lists at `universe` + 0x12C, 0xFC, 0x114, 0x108 (`LinkedList` = {head, tail, num}; offsets from `docs/cet-knowledge.md` section 4.2, unasserted except `ShipList`), hash of position (+0x58) per live object | objects the engine checksum ignores (section 2.2) |
| players | 8 x (u32 RU, u32 totalships) | `universe.players` 0x00B67500, stride 0x6530; field offsets from the R2 Player layout (UNVERIFIED now; fill with 0 until then) | economy drift |

Size: about 235 bytes per tick with the 4-byte prefix, about 14 MB per hour at 16 Hz.

**`S` snapshot record** (optional, every K ticks, for the fallback of section 6.2): per live ship `shipID`, type, owner, position, velocity, rotation matrix, health; per player the resource units.

Offsets marked unasserted are read only after the DLL's start-up self-test has checked them against known values in the first tick (e.g. the ship count from `univGetChecksum` must equal `ships_n`; every `ShipList` node's `objtype` at +0x10 must be 0 = `OBJ_ShipType`). A failed self-test zeroes that field and sets a flag in the header.

**`P` packet record** (from H2): `tick` (u32), `packetnum` (u16), `randcheck` (u16), `univcheck` (f32 bits), `blobcheck` (u32), `extra` (u32, the new field), `numberOfCommands` (u16, packet +0x18), `randSyncErr`, `univSyncErr`, `blobSyncErr`, `pktSyncErr` (4 x u32) and the three first-error frames (3 x u32). In playback these counters are the game's own verdict on whether the replay matches the recording.

**`E` event record**: game start / load (with the file name), recording start, end of playback (flag 0x00A6C5C8 becomes 1), quit, self-test results.

A text dump (`hwi-oracle dump run.hwo`) prints one line per record for eyeballing.

---

## 4. Driving a run without input injection

Requirement: the GOG build plays a deterministic stretch of game with nobody touching mouse or keyboard (docs/CONSTRAINTS.md rule 2), and two runs produce the same stream. Calling the game's own functions from the hook DLL is not input injection (no OS input events are synthesised), but it is still a way of steering the game, so this section says exactly which calls are made. The user should confirm that this reading of rule 2 is acceptable (open question 1).

### 4.1 Candidates, judged from the code

| candidate | how it starts | what drives the simulation | non-determinism left | verdict |
|---|---|---|---|---|
| **A. Recorded game played back** (`recPack*`) | front end `LoadRecordedGame` (code at 0x00487CF0: `VerifySaveFile`, `recPackPlayInGameInit` 0x004CC010, `utyLoadMultiPlayerGameGivenFilename` 0x005AAA50), or the hook making those same two calls (H6) | the recorded sync packets, two ticks per packet, in the lockstep branch (`universeUpdateTask`, section 2.5). **AI is not run**: `aiplayerUpdateAll` skips every computer player when `playPackets` (0x00411240, `if (DAT_00a7518c == 0 ...)`, `decompiled.c:14868`), because their commands are in the file. The RNG streams, all 27, come from the save made when recording started (`ranLoad`) | stream 0 render draws and `realtimeelapsed`, if the replayed stretch reaches the sim code that reads them (sections 2.5, 2.6); removed by H3 and H4 | **chosen** |
| B. Fresh skirmish of computer players | menus (input), or the hook setting `sigsPressedStartGame` 0x00A43CA8 so `universeUpdateTask` calls `utyNewGameStart(NULL,NULL)` (`decompiled.c:287755`) with whatever `tpGameCreated` holds | live AI, free-running branch | ~~`ranRandomize(6)` from `GetSystemTime().wSecond` unless `determCompPlayer` (H5)~~ [audit: not a source here. For a new non-single-player game, `gameStart` calls `cat_ranResetGameStreams` (0x005A9A98) after `aiplayerStartup` (0x005A9367), which resets stream 6 to its fixed constants. See section 2.6]; stream 0 advanced by `wSecond` in `ranStartup` and then read by `gameStart` (`decompiled.c:379301`) and `mgStartGameCB`/`lgStartGame`; a human slot always exists (HW1 `numPlayers = 1 + numComputers`, `ORIG:Win32/utility.c:3133-3140`) and idles | not reproducible without H3+H4+H5 and a seeded stream 0; used only to **make** recordings |
| C. Saved game loaded and left to run | the hook calling `utyLoadSinglePlayerGameGivenFilename` 0x005AA5F0 or `utyLoadMultiPlayerGameGivenFilename` 0x005AAA50 (H6) | live AI (`cat_aisPlay` in skirmish), free-running branch | RNG restored by `ranLoad`; `gameStart` calls `cat_aisReset()` on load too (`decompiled.c:379792` [audit: was ":379136"; that line is inside the overlapping decompile of `utyFatalErrorWaitLoop`, and `gameStart`'s own call is at 379792]), the same in both runs; stream 0 render draws and `realtimeelapsed` remain (H3, H4) | **second mode**: exercises the AI, which A skips |
| D. A mission's NIS | starting a mission needs the menus, or a save loaded by the hook | KAS + NIS script; `singlePlayerGameUpdate` reads `realtimeelapsed` 9 times | wall clock unless H3 | later, for the mission rounds; it covers little simulation |
| E. Demo record/playback (`demo.c`, `DEM_CHECKSUM`) | not reachable: `/demoRecord`, `/demoPlay` are not in the command table (section 2.7) | replays recorded **input events** | frame-timing dependent by design (HW1 `DEM_FAKE_RENDER_SWITCH`) | rejected: it is input replay, closest to what rule 2 forbids |
| F. LAN game between two GOG instances | lobby (input) | lockstep with real packets | none beyond A | not needed; A gives the same lockstep without a network |

### 4.2 Why A is reproducible (evidence)

1. **No live decision makers.** In playback, every command comes from the `.pkts` file (`clWaitSyncPacket` -> `recPackPlayGetNextPacket` 0x004CBF00), and computer players are skipped (above). Human input is ignored too: HW1's `clWrap*` functions return early when `playPackets` (`ORIG:CommandWrap.c:91` and 20 more); the Cataclysm equivalents are R2 work, but no input is given anyway.
2. **Tick cadence comes from data.** In the lockstep branch one packet means exactly two `univUpdate(0.0625)` calls. Frame rate, machine speed and Wine scheduling change only how fast ticks happen, not what they contain. (The free-running branch of C has the same property for tick contents; only rendering interleaving differs.)
3. **The game checks itself.** Each recorded packet carries the recorder's `randcheck`/`univcheck`/`blobcheck` (filled by `netcheckFillInChecksum` at record time). Playback recomputes them in `netcheckShow` and counts mismatches in `randSyncErr`/`univSyncErr`/`blobSyncErr` (0x00C317D4/0x00C317DC/0x00C319E0), with the first bad frame. H2 copies these into every `P` record. A playback with all three counters at 0 is, by the engine's own definition, a faithful replay of the recording. That gives a same-binary repeatability test before any recreation code exists.
4. **RNG starts from saved state.** Recording starts with `SaveGame` (`recPackInGameStartCBSafeToStart` 0x004CBBB0), and playback starts with that save, so all 27 streams, including stream 0 and its wall-clock offset, are restored. After that, stream 6 and part of stream 0 are drawn only while recording (player AI), see 4.4 rung 1.
5. **The switches exist in this exe.** `/packetPlay`, `/packetRecord`, `/logOn` are in the hidden command table (section 2.7); `LoadRecordedGame` and `IG_SaveRecordedGame` are wired and referenced by the shipped `.fib` screens.

### 4.3 Procedure

**Making a recording (once per scenario; needs the user, because starting a skirmish takes menu clicks):**
1. The user launches `HWI_ORACLE="mode=record:oracle01;stop=9600" tools/run_game.sh --no-gamescope --virtual-desktop 1024x768 --exe hwi_oracle_loader.exe -- /logOn`.
2. The user starts a skirmish from the menus (for example one human slot plus three computer players on a fixed map) and then leaves the game alone.
3. At the first tick the DLL calls `recPackInGameStartCB("SavedGames\\RecordedGames\\oracle01")` (0x004CBAF0, `void __cdecl (char*)`). That only copies the names into `recordPacketSaveFileName`/`recordPacketFileName` (0x00B39F40/0x00B39F00, 64-byte buffers judging by their spacing) and sets `startRecordingGameWhenSafe` (0x00A43E18). `universeUpdateTask` then calls `recPackInGameStartCBSafeToStart` at its safe point (`decompiled.c:287926-287928`): it saves the game, sets `recordPackets`, `recordFakeSendPackets`, `sigsPressedStartGame`, captain index 0, resets the checksum ring and opens `netlog.txt`. From then on every command, the computer players' included, travels through `captainServerTask` (started at start-up, 8 Hz, `decompiled.c:381909`) as sync packets and is appended to `oracle01.pkts`. The in-game ESC menu's own `Game_record_save` entry does the same if it is visible.
4. At tick `stop` (9,600 ticks = 10 minutes) the DLL calls `recPackInGameStopCB` (0x004CBD10) and writes an `E` record; the user quits.
5. Output: `original/game/SavedGames/RecordedGames/oracle01` and `oracle01.pkts` (game-derived, gitignored, never committed), plus the recording run's own `.hwo` stream, which is the first reference stream.

**Playing it back (every oracle run; no user):**
1. `HWI_ORACLE="mode=play:oracle01;clock=virtual" tools/run_game.sh --no-gamescope --virtual-desktop 1024x768 --exe hwi_oracle_loader.exe -- /logOn`
2. H6 waits until the main menu is up (first `taskExecuteAllPending` call after `gameIsRunning == 0 && startingGame == 0` has held for N calls; N tuned at build time) and then, once, on the main thread: copies the names into 0x00B39F40/0x00B39F00 and calls `recPackPlayInGameInit()` then `utyLoadMultiPlayerGameGivenFilename(save)`, the two calls `LoadRecordedGame` makes (0x00487E3E, 0x00487E48). [audit: those two calls are not the whole recipe. After `VerifySaveFile`, `LoadRecordedGame` first calls `feScreenDisappear(NULL,NULL)` (0x00475200, call at 0x00487E39), which HW1's `gpLoadTheRecordedGame` also does (`ORIG:gamepick.c:786`). After `utyLoadMultiPlayerGameGivenFilename` returns, the Cataclysm version also writes `[0x00A7519C] = 1` and `[0x00A751A0] = 1`, and sets `[0x00A75190]` to 0, or to 1 if a per-player name check (format 0x008812D8 against the 0x12-byte names at 0x00C39652) fails (0x00487E4D-0x00487EDE). 0x00A7519C and 0x00A75190 are read by simulation-side command code (`processAttackToDo`, `cat_clHarvestNearestResource`, `cat_cmBuildJobFinished`, `cat_cmExternalBuild*`; 28 reads of 0x00A7519C in all). H6 must reproduce all of these, or a hook-started playback can differ from a menu-started one. The flags' meanings are UNVERIFIED] Calling them from the main loop rather than from inside the front-end task is UNVERIFIED; the fallback is to set the same state and let `universeUpdateTask` start the game through `sigsPressedStartGame`, or to ask the user to click "Load recorded game" once (still no injection).
3. The run ends when the end-of-playback flag 0x00A6C5C8 is set: the DLL writes an `E` record, closes the file and exits the process with `ExitProcess(0)` after a final flush (no quit-menu interaction).

### 4.4 The acceptance ladder for the oracle itself

Before comparing against `hwi-engine`, the oracle has to prove that the GOG build agrees with itself:
1. **Recording vs playback**: `randSyncErr == univSyncErr == blobSyncErr == 0` over the whole playback, and the playback's `T` stream equal to the recording's `T` stream tick for tick, except `rng6` (player AI draws stream 6 while recording and is skipped in playback, so it must differ) and `real` when `clock=real`. `rng0` is the field to watch: the player AI also draws stream 0 (`airDumbResearchRequests`), so without `rng0=split` stream 0 is expected to differ between recording and playback, and any simulation use of stream 0 after that point (`univFindNearestResource`, Leech, droids) would make the stock game's own recorded-game playback desync. Only `ranRandomFn(10)` callers matter for `randcheck`; none of the 62 is player-AI code (the one `ai*` name, `aishipmineaquiretarget`, is ship-level mine targeting run by the command layer).
2. **Playback vs playback**: two playbacks with `clock=virtual`, compared by the tool of section 5. If they differ, check `rng0` first (stream-0 render draws); enable `rng0=split` and repeat.
3. **Under load**: a playback with a different `--virtual-desktop` size (different frame rate) must give the same stream. This is the test that shows rendering does not leak into simulation.
4. Same ladder for mode C (save-run) with `clock=virtual;rng0=split`.

Only a stream that passes its rung is used as a reference.

---

## 5. The recreation side

### 5.1 Where `hwi-engine` emits the same stream

The `src/` tree starts as the unmodified HW1 code with the GoK SDL layer (M2-R1 item 4), so the emit points are the GoK functions that match the hook sites. Line numbers are GoK `3af31fb` as cloned in `reference/gardens-of-kadesh`:

| GOG hook | `hwi-engine` emit point | notes |
|---|---|---|
| H1 `univUpdate` exit | after each `univUpdate(UNIVERSE_UPDATE_PERIOD)` call in `universeUpdateTask` (`GoK:src/Game/Universe.c:3786`, `DEFINE_TASK`), emitting only if `universe.univUpdateCounter` moved; `univUpdate` itself is `GoK:src/Game/UnivUpdate.c:7271` | wrapping the call sites rather than the function body keeps `univUpdate` byte-identical to the ported code |
| H2 `netcheckShow` | end of `netcheckShow` (`GoK:src/Game/NetCheck.c:918`) | the Cataclysm form (16-byte entry, section 2.4) once ported |
| `univGetChecksum` | `GoK:src/Game/UnivUpdate.c:5004`, ported to the Cataclysm form (ships only, `SOF_Dead` per the Cataclysm enum) | computed by the oracle with the net log muted, as the DLL does |
| `statsGetStatChecksum` | `GoK:src/Game/Stats.c:2042`, ported to the Cataclysm form | |
| RNG states | `ranStream[]` in `GoK:src/Game/Randy.c` (27 streams after the port) | read directly, never drawn |

Implementation: one new file `src/Game/OracleDump.c` (+ `.h`), the only producer of `.hwo` on the recreation side, enabled at run time by a command-line switch `/oracleDump <file>` (a new entry in the GoK command table, `GoK:src/SDL/main.c`), and compiled in always so that the oracle tests the shipping binary. It must not change behaviour: no RNG draws, no allocation inside the simulation's memory pools, no reordering of lists. The `.hwo` writer and the field order are shared with the DLL by putting the record definitions in one header, `tools/oracle/hwo_format.h`, used by both the 32-bit DLL and the 64-bit engine (fixed-width integer types only, explicit little-endian packing).

Canonical values, so the 64-bit build hashes what the 32-bit exe hashes:
- Hash fields by **value** in a fixed order (section 3.4), never struct bytes: layouts differ between the GOG exe and a 64-bit build (`docs/hw1-port-lessons.md` A5 lists the pointer-size changes).
- Floats are hashed as their IEEE-754 bit patterns; `-0.0` and `+0.0` differ, as in the exe.
- The `univcheck` recipe that reproduces the exe exactly from the same ship positions: accumulate `x`, `y`, `z` as `float` with one rounding per addition, then `(float)(((double)z + (double)y) + (double)x)`. This equals the x87 sequence at 0x0054A210 when the precision control is 53 bits (both round each operation to a 53-bit significand, and a sum of three floats cannot overflow or underflow differently in this range). Recorded `fpucw` tells which precision the GOG run used.
- Oracle-run settings in `hwi-engine` must mirror the DLL options: `UNIVERSE_UPDATE_RATE_SHIFT = 0` (GoK defaults to 2, i.e. 64 Hz, `GoK:src/Game/Universe.c:75`; only `/vanilla` resets it, `GoK:src/SDL/main.c:848,2195`; `hwi-engine` should default to 0 anyway for a 1:1 recreation); `clock=virtual` = `realtimeelapsed` derived from the tick counter with the same formula as `vqpc`; `rng0=split` = the same render callers and `airDumbResearchRequests` draw from a private stream.

### 5.2 Driving the recreation with the same input

Mode A needs `hwi-engine` to load the Cataclysm save made at record start (version 0x10AD0027, M2-R1 item 2) and to parse Cataclysm `.pkts` files (`HEAD` block of a 0x2B0-byte `CaptainGameInfo`, then `PACK` records whose packet header is 0x1C bytes, section 2.4). Mode C needs the save loader only. Neither exists until the format and universe rounds land, so the oracle comes online in stages:

| stage | needs in `hwi-engine` | comparison |
|---|---|---|
| 0 | nothing | GOG vs GOG (section 4.4): proves the reference is stable |
| 1 | Cataclysm save loader | load the recording's start save, emit one `T` record **before** the first tick, compare with a GOG `T` record taken right after load (the `E`+`T` pair hook H7 writes on `gameStart` return). Tests the loader and the static data (145 ship types, struct contents) |
| 2 | + `univUpdate` and the command layer for the ships in the save | mode C, compared tick by tick |
| 3 | + Cataclysm packet parsing (`clProcessSyncPacket` command types) | mode A, compared tick by tick and packet by packet; the playback's own `randSyncErr`/`univSyncErr` counters inside `hwi-engine` become a second, independent verdict |
| 4 | + LAN | `hwi-engine` joins a LAN game with the stock GOG build; the engine's real sync check is the oracle (docs/PLAN.md M2 proof tool). Needs `docs/PLAN.md` decision 3 |

### 5.3 The comparison tool

`tools/oracle_compare.py` (Python 3, standard library only; reads two `.hwo` files, never game data):

```
oracle_compare.py REF.hwo CAND.hwo [--from-tick N] [--to-tick M] [--fields ...] [--json out.json]
```

1. Check both headers: same format version, same ticks per second, same option bits. A mismatch is reported first, because comparing a `clock=real` run against a `clock=virtual` run is meaningless.
2. Join `T` records on `tick`; report ticks present in only one file.
3. For each tick in order, compare fields in causal order: `rng10`, `rng6`, `rng0`, `rng_all`, `ships_n`, `ships_misc`, `ships_pos`, `ships_vel`, `ships_rot`, `missiles`, `bullets`, `derelicts`, `resources`, `statcheck`, `univcheck`, `players`, then `dt`/`total`/`real`/`fpucw`. Stop at the **first divergent tick** and print every differing field at that tick, the first one in causal order flagged as the likely origin. For an RNG field, step a copy of the reference stream forward up to 1,000 draws to see whether the candidate is simply N draws ahead ("candidate drew 3 extra `gamerand()` numbers before tick T") or unrelated.
4. Join `P` records on `packetnum`; report the first packet whose `randcheck`/`univcheck`/`blobcheck` differ, and any non-zero sync-error counter in either file.
5. Exit code 0 = identical over the compared range, 1 = diverged, 2 = unusable input. `--json` writes `{first_divergent_tick, fields, likely_origin, last_equal_tick, ticks_compared, packets_compared}` for scripted gating in later M2 rounds.

`.hwo` files are derived from running the game, so they live in `original/scratch-m2/oracle-runs/` (gitignored) like every other game-derived file; the repo holds only the tool, the format header and the procedure.

---

## 6. Risks, and the fallback

### 6.1 Why bit-exact GOG vs `hwi-engine` is unlikely across the board

| risk | evidence | effect | mitigation |
|---|---|---|---|
| **x87 excess precision vs SSE** | the exe is 32-bit x87 code; optimised MSVC 6 code keeps intermediates on the FPU stack between spills (e.g. `(z+y)+x` at 0x0054A210 with no store), while x86-64 C evaluates `float` expressions in `float` (`FLT_EVAL_METHOD == 0`) | any multi-operation float expression can round differently: dot products, matrix-vector products, integrations | bit-exact only by mirroring each spill point (`double` intermediates, `float` casts where MSVC stores to memory), done per function, only where the comparison shows divergence starts. At 53-bit precision control, `double` arithmetic in C matches x87 rounding exactly between spills (barring overflow and underflow, where the x87's wider exponent differs) |
| **precision-control word** | Windows/Wine default is 53 bits (assumed; recorded by the DLL in `fpucw`); D3D devices created without `FPU_PRESERVE` switch to 24 bits; hw_rgl's D3D9 path sets `FPU_PRESERVE` (`docs/cet-knowledge.md:1294`) | a different renderer or driver can change every float result of the GOG run itself | record `fpucw` per tick; reject reference runs whose word changes; use the same renderer DLLs for every reference run |
| **x87 transcendental instructions** | 133 `fsin`, 142 `fcos`, 16 `fpatan`, 6 `fptan`, 16 `fsqrt` in code below 0x00670000 (section 1.5) | `fsin`/`fcos`/`fpatan`/`fptan` are not correctly rounded and do their own argument reduction; no libm matches them bit for bit | `fsqrt` is safe: at 53-bit precision, sqrt rounded twice (53 then 24 bits) equals a correctly rounded `sqrtf`. For the others, a software model of the x87 algorithms is possible but large (UNVERIFIED that a usable one exists); otherwise they are a known tolerance source |
| **fused multiply-add and fast-math** | GoK's `meson.build` sets none of `-ffast-math`, `-ffp-contract`, `-ffloat-store`, `-mfpmath` (grep of the whole file) | on hardware with FMA, `-march=native` or a default `-ffp-contract=fast` lets the compiler fuse `a*b+c`, which the exe never does | build `hwi-engine` with `-ffp-contract=off`, no `-ffast-math`, no `-march=native` (Linux) and `/fp:precise` (Windows); the build doc (M2-R1 item 4) should pin these |
| **compiler differences in general** | the GOG exe was linked by MSVC 6 (linker 6.0, MSVC 6 CRT entry, `docs/binary-fingerprint.md:12-15`); the compiler of each object file is not recorded; Relic's own HW1 release mixed Intel C and `cl -Od` per file (`docs/hw1-port-lessons.md:27-35`), so even HW1's file-level float behaviour is not uniform | evaluation order inside expressions, constant folding in higher precision, inlining | per-function mirroring as above; accept tolerance elsewhere |
| **wall clock inside the simulation** | `realtimeelapsed` from QPC (section 2.5), stream 0 seeded by `wSecond` and drawn by the renderer (section 2.6), AI seed by `wSecond` (`ranRandomize(6)`) [audit: the AI seed applies only to a new single-player campaign game; for skirmish and multiplayer, `gameStart` resets stream 6 with `cat_ranResetGameStreams` (section 2.6)] | the GOG build is not reproducible against itself unless neutralised | H3, H4, H5 in the GOG run; the same settings in `hwi-engine`; proven by the section 4.4 ladder |
| **checksum blind spots** | the engine's `univcheck` sums ship positions only (section 2.2) | a recreation bug that moves only resources, derelicts, bullets or missiles, or changes health, is invisible to the engine's own check | the oracle's sub-checksums cover those lists (section 3.4) |
| **unverified offsets** | `Ship.health` is at +0x170 by the code but CET says 0x158 (section 2.8); list offsets other than `ShipList` and the `Player` field map are unasserted | wrong fields hashed: stable but meaningless, or a crash on a bad pointer | the DLL's first-tick self-test (section 3.4); R2's struct work fixes the offsets before the DLL is built |
| **hidden nondeterminism in the exe** | not yet looked for: uninitialised fields, pointer-keyed ordering, `memAlloc` fill patterns | GOG runs that differ for reasons outside the simulation code read here | the section 4.4 ladder catches it; then narrow with `/logOnVerbose` `S:` lines |
| **the hook disturbs the game** | the detours run on the universe task's stack; calls into the game happen at main-menu time (H6) | crashes or a different code path than a human-started game | keep detours minimal; compare a recording made with the DLL against one made without it (the in-game record button, if visible) using the game's own sync counters |

### 6.2 Fallback: tolerance-based, windowed state comparison

If bit-exact agreement cannot be reached (expected for long horizons because of 6.1), the oracle switches from "equal streams" to "same game within tolerance over short windows":

1. **Exact gates that must still hold**, because they are integers or discrete: ship counts per list, ship IDs and types in list order, owner and command order/attributes per ship (`ships_misc`), player resource units and research state, ship deaths (an `E` record per removal, keyed by ship ID and tick), win/lose events. A difference in any of these is a real behaviour difference, not float noise.
2. **Snapshots instead of hashes for floats.** A `S` record every K ticks (K = 16 by default, i.e. once per game second) lists, per live ship: ID, type, owner, position, velocity, rotation matrix, health, and per player the RU count. About 100 bytes per ship, roughly 100 MB per hour with 300 ships at K = 16; K is a DLL option.
3. **Windowed re-synchronisation.** Small float differences grow in a chaotic simulation (collisions, targeting), so long-horizon tolerance comparison says little. Instead, the GOG run saves the game every W ticks with the game's own `SaveGame` (0x004FDAF0, which CET's autosave already calls from inside `univUpdate`; whether a save has simulation side effects is UNVERIFIED and is checked by the section 4.4 ladder with saving on). `hwi-engine` loads the save at tick N, plays the same packets for W ticks, and is compared at N+W. Each window is an independent test, so one divergent mechanic does not invalidate the rest of the run.
4. **Tolerances**: position error relative to the ship's collision radius (from its static info), velocity relative to its max speed, rotation as an angle, health absolute. Starting values (to be tuned on GOG-vs-GOG noise, which should be zero, and on the first recreation runs): 1% of radius, 1% of max speed, 0.5 degrees, 0.5 health points, after W = 160 ticks (10 s). The tool reports the error growth per tick within each window, not only pass/fail, because a steady exponential growth from tick 1 points to a float-order difference, while a step change points to a logic bug.

The tolerance mode is a fallback, not the target: every mechanic whose window shows a step change is a porting bug to fix, and every function where bit-exact agreement turns out to be cheap (integer code, the RNG, the checksums themselves) stays under the exact gate.

---

## Counts

| item | value |
|---|---|
| HW1 checksum components per sync packet | 3 (`randcheck` draws 1 `gamerand`, `univcheck`, `blobcheck`) |
| Cataclysm checksum entry | 16 bytes (HW1: 12), packet header 0x1C bytes (HW1: 0x18) |
| Cataclysm functions located for the oracle | 23 (table 2.1 incl. the `ran*` and `recPack*` groups counted individually: `univGetChecksum`, `statsGetStatChecksum`, `netcheckShow`, `netcheckCheckChecksum`, `netcheckFillInChecksum`, `netcheckInit`, `netcheckReset`, `netcheckClose`, `clProcessSyncPacket`, `clWaitSyncPacket`, `universeUpdateTask`, `univUpdate`, `ranRandomFn`, `ranStartup`, `ranRandomize`, `ranParametersReset`, `ranSave`, `ranLoad`, `recPackInGameStartCB`, `recPackInGameStartCBSafeToStart`, `recPackInGameStopCB`, `recPackPlayInGameInit`, `recPackPlayInit`) |
| Hook sites designed | 7 (H1-H7; H1, H2, H7 always, H3-H6 by option) |
| RNG streams | 27 (HW1: 25); stream 0 used by 27 functions, both renderer and simulation |
| functions reading the wall-clock `realtimeelapsed` | 23 including `univUpdate` and `univUpdateReset` |
| x87 transcendental instructions in code below 0x00670000 | 313 (`fsin` 133, `fcos` 142, `fpatan` 16, `fptan` 6, `fsqrt` 16) |
| hidden logging/packet switches in the GOG command table | 6 (`/logOff`, `/logOn`, `/logOnVerbose`, `/logFilePath`, `/packetRecord`, `/packetPlay`) |
| driving candidates evaluated | 6 (A chosen, C second) |

## Open questions

1. **Rule 2 reading (user decision).** Is it acceptable for the oracle DLL to call the game's own functions to start recording and to load a recorded game (`recPackInGameStartCB`, `recPackPlayInGameInit`, `utyLoadMultiPlayerGameGivenFilename`), given that no OS input events are synthesised? If not, each playback needs the user to click "Load recorded game" once.
2. **One recording by the user.** Mode A needs a skirmish started through the menus once per scenario (section 4.3). Which map and player setup should be the first reference scenario?
3. **Toolchain for the DLL.** Freestanding `clang-18` + `lld-link` + `llvm-dlltool-18` (present, untried) or installing `gcc-mingw-w64-i686` (a package install).
4. **Does Cataclysm's own recorded-game playback stay in sync?** Player AI draws stream 0 while recording and not during playback (section 4.4 rung 1); if simulation then reads stream 0, the stock playback desyncs and `rng0=split` becomes mandatory. Only a run answers this.
5. **`Ship.health` offset**: +0x170 by `ApplyDamageToTarget`, 0x158 by CET (section 2.8). R2 struct work.
6. **The new 4-byte sync field** (global 0x00AB7658, a game-start sum over 0x00BB3020): what it sums, and whether anything compares it.
7. **The `univUpdate` tick gate** (0x00A6ED74/0x00A6ED78) and the end-of-playback flag 0x00A6C5C8: meanings inferred, not confirmed.
8. **Which of the other 28 `QueryPerformanceCounter` call sites and the 21 `realtimeelapsed` readers affect simulation state** beyond the ones named in section 2.5.
9. **Calling `utyLoadMultiPlayerGameGivenFilename` from the main loop** (H6) instead of from the front-end task: safe or not.
10. **`taskFrequency`** (0x00BB3260) value at run time, assumed 240 for the virtual clock. [audit note: largely settled from the code. `utyGameSystemsInit` sets `utyTimerDivisor = QPF / 240` (`push 0xF0`, confirmed in the round-5 notes) and calls `taskStartup(QPF / utyTimerDivisor)` (`decompiled.c:381803-381807`). With integer division that is exactly 240 for any QPF above 57,600, and Windows/Wine QPF is about 10 MHz. Not a fix, so not counted]

