<!-- done: cat-functions, cat-bodies, rng, cmdline-recgame, hooks, hw1, gok-other, driving, fixes, verdict -->
# Audit: M2-R1 item 5 "checksum-oracle"

Date: 2026-10-10. Auditor: the adversarial audit stage of the M2-R1 workflow. Worktree `.claude/worktrees/m2`, branch `m2`.

Producer output audited: `docs/m2-checksum-oracle.md`, a design only.

Inputs read:
- The round-5 export `ghidra/exports/cataclysm_gog_r5/`: `functions.csv`, `symbols.csv`, `call_graph.csv`, `decompiled.c` + `decompiled_index.csv`, `datatypes.h`, `imports.txt`, `strings.csv`.
- A full `objdump -d -M intel` of `original/game/cataclysm.exe`, written to `/tmp/hwi-audit-oracle/cat.dis`.
- `objdump -s` reads of the command table, the `gp` callback table and the float constants.
- `strings` on the exe and on two `feman/*.fib` files.
- The producer's ORIG@ad1bb82 download in `original/scratch-m2/oracle-ad1bb82-dl/`. This file tree is the same source that the doc cites.
- `reference/gardens-of-kadesh` (working tree at `3af31fb`, read-only).
- `tools/run_game.sh` and `docs/CONSTRAINTS.md`.

Audit helper scripts are in `/tmp/hwi-audit-oracle/` (`fn.py`, `cmdtab.py`, `count.py`, `streams.py`, `reach.py`), outside git.

Nothing was committed. Ghidra was not run, the game was not launched, no input was injected, and the reference clones were only read.

## Method

1. I looked up every function the doc names in `functions.csv` (address, size, calls_in, signature). I checked every call site the doc cites against the disassembly, and the callers against `call_graph.csv`.
2. I re-read the claimed bodies in the disassembly: `univGetChecksum`, `statsGetStatChecksum`, `netcheckShow`, the start of `univUpdate` up to the QPC/`realtimeelapsed` block, and `LoadRecordedGame`. I read the decompiled text of `universeUpdateTask`, `aiplayerStartup`, `aiplayerUpdateAll`, `recPack*`, `netcheck*`, `ran*`, `gameStart` and `utyLoadMultiPlayerGameGivenFilename`.
3. I dumped the first 7 to 11 bytes of every hook site and compared them with the prologue column of table 3.2.
4. I recounted with scripts:
   - the stream-index users (`ranRandomFn(N)` per function) and the `realtimeelapsed` readers, over `decompiled.c`;
   - the x87 transcendental instructions in 0x00401000-0x0066FFFF;
   - the `recPack*` functions;
   - the RNG streams reachable from the player-AI roots `cat_aisPlay` and `aifFleetCommand`, as a transitive closure over `call_graph.csv`.
5. I decoded the 24-byte `commandoption` table at 0x008B8870 with `cmdtab.py`.
6. I checked every `ORIG:` file and line citation in the ad1bb82 tree, and every `GoK:` citation in the 3af31fb working tree.
7. I followed the playback driving path through the code, to see whether anything in it needs input.

## Findings

C = correct, W = wrong (fixed inline in the producer doc with `[audit: ...]`). Items I could not settle are listed separately and are not counted.

### Cataclysm functions, globals and bodies (54 claims, 6 wrong)

