<!-- done: setup, AIAttackMan-AIHandler, AIMoves, AIOrders-AIVar, Alliance-BTG, Camera-ColPick, CommandLayer-Dock, ETG, fastmath-HS, KAS-KASFunc, Key-MEX, MultiplayerGame-NetCheck, NIS-Options, Particle, Physics-ResearchGUI, SaveGame, ScenPick-SinglePlayer, SoundEvent-StringSupport, Tactical-TradeMgr, Tutor-Universe, UnivUpdate-Ships, bink-Light, main-Queue, render-Titan, TitanInterface-utility, audit-reclass, name-fixes, tool-fixes, stage-counts, report -->
# M2-R2 re-read of the low-confidence IDENTICAL functions (user decision 4)

Status: **COMPLETE, 2026-10-10.** Analysis only: `src/` was not edited, Ghidra was not run, nothing was committed. Branch `m2`, worktree `.claude/worktrees/m2`.

## Result

All **1,013** functions that M2-R1 classified IDENTICAL with low confidence were read by hand. For each one, both bodies were read side by side: the Cataclysm decompilation (`ghidra/exports/cataclysm_gog_r5/decompiled.c`) and Relic's source at ad1bb82 (`src/reference/hw1-ad1bb82`; `Win32/` from the ad1bb82 archive). Where GoK's `src/` body differs from ad1bb82, that diff was read too.

| result | functions | share |
|---|---:|---:|
| IDENTICAL | 870 | 85.9% |
| MODIFIED | 140 | 13.8% |
| REWRITTEN | 3 | 0.3% |
| **read** | **1,013** | |

- **143 functions changed class (14.1%).** The re-read IDENTICAL agreement for the IDENTICAL/low bucket is **85.9%**, close to the 86% that M2-R1's 51-function sample predicted.
- **What the 143 changes are.** Counted from the wording of the change lines:
  - 59: ETG/particle script callables that take a pointer to their argument slot instead of the value.
  - 10: KAS functions keyed by hashes.
  - 8: changes only in which ship types or races a test names (change line starts `enum:`).
  - 4: a callee gained an argument (`trTextureRegister` 4th, `clCommandMessage` 2nd, `tutSetPointerTargetAIVolume` 3rd).
  - 62: other logic changes, listed below.
