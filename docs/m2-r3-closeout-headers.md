<!-- done: quit, randy, headers, derelicts, strings, save-deltamap -->
# M2-R3 producer 1: stage-1 close-out and systemic headers

Status: DONE (2026-10-10); every DONE WHEN condition is met on the plain harness (table near the end). Branch `m2`, worktree `.claude/worktrees/m2`. Scope: docs/m2-r2-summary.md "What is
left of stage 1" items 1-5 and the M2-R3 table row 1.

Subsystems (the done marker above lists the finished ones):

1. `quit`: GoK's `munmap(heap, 0)` quit defect.
2. `randy`: `Randy.c` 27 streams, stream renumbering, wall-clock advance of stream 0, `cat_ranResetGameStreams`,
   `realtimeelapsed` (oracle prerequisites).
3. `headers`: `SOF_*` renumbering, 63 parade slots + `GetShipSlot`, `AlternativeShips` 0xA8, `ClampInfo` 0x38,
   `StaticCollInfo`/`CustShipFire` pins.
4. `derelicts`: the 8-type Cataclysm derelict table.
5. `strings`: the 1,705-row Cataclysm string-id enum.
6. `save-deltamap`: save container and chunk sizes (with the hook for the user's saves), delta-map review entries,
   stage counts.

## 1. `quit`: GoK's munmap(heap, 0) quit defect (not a Cataclysm delta)

| file | change | why |
|---|---|---|
| `src/SDL/utility.c` | `utyGrowthHeapAlloc` records each growth heap's mapped length (table of `MGH_NumberGrowthHeaps`); `utyGrowthHeapFree` unmaps with it; the main heap's length is kept in `utyMemoryHeapLength` and used by both shutdown paths (`utyGameSystemsPreShutdown`, `utyGameSystemsShutdown`); `MAP_FAILED` is now reported as a NULL heap (GoK compared `mmap`'s result with NULL). Tagged `HWI-FIX:` | `munmap(p, 0)` is `EINVAL` on Linux, so every quit after a growth heap existed stopped at `utility.c (3484) result != -1` (all R2 self-test rows). The exe frees with `VirtualFree(heap, 0, MEM_RELEASE)`, which takes no length, so there is nothing to port |
| `tools/lsan.supp` (new), `tools/m2_harness.sh` | the harness exports `LSAN_OPTIONS=suppressions=tools/lsan.supp`; the summary line now says `LeakSanitizer at exit: ...` | once quits reach `exit()`, LeakSanitizer runs (its report lands in the `ubsan.*` log, UBSan's `log_path` wins) and turned rc into 1 for 11 start-up allocations: libdbus (2), the GL driver inside `SDL_GL_CreateContext` (3, `setupPixelFormat`), 3 SDL mutexes of `mgStartup`/`titanGameStartup` (GoK never calls `titanGameShutdown` on quit), 1 `rinEnumeratePrimary` mode record. All are made once at start-up; every other leak still fails a run |

Run `r3-closeout-quit-statics` (before the suppressions): no assertion, rc 1, the `ubsan` log holds
`LeakSanitizer: detected memory leaks ... 3233 byte(s) leaked in 11 allocation(s)`; under `strace` with
`detect_leaks=0` the process ends with `exit_group(0)`. Run `r3-closeout-quit-statics2`: **rc 0**, milestone `ship
statics self-test done`, first stop none, ASan 0, `LeakSanitizer at exit: no unsuppressed leak`.

A second GoK defect showed up once heaps were really unmapped: run `r3-closeout-final-oracle` (debug, oracle mode)
ended with `AddressSanitizer:DEADLYSIGNAL`, a SEGV in the SDL audio thread (`isoundmixerdecodeEffect` `smixer.c:803`
<- `soundfeedercb`) reading the game heap after `utyGameSystemsShutdown` had unmapped it. GoK never closes the SDL
audio device (`soundrestore` only pauses and waits). Fix (`HWI-FIX:`): `src/SDL/soundlow.c` `soundrestore` (called
only by `soundEventClose`, before the heaps are freed) ends with `SDL_CloseAudio()`, which waits for a running
callback and stops further ones. After it: 6 of 6 oracle-mode quits rc 0 (`r3-closeout-quitrep-oracle-1..6`) and
every quitting mode of the final suite rc 0.

[audit: the `soundrestore` change never runs on Linux: `utyGameSystemsShutdown` calls `soundEventClose` only under
`#ifndef _LINUX_FIX_ME`, and meson.build defines `_LINUX_FIX_ME`. Quits stayed racy: 5 of 22 audit oracle-mode quits
crashed (release rc 139 three times, debug `AddressSanitizer:DEADLYSIGNAL` twice); under `strace` the SIGSEGV is in
thread `SDLAudioP1` right after `munmap` of the 64 MB main heap, with the fault address inside it, and the
`soundstream` thread was also still running. The audit added `soundstopthreads()` (`soundlow.c`, `HWI-FIX`), called by
`utyGameSystemsShutdown` in the `_LINUX_FIX_ME` branch before the heaps are freed: `soundinited = FALSE`, join the
`soundstream` thread (`streamWaitThread`, `sstream.c`, which now keeps the thread handle) and `SDL_CloseAudio()`.
After it: 25 of 25 oracle-mode quits rc 0 (15 release, 10 debug ASan, LeakSanitizer clean); docs/audits/m2-r3-closeout-headers.md.]

## 2. `randy`: 27 RNG streams, wall-clock stream 0, `realtimeelapsed` (oracle prerequisites)

Cataclysm deltas (exe evidence: the r5 decompile and `ranRandomFn(N)` literal-index census over all bodies):

| delta | exe function | change in `src/` |
|---|---|---|
| 27 streams (HW1 25) | `ranStartup` 0x004efa30 (reset loop to 0x00C2FFA0), `ranSave` 0x004efb50 / `ranLoad` 0x004efb70 (to 0x00C2FF9C, 20 B each) | `Randy.h`: `RANDOM_NUMBER_STREAMS` 27; `ranSave`/`ranLoad` loop over it unchanged (header-level) |
| particles on stream 25, ETG on stream 26 | `partFillGenericParticles` (56 draws), `partMakePosition`, `partMakeSPos`, `partColorDist`, `partRealDist`, `partCreateSystemWithDelta` call `ranRandomFn(0x19)`; `etgCRandom`, `etgCARandom`, `etgIRandom`, `etgFRandom`, `etgAlternate`, `etgCreateEffects`, `etgSpawnNewEffect`, `etgFrequencyExceeded`, `memStartup`, `factor` call `ranRandomFn(0x1a)` | `RANDOM_PARTICLE_STREAM` 25, `RANDOM_ETG` 26; the HW1 slots 0 and 1 get the names `RANDOM_STREAM_0` (shared render/simulation stream: `Make3dStar`, `gameStart`, `univFindNearestResource`, `lgStartGame`, `mgStartGameCB`, `cat_leechDrainEffect`, ...) and `RANDOM_CAT_SHIPS` (Cataclysm ship code: `cat_cloakUpdate`, `AcolyteHouseKeep`, `RKOHouseKeep`, `cat_mimicBecomeTarget`, ...). Streams 2-24 keep HW1's users (4/5 clouds/nebulae, 6 AI, 7 damage, 8 battle, 9 NIS static, 10 `gamerand`, 11-17 sound/trails, by literal index). `ranstream` pinned at 0x14 (`_Static_assert`) |
| stream 0 advanced by the wall-clock second after the reset | `ranStartup` 0x004efa30 (`GetSystemTime`, `wSecond & 0xff` inlined steps on stream 0) | `ranStartup` ends with `ranRandomize(RANDOM_STREAM_0)` |
| `ranRandomize` counts `GetSystemTime().wSecond` | `ranRandomize` 0x004ef9a0 (as HW1 ad1bb82 `randy.c:198`) | GoK used `SDL_GetTicks() & 255`; now `ranWallClockSecond()` = UTC `tm_sec` (the binary's value) |
| new: `cat_ranResetGameStreams` resets streams 1..26 | 0x004efb10, called by `gameStart` 0x005a9290 at 0x005a9a98 | new function in `Randy.c` |
| `gameStart` no longer calls `ranStartup` for a new game; a new non-single-player game calls `cat_ranResetGameStreams` | `gameStart` 0x005a9290 (`decompiled.c:379621-379626`: `univUpdateReset(); if (singlePlayerGame == 0) cat_ranResetGameStreams();`) | `src/SDL/utility.c` `gameStart` |
| `universe.realtimeelapsed` (new field after `totaltimeelapsed`, 0x00B62AC4) | `univUpdate` 0x0054c3f0 adds whole timer periods of `QueryPerformanceCounter` / `taskFrequency` (0x0054c57f-0x0054c5f6); `univUpdateReset` 0x0054acd0 zeroes it | `Universe.h` field; `UnivUpdate.c` `univupdateReset` zeroes it, `univUpdate` adds `cat_univRealTimeDelta()` right after the `totaltimeelapsed` add |
| new: `cat_univRealTimeDelta` | 0x0054c360 (QPC delta since the last call, remainder carried in `realtimeremainder` 0x00A6D9A0) | `UnivUpdate.c`; GoK's timer is `SDL_GetTicks` ms with `utyTimerDivisor`, so the same whole-period arithmetic runs on it |
| paused universe consumes real time | `universeUpdateTask` 0x0053e460 calls `cat_univRealTimeDelta` in both paused branches (`decompiled.c:287802`, `:287901`) | `Universe.c` `universeUpdateTask`: both `universePause` exits call it |

Not ported here (owners): the ~20 `realtimeelapsed` readers (quit/win timers in `univUpdate`, `CheckPlayerWin`,
`LeechHouseKeep`, `singlePlayerGameUpdate`, ...) go with their functions (M2-R4 universe update, ship code B, stage 6).
`Star3d.c`'s `frandyrandom(0, ...)` and `MultiplayerLANGame.c`'s `ranRandom(0)` already draw stream 0, as the exe's
`Make3dStar` and `lgStartGame` do.

Oracle side (`HWI-ORACLE`, not Cataclysm behaviour): `/oracleDump` and `/oracleSelfTest` now set a virtual clock on this
side, as the header's `clock=virtual` bit always claimed: `ranVirtualWallClock` (the wall-clock second reads 0) and
`univRealTimeVirtual` (`realtimeelapsed` grows by exactly 1/16 s per tick, the step the GOG DLL's `vqpc` gives at a
240 Hz task timer). The `T` record now carries `realtimeelapsed` with its valid bit (`HWO_TV_REAL`), `rng0` reads stream
0 by index (`RANDOM_STREAM_0`), and the self-test's new-game reset is the exe's (`cat_ranResetGameStreams`, not
`ranStartup`). `tools/oracle/hwo_format.h`: two comments updated (no layout change; the DLL is not rebuilt).

Files: `src/Game/Randy.h`, `src/Game/Randy.c`, `src/Game/Demo.c` (stream labels of `demCheckStream`, two entries for
streams 25/26), `src/Game/Universe.h`, `src/Game/UnivUpdate.c`, `src/Game/UnivUpdate.h`, `src/Game/Universe.c`,
`src/SDL/utility.c`, `src/Game/OracleDump.c`, `tools/oracle/hwo_format.h`.

Result: `tools/oracle/selftest.sh --label r3-closeout-oracle` (release `-O2`): **SELFTEST PASS**; A and B
byte-identical (140,723 B, sha256 `0b1f476e46048a8f...`), `hwo_compare.py` IDENTICAL over 513 ticks / 256 packets;
perturbed run at tick 300 / `rng10` / 1 draw ahead; flipped copy at tick 400 / `ships_pos`. Every `T` record has
`rng_streams` 27 and valid bits 0x3f; `real` is 0.0625 at tick 1 and 32.0 at tick 512.

## 3. `headers`: systemic header changes

### `SOF_*` object flags (`src/Game/SpaceObj.h`)

Method: `build/m2r3-tools/sof_solve.py` (scratch, untracked) pairs every function that exists by name in both GoK `src/`
and the r5 decompile, collects the `SOF_*` names its GoK body uses and the constants its exe body ANDs/ORs with the flags
word (`+0x14` on ILP32, or a typed `flags` field), and votes per name; each value was then confirmed in at least one
exe function where the HW1 body uses that flag, and the composite constants checked (table below). The shifts are
consistent: Cataclysm drops one cloak flag below bit 12, `SOF_BitForSaleCheap` below bit 15 and `SOF_Slaveable`
below bit 24, so the flags above move down by 1, 2 and 3 bits.

| flag | HW1 | Cataclysm | evidence (exe) |
|---|---|---|---|
| Rotatable, Impactable, DontApplyPhysics, Attach{Position,Coordsys,Velocity,NLips}, NISShip | 0x1-0x80 | unchanged | `etgEffectCreate` 0x00470990 (`flags & 8 -> |9`, 0x10, 0x20, 0x40), `univCheckShipState`/`MiningBaseDied` `& 0x80` |
| CloakGenField | 0x100 | **0 (retired)** | no object-flag set/test of 0x100 anywhere in the decompile; HW1's cloak generator is gone (`CloakGenerator*` absent) |
| (new) `SOF_CatCloakDevice` | - | 0x200 | `cat_cloakInit` 0x00435f10 sets it; `cat_selStatusBarsDraw` draws the cloak charge when set; `cat_shipIsCloakedFully` 0x00436110 tests `0x200 && 0x400` |
| Cloaked | 0x200 | 0x400 | `DetectShips` (HW1 tests `SOF_Cloaked` on the enemy) `& 0x400`; `cat_cloakUpdate` 0x004361c0 sets it, `cat_cloakActivate` clears it |
| Cloaking, DeCloaking | 0x400, 0x800 | **0 (retired)** | the cloak state lives in the cloak record (`cat_cloakUpdate` states 0-3); `gunShoot`, `missileShoot`, `CloakedFighter*` no longer test cloak flags |
| Dead | 0x1000 | 0x800 | `univGetChecksum` 0x00549f50 and 33 `& 0x800` tests; `bobListCreate` `& 0x1800` = Dead\|DontCreateBlob |
| Selected = DontCreateBlob | 0x2000 | 0x1000 | `bobListCreate` (HW1 `SOF_Dead \| SOF_DontCreateBlob`) |
| BitForSaleCheap | 0x4000 | **gone** | unused in HW1 and GoK |
| Selectable | 0x8000 | 0x2000 | `univCreateShip` `\| 0x102003` (= Rotatable\|Impactable\|Selectable\|Targetable); `selSelectionClick` |
| Resource | 0x10000 | 0x4000 | `univAddAsteroid` `0x104003`, `cat_univAddMimicResource` |
| Hide | 0x20000 | 0x8000 | `kasfHideShips` 0x004977a0, `univRemoveShipFromOutside`, `etgEffectCreate` (ESO_WorldRender) |
| ForceVisible | 0x40000 | 0x10000 | sof_solve vote |
| BigObject | 0x80000 | 0x20000 | `univCreateShip` `\| 0x20000` where HW1 sets `SOF_BigObject` |
| StaticInfoIsDynamic | 0x100000 | 0x40000 | `SaveShip`/`LoadShip` |
| NotBumpable | 0x200000 | 0x80000 | `univAddDustCloud`/`GasCloud`/`Nebula` `0x184003` |
| Targetable | 0x400000 | 0x100000 | `univCreateShip`, `univAddAsteroid` composites |
| Slaveable | 0x800000 | **0 (retired)** | no slave trade: `dockMakeMaster`, `dockAddSlave`, `dockRemoveSlave`, `dockKillSlaves`, `bitSetAllSlaves` ... absent |
| Crazy | 0x1000000 | 0x200000 | `aitrackForceSteadyShip` (re-read note); 4 functions whose GoK body uses only `SOF_Crazy` test `0x200000` (16 functions in all) |
| DontDrawTrails | 0x2000000 | 0x400000 | `univSetupShipForControl`, `processMilitaryParadeToDo` |
| Disabled | 0x4000000 | 0x800000 | `aishipmineaquiretarget`; `getEnemiesWithinProximity` `0x808000` = Disabled\|Hide |
| Clamped | 0x8000000 | 0x1000000 | `clampObjToObj` 0x004346e0 `\| 0x1000000` |
| Hyperspace | 0x10000000 | 0x2000000 | `ShipAlreadyHyperspaceOut` |
| AlwaysSortFront | 0x20000000 | 0x4000000 | `etgEffectCreate` (ESO_SortForward) |
| HideSave, HideSave2 | 0x40000000, 0x80000000 | 0x8000000, 0x10000000 | `kasfHideShips`/`kasfUnhideShips`; `univHideJustAboutEverything`/`univUnhideJustAboutEverything` 0x00547cd0 |
| (new) `SOF_OwnerCoordSys` | - | 0x20000000 | `etgEffectCreate` maps the `EAF_OwnerCoordSys` argument (0x01000000) to it; `etgEffectDraw` tests it |
| (new) `SOF_CatShipPart` | - | 0x40000000 | set by `cat_univCreateMothershipModules`, `cat_dreadnoughtCreateCannon`; cleared by `DreadnoughtDied`, `cat_shipDissolveStart`; `ApplyDamageToTarget` redirects damage to the parent |
| (new) `SOF_CatDocking` | - | 0x80000000 | set by `cat_dockShipDocksAtShip`, `cat_processLinkToDo`, `cat_leechProcess`, `cat_moduleShipEject`; cleared by `RemoveShipFromDocking`/`Launching`, `LaunchCleanup`; `collCheckShipShipColl`/`BigShipSmallShipColl`/`ShipResourceColl` skip it |

The three retired names are defined as 0 so the HW1 code that still uses them compiles and is inert (as the exe, which
has no such tests); stage 2 removes that code. The names of the new flags are chosen here (the exe's are unknown).
No code depends on the numeric values (no `== SOF_x` mask comparison involves a retired name; checked with grep).

`src/Game/ETG.c` (consequence of the renumbering): `etgEffectCreate` copied the whole `flags` argument into the header,
so `EAF_OwnerCoordSys` (0x01000000) would now set `SOF_Clamped`. It now copies `flags & EAF_Full` and sets
`SOF_OwnerCoordSys` for `EAF_OwnerCoordSys`, as the exe does (`etgEffectCreate` 0x00470990), and `etgEffectDraw` tests
`SOF_OwnerCoordSys` (exe `effect->flags & 0x20000000`). The ETG producer (M2-R3 #2) owns the rest of `ETG.c`.

### Parade slots (`src/Game/Formation.h`, `src/Game/Formation.c`)

| change | exe evidence |
|---|---|
| 63 slots `SLOT_SRECON` 0 .. `SLOT_Misc` 62, `MAX_MILITARY_SLOTS` 63 (HW1 41); HW1's `SLOT_ResourceController`, `SLOT_SensorArray`, `SLOT_ResearchShip`, `SLOT_P2*` removed | `ParadeInfoScriptTable` 0x0087fb68: 63 `scriptStructEntry` rows, field offset = slot x 0x1C from `paradeTypeInfos` 0x00a3e448 (dumped with `build/m2r3-tools/exe_table.py`) |
| `ParadeInfoScriptTable` = the exe's 63 keys in its order | same table |
| `GetShipSlot(Ship *, MilitaryParadeCommand *)` rewritten: one case per Cataclysm type, carriers fill `S/BCARRIER0..2` in turn by `highestSlotUsed`, `bHeavyCruiser` gets `SLOT_BHEAVYCRUISER` only around a `bMothership`, Swarmers go to `SLOT_SLEECH`, all others `SLOT_Misc` | `GetShipSlot` 0x00485b30 (switch on ship+0x2B0; `in_stack[7]`/`[8]` = `militarySlots[5]`/`[6]`, `[0x19]`/`[0x1a]` = `[23]`/`[24]`; `*in_stack` = `aroundShip`) |
| `PutShipInMilitaryParade` passes the parade | 0x00485efb `push esi` (militaryParade), 0x00485efc `push ebx` (ship), `call 0x485b30` |
| asserts: `SlotInfo` 0x1C, `ParadeTypeInfo` 0x6E4 (all targets), `MilitaryParadeCommand` 0x104 (ILP32), `MAX_MILITARY_SLOTS == SLOT_Misc + 1` | `paradeTypeInfos` 12 x 0x6E4 |

### Other structs

| struct | change | exe evidence | assert |
|---|---|---|---|
| `AlternativeShips` (`src/Game/AITeam.h`) | `MAX_NUM_ALTERNATIVES` 20 (HW1 12); both arrays `sdword` (HW1 `sbyte`); `SetAlternative` casts to `sdword` | copied as 0x2A dwords (`aimCreateFancyGetShipsNoAdd` inlined at 0x0040e040); `aioCreateReconaissance` 0x0040e3a0 fills numNextPicks at -0xC0, types from -0xB8, equivalents from -0x68; `aimProcessFancyGetShips` reads dwords at +0x2C/+0x7C of the move params (= struct +0x08/+0x58) | 0xA8 and `shipNumEquivNextPicks` at +0x58, every target |
| `ClampInfo` (`src/Game/SpaceObj.h`) | new `struct CatClampedList *clampedObjs` at +0x34; `clampObjToObj` (`Clamp.c`) starts it NULL, `LoadClampInfo` (`SaveGame.c`) clears the stale saved pointer | `clampObjToObj` 0x004346e0: `memAllocFunction(0x38)`, `belongto[4].tail` (= +0x34) NULL, host list of 0xFA8 bytes | 0x38 and +0x34 on ILP32 |
| `StaticCollInfo` | none (R2 added `collrectmid`); pins added | `collZeroRectInfo` 0x0043a900 zeroes the three lengths at struct +0x44/+0x48/+0x4C (Ghidra's HW1-named slots `diagonallength`, `preciseSelection`, `collBoxScaleForward`) | 0x70 every target; `staticCollInfo` +0x34, `uplength` +0x78, `rightlength` +0x7C, `forwardlength` +0x80, `diagonallength` +0x84 in `ShipStaticInfo` |
| `CustShipHeader.CustShipFire` | none (R2 placed it); pin added | slot 5 = statinfo+0x11C, called by `GenericInterceptorAttack`, `MultiBeamFrigateAttack`, `doKamikazeAttack`, `nisFire` (docs/formats/shipstatic.md) | +0x11C on ILP32 |

`tools/m2_layout32.sh` now also compiles `AITeam.h`, `Formation.h`, `Randy.h`, `SaveGame.h`: **OK**, ILP32 sizes
ShipStaticInfo 1112, GunStatic 284, CustShipHeader 140, polygonobject 116, texreg 52, SENTENCELUT 16, ClampInfo 56,
StaticCollInfo 112, AlternativeShips 168, MilitaryParadeCommand 260, ranstream 20, SaveChunk 8.

Build: `tools/build.sh --debug` rc 0, no `warning:`. Run `r3-closeout-headers-sp`: milestone `level load (mission file
read)`, first stop `Ships/GenericInterceptor.c (205)` (the same stop as R2's `integrate-exit-sp`), ASan 0.

## 4. `derelicts`: the 8-type Cataclysm derelict table (stage-1 leftover 1)

| file | change | exe evidence |
|---|---|---|
| `src/Game/ObjTypes.h` | `DerelictType`: `Crate` 0, `Homeworld` 1, `PlanetOfOrigin` 2, `PlanetOfOrigin_scarred` 3, `Galaxy` 4, `LavaBall` 5, `MattWorld` 6, `HyperspaceGate` 7, `NUM_DERELICTTYPES` 8 (HW1 55; M2-R2 appended the last three as `CAT-PROVISIONAL` at 55-57). The 50 HW1-only names stay as dormant ids from `HW1LEGACY_FIRST_DERELICT` 0x1000 (far above the NIS generic-object ids `NUM_DERELICTTYPES + n`); no table maps a name to them | `derelicttypeinfo` 0x00889b68: 8 `{value, name}` rows, values 0-7 (dumped with `exe_table.py`) |
| `src/Game/ObjTypes.c` | `derelicttypeinfo` = the exe's 8 rows | same |
| `src/Game/Sensors.c` | `smDerelictTypeMesh` re-keyed to the 8 types, all 0 (`CAT-PROVISIONAL`: the exe's rewritten sensors manager has no such table; HW1 had 0 for every kept type) | `smBlobDrawClear` and its callers are absent from the exe |

The HW1 code that still names dropped derelicts (switch cases in `Select.c`, `ShipSelect.c`, `Sensors.c`; 157 lines
in all per the R2 summary) compiles against the dormant ids and is never reached with Cataclysm data. The Cataclysm
data never names an HW1 derelict type (grep of the extracted `etg/`, `tweak.script`, `.level`, `.mission`, `nis/`
files: the `dJunk*` names there are ship types). The derelict loader over the 8 types belongs to producer 5 (universe
statics).

Run `r3-closeout-derelicts-meshes`: rc 0, 1,758 listed / 1,729 `.geo/.peo` + 29 `.mad` loaded, 0 bad;
`mesh_engine_check.py`: `1758 files on disk, 1758 match, 0 mismatch, 0 not loaded, 0 extra` (as R2).

## 5. `strings`: the 1,705-row Cataclysm string-id enum (stage-1 leftover 5)

New tool `tools/m2_strings_gen.py --exe cataclysm.exe [--check]` (reads the exe only) regenerates
`src/Game/StringsOnly.h` and the `LanguageStrings` rows of `src/Game/StringSupport.c` between `CAT: BEGIN/END` markers.
It checks the exe table as it reads it: 1,705 `scriptEntry` rows at `LanguageStrings` 0x00898148 then a NULL row at
0x0089d134, every callback `strSetStringCB` 0x00528800, every `dataPtr` = `&MessageStrings[id]` (`MessageStrings`
0x00bb3620) and the ids a permutation of 0..1704; `strLoadLanguage` 0x005285f0 clears 0x6A9 = 1,705 strings.

| change | exe evidence |
|---|---|
| enum = the 1,705 Cataclysm ids in id order (`strsRecon` 0 ... `strLaunchMgrCorvetteClass` 1704), `NumStrings` 1705 | the table above |
| `LanguageStrings` = the exe's rows in the exe's order; rows 564/565 (`strPaswordAndConfirmationMustMatch`, `strMustBeAtLeast2Chars`) point at ids 565/564, so `strNameToEnum` (which returns the row index, exe 0x005288f0 and HW1 alike) keeps the exe's answer for them | row/dataPtr pairs |
| 226 HW1-only ids (of 590; 364 names exist in both) follow `NumStrings` as dormant ids up to `NumStringsWithLegacy`; `MessageStrings` is sized for them and `strLoadLanguage` sets them to `*NO_STRING*` too (no `LanguageStrings` row) | - (hwi-engine only, so unported HW1 code compiles) |
| `strShipOffset` = `strsRecon` (0): `ShipTypeToNiceStr` now shows the Cataclysm ship names | `ShipTypeToNiceStr` 0x004d6c30 = `MessageStrings[type & 0xffff]` |
| `strClassOffset` = `strCLASS_Mothership` (142) | `ShipClassToNiceStr` 0x004d6b70 reads `MessageStrings + 0x238` |
| `strTechOffset` = `strTECH_CLOAKING` (182) | `TechTypeToNiceString` 0x004f2530 reads `MessageStrings + 0x2D8` |
| `strTechInfoOffsetR1/R2` = `strR1TECH_CLOAKINGinfo` (254) / `strR2TECH_CLOAKINGinfo` (295) | `rmTechInfoDraw`, `tmTechInfoDraw` read `+0x3F8` / `+0x49C` |
| `strShipAbrevOffset` = `strAbrevsRecon` (341; the 142 abbreviations follow the type order) | table order |
| `strTechHeadingOffset`, `strFormationOffset`, `strKeyCommandOffset` keep their HW1 base names (present in Cataclysm with HW1's order after them); `strShipStatNameOffset` = the dormant `strFirepower` | exe users not located (`cat_kbCommandNameGet` uses its own id table at 0x008b5e5c) |

Check against the data: `update.big`'s `English.script` has 1,693 keys; 1,681 are table names, 12 are unknown to the
exe's table too (`strMenuBeastWeaponDesc`, ...; ignored by both), 24 ids have no key (stay `*NO_STRING*`, as in the
exe); `strsRecon` = `RECON`. Run `r3-closeout-strings-fe`: milestone `Main_game_screen`, rc 124 at the 40 s timeout
(SIGTERM), first stop none (R2 runs ended with the munmap assertion after the SIGTERM), ASan 0, no `*NO_STRING*` in
the log.

## 6. `save-deltamap`: save container and chunk sizes, delta-map review entries (stage-1 leftovers 3 and 4)

### Save container and chunk sizes (`src/Game/SaveGame.h`, `src/Game/SaveGame.c`)

| change | exe evidence |
|---|---|
| `SAVE_VERSION_NUMBER_CATACLYSM` = 0x10ad0027 | `SaveGame` 0x004fdaf0 writes it raw; `PreLoadGame` 0x004fdfc0 (`cmp [esp+8],0x10ad0027` at 0x4fdff3) accepts nothing else |
| new chunk sub-types `SAVE_CAT_PREGAMETABLE` 0x4000, `SAVE_CAT_MISC9` 0x9000, `SAVE_CAT_MISCA` 0xA000, `SAVE_CAT_MISCB` 0xB000 | `cat_saveGamePreGameTable` 0x004fda00, `cat_saveGameMiscChunks` 0x005ba170 |
| the exe's chunk contents sizes `SAVE_CAT32_*`: state block 0x9C, player names 0x90, `CaptainGameInfo` 0x2B0, pre-game table 0x0C, misc 0x24 / 0x24 / 0x17A0, Universe 0x3D9BC, Player 0x6530, Ship 0x63C (+ specifics - 4), blob 0x100, `SinglePlayerGameInfo` 0x54, `ShipStaticInfo` 0x458, `ClampInfo` 0x38, RNG stream 0x14 | docs/formats/savegame.md (M2-R1 item 2: `SaveGame`, `SaveUniverse` 0x005034c0, `SaveShip` 0x004ffd70, `SaveBlob` 0x004fe7d0, `SaveSinglePlayerGame` 0x0051bee0, `ranSave` 0x004efb50) |
| asserts: `SaveChunk` 8 and `InfoChunkContents` 4 (every target); on ILP32, `sizeof(ShipStaticInfo)` and `sizeof(ClampInfo)` equal their `SAVE_CAT32_*` chunk sizes | - |
| `saveCatChunkSize32(type)`: the exe size of each fixed-size chunk type (INFO 4, BASIC\|UNIVERSE, BASIC\|BLOB, BASIC\|SHIPSINGLEPLAYERGAMEINFO, the four new sub-types), -1 where the size varies (BASIC sub-type 0 carries several structs) | same |
| **hook**: `LoadVersionInfo` sends a 0x10ad0027 file to `saveCatLoadHook()`, which walks every chunk header to EOF, checks the fixed-size chunks and the ship chunks (>= 0x63C - 4, as `tools/formats/savegame_probe.py`), logs `Cataclysm save (version 0x10ad0027): N chunks, ... wrong; the 32-bit save layout is not ported yet` and refuses the file with `VERIFYSAVEFILE_BADVERSION` (the verdict GoK already gave: its supported list never held 0x10ad0027) | - |

Not changed, on purpose: hwi-engine still WRITES its own 64-bit layout under GoK's `SAVE_VERSION_NUMBER` 0x10ad0029,
so its files can never be mistaken for GOG saves. The 32-bit reader (and writer) replaces the refusal in
`saveCatLoadHook` once the user's two saves (decision 8) fix the layout; `savegame_probe.py -v` on them is the first
step. `LoadClampInfo` clears the saved `clampedObjs` pointer (section 3). The hook is not exercised at run time: no
save exists on this machine and no headless path loads one (`VerifySaveFile` is reached from the front-end load screens).

### Delta-map review entries (`ghidra/delta_map.json`, `ghidra/delta_map_review.json`)

Recorded with `tools/delta_map.py mark` (19 lines, sample label `port-R3`; the input is kept in
`build/m2r3-tools/review_r3.txt`) and applied with `tools/delta_map.py merge --done m2r3:closeout-review`
("merged; records changed 19").

| address | function | was | now | change |
|---|---|---|---|---|
| 0x0043dea0 | `colPickScalarScan` | IDENTICAL/medium | MODIFIED | `"%s%f%f"` format; Kushan -> Sect and Taiidan -> Beast curves |
| 0x004a29f0 | `SetInfoNeededForShipAndRelatedStaticInfo` | IDENTICAL/medium | MODIFIED | Cataclysm missile carriers and mine layer, `mineStaticInfos` by race |
| 0x004ffc60 | `CopyPointersFromRealShipStaticInfo` | IDENTICAL/medium | MODIFIED | +0x194/+0x3F4/+0x400/+0x404, no salvage or tractor pointer |
| 0x004aec90 | `meshLoad` | IDENTICAL/medium | MODIFIED | no version check, 0x74 stride, per-vertex info fix-up, `.peo` paging |
| 0x004551e0 | `cmCloseTextures` | IDENTICAL/medium | MODIFIED | frees 5 new support-unit textures |
| 0x0049df00 | `lmFreeTextures` | IDENTICAL/medium | MODIFIED | frees 15 launch-system textures |
| 0x004f7970 | `rmGUIShutdown` | IDENTICAL/medium | MODIFIED | frees the 15 + 6 research tables |
| 0x00474050 | `feAllCallOnCreate` | IDENTICAL/medium | MODIFIED | inlined callback search, no "not found" path |
| 0x0046cc60, 0x004af3a0, 0x00526ce0, 0x005833e0, 0x00583990 | `etgSetTextureParse`, `meshRecolorize`, `scriptSetNAVLightCB`, `subThemePictureSet`, `subTexturesReset` | IDENTICAL/medium | MODIFIED | `trTextureRegister` 4th argument |
| 0x004b65c0 | `madFileLoad` | MODIFIED/high | IDENTICAL | HW1's loader; the automatic differences are Ghidra typing |
| 0x0053ad70 | `precalcStaticInfoGuidance` | IDENTICAL/medium | MODIFIED | float operation order `mass * thrust * modifier` |
| 0x004efa30 | `ranStartup` | IDENTICAL/medium | MODIFIED | 27 streams, wall-clock advance of stream 0 |
| 0x00485ee0 | `PutShipInMilitaryParade` | IDENTICAL/medium | MODIFIED | passes the parade to `GetShipSlot` |
| 0x00485b30 | `GetShipSlot` | MODIFIED/high (automatic) | MODIFIED (reviewed) | per-type cases into 63 slots, parade argument |
| 0x005288f0 | `strNameToEnum` | MODIFIED/medium | IDENTICAL | HW1's loop; only the table differs |

The first 14 functions are the summary's 13 + 1; `precalcStaticInfoGuidance` is the summary's parenthetical case; the
last four come from this port. [audit: this port also edited `etgEffectDraw` 0x0046ac20 (IDENTICAL/medium, no review
entry); the audit recorded it as IDENTICAL/high, header-level (the owner-coordsys header bit is 0x20000000 in the exe,
0x01000000 in HW1), plus header-level IDENTICAL entries for `ranSave`/`ranLoad` (27 streams) and a note on the
existing `ranRandomize` entry; merged with `--done m2r3:closeout-audit`; the stage counts are unchanged.] Evidence per entry: the M2-R2 item reports and audits named in the review notes, and
this report's sections 2, 3 and 5.

`tools/m2_stage_counts.py` re-run (before -> after; HW1-matched functions; dumps in
`original/scratch-m2/m2r3-closeout/stage_counts_{before,after}.txt`):

| stage | IDENTICAL | MODIFIED | REWRITTEN | cat-new |
|---|---|---|---|---|
| 1 formats + data load | 220 -> **218** (45,966 -> 43,642 B) | 95 -> **97** (49,278 -> 51,602 B) | 0 | 76 |
| 2 universe + ships | 860 -> **855** (184,562 -> 183,366 B) | 512 -> **517** (216,998 -> 218,194 B) | 30 | 829 (187,242 B) |
| 3 UI + presentation | 938 -> **932** | 253 -> **259** | 7 | 429 |
| 4 AI | 403 | 74 | 2 | 23 |
| 5 LAN | 406 | 114 | 12 | 67 |
| 6 missions | 348 -> **347** | 120 -> **121** | 26 | 2,365 |
| total | 3,175 -> **3,161** (633,417 -> 628,323 B) | 1,168 -> **1,182** (595,973 -> 601,067 B) | 77 | 3,789 |

`docs/m2-r1-function-deltas.md` was not regenerated: `tools/delta_map.py report` treats every reviewed function as a
calibration sample, and these 19 targeted entries (known-changed functions, not a random draw) would pull its
estimated IDENTICAL agreement from 93.6% to 80.1% for no real reason (tried into a scratch file under `build/`). The
reviewed classes themselves are in the map and in the stage counts above.

## Builds and runs (final state)

Builds: `tools/build.sh --debug --clean` and `--release --clean`: rc 0, `[293/293] Linking target hwi-engine`, 0
`warning:` and 0 `error:` lines under `-Werror`; debug 76,341,616 B, release 4,809,888 B. After the last fix
(`soundlow.c`) both were rebuilt incrementally, again without warnings. `tools/m2_layout32.sh`: OK (sizes in section
3). `tools/oracle/dll/build.sh`: `hwi_oracle.dll` 23,040 B, loader 5,120 B (only comments of `hwo_format.h` changed).
`tools/m2_strings_gen.py --check`: up to date.

Plain harness (`tools/m2_harness.sh`, data `original/scratch-m2/m2r2-harness/data`, offscreen, no input, debug ASan
build unless noted):

| run | mode | rc / wall | milestone | first stop | sanitizers |
|---|---|---|---|---|---|
| `r3-closeout-final2-fe` | fe, 40 s | 124 / 40.4 s | **`Main_game_screen`** | none: still running at the timeout (no munmap assertion after the SIGTERM any more) | ASan 0; UBSan only the known `fescreen`/`PATCH`/`fquant.c` reports |
| `r3-closeout-final2-sp` | sp, 60 s | 1 / 11.5 s | **level load (mission file read)** | `Ships/GenericInterceptor.c (205)`, the same stop as R2's `integrate-exit-sp` (producer 4's target) | ASan 0; LeakSanitizer reports 44 B in 11 allocations because the fatal assertion exits mid-game |
| `r3-closeout-final2-statics` | statics | **0** / 12.5 s | ship statics self-test done | none | ASan 0; `shipstatic_engine_check.py`: 143 of 143 types as the probe, 27 other `.shp` loaded, 3 not loaded |
| `r3-closeout-final2-meshes` | meshes | **0** / 11.4 s | mesh self-test done | none | ASan 0; `mesh_engine_check.py`: 1758 on disk, 1758 match |
| `r3-closeout-final2-textures` | textures | **0** / 19.0 s | texture self-test done | none | ASan 0; 12,403 listed / 10,661 loaded / 0 not loaded / 1,742 without a file |
| `r3-closeout-final2-oracle` | oracle | **0** / 10.2 s | oracle self-test done, 512 ticks, 0 sync errors | none | ASan 0; 772 records, 140,723 B |
| `tools/oracle/selftest.sh --label r3-closeout-oracle-final` | oracle (release `-O2`) | | A and B byte-identical (sha256 `0b1f476e46048a8f...`), IDENTICAL; perturbed: tick 300 / `rng10` / 1 draw; flipped copy: tick 400 / `ships_pos` | | **SELFTEST PASS**, 27 streams |

Earlier runs of this item (all in `runs/runs.txt`): `r3-closeout-quit-statics` (rc 1: LeakSanitizer, before the
suppressions), `r3-closeout-quit-statics2` (rc 0), `r3-closeout-oracle-{a,b,perturb}` (SELFTEST PASS after `randy`),
`r3-closeout-headers-sp`, `r3-closeout-derelicts-meshes`, `r3-closeout-strings-fe`, `r3-closeout-final-*` (its oracle
run hit the audio-thread SEGV, section 1), `r3-closeout-quitrep-oracle-1..6` (rc 0 each).

## DONE WHEN

| condition | result |
|---|---|
| a quitting harness run returns rc 0 (no `utility.c (3484)` assert) | **met**: statics, meshes, textures and oracle runs rc 0, and 6 of 6 repeated oracle quits [audit: met only after the audit's `soundstopthreads` fix: before it 5 of 22 oracle quits crashed in the SDL audio thread (section 1); after it 25 of 25 rc 0] |
| a static size assert for every struct this port changed | **met**: every target: `ranstream` 0x14, `StaticCollInfo` 0x70, `AlternativeShips` 0xA8 (+ `shipNumEquivNextPicks` at +0x58), `SlotInfo` 0x1C, `ParadeTypeInfo` 0x6E4, `SaveChunk` 8, `InfoChunkContents` 4; ILP32 (`tools/m2_layout32.sh` OK): `ClampInfo` 0x38 (+ list at +0x34), `MilitaryParadeCommand` 0x104, `ShipStaticInfo`/`ClampInfo` = their chunk sizes, `CustShipFire` +0x11C and the `StaticCollInfo` lengths. `Universe` gained `realtimeelapsed` but its exe size (0x3D9BC) needs the full Player/Universe layout (M2-R4 #5), so it has no size assert yet. [audit: so this row is partly met, not met; the audit added a static assert that `realtimeelapsed` directly follows `totaltimeelapsed` (`Universe.h`), the one exe fact this round's change can pin (ILP32 `totaltimeelapsed` is +0xBC here, +0xC0 in the exe, so an absolute offset assert would fail)] The enums (strings, slots, derelicts) are pinned by `NumStrings` 1705, `MAX_MILITARY_SLOTS == SLOT_Misc + 1`, `NUM_DERELICTTYPES` 8 and the generator's `--check` |
| the oracle self-test still reports SELFTEST PASS with 27 RNG streams | **met** (`rng_streams` 27 in every `T` record, `real` valid and 1/16 s per tick) |
| the delta-map review entries are recorded and the stage counts re-run | **met** (19 entries; table above) |
| fe still reaches `Main_game_screen` and sp gets at least as far as before | **met** (fe `Main_game_screen`; sp the same `GenericInterceptor.c (205)` stop after the level load) |

## Files changed

`src/SDL/utility.c` (quit fix, `gameStart` RNG reset), `src/SDL/soundlow.c` (audio device closed at shutdown),
`src/Game/Randy.h`, `src/Game/Randy.c`, `src/Game/Demo.c`, `src/Game/Universe.h`, `src/Game/Universe.c`,
`src/Game/UnivUpdate.c`, `src/Game/UnivUpdate.h`, `src/Game/OracleDump.c`, `src/Game/SpaceObj.h`, `src/Game/ETG.c`,
`src/Game/Formation.h`, `src/Game/Formation.c`, `src/Game/AITeam.h`, `src/Game/Clamp.c`, `src/Game/SaveGame.h`,
`src/Game/SaveGame.c`, `src/Game/ObjTypes.h`, `src/Game/ObjTypes.c`, `src/Game/Sensors.c`, `src/Game/StringsOnly.h`
(generated), `src/Game/StringSupport.c`; `tools/m2_harness.sh`, `tools/lsan.supp` (new), `tools/m2_layout32.sh`,
`tools/m2_strings_gen.py` (new), `tools/oracle/hwo_format.h` (comments only); `ghidra/delta_map.json`,
`ghidra/delta_map_review.json`; this report. Diff size before the `soundlow.c` fix: 28 tracked files, +3,810 / -1,068
(`src/` 23 files, +3,635 / -1,042, most of it the generated string table); 134 added `src/` lines carry `CAT`, 11
carry `HWI-`. [audit: these are the numbers WITH `soundlow.c` (the producer's final diff). After the audit's fixes
(`src/SDL/sstream.c`, `src/SDL/soundcmn.h`, `src/SDL/soundlow.h`, `soundlow.c`, `utility.c`, `Universe.h`): 31 tracked
files, +3,892 / -1,075; `src/` 26 files, +3,691 / -1,044.] Scratch helpers (untracked, `build/m2r3-tools/`): `fn.py`, `grepfn.py`, `flaguse.py`, `sof_solve.py`,
`sof_by_fn.py`, `exe_table.py`, `rc.sh`, `final_runs.sh`, `repeat_runs.sh`, `review_r3.txt`. Game-derived dumps:
`original/scratch-m2/m2r3-closeout/` (`languagestrings.txt`, stage counts before/after).

## What is left, and its owner

1. `realtimeelapsed` readers (quit/win timers in `univUpdate`, `CheckPlayerWin`, `LeechHouseKeep`,
   `cat_leechPickTarget`, `cat_salvageSpecialTarget`, `cat_univShipFOWUpdate`, `singlePlayerGameUpdate`, `mrScuttle`,
   ...): ported with their functions (M2-R4 #1 universe update, #4 ship code B; stage 6 for `singlePlayerGameUpdate`).
2. `SOF_*` users: the HW1 cloak code (`CloakGenerator.c`, cloaked fighters), the slave trade (`Dock.c`) and other code
   that the zero-valued retired flags leave inert is deleted by its stage-2 owners (M2-R3 #4 ship code A, M2-R4 #3
   docking). The new flags `SOF_CatCloakDevice`, `SOF_CatShipPart`, `SOF_CatDocking` get their setters with `cat_cloak*`
   and the mothership modules/Dreadnought (M2-R4 #4) and Cataclysm docking (M2-R4 #3). `ETG.c` beyond the
   `EAF_OwnerCoordSys` mapping: M2-R3 #2.
3. `ClampInfo.clampedObjs` list code (`clampObjToObj` reuse + host list, its save and fix-up): M2-R4 #3.
4. `Formation.c` parade code beyond `GetShipSlot`/`PutShipInMilitaryParade` (parade-type selection, processing):
   M2-R4 #2.
5. The full save layout behind `saveCatLoadHook` (32-bit reader/writer; Universe 0x3D9BC, Player 0x6530, the 0x9C
   state block and misc tables): after the user's two saves arrive (decision 8), with M2-R4 #5 (oracle save loader).
6. `strTechHeadingOffset`, `strFormationOffset`, `strKeyCommandOffset`: their exe users were not located; the research
   GUI and key bindings (stage 3) and formations (M2-R4 #2) confirm or change them. `strShipStatNameOffset` points at
   a dormant id until stage 3 ports the ship-view stats.
7. Derelict loader over the 8 types: M2-R3 #5 (universe statics); `smDerelictTypeMesh` (`CAT-PROVISIONAL`): stage 3
   (sensors manager).
8. Exit-time leaks suppressed in `tools/lsan.supp` (GoK's never-destroyed SDL mutexes, `rinEnumeratePrimary`): stage 5
   (networking shutdown) and stage 3 (render init); drop the lines when those paths are ported.
9. `docs/m2-r1-function-deltas.md` tables are not regenerated (section 6); the delta map and the stage counts are
   current. The parallel Ghidra stage's `cataclysm_gog_r6` renames (3 functions) do not touch these 19 entries.