| # | Claim | Evidence | Result |
|---|---|---|---|
| C1 | `univGetChecksum` 0x00549F50, 732 B, only caller `netcheckShow` at 0x004CC40C | functions.csv; `4cc40c: call 0x549f50` | C |
| C2 | `statsGetStatChecksum` 0x00525170, 398 B, called at 0x004CC415 | functions.csv; disasm | C |
| C3 | `netcheckShow` 0x004CC400, 189 B, called by `clProcessSyncPacket` at 0x00450985 | `450985: call 0x4cc400` | C |
| C4 | `netcheckCheckChecksum`/`FillIn`/`Init`/`Reset`/`Close` addresses; `FillIn` called by `captainServerTask` at 0x00451901 | functions.csv; `451901: call 0x4cc2f0` | C |
| C5 | `clProcessSyncPacket` 0x00450910, 2019 B, 2 sites in `clWaitSyncPacket` | 2 × `call 0x450910` | C |
| C6 | `clWaitSyncPacket` 0x00451280, called at 0x0053E872 | disasm | C |
| C7 | `univUpdate` 0x0054C3F0, 2605 B; sites 0x0053E8AA, 0x0053E8E0, 0x0053E9BA, plus `statsShowFancyFight` and `statsGatherFightStats` | disasm (5 call sites); call_graph | C |
| C8 | `ranRandomFn`: 179 functions, 574 sites | calls_in 179; 574 × `call 0x4ef860` | C |
| C9 | `ranStartup`/`ranRandomize`/`ranParametersReset`/`ranSave`/`ranLoad` addresses | functions.csv | C |
| C10 | "`recPack*` (12 functions)" | functions.csv has 11 (0x004CBAF0 to 0x004CC1B0 `recPackPlayClose`) | **W**, fixed |
| C11 | `univGetChecksum`: ships only; `test ah,0x8`; +0x58/+0x5C/+0x60, each stored after its add; returns `(z+y)+x` at 0x0054A210 | disasm 0x549F50-0x54A22B; the third `fadd [esp+0x18]` follows two `pop`s, so it reads x | C |
| C12 | `statsGetStatChecksum`: 10.0f at 0x0072A4F0; 59 `GetShipStaticInfo` calls whose sum is discarded; fields +0x4/+0xC/+0x2C/+0x35C; `crc32Compute(+0x30, 0x244)`; bounty byte; stride 0x6530; `numPlayers` a uword at 0x008860EE | disasm 0x525170-0x5252FD; constant bytes `00002041` | C |
| C13 | `netcheckShow`: univ, stat, `ranRandomFn(10)`; 16-byte ring at 0x00C30FC0; valid flags at 0x00C317E0; last at 0x00C30FA4; +0xC copied from 0x00AB7658 | disasm 0x4CC400-0x4CC4BC | C |
| C14 | `rand/univ/blob/pktSyncErr` and the frame globals | `netcheckCheckChecksum`/`netcheckReset` decompiles; `clProcessSyncPacket` 0x45093A | C |
| C15 | 0x00AB7658 is written only in `gameStart`, as an MP-only sum over 0x00BB3020 | only write in the exe is at 0x005A9F1D | C |
| C16 | citation `decompiled.c:379139-379144` for that write | those lines belong to `utyFatalErrorWaitLoop` (index 378477-379152), as a decompile overrun | **W**, fixed |
| C17 | `netcheckCheckChecksum` compares only rand/univ/blob, then `|= 2` | decompile 183286-183335 | C |
| C18 | `numberOfCommands` at packet +0x18, commands at +0x1C | `45098c: mov ax,[esi+0x18]`, `add esi,0x1c` | C |
| C19 | `.pkts` header 0x2B0; `HEAD`/`PACK` magic values | `recPackPlayInit`, `recPackPlayGetNextPacket` | C |
| C20 | `universeUpdateTask` branch conditions; `push 0x3D800000` at the three sites; `netCheck` at 0x0053E8A0 | decompile 287669-287934; disasm 0x53E8A5/0x53E8DB/0x53E9B5 | C |
| C21 | turbo x9 is a "Cataclysm addition" | HW1 `UNIVERSE_TURBO_REPEAT 19`, `>>1` = 9 (`universe.c:96,3935`) | **W**, fixed |
| C22 | 0x00A6C5C8 is set on `NO_PACKET` during playback | `decompiled.c:287850` (also cleared at the top of each loop) | C |
| C23 | tick gate 0x00A6ED74/0x00A6ED78: early `return 0` with no counter increment | `54c40a: jle 0x54ce10` leads to `xor eax,eax; ret`; the increment is at 0x54C56C | C |
| C24 | `/SaveMPGames` autosave 0x00A74DAC, 200 s (0x0072A440) | disasm 0x54C416-0x54C489; bytes `00004843` = 200.0 | C |
| C25 | QPC at 0x0054C57F `FF 15 28 A2 72 00`, argument pushed at 0x0054C56D, feeds 0x00B62AC4 | disasm; imports.txt `iat=0x0072a228 code_refs=29` | C |
| C26 | `phystimeelapsed`/`totaltimeelapsed`/`realtimeelapsed`/`univUpdateCounter` = 0x00B62ABC/0x00B62AC0/0x00B62AC4/0x00B62BAC | disasm 0x54C54E-0x54C5F6 | C |
| C27 | win timer compared with `realtimeelapsed` | `54c62f: fld [0xb62ac4]; fcomp [0xb62bbc]`, then `call DeclarePlayerWin` | C |
| C28 | 23 functions read `realtimeelapsed`; `singlePlayerGameUpdate` 9 times | `streams.py`: 24 hits, one of them only in `cat_univRealTimeDelta`'s comment, so 23 | C |
| C29 | 27 streams (`ranStartup` up to 0x00C2FFA0, `ranSave`/`ranLoad` up to 0x00C2FF9C) | 540 B / 20 = 27 | C |
| C30 | `ranStartup` advances stream 0 by `wSecond` | decompile 214550-214594 (`SYSTEMTIME` +12) | C |
| C31 | HW1 generator and constants unchanged | `ranRandomFn`/`ranParametersReset` decompiles | C |
| C32 | `determCompPlayer` 0x00A75248 is not in the command table; its only writer is demo-record start | `strings` has no `/determCompPlayer`; the only write is at 0x005AC550 (`decompiled.c:382068`) | C |
| C33 | stream 6 stays wall-clock seeded in a fresh skirmish unless H5 is used (2.6 table, 4.1 B, 6.1, H5) | `gameStart` calls `aiplayerStartup` at 0x005A9367 and then `cat_ranResetGameStreams` (resets streams 1..26) at 0x005A9A98 when `loadfilename==NULL && !singlePlayerGame` | **W**, fixed in 4 places |
| C34 | per-stream function counts 0:27, 1:14, 4:9, 5:5, 6:34, 7:1, 8:5, 9:2, 10:62 | `streams.py`, exact match | C |
| C35 | `univFindNearestResource` draws stream 0 under `RepairTech` | `decompiled.c:297805` | C |
| C36 | `airDumbResearchRequests` draws stream 0 and is player-AI code | `reach.py`: reached only via `aifFleetCommand` → `airResourceManager` | C |
| C37 | `gameStart` draws stream 0 (`:379301`) | decompile | C |
| C38 | none of the 62 stream-10 users is player-AI code | direct-caller list; the transitive paths from AI roots all pass through `clWrap*`/`BuildShip` | C |
| C39 | hidden table entries `/logOff`/`/logOn`/`/logOnVerbose` → 0x00A43CC0 = 0/1/2; `/logFilePath` 0x0055F4D0; `/packetRecord` 0x0055F5A0 / 0x00A75188; `/packetPlay` 0x0055F550 / 0x00A7518C; new `/aiplayerLog`, `/SaveMPGames` 0x00A74DAC, `/LoadMPGame` 0x0055F500 | `cmdtab.py` decode at 0x008B8D20-0x008B8DE0 (flag 0 on the log and packet entries) | C |
| C40 | `/determCompPlayer`, `/debugSync`, `/autosavedebug`, `/demoRecord`, `/demoPlay` absent | `strings -a` on the exe | C |
| C41 | `LoadRecordedGame` at 0x00487CF0 is not a Ghidra function and calls `VerifySaveFile`, `recPackPlayInGameInit`, `utyLoadMultiPlayerGameGivenFilename`; `IG_SaveRecordedGame` at 0x00487EF0 calls `recPackInGameStartCB`; both are registered at 0x00881138/0x00881140 | disasm; `objdump -s` of the table and its name strings | C |
| C42 | `multiplayer_game.fib` has "LOAD RECORDED GAME"; `in_game_esc_menu.fib` links to `Game_record_save` | `strings` | C |
| C43 | H6 replicates `LoadRecordedGame` with "the two calls it makes" | it also calls `feScreenDisappear(NULL,NULL)` (0x00487E39), then writes 0x00A7519C=1 and 0x00A751A0=1 and sets 0x00A75190 from a name check (0x00487E4D-0x00487EDE). Simulation code reads 0x00A7519C and 0x00A75190 (`processAttackToDo`, `cat_clHarvestNearestResource`, `cat_cm*`) | **W**, fixed |
| C44 | `aiplayerUpdateAll` skips per-player AI when `playPackets` | `decompiled.c:14868` | C |
| C45 | `recPackInGameStartCB` copies the names to 0x00B39F40/0x00B39F00 (+ ".pkts") and sets 0x00A43E18 | decompile 182534-182668 | C |
| C46 | the safe point (`decompiled.c:287927`) saves the game, sets `recordPackets`, `recordFakeSendPackets`, `sigsPressedStartGame`, captain 0, resets the ring and opens `netlog.txt` (SP/skirmish branch) | decompile 182669-182751 | C |
| C47 | `recPackInGameStopCB` 0x004CBD10 | functions.csv; decompile | C |
| C48 | `taskStart("CaptServTask", captainServerTask, 0.125, 0)` at `:381909` | decompile | C |
| C49 | in playback, packets come from `recPackPlayGetNextPacket` 0x004CBF00 via `clWaitSyncPacket` | call_graph | C |
| C50 | `utyLoadMultiPlayerGameGivenFilename` sets 0x00A43CB8 and then calls `gameStart(filename)` | decompile 380423-380485 | C |
| C51 | `ApplyDamageToTarget` reads and writes +0x170 (0x00546257, 0x0054643E, 0x00546448) | disasm | C |
| C52 | x87 count: `fsin` 133, `fcos` 142, `fsqrt` 16, `fpatan` 16, `fptan` 6 (313) | `count.py`, exact | C |
| C53 | `cat_aisReset` on load cited at `decompiled.c:379136` | line 379137 is the overrun text; `gameStart`'s own call is at 379792 | **W**, fixed |
| C54 | `gameStart` calls `cat_aisReset()` on load too | 379792 runs unconditionally before `gameIsRunning = 1` | C |