- **The 3 REWRITTEN functions:** `AsteroidTakesDamage` (new asteroid damage model), and `statsNumShipsNeededToKillFleet` and `statsGetRelativeFleetStrengths` (HW1's kill-rating computation replaced by table lookups and a ship-count ratio).
- **Confidence of the 1,013 verdicts:** 969 high, 42 medium, 2 low. The 2 low are `fqMix` and `fqEqualize` (see open questions).

**Counted as IDENTICAL (not as a change):**
- header-level constants: renumbered enums (`MOVE_*`, `SOF_*`, `CLASS_*`, `RAN_*`, screen enums), struct offsets and sizes, and macro values;
- inlined or linker-folded callees;
- Ghidra prototype noise (`__thiscall` as `__fastcall`, dropped `sscanf` out-arguments, merged parameters);
- debug code that is compiled out.

**Conventions for MODIFIED:**
- A test that names other ship types or races than HW1 (Cataclysm's per-race types in place of HW1's, or types added or dropped) is MODIFIED. Pure renumbering stays IDENTICAL.
- A callee that gained or lost an argument makes the caller MODIFIED.
- An inlined callee's change counts against the callee's own record when it has one. When it has none (for example `Load_shipsinprogress`), the change counts against the caller.

### Totals after M2-R2

| class | M2-R1 functions | M2-R2 functions | M2-R1 bytes | M2-R2 bytes | M2-R2 share of bytes |
|---|---:|---:|---:|---:|---:|
| IDENTICAL | 3,357 | 3,175 | 658,389 | 633,417 | 51.1% |
| MODIFIED | 983 | 1,168 | 569,953 | 595,973 | 48.1% |
| REWRITTEN | 80 | 77 | 10,505 | 9,457 | 0.8% |

These totals include the re-read, the audit reclassifications and the tool fixes. 1,191 functions are now hand-reviewed; M2-R1 had reviewed 168.

**New IDENTICAL agreement estimate:**
- Of the 3,175 IDENTICAL functions, 984 were read by hand.
- The other 2,191 are IDENTICAL by the tool alone. They sit in the automatic IDENTICAL/high and /medium buckets. Those buckets agree with review 87.5% (14 of 16 reviewed) and 91.3% (42 of 46).
- Applying those rates, about 1,987 of the 2,191 (90.7%) are truly identical, so about 93.6% of all IDENTICAL functions are. The M2-R1 audit estimate was 80%.
- The two buckets hold only 62 reviewed functions between them, so this is a rough figure.

The estimated true byte split, correcting each automatic bucket by its review agreement, is IDENTICAL 56.4% / MODIFIED 43.3% / REWRITTEN 0.2% (M2-R1: 58% / 41% / 0.5%).

## Audit reclassifications (step 2)

Each item from `docs/audits/m2-r1-function-deltas.md` was checked against both bodies before it was applied. There are 20 review entries with `"sample": "audit-R2"`. The audit's other kasfTimer items, and `meshTextureRegisterAllPlayers`, had already been settled in the re-read.

- **kasfTimer\* family → MODIFIED.** The 8 read in the re-read carry the hash-key change line. `kasfTimerStop` 0x00491fe0 is MODIFIED too (it was REWRITTEN): it is hash-keyed, and it now calls `timTimerStop`, where HW1 KASFunc.c:239 called `timTimerStart` (a bug, now fixed).
- **SpaceObjRegistryGetShip callers.**
  - 7 → IDENTICAL: `aimFix_Intercept`, `aimFix_AdvancedAttack`, `aimLoad_MoveAttack`, `aimFix_HarassAttack`, `aimFix_Dock`, `LoadRequestShipsCB`, `GravWellGenerator_Fix`. `GetShip` is linker-folded into `GetObj`, `aimInterceptShipDied` into `aimCaptureShipDied`, and `aimAdvancedAttackClose` into `aimCloseKamikaze`.
  - 3 stay MODIFIED, for other reasons found by reading:
    - `LoadThisAIPlayer`: `aiaArmada.targets` is an embedded MaxSelection loaded with `LoadMaxSelectionAndFix`.
    - `LoadConsMgr`: the inlined `Load_shipsinprogress` also fixes a ship pointer per progress entry and uses `GetShipStaticInfo(type)`.
    - `SalCapCorvette_Fix`: the body is now a call to the new `cat_salvage_Fix`.
- **Sample fixes:**
  - `psCurrentScreenDelete` → MODIFIED (cosmetic: it nulls onImage and offImage after freeing them).
  - `aimProcessMoveAttack` → IDENTICAL.
  - `TitanInterface_GetPatch_2` → MODIFIED (HW1's launcher, with more arguments).
  - `GroupShip` → MODIFIED (the research-ship branch is removed and a shipclass != 10 guard is added).
  - `ccGetShipCollCenter` → MODIFIED (the slave/master branch is deleted).
  - `P1MissileCorvetteSpecialTarget` → MODIFIED (the firing loop is deleted).
- **The two suspect binary-map names, re-checked:**
  - `aieCheckEnemyNearby` 0x00404090 is HW1 `aieCheckHealthLow`. Its twin, 0x00404150 `aieCheckEnemyNotNearby`, is `aieCheckHealthHigh`. Both are IDENTICAL to those functions, and HW1's enemy-nearby checks are inlined in `aieExecute`.
  - `RemoveShipFromFormation` 0x00447950 is named right. The same body is inlined in `RemoveShipsFromDoingStuff`, right after `ship->command = NULL`, where HW1 CommandLayer.c:3271-3272 calls it. Cataclysm replaced HW1's `formationcommand = NULL` with a disable-reason and unclamp routine, so it is REWRITTEN/high.
- **Name-fix proposals:** `ghidra/proposals/m2-r2-name-fixes.json`, in the binary-map proposal schema, with no Ghidra run. It holds 0x00404090 → `aieCheckHealthLow`, 0x00404150 → `aieCheckHealthHigh` and 0x004aeb10 → `meshObjectFixupPacoUV`.

## Tool fixes (step 3)

The fixes are in `tools/delta_map.py`. A fresh `classify` keeps every reviewed class (0 mismatches over 1,191 reviewed functions).
- **Linker folds.** The `ICF_FOLDED` table holds the 4 folds that were read by hand. The tool also prints candidate folds it finds in the data. It does not apply them: the 4 it finds are inlined or reworked callees, not folds.
- **Callee arity.** The tool now compares a callee's parameter count with HW1's when the callee's Cataclysm prototype has real parameter names, or has extra `cat_argN` arguments that the binary map counted from caller stack cleanup. It drops over-declared prototypes when the extra slots hold register or stack garbage at the call site, and under-declared ones when the callee reads `in_EAX` or `in_stack_`. It does not use `param_N` prototypes, because a double or a by-value struct splits into several slots (`rgluLookAt` 9 → 12). So the real 4th argument of `trTextureRegister` is still caught only by hand review.
- **One-parameter callables.** It now detects the ETG/particle callables that take one pointer to their argument slot.
- **KAS hash keys.** A function that calls `kasfScopeName` in HW1 and tests the high hash bit in Cataclysm is MODIFIED.
- **Deterministic iteration.** The inlined-callee walk now goes in sorted order. Four `python3 -I` runs gave byte-identical JSON.
- **Audit fix #5** ("`matched` excludes `unres`") is now part of the report text.
- **Effect before review overrides:** 117 functions went from automatic IDENTICAL to MODIFIED and 8 from MODIFIED to IDENTICAL. 48 unreviewed functions changed class, all of them from IDENTICAL to MODIFIED (for example the callers of `clFormation`, `RemoveShipsFromDoingStuff`, `ApplyDamageToTarget` and `spMainScreen`).

## Stage counts (step 4)

`tools/m2_stage_counts.py` is new (a tracked file, not yet committed). It reproduces the M2-R1 per-stage table exactly from the M2-R1 map (3,357 / 983 / 80 HW1-matched, 3,789 cat-new) and recomputes it from the current map. The new table is in `docs/m2-r1-summary.md` ("Delta counts per M2-R2 stage").

The per-file tables in `docs/m2-r1-function-deltas.md` were regenerated with `tools/delta_map.py report`. That file has a new section, "M2-R2 re-read".

## Findings for the porting stages

**Header-level changes seen while reading.** These change no function body, but each one must be in the headers before the IDENTICAL bodies compile to the same code:
- **AI move types.** `AIMoveTypes` is renumbered: the three swarm moves are gone, so `MOVE_FANCYGETSHIPS` is 0x16.
- **Ship flags.** `SOF_*` flags are renumbered (`SOF_Crazy` 0x200000, HW1 0x01000000; the hidden flag is 0x8000).
- **Array and type sizes:**
  - `MAX_MILITARY_SLOTS` is 63 (HW1 41).
  - `AlternativeShips` is 0xA8 bytes (HW1 0x20).
  - `TOTAL_NUM_SHIPS`/`TOTAL_STD_SHIPS` is 0x91.
  - `NUM_RACES` is 7 and `NUM_CLASSES` is 12.
- **Network packets.** `HWPacketHeader` is 0x1C bytes and `AliveStatusPacket` is 0x24. The sync checksum entry is 16 bytes (`netcheckFillInChecksum`, MODIFIED).
- **Struct layouts:**
  - `ClampInfo` is 0x38 bytes (HW1 0x34) and has a per-host list of clamped objects.
  - In `StaticCollInfo`, `uplength`, `rightlength` and `forwardlength` sit 3 dwords later than in HW1.
  - `CustShipFire` is at `staticinfo+0x11C`.
  - The `filesOpen` stride is 0x130.
- **RNG streams.** Streams are renumbered: particles draw from stream 25, HW1's `RAN_ParticleStream` 0. That matters for the checksum oracle (docs/m2-checksum-oracle.md 4.4).
- **Strings.** Cataclysm's string table has `strShipOffset` 0.

**Where GoK `src/` differs from both HW1 and Cataclysm.** The port should follow the binary here:
- `rgluLookAt` and `rgluOrtho2D` are double precision in HW1 and Cataclysm; GoK uses GLfloat.
- The fq audio library has no C source in ad1bb82. 6 of the 9 fq functions in the re-read match GoK's reconstruction. Three differ from it:
  - `fqMix` returns when the level is 0 and has an add-only path for level 1.
  - `fqEqualize` stops zeroing at `nBSize`.
  - `fqDecOver` 0x00558800 copies `FQ_DSIZE` (0x200) floats where GoK copies `nSize`.

## Progress and changed functions

The re-read went by HW1 file, in 24 batches. Each batch was recorded with `tools/delta_map.py mark` and `merge --done reread:<label>`. Class counts below are the final classes.

| batch | HW1 files | read | IDENTICAL | MODIFIED | REWRITTEN |
|---|---|---:|---:|---:|---:|
| 1 | AIAttackMan, AIDefenseMan, AIEvents, AIFleetMan, AIHandler | 16 | 14 | 2 | 0 |
| 2 | AIMoves | 41 | 41 | 0 | 0 |
| 3 | AIOrders, AIPlayer, AIResourceMan, AIShip, AITeam, AITrack, AIUtilities, AIVar | 38 | 33 | 5 | 0 |
| 4 | Alliance, Animatic, Attack, AutoDownloadMap, AutoLOD, B-Spline, Battle, BigFile, BitIO, Bounties, BTG | 26 | 22 | 4 | 0 |
| 5 | Camera, CameraCommand, Captaincy, ChannelFSM, Chatting, Clamp, Clipper, Clouds, Collision, ColPick | 26 | 24 | 2 | 0 |
| 6 | CommandLayer, CommandNetwork, CommandWrap, ConsMgr, CRC32, Damage, Demo, Dock | 38 | 34 | 4 | 0 |
| 7 | ETG | 76 | 55 | 21 | 0 |
| 8 | fastmath, FEFlow, FEReg, File, FlightMan, FontReg, Formation, GameChat, GamePick, Globals, HorseRace, HS | 36 | 34 | 2 | 0 |
| 9 | KAS, KASFunc | 131 | 118 | 13 | 0 |
| 10 | Key, KeyBindings, LaunchMgr, LevelLoad, LinkedList, LOD, Matrix, Memory, Mesh, MeshAnim, MEX | 34 | 28 | 6 | 0 |
| 11 | MultiplayerGame, MultiplayerLANGame, StubTransform, Nebulae, NetCheck | 43 | 41 | 2 | 0 |
| 12 | NIS, Objectives, ObjTypes, Options | 40 | 39 | 1 | 0 |
| 13 | Particle | 60 | 17 | 43 | 0 |
| 14 | Physics, PiePlate, Ping, PlugScreen, Region, ResCollect, ResearchAPI, ResearchGUI | 27 | 22 | 4 | 1 |
| 15 | SaveGame | 37 | 32 | 5 | 0 |
| 16 | ScenPick, Scroller, Select, Sensors, Shader, ShipSelect, ShipView, SinglePlayer | 38 | 35 | 3 | 0 |
| 17 | SoundEvent, SoundEventPlay, SoundEventStop, SpeechEvent, Stats, StatScript, StringSupport | 45 | 38 | 5 | 2 |
| 18 | Tactical, Task, TaskBar, Teams, Timer, TitanNet, TradeMgr | 37 | 37 | 0 | 0 |
| 19 | Tutor, Tweak, Twiddle, UIControls, Undo, Universe | 46 | 36 | 10 | 0 |
| 20 | UnivUpdate, Vector, Volume, Carrier, DefaultShip, DefenseFighter, GenericInterceptor, GravWellGenerator, HeavyCorvette, MinelayerCorvette, RepairCorvette | 27 | 27 | 0 | 0 |
| 21 | bink, Color, ddraw.cpp, debugwnd, font, fqcodec, fqeffect, fquant, glcaps, glcompat, Light | 36 | 36 | 0 | 0 |
| 22 | main, mainrgn, mixfft, mouse, prim2d, prim3d, Queue | 34 | 31 | 3 | 0 |
| 23 | render, rglu, rinit, rinit.cpp, smixer, soundlow, sstream, Subtitle, texreg, TimeoutTimer, Titan | 41 | 37 | 4 | 0 |
| 24 | TitanInterface.cpp, Trails, utility | 40 | 39 | 1 | 0 |
| **total** | | **1013** | **870** | **140** | **3** |

| address | function | HW1 file | class | conf | change |
|---|---|---|---|---|---|
| 0x00401b50 | aiaDivideNewShips | AIAttackMan.c | MODIFIED | high | ship-type count table is 145 entries and the team-making tests use Cataclysm ship types and extra pairings (two alternatives for the BIG fleet); the last two tests (GRAVWELL/CLOAKGEN slots, different types) run only when singlePlayerGame |
| 0x00403eb0 | aieHandlersClear | AIEvents.c | MODIFIED | high | clears 12 handler slots, not 14: fuelLow/fuelHigh are gone from AITeamMove.events (no fuel; the teamDied slot moves to +0x13c) |
| 0x0040f550 | aioCreateCapture | AIOrders.c | MODIFIED | high | race-dependent capture ship: kSalCapCorvette (Kushan) or tSalCapCorvette (Taiidan), same 3-7 count; any other race gets no GetShips move |
| 0x00412490 | NumRUDockPointsOnThisShip | AIResourceMan.c | MODIFIED | high | enum: 2 RU dock points for kCarrier/tCarrier (0x3e/0x4c); Mothership no longer listed |
| 0x00417240 | aitTeamIsDone | AITeam.c | MODIFIED | high | adds a team == NULL guard (returns TRUE); HW1 dereferences team unconditionally |
| 0x0041aa90 | aiuShipNotGoodAgainstFighters | AIUtilities.c | MODIFIED | high | enum: kushan/taiidan IonCannonFrigate, StandardDestroyer, HeavyCruiser plus the Beast bIonArrayFrigate and bHeavyCruiser (8 cases, HW1 3) |
| 0x0041ac40 | aiuIsShipDangerous | AIUtilities.c | MODIFIED | high | enum and classes: Cataclysm-only CLASS_MonsterShip (9) and CLASS_Micro (11) count as dangerous; frigate exception is bDFGFrigate/tDFGFrigate (no ResourceController); the RepairCorvette and DefenseFighter exceptions are gone (corvettes and fighters always dangerous) |
| 0x00421a60 | needToGoToSameVerticalPlane | Attack.c | MODIFIED | high | enum and class: always TRUE for any CLASS_Carrier ship and for sProcessor, bProcessor, sDreadnought, tMissileDestroyer, p1Mothership (HW1: Carrier, MissileDestroyer, P1Mothership by ship type) |
| 0x00428050 | BountyInit | Bounties.c | MODIFIED | high | clears a new 8-float per-player array and stores the raw bounty setting; HW1 stores TW_BountySizes[setting] in universe.bountySize |
| 0x00428070 | calculatePlayerBounties | Bounties.c | MODIFIED | high | bounty computation cut down: per player, sum of buildCost of live ships (no ships-inside-carriers pass, no Drone test) plus resourceUnits, floored at 1.0, written to a new per-player float array; HW1's worth/ruworth shares, maxValue, 250 cap and player->bounty writes are gone |
| 0x00428250 | getPlayerBountyRender | Bounties.c | MODIFIED | medium | returns (sdword) of a float through __ftol (the new per-player float worth array); HW1 returns the sdword player->bounty |
| 0x004345e0 | sendChatMessage | Chatting.c | MODIFIED | high | new 4th parameter selects the packet messageType ((arg != 0) + 1); HW1 always sends CHAT_PACKET |
| 0x004346e0 | clampObjToObj | Clamp.c | MODIFIED | high | reuses an existing clampInfo (alloc only if NULL; ClampInfo 0x38 bytes, HW1 0x34) and registers the clamped object in a new per-host list (host clampInfo + 0xfa8-byte array, count++) |
| 0x004435d0 | needToWaitForThisShip | CommandLayer.c | MODIFIED | high | AdvanceSupportFrigate branch: any docking ship whose dockship is this ship makes it wait (HW1 skips ships in ASF_WAITLATCHPOINT/BACKUP/WAIT/REFUEL); enum: kAdvanceSupportFrigate/tAdvanceSupportFrigate (0x3c/0x4a); the Mothership branch tests shipclass == CLASS_Mothership |
| 0x00449af0 | clSetKamikaze | CommandLayer.c | MODIFIED | high | enum: SalCapCorvette test becomes kSalCapCorvette/tSalCapCorvette (0x48/0x59) |
| 0x00452960 | clFlashShips | CommandWrap.c | MODIFIED | high | flash time is the new cat_taskRealTimeElapsed (real time) instead of taskTimeElapsed, and the ships in the ship's docked-ship list get the same flash time |
| 0x004631f0 | GetDirectionVectorOfShip | Dock.c | MODIFIED | high | the third parameter is a coordinate-system matrix, not a Ship (callers pass creator+0x98 or a local matrix); direction&3 == 3 no longer selects column 3 (result left unchanged) |
| 0x0046caf0 | etgSubTextureDWORD | ETG.c | MODIFIED | high | trTextureRegister gets the new 4th argument (1) |
| 0x0046efc0 | etgEvery | ETG.c | MODIFIED | high | adds a zero-period guard: when the period rounds to 0 frames the block runs (returns sizeof) instead of dividing by zero |
| 0x0046f020 | etgEveryVar | ETG.c | MODIFIED | high | adds the same zero-period guard before totalTime % time |
| 0x0046fd70 | etgInc | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x0046fd80 | etgDec | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x0046fe70 | etgSin | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x0046fe90 | etgCos | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x00470100 | etgOwnerColorSchemeGet | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x004701f0 | etgDeathCriesPlay | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x00471030 | etgEffOffsetLOF | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x004710b0 | etgEffOffsetVelLOF | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x004710c0 | etgEffOffsetSpin | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x004710d0 | etgEffOffsetTime | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x00471100 | etgEffDrag | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x00471120 | etgEffAttachParent | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x00471130 | etgEffSpin | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x00471140 | etgEffDeltaSpin | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x00471230 | etgSetDrag | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x00471250 | etgSetDepthWrite | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x00471630 | etgFloat2Int | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x00471640 | etgInt2Float | ETG.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot (args[0]) instead of the value |
| 0x00476920 | nextLog2 | FEReg.c | MODIFIED | medium | n > 256 now returns -1 (branchless (n <= 256) ? 256 : -1); HW1 returns 256 for every n > 128 |
| 0x00486050 | CreateMilitaryGroupAroundShip | Formation.c | MODIFIED | high | enum: paradeType chosen by a switch over Cataclysm ship types (sCarrier, sProcessor, sMothership, bHeavyCruiser, bCarrier, kCarrier, tCarrier, p1Mothership, trader and derelict types... 23 cases) instead of shiprace R1/R2/P1 x Mothership/Carrier; header-level MAX_MILITARY_SLOTS 63 |
| 0x00490b30 | kasVectorPtr | KAS.c | MODIFIED | high | KAS hash keys: labelled vectors are looked up by comparing the label hash (first dword of the entry) with the argument; HW1 strnicmp on label strings (kasVectorPtrIfExists inlined) |
| 0x00491eb0 | kasfCommandMessage | KASFunc.c | MODIFIED | high | clCommandMessage gets the new 2nd argument (0xffffffff) |
| 0x00491ec0 | kasfTimerCreate | KASFunc.c | MODIFIED | high | KAS hash keys: the timer name arrives as a hash; high bit set = global scope (timTimerX(0, name & 0x7fffffff)), else timTimerX(currentScopeHash or 0, name); kasfScopeName string building is gone and timTim* take (scopeHash, nameHash) |
| 0x00491f00 | kasfTimerSet | KASFunc.c | MODIFIED | high | KAS hash keys: the timer name arrives as a hash; high bit set = global scope (timTimerX(0, name & 0x7fffffff)), else timTimerX(currentScopeHash or 0, name); kasfScopeName string building is gone and timTim* take (scopeHash, nameHash) |
| 0x00491f50 | kasfTimerStart | KASFunc.c | MODIFIED | high | KAS hash keys: the timer name arrives as a hash; high bit set = global scope (timTimerX(0, name & 0x7fffffff)), else timTimerX(currentScopeHash or 0, name); kasfScopeName string building is gone and timTim* take (scopeHash, nameHash) |
| 0x00491f90 | kasfTimerCreateSetStart | KASFunc.c | MODIFIED | high | KAS hash keys: the timer name arrives as a hash; high bit set = global scope (timTimerX(0, name & 0x7fffffff)), else timTimerX(currentScopeHash or 0, name); kasfScopeName string building is gone and timTim* take (scopeHash, nameHash) |
| 0x00492020 | kasfTimerRemaining | KASFunc.c | MODIFIED | high | KAS hash keys: the timer name arrives as a hash; high bit set = global scope (timTimerX(0, name & 0x7fffffff)), else timTimerX(currentScopeHash or 0, name); kasfScopeName string building is gone and timTim* take (scopeHash, nameHash) |
| 0x00492060 | kasfTimerExpired | KASFunc.c | MODIFIED | high | KAS hash keys: the timer name arrives as a hash; high bit set = global scope (timTimerX(0, name & 0x7fffffff)), else timTimerX(currentScopeHash or 0, name); kasfScopeName string building is gone and timTim* take (scopeHash, nameHash) |
| 0x004920a0 | kasfTimerExpiredDestroy | KASFunc.c | MODIFIED | high | KAS hash keys: the timer name arrives as a hash; high bit set = global scope (timTimerX(0, name & 0x7fffffff)), else timTimerX(currentScopeHash or 0, name); kasfScopeName string building is gone and timTim* take (scopeHash, nameHash) |
| 0x004920e0 | kasfTimerDestroy | KASFunc.c | MODIFIED | high | KAS hash keys: the timer name arrives as a hash; high bit set = global scope (timTimerX(0, name & 0x7fffffff)), else timTimerX(currentScopeHash or 0, name); kasfScopeName string building is gone and timTim* take (scopeHash, nameHash) |
| 0x004946a0 | kasfShipsDockStay | KASFunc.c | MODIFIED | high | special case: when the first ship is tResourceCollector (0x58) and singlePlayerGameInfo.currentMission == 9 the dock type is 7, else 1 (HW1 always DOCK_AT_SPECIFIC_SHIP |
| 0x00496490 | kasfTutSetPointerTargetAIVolume | KASFunc.c | MODIFIED | high | tutSetPointerTargetAIVolume gets a new 3rd argument (0); callers clean 12 bytes (binary-map prototype evidence) |
| 0x00497ad0 | kasfOtherKASPause | KASFunc.c | MODIFIED | high | KAS hash keys: timTimerPauseAllNotScoped gets the current scope hash instead of a "%s." scope-name prefix built with sprintf |
| 0x0049bea0 | kbInitKeyBindings | KeyBindings.c | MODIFIED | high | only the font registration (frFontRegister "hw_eurosecond_11.hff") remains; HW1's kbCanMapKey table, key strings, default bindings and the bindings-file load (about 200 lines) are gone (Cataclysm reloads bindings in new code, cat_kbReloadKeyBindings) |
| 0x004a13c0 | GetAppropriateShipTypeForRace | LevelLoad.c | MODIFIED | high | enum: the replacement ship type comes from a per-ship-type table (DAT_00883cf8[request]) when the race may not build it; HW1 swaps CloakedFighter/DefenseFighter and DDDFrigate/DFGFrigate in a 4-case switch |
| 0x004ae700 | meshSetFade | Mesh.c | MODIFIED | high | fade is per slot: meshSetFade(slot, fade) stores 1 - fade in an 8-entry array and meshFadeAlpha is the product of all slots; bFade = alpha != 1; the usingShader test only gates the store |
| 0x004ae910 | meshTextureRegisterAllPlayers | Mesh.c | MODIFIED | high | trTextureRegister gets the new 4th argument (1) at both calls |
| 0x004b47b0 | meshBindingListRecurse | Mesh.c | MODIFIED | high | new 4th parameter, the parent binding index, stored in each binding (byte at +0x14; mhbinding stride 0x18) and passed down the recursion |
| 0x004b68f0 | madBindingListCallback | MeshAnim.c | MODIFIED | medium | a matched binding also gets its matrix set to identity (16-dword copy), an unmatched one also gets its object pointer; binding stride 0x58 (HW1 mhlocalbinding 0x54) |
| 0x0056e400 | mouseInitGL | mouse.c | MODIFIED | high | 31 cursor textures created instead of HW1's 18 (new Cataclysm cursor types) |
| 0x0056e620 | mouseReset | mouse.c | MODIFIED | high | deletes and clears 31 cursor textures instead of 18 |
| 0x0056e9c0 | mouseGetLIF | mouse.c | MODIFIED | high | cursor-type switch extended for Cataclysm's added cursor types (cases up to 0x1e+, 31 bitmaps) |
| 0x004c08e0 | chatReceiveUsersHere | MultiplayerGame.c | MODIFIED | medium | new 3rd parameter, stored in the queued user record next to userID (queue record 0x54 bytes) |
| 0x004cc2f0 | netcheckFillInChecksum | NetCheck.c | MODIFIED | high | the sync checksum entry is 16 bytes (packetnum/randcheck, univcheck, blobcheck plus a new 4th dword copied into the header); with no checksum yet the new dword is set from a global (DAT_00ab7658) besides randcheck = 0 |
| 0x004d0770 | nisBlackFadeSet | NIS.c | MODIFIED | medium | also stores a byte from the event's second parameter in a new global (0x00a4cb4c) next to nisBlackFade/nisBlackFadeDest |
| 0x004e25a0 | partHeaderSize | Particle.c | MODIFIED | high | two new particle system types: type 5 returns a 0x24-byte header and type 6 the point-system size; HW1 knows PART_POINTS..PART_BILLBOARD (0-4) |
| 0x004e39b0 | partSetIsWorldspace | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e39d0 | partSetVelocityInWorldSpace | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3a00 | partSetWorldVel | Particle.c | MODIFIED | high | stores a copy of the velocity in a second vector instead of keeping the partEffectVelocity pointer |
| 0x004e3a80 | partSetMeshStartFrame | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3aa0 | partSetLoopFlag | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3ab0 | partSetStartFrame | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3c20 | partSetScale | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3c60 | partSetDeltaScale | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3c70 | partSetDeltaScaleDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3c80 | partSetOffsetLOF | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3c90 | partSetOffsetR | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3ca0 | partSetOffsetTheta | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3e20 | partSetDeltaLOF | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3e30 | partSetDeltaLOFDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3e90 | partSetVelLOF | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3ea0 | partSetVelLOFDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3eb0 | partSetVelR | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3ec0 | partSetVelRDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3ed0 | partSetDeltaVelLOF | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3ee0 | partSetDeltaVelLOFDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3ef0 | partSetDeltaVelR | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3f00 | partSetDeltaVelRDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3f10 | partSetAng | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3f20 | partSetAngDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3f30 | partSetAngDelta | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e3f40 | partSetAngDeltaDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e4170 | partSetLighting | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e4180 | partSetIllum | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e4190 | partSetIllumDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e41a0 | partSetDeltaIllum | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e41b0 | partSetDeltaIllumDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e41c0 | partSetLifespan | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e41d0 | partSetLifespanDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e41e0 | partSetWaitspan | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e41f0 | partSetWaitspanDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e4200 | partSetLength | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e4230 | partSetLengthDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e4240 | partSetDeltaLength | Particle.c | MODIFIED | high | ETG argument-array convention (pointer argument), and also zeroes the field after deltaLengthDist (new) |
| 0x004e4260 | partSetDeltaLengthDist | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e42f0 | partSetPseudoBillboard | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e4300 | partSetTrueBillboard | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e4310 | partSetColorScheme | Particle.c | MODIFIED | high | ETG argument-array convention: the script-callable takes a pointer to its argument slot instead of the value |
| 0x004e43f0 | physApplyForceToObj | Physics.c | MODIFIED | medium | new single-player condition: a ship gets no translation force when singlePlayerGame and a pointer field of the ship (Ghidra: obj[9].cameraDistanceVector) refers to an object of type 6 or whose state at +0x40 is neither 0 nor 7 |
| 0x0057afe0 | rndMaterialfv | render.c | MODIFIED | medium | only GL_FRONT is cached and applied; any other face returns FALSE without calling glMaterialfv |
| 0x004f1730 | AsteroidTakesDamage | ResCollect.c | REWRITTEN | medium | asteroid damage model replaced: a value converted with ftol is stored in an asteroid field and the function returns TRUE when targetWasAlive and resourceVolume <= 0; HW1's RU damage ratio, resourcevalue decrement, health test and HandleAsteroidScaling are gone |
| 0x004f2090 | PreFix_ShipXHarvestsResourceY | ResCollect.c | MODIFIED | high | a third harvest callback (Cataclysm-only code at 0x005c9130) is saved as index 2 |
| 0x004f20e0 | Fix_ShipXHarvestsResourceY | ResCollect.c | MODIFIED | high | index 2 restores the third harvest callback (0x005c9130); any other unknown index -> NULL as HW1 |
| 0x004f75f0 | rmSetPrintList | ResearchGUI.c | MODIFIED | high | three cases (1, 3, 4) copy a static tech list up to its -1 terminator with no ShipsNeedTech filter; HW1 has six race/mode cases (SPR1/2, MPR1/2, MPCR1/2) that filter technologies with ShipsNeedTech |
| 0x004fdfc0 | PreLoadGame | SaveGame.c | MODIFIED | medium | LoadVersionInfo and LoadPreGameInfo inlined; single-player saves carry a second number after currentMission (stored at 0x00894194); the per-player race number is stored as (value != 0) through Ghidra's HW1-layout Player field; HW1's MG_HarvestinEnabled rmEnableShip block at the end is gone |
| 0x004ff9b0 | SaveShipSinglePlayerGameInfo | SaveGame.c | MODIFIED | high | no longer clears shipSinglePlayerGameInfo->hyperspaceEffect before saving (the 0x48-byte struct is written as is) |
| 0x004ffd20 | FixClampInfo | SaveGame.c | MODIFIED | high | SpaceObjRegistryGetTarget inlined; a host clampInfo (host == NULL) also gets its new clamped-object list fixed up through the registry |
| 0x00501050 | SaveDerelict | SaveGame.c | MODIFIED | high | SaveSalvageInfo is no longer called for derelicts (HW1 saves derelict->salvageInfo); SaveClampInfo inlined, now also saving the new clamped-object list as a selection chunk |
| 0x00503380 | LoadRetreatAtom | SaveGame.c | MODIFIED | high | RetreatAtom is variable-sized (0x24 + 4n): the retreater and n ship IDs are fixed through the registry; HW1's fleeingfrom conversion through the main command layer list is gone |
| 0x00513310 | MothershipOrCarrierIndexInSelection | ShipSelect.c | MODIFIED | high | enum and class: the test is staticinfo->shipclass == CLASS_Mothership (return) / CLASS_Carrier (remember); HW1 tests the ship types Mothership, P1Mothership, P2Mothership and Carrier |
| 0x00513680 | ShipCanHarvest | ShipSelect.c | MODIFIED | high | enum and class: a ship can harvest when shipclass == CLASS_Resource and a static capacity field (+0x260) is above 0; HW1 tests shiptype == ResourceCollector |
| 0x00514c70 | svClose | ShipView.c | MODIFIED | high | only one region pointer is cleared (the six stat-bar regions svFirepower..svMass are gone); svShipType reset to 0x91 (DefaultShip in Cataclysm's enum) |
| 0x005812a0 | soundshipheading | soundlow.c | MODIFIED | high | early-out compares the new heading with the stored one (HW1 compared the handle) and clamps both cardioid factors to [0.4, 1.0] |
| 0x00521e30 | speechEventClose | SpeechEvent.c | MODIFIED | high | frees 8 buffers instead of HW1's 11 (three of the music/speech buffers or LUTs are gone from the speech module) |
| 0x00524b30 | statShipConstraintsFightingShipsCB | Stats.c | MODIFIED | high | enum: frigate exceptions are bDFGFrigate, kAdvanceSupportFrigate, tAdvanceSupportFrigate, tDFGFrigate (no ResourceController); corvette exceptions kSalCapCorvette/tSalCapCorvette (no RepairCorvette); fighters always count (no DefenseFighter) |
| 0x00524bd0 | statShipConstraintsCarrierFightingShipsCB | Stats.c | MODIFIED | high | enum: same exception lists as statShipConstraintsFightingShipsCB (Cataclysm ship types; no ResourceController/RepairCorvette/DefenseFighter) |
| 0x00524c70 | statsGetMostDangerousShipNonStatConstraints | Stats.c | MODIFIED | high | a NULL constraints callback now accepts every ship, and the kill rating comes from a per-ship-type table for types < 0x3c (1.0 for the rest) instead of statsGetOverallKillRating |
| 0x00524eb0 | statsNumShipsNeededToKillFleet | Stats.c | REWRITTEN | high | returns a per-class table value times 3; HW1's kill-ratio computation against the target fleet (statsGetKillRatingAgainstFleet, overkill factor) is gone |
| 0x00524ed0 | statsGetRelativeFleetStrengths | Stats.c | REWRITTEN | high | returns fleet2->numShips / fleet1->numShips; HW1 sums statsGetKillRatingAgainstFleet over fleet2 |
| 0x00525c40 | scriptStringToBool | StatScript.c | MODIFIED | high | digit test fixed: '1'..'9' characters count as TRUE (HW1 tests the values 1..9, a bug) |
| 0x005860d0 | trTextureUnregister | texreg.c | MODIFIED | medium | pending palette slot reset only when nUsageCount reaches 0 (HW1 reset it on every unregister) |
| 0x00587a50 | trRegistryRefresh | texreg.c | MODIFIED | medium | size-sort qsort + trCramRAMScaleCompute/trCramIntoRAM replaced by one call to trCramRAMComputeAndScale(); trMeshSortList nulled after free |
| 0x0052ff70 | tutSetPointerTargetXY | Tutor.c | MODIFIED | high | pointer coordinates are scaled from 640x480 to the window (x * MAIN_WindowWidth / 640, y * MAIN_WindowHeight / 480); HW1 stores x, y as given |
| 0x00530450 | tutSetPointerTargetShip | Tutor.c | MODIFIED | high | new 3rd parameter stored in the pointer (+0x28 of the 0x68-byte tutpointer, the field tutRemoveAllPointers now tests) |
| 0x00530500 | tutSetPointerTargetShipSelection | Tutor.c | MODIFIED | high | new 3rd parameter stored in the pointer (+0x28), as tutSetPointerTargetShip |
| 0x005308f0 | tutSetPointerTargetAIVolume | Tutor.c | MODIFIED | high | new 3rd parameter stored in the pointer (+0x28); callers clean 12 bytes |
| 0x00530a20 | tutRemoveAllPointers | Tutor.c | MODIFIED | high | a pointer is removed only when its new +0x28 field (set by the 3-argument pointer setters) is 0 |
| 0x00532300 | tutShowPrevButton | Tutor.c | MODIFIED | high | the Prev button is its own region and atom ("TutorialPrev", x = MAIN_WindowWidth - 320) with new process/draw callbacks (cat_tutProcessPrevButton, cat_tutDrawPrevButtonFunction); HW1 sets tutPrevVisible and reuses the Back button (tutShowBackButton) |
| 0x00538960 | uicTextEntryCleanUp | UIControls.c | MODIFIED | high | also sets entry->textBuffer = NULL after freeing it |
| 0x0053ba20 | isStrikeCraft | Universe.c | MODIFIED | high | enum: the strike-craft list is 27 Cataclysm ship types (Somtaaw/Beast/Kushan/Taiidan/Turanic fighters and corvettes); HW1 lists 17 HW1 types |
| 0x0053c940 | universeDefaultTeamColors | Universe.c | MODIFIED | high | the race/player team-colour table is filled by tutorial / multiplayer / single-player mission number (a switch over currentMission); HW1 sets fixed slots for Traders, P1, P2 and P3 |
| 0x0053ea50 | GetShipStaticInfo | Universe.c | MODIFIED | high | returns &ShipStaticInfos[shiptype] from one flat 145-entry array; HW1 indexes the per-race table RaceShipStaticInfos[race] by shiptype - FirstShipTypeOfRace[race] |
| 0x005ace60 | utyForceTopmost | utility.c | MODIFIED | high | SetWindowPos now guarded by ghMainWindow != NULL |

## Reproduce

```
python3 -I tools/delta_map.py classify            # -> ghidra/delta_map.json, review verdicts applied
python3 -I tools/delta_map.py show 0x00404090     # both bodies, as read in the re-read
python3 -I tools/delta_map.py report              # -> docs/m2-r1-function-deltas.md
python3 -I tools/m2_stage_counts.py --md          # the per-stage table
```

Verdicts are `ghidra/delta_map_review.json` entries: `"sample": "reread-R2"` (1,013 re-read), `"sample": "audit-R2"` (20 audit items). The frozen target list and the per-batch dumps are scratch data (`original/scratch-m2/reread/`, gitignored).

## Open questions

1. **Apply the name fixes?** `ghidra/proposals/m2-r2-name-fixes.json` holds 3 renames. Applying them needs a Ghidra run, and Ghidra was not run in this round.
2. **The fq audio library.** ad1bb82 has no C source for it, and GoK's reconstruction differs from the binary in `fqMix`, `fqEqualize` (both IDENTICAL/low, the only low verdicts) and `fqDecOver` 0x00558800 (IDENTICAL/medium). Port the binary's behaviour or keep GoK's?
3. **The IDENTICAL/high and /medium buckets.** These are the 2,191 functions that are IDENTICAL by the tool alone. They rest on 62 reviews, and the next hand-review sample should come from them.
4. **`kasfDockInstant` 0x00494750** is listed with size 1 in `functions.csv`, although its body runs about 0x70 bytes. It is an export anomaly for a later Ghidra pass, and it changes only the byte counts.