### Hook sites (10 claims, 1 wrong)

| # | Claim | Evidence | Result |
|---|---|---|---|
| H1 | `A1 74 ED A6 00`, then `81 EC D0 00 00 00`; the detour logic (counter compare) is sound | disasm 0x54C3F0; early return at 0x54CE10 skips the increment | C |
| H2 | `83 EC 08 53 55`, then `56 57` | actual bytes are `56 8D 44 24 10 57` | **W**, fixed |
| H3 | site, bytes and `vqpc` formula; the code after the call is `delta=QPC-[0xA6D9A0]`, `[0xA6D9A0]=QPC-(delta mod div)`, `realtime += (delta div div)/taskFrequency` | disasm 0x54C585-0x54C5F6 | C |
| H3b | 28 other QPC code refs | imports.txt: 29 in all | C |
| H4 | `8B 44 24 04 56`, then `8D 04 80` | disasm | C |
| H6 | `55 8B EC 51 53`, then `56 57`; caller `utyTasksDispatch` 0x005ACD10 | disasm; call_graph | C |
| H7 | `83 EC 0C 53 55`, then `56 57` | disasm | C |
| Hb | the first 5 bytes end on an instruction boundary at every site and contain no relative branch | disasm of all 5 entry sites | C |
| Hs | ship hash offsets: `shipID` +0x43C, velocity +0x64, `coordsys` +0x80 | `univGetChecksum` log arguments; `datatypes.h` `PosInfo` is 40 B from +0x58 | C |
| Hm | `[+0x370]+0x14/+0x16` command order/attributes; owner `[+0x32C]+0x24` | `univGetChecksum` disasm 0x549FDF-0x54A06B | C |

### HW1 source (ORIG@ad1bb82) (21 claims, 2 wrong)

| # | Claim | Evidence | Result |
|---|---|---|---|
| O1 | `NetSyncChecksums`/`HWPacketHeader` at `CommandNetwork.h:207-233`; `SYNC_CHECK 1` at `switches.h:16` | read | C |
| O2 | `gamerand` at `randy.h:105,114` | read | C |
| O3 | stream enum at `randy.h:25-50` | actually lines 34-59 | **W**, fixed |
| O4 | HW1 `univGetChecksum` covers live ships, live derelicts and all resources, returns `x+y+z` (`univupdate.c:5096`) | read | C |
| O5 | HW1 `statsGetStatChecksum` CRCs `shiptotals` twice (`stats.c:2316-2396`) | read | C |
| O6 | `SendCheatDetect` at `netcheck.c:902-904` | actually at 916-917 | **W**, fixed |
| O7 | `SOF_Dead 0x1000` (`spaceobj.h:929`) | read | C |
| O8 | `univCalcShipChecksum` ship loop is commented out (`/* ... */` 5382-5582) and 128 is added per owned bullet; `GOD_LIKE_SYNC_CHECKING` only `#ifndef HW_Release`; `TreatAsUdword` at `TYPES.H:186` | read | C |
| O9 | `UNIVERSE_UPDATE_RATE 16`/`PERIOD` (`universe.h:57-58`); `CAPTAINSERVER_PERIOD` (`CommandNetwork.h:124`) | read | C |
| O10 | `universeUpdateTask` lines :3885, :3946, :3957, :3972, :4027 | read | C |
| O11 | `clProcessSyncPacket` order: packet check, record, `netcheckShow` | `CommandNetwork.c:1501-1527` | C |
| O12 | `netcheckShow` order univ, blob, rand; ring; log format | `netcheck.c:937-985` | C |
| O13 | `netcheckFillInChecksum` 832-842; called at `CommandNetwork.c:2147` | read | C |
| O14 | `determCompPlayer` at `AIPlayer.c:684-687`; the switch is only `#ifndef HW_Release` (`Win32/main.c` ~1272-1280) | read | C |
| O15 | `ranSave`/`ranLoad` at `SaveGame.c:654,775` | read | C |
| O16 | `AIPlayer.c:859` `if (!playPackets)` | read | C |
| O17 | `CommandWrap.c:91` guard "and 20 more" | 21 `playPackets` occurrences | C |
| O18 | `fmathSqrtDouble` at `fastmath.c:87`, macro at `fastmath.h:18`; inline asm at `matrix.c:381,482,564` | read | C |
| O19 | `MAKEFILE` mixes `cl -Od` (CCMD0) and `icl /Ogitb1` | `MAKEFILE:142-149` | C |
| O20 | `numPlayers = 1 + numComputers` (`Win32/utility.c:3133-3140`) | read | C |
| O21 | `src/Game` has 302 files | `ls \| wc -l` = 302 | C |

### GoK and other references (6 claims, 0 wrong)

| # | Claim | Evidence | Result |
|---|---|---|---|
| G1 | GoK `Universe.c:3786`, `UnivUpdate.c:7271/5004`, `NetCheck.c:918`, `Stats.c:2042`, `Universe.c:75` (`RATE_SHIFT = 2`), `SDL/main.c:848,2195` (`/vanilla`); clone at 3af31fb | read; `.git/shallow` = 3af31fbc… | C |
| G2 | GoK `meson.build` sets no fast-math, fp-contract, float-store or mfpmath flags | grep: none | C |
| G3 | `run_game.sh --exe`, `--no-gamescope`, `--virtual-desktop` | `tools/run_game.sh:54-76,110-112,197-205` | C |
| G4 | the CET log lines dated 2026-10-08 13:54; `clang-18`, `lld-link` and `llvm-dlltool-18` in `/usr/bin` | read; `ls` | C |
| G5 | `cet-knowledge.md:1294` documents `FPU_PRESERVE` | read | C |
| G6 | `DIRSERVER_PORTS` at 0x00BB2FA0 | symbols.csv | C |

### Totals

- Claims sampled: 91 (54 + 10 + 21 + 6).
- Wrong: 9 (C10, C16, C21, C33, C43, C53, H2, O3, O6).
- Correct: 82.
- Precision = 82 / 91 = **0.901**.

## Driving method: does it really need no input?

I followed the code path for mode A playback:

1. **Start.** H6 runs inside `taskExecuteAllPending`, which `utyTasksDispatch` calls in the main loop. From there it calls `recPackPlayInGameInit` and `utyLoadMultiPlayerGameGivenFilename`. These are ordinary function calls on the main thread. No OS input event is created.
   - `recPackPlayInGameInit` sets `playPackets` (0x00A7518C) and opens the `.pkts` file.
   - `utyLoadMultiPlayerGameGivenFilename` calls `gameStart(file)`, which calls `LoadGame` and then `ranLoad`, and sets `gameIsRunning = 1`.
2. **Ticks.** With `playPackets` set, `universeUpdateTask` takes the lockstep branch. `clWaitSyncPacket` pulls each packet from `recPackPlayGetNextPacket`, `clProcessSyncPacket` executes its commands, and two `univUpdate` calls follow each packet.
3. **No live decision makers.** `aiplayerUpdateAll` skips every computer player when `playPackets` is set. In HW1 the `clWrap*` functions return early on `playPackets`; the Cataclysm equivalents were not checked (R2).
4. **End.** On end of file `clWaitSyncPacket` returns `NO_PACKET`, which sets 0x00A6C5C8. The DLL then calls `ExitProcess`, so no quit menu is involved.

Nothing in this path needs keyboard or mouse input.

- **Recording.** The user starts the skirmish through the menus personally. Rule 2 explicitly allows asking the user when input is needed. The DLL starts the recording by calling `recPackInGameStartCB`, a function call and not an input event.
- **No input tools or gamescope.** Nothing in the design uses xdotool, XTEST or ydotool, and no launch line uses gamescope. All launch lines use `--no-gamescope --virtual-desktop`, as `docs/CONSTRAINTS.md` rule 1 recommends.
- **Rule-2 reading.** The doc correctly leaves to the user whether calling game functions from the hook counts as acceptable under rule 2 (open question 1).

The driving design holds, with one correction (finding C43): H6 must also make the `feScreenDisappear` call and the three flag writes that `LoadRecordedGame` performs.

## Fixes made in `docs/m2-checksum-oracle.md`

Nine findings, all marked inline with `[audit: ...]` (13 inline marks; one finding needed marks in several places), plus one `[audit note]` that is not counted:

1. `recPack*` count 12 → 11 (table 2.1).
2. `randy.h:25-50` → `34-59` (section 1.4).
3. `netcheck.c:902-904` → `916-917` (table 1.1).
4. The 0x00AB7658 writer citation: `379139-379144` is a decompile overrun into `utyFatalErrorWaitLoop`; the real write instruction is at 0x005A9F1D (section 2.4).
5. Turbo x9 is the same as HW1, not an addition (section 2.5 and the diff table 2.8).
6. Stream 6 is reset by `cat_ranResetGameStreams` in new skirmish and multiplayer games. Fixed in the 2.6 table, in the H5 purpose, in candidate B of 4.1, and in the wall-clock row of 6.1.
7. H2 prologue continuation `56 57` → `56 8D 44 24 10 57` (table 3.2).
8. `cat_aisReset` citation `379136` → `379792` (table 4.1, row C).
9. The H6 recipe is incomplete: added `feScreenDisappear(NULL,NULL)` and the post-load writes to 0x00A7519C, 0x00A751A0 and 0x00A75190 (section 4.3).

I also added an `[audit note]` to open question 10, which the code largely settles: `taskFrequency` = QPF / (QPF/240) = 240 for any QPF above 57,600. It is not counted as a fix.

## Unverifiable or open (not counted)

- **x87 precision under `rgl.dll`.** That the OpenGL path through `rgl.dll` leaves 53-bit precision is labelled a GUESS in the doc and was not checked. Only a run with `fpucw` recorded answers it.
- **Cataclysm `clWrap*` behaviour.** Whether Cataclysm's `clWrap*` functions return early on `playPackets`, and turn player-AI orders into packets while recording, is R2 work. C38 depends on it: the only paths from the AI roots to stream-10 users go through `clWrap*` and `BuildShip`.
- **Meaning of the three flags** that `LoadRecordedGame` writes (0x00A7519C, 0x00A751A0, 0x00A75190). They are unnamed in `symbols.csv`.
- **Remaining doc-labelled unknowns**, not re-derived:
  - the summands of 0x00AB7658;
  - the meanings of the tick gate and the end-of-playback flag;
  - the full `SOF_*` renumbering;
  - whether the record buttons are visible in the running game;
  - whether it is safe to call the loader from the main loop rather than from the front-end task (open question 9);
  - the `/LoadMPGame` semantics.
- **H3 coverage.** `cat_univRealTimeDelta` (0x0054C360) also reads the real QPC and advances `realtimeremainder` and `realtimeelapsed`, but only while paused (`universeUpdateTask` calls it in place of `univUpdate` when 0x00A6C5B4 is set). H3 does not cover it. This is harmless in an unattended run, which never pauses, but it is worth one line in the design.
- **0x00A6C5C8 is transient.** It is cleared at the top of every `universeUpdateTask` iteration and set again on each `NO_PACKET`. The DLL must sample it between task runs (H6 can) or at the moment it is set.
- **`--exe` drops the default arguments.** In `--exe` mode `run_game.sh` does not add GOG's default `/noswddraw`, or `/window` (`run_game.sh:207-212`). The oracle launch lines pass only `/logOn`, so they run with different renderer and window arguments from a normal launch. Pass `/noswddraw` explicitly if a reference run should match the stock launch.
- **Artifact functions in the stream counts.** Both the stream-0 count (27) and the stream-10 count (62) include `utyFatalErrorWaitLoop`, whose decompile overruns into `gameStart`. The producer's method reproduces them exactly, but there are 26 and 61 distinct real functions.

Verdict: PASS-WITH-FIXES - 82/91 sampled claims correct (precision 0.901); every function address, hook prologue (first 5 bytes) and HW1 mechanism checks out and the playback driving path needs no input or gamescope; 9 errors fixed inline, the two substantive ones being that gameStart's cat_ranResetGameStreams already makes stream 6 deterministic in skirmish (H5 is redundant there) and that H6 must also replicate LoadRecordedGame's feScreenDisappear call and its three post-load flag writes.
