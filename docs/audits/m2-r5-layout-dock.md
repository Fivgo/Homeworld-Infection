# Audit: M2-R5 producer 1 "layout-dock" (layout pass, dock purposes, experience, flight/math compares)

Auditor: adversarial audit of `docs/m2-r5-layout-dock.md` (marker `<!-- done: layout, dock, experience, compares, final -->`),
branch `m2`, base `1434b90`; the port is the WIP snapshot `f59e2ef` plus this audit's working-tree fixes. Commits
`923feea`, `84e5b7d`, `3e73cc4`, `aa28e99` (workflow script, STATE.md, the relaunch) are the orchestrator's and the
user's, not port work. Every engine run went through `tools/m2_harness.sh` / `tools/oracle/mission_test.sh` (silent,
headless, the plain harness). Ghidra was not run; the exe was read only as data (`objdump -d` of `.text`, a small PE
reader for `.rdata` / `.data`), never executed.

**An interrupted first audit.** The WIP snapshot already carries five `[audit: M2-R5 layout-dock]` edits from an audit
session that was cut off before it wrote this file (the `cat_dockGetDockClass` purpose-10 table, the flags of
`cat_dockFlyInFrontOfHost`, the deposit-before-dock-time order of dock state 5, the `SPECIAL_EXITING_DOOR` comment and
the report's `Dock.c` line count). They are port content for this audit: each was re-checked below (4 right, 1 wrong).

## 1. Builds (from clean)

`rm -rf original/scratch-m2/build-audit-r5-layout-dock`, then `meson setup` with `tools/build.sh`'s arguments
(debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`)
and `meson compile`: **0 warnings, 0 errors under `-Werror`**, both. Sizes (`size`): debug text 43,057,593 / data
24,944,328 / bss 2,841,984 (file 79,694,424 B); release text 3,880,965 / data 271,352 / bss 2,268,576 (file 4,901,800 B).
After this audit's fixes both audit builds and `tools/build.sh --debug` / `--release` (the worktree's `build/`)
rebuild with 0 warnings (the only "WARNING" line is meson's sanitizer notice).

## 2. Layout (exe, CET, `tools/m2_layout32.sh`)

`tools/m2_layout32.sh`: **OK** (Ship 1596 = 0x63C, Missile 600, Bullet 356, Derelict 660, Asteroid 436; 45 ship
specifics). Offsets checked against the exe's own stores (Ghidra's typed `SpaceObj` is 0x80 bytes, so `obj[k].field`
= 0x80 k + the field's offset; its `PosInfo` has `position` first) and against CET's `spaceobj.h`, the tree's ILP32
offsets probed with `clang -m32` (24 checks, all as the tree has them):

| field (tree) | exe evidence | result |
|---|---|---|
| `health` 0x170 | `univCreateShip` 0x00542320: `obj[2].posinfo.force.x = maxhealth` (0x100 + 0x58 + 0x18) | ok (CET calls 0x170 `buildProgress`) |
| attributes dword +0x38 | `kasfShipsAttributesSet` 0x004933d0 stores all four bytes at +0x38; `cat_blastAffectShip` `*(uint*)(target+0x38)` 0x20000 / 0x10000 | ok |
| `attributesParam` sword +0x3C | `univAddAsteroid` 0x00540a30 (Asteroid typing shifted: `cameraDistanceVector.y` = +0x3C, a 16-bit store) | ok |
| `Ship` 0x63C | `univCreateShip`: `local_18 = 0x63c`, the zeroing loop over `local_18 >> 2` | ok |
| `catNavLightStatic` 0x634, `catKilledByPlayer` 0x60C (-1), `catShipReserved5B0` (1) | `obj[0xc].currentLOD = navlight static`, `obj[0xc].objlink.structptr = -1`, `obj[0xb].renderlink.structptr = 1` | ok |
| `damageModifier` 0x3DC, `catDamageFactor` 0x3E0 | `obj[7].posinfo.position.y/z = 1.0`; `gunShoot` 0x0048b5b0 scales by `ship+0x3e0` | ok |
| tractor block 0x3F8..0x414, `needNewLeader` 0x41C | stores at +0x3F8, +0x400, +0x408..+0x414, +0x404, +0x41C after `univInitSpaceObjPosRot` (HW1's order) | ok: `tractorPosinfo` is at **0x408**; CET's `ASSERT_OFFSET(Ship, tractorPosinfo, 0x400)` is wrong |
| `shipDeCloakTime` 0x428, `shipID` 0x43C | `obj[8].renderlink.prev = -20.0f`, `*(uword*)&obj[8].unknown2 = universe.shipNumber` | ok |
| `gunInfo` 0x490 | `obj[9].objtype = memAlloc(guns * 0x88 + 4)` | ok |
| `dockvars` 0x494 / 0x49C / 0x4A0 / 0x4A4 | `InitializeDockVars` 0x00463c40, `cat_dockShipDocksAtShip` 0x00465860 | ok |
| `dockInfo` 0x4A8 | `cat_dockInitShipDockInfo` 0x00463bb0 | ok |
| `catRepairDroids` 0x4C0 | `obj[9].cameraDistanceSquared = droidCreateArray(...)` | ok |
| `catKillsByClass` 0x5C8, `catExperienceLevel` 0x5F8, `catExperienceMessageTime` 0x5FC | `cat_shipUpdateExperienceLevel` 0x005147d0, `cat_shipGetDamageMultiplier` 0x005148f0 | ok |
| `catAttackStarted` 0x616 | `gunShoot`: `ship[0x616] = 1` | ok |
| `catDockLatchHeading` 0x5A4, `catDockStateTime` 0x620 | dock states 2 and 5 | ok |
| `ShipSpecifics` 0x638 | the retire refund's crystal value `+0x638`; the slipgate partner `*(host+0x638)` | ok |
| `navLightInfo` 0x234 | `InitializeNavLights` 0x00542040 | ok |
| `specialFlags` 0x1C8 / `specialFlags2` 0x1CC | `InitializeDockVars` (0x1000), `RemoveShipFromDocking` (~0x40) | ok |
| `catDisableReason[1]` 0x1E0, `catDisabled` 0x210 | `ThisShipCanDockWith` 0x00464290 | ok |
| `rotinfo` 0x98 | `gunShoot`, `cat_dockFlyInFrontOfHost` pass `ship+0x98` as the matrix | ok |
| `catSalvageDepot` 0x3EC, `resources` 0x330, `howDidIDie` 0x39C | `ThisShipCanDockWith`, `cat_dockFinishedDocking` 0x004667f0 (`0x3e9` = retirement) | ok |
| `catFlags18` +0x18 | `InitializeDockVars` / `RemoveShipFromLaunching` bit 0 | ok |
| `DockInfo` 0x24 + 4n (byte counts at +0, busy words at +0x24), `DockStaticInfo` (+0x23 counts, +0x48 n, +0x4C centre, +0x50 points of 0x4C) | `cat_dockInitShipDockInfo`, `dockReserveDockPoint` 0x00464610 | ok |
| Bullet 0x164, Asteroid 0x1B4, Nebula 0x1A8, Derelict 0x294, Missile 600 | `LoadSpaceObj` 0x00501660 and the creators | ok |

CET: `spaceobj.h` has **66** active `ASSERT_OFFSET(Ship, ...)` lines (3 more commented out), not "40". The tree's
`Ship` matches all of them but three, each settled by the exe for the tree: `tractorPosinfo` (above), `beastSystemType`
/ `beastUnk` at 0x650 / 0x654 (past the exe's 0x63C allocation), and the 0x170 name.

## 3. Bodies compared with `cataclysm_gog_r6/decompiled.c` (and the disassembly where noted)

29 ported bodies + 6 IDENTICAL compares:

| # | function | result |
|---|---|---|
| 1 | `InitializeDockVars` 0x00463c40 | as the exe (the host NULL test before the hive reset is a marked CAT-FIX) |
| 2 | `cat_dockInitShipDockInfo` 0x00463bb0 | as the exe |
| 3 | `dockReserveDockPoint` 0x00464610 | as the exe (signed-byte index, busy word, per-type byte count) |
| 4 | `RemoveShipFromDocking` 0x00465360 | as the exe; the `madBindings != NULL` test was an unmarked deviation (F4) |
| 5 | `RemoveShipFromLaunching` 0x00465590 | as the exe |
| 6 | `DockCleanup` 0x00467970 | as the exe |
| 7 | `LaunchCleanup` 0x004679b0 | as the exe (it inlines `RemoveShipFromLaunching`) |
| 8 | `ShipIsWaitingForSoftLaunch` 0x00467a60 | as the exe |
| 9 | `ShipIsRefuelingAtCarrierMother` 0x00467a90 | as the exe (types 0xC / 0x2F / 0x4C / 0x3E, states 8 / 9) |
| 10 | `ThisShipCanDockWith` 0x00464290 | every test in the exe's order and with the exe's bits (0x6D guests 2/5/0x3A, 0x82 vs player 0, 0x800, 0x400, missions 6/7/15 and 0x100000, 0x40000, 0x20000000, crystals, 0x8, latch centre +0x4C, counts) |
| 11 | `cat_dockCanDockWithBusyHost` 0x00463a70 | equivalent: with index -1 the exe reads `DockStaticInfo+4` as a type, which cannot be 4 or 5 (it would need `firstOfType[5..7]` all 0) |
| 12 | `clDock` 0x00466c70 | as the exe (slot filter, in-place change unless order 5, `wasHarvesting` for 0/3/10, inlined dock vars, `allDockingWithSameShip`) |
| 13 | `dockChangeSingleShipToDock` 0x004644c0 | as the exe |
| 14 | `FindNearestShipToDockAt` 0x00465750 | as the exe |
| 15 | `processDockToDo` 0x00466950 | as the exe |
| 16 | `cat_dockFinishedDocking` 0x004667f0 | as the exe; the refund's float is **confirmed**: 0x0046685e `fildl 0x238(staticinfo)`, `fmuls 0x00ba1fa8`, `__ftol`, and 0x00ba1fa8 is the variable of the tweak-table entry 0x008ad324 "TW_RETIRE_MONEYBACK_RATIO" |
| 17 | `cat_dockShipDocksAtShip` 0x00465860 | all 17 states read: as the exe (incl. the first audit's state-5 order fix: the hold is emptied before the "resources > 0" bonus test) |
| 18 | `cat_dockGetDockClass` 0x00463d70 | the whole table matches **except** the launch purpose's class default (F1); the first audit's purpose-10 fix (classes 5/6/8/10/0xB -> 0 = `DPT_Small_In`) is right |
| 19 | `cat_dockFlyInFrontOfHost` 0x00465630 | as the exe; the first audit's flags fix (6 = PointInDirectionFlying \| FastAsPossible, -80, host velocity at +0x64) is right |
| 20 | `dockPutShipOutside` 0x00463350 | as the exe (right = heading x up; the support-module rotations by `index - firstOfType[type]`: bit 0 pi about X for 0x0D / 0x1A; for 0x39 bit 1 pi about Y then -/+7 deg about Z, bit 0 pi about X); the index -1 test was an unmarked deviation (F4) |
| 21 | `clLaunchShip` 0x0044a220 | the exe passes `(ship, receiverShip)` to the `CustShipLaunchCommand` slot; the tree's slot took one argument (F2) |
| 22 | `processLaunchShipToDo` 0x00466b00 | as the exe (ore-can armour 0x20 ejected for module types 0x13/0x15/0x16/0x17, 7.0 / 10.0) |
| 23 | `cat_clHarvestNearestResource` 0x00449790 | as the exe (10 s message interval 0x00753304 = 10.0; the sound/speech/FOW argument are named CAT-TODOs) |
| 24 | `cat_shipUpdateExperienceLevel` 0x005147d0 | as the exe (it posts for every threshold passed, as the tree) |
| 25 | `cat_shipGetDamageMultiplier` 0x005148f0 | as the exe |
| 26 | `ApplyDamageToTarget` 0x00546250, the kill credit | as the exe (swarmers credit the hive at +0x644, then the level) |
| 27 | `cat_collSentinelFieldExempt` 0x0043acc0 | equivalent (the exe's out-parameter is only written on TRUE or with no Sentinel; the caller zeroes it first) |
| 28 | `cat_shipHasRamUpgrade` 0x005bf840 | as the exe (bit 0x100) |
| 29 | `aimProcessDock` 0x00409350 | the dock-type part as the exe (stay flag 2 -> 1, instant flag 4 -> +8, host with flag 1) |
| 30-35 | IDENTICAL: `volFindCenter`, `volFindRadius` (1/3 = 0x0072a520, 0.5 = 0x0072a4c8), `teReset`, `flightmanEndoverInit`, `flightmanRoll180Init`, `flightmanSandwichInit` | as the tree after the port (`flightmanEndoverInit`'s 0xC block written to +0x10 is a real overrun: the CAT-FIX is justified) |

Exe constants and tables read from the PE: 4.0 (0x0072a530), 6.0 (0x0072a7f0), 1.5 (0x0072a7ec), 100.0 (0x0072a464),
2.0 (0x00879298), 0.0 (0x0072a460), pi double (0x0072a7d8), +-0.12217305 (0x0072a7cc / d0), 7.0 / 10.0
(0x0087929c / a0); the kill weights 0x0072bbb4 (20, 8, 10, 9, 5, 3, 1 x 6), the 12 x 4 thresholds 0x0072bbe4, the
string ids 0x0072bba4 (1525..1528), the 2.0 s message time 0x0072a6c0, the upgrade times 0x0072b1f8 (10/20/30) and
0x0072b204 (10, 10, 0 x 7): all as the tree.

## 4. Greps, diff and delta map

- No HW1 dock bitmask in compiled code: `DOCK_FOR_*` / `DOCK_AT_SPECIFIC_SHIP` / `DOCK_STAY_TILL_EXPLICITLAUNCH` /
  `DOCK_INSTANTANEOUSLY` appear only in `src/Win32/mainrgn.c` / `mouse.c` (GoK's legacy copies; 0 of 1,574 compile
  commands use `src/Win32/`) and in one comment. No `CAT-PROVISIONAL` dock stand-in; `grep "M2-R4 #5"` empty; no
  `CAT-TODO` / `CAT-PROVISIONAL` naming R5-1 or this producer.
- The full diff (`git diff 1434b90`, 70 files): every change outside `Dock.c` / `LaunchMgr.c` / `SpaceObj.h` /
  `ShipSelect.c` / the R5-1 leftovers / the compares is a consequence of the layout (the `catFlags18`,
  `shipOrDerelict.` and `attributes` renames), the slave-trade removal or the purpose conversion: no out-of-scope
  change found. A script mapped every hunk to its function and the delta map: 7 edited IDENTICAL functions had no
  review entry (F5), and 6 purpose-constant conversions had no `CAT:` comment (F6).
- `ghidra/delta_map.json` vs `ghidra/delta_map_review.json`: the producer's review entries were written only into the
  map (F7).

## 5. DONE WHEN, on the plain harness

| claim | run | result |
|---|---|---|
| `m2_layout32.sh` holds Ship 0x63C (health 0x170) and the +0x38 dword | section 2 | met |
| `/catDockTest` (mode `dock`) ends with the ships where the exe puts them, ASan 0 | `r5ld-aud2-dock` (rc 0, 16.3 s); after the fixes `r5ld-aud2-fix-dock`, `r5ld-aud2-fix2-dock`, `r5ld-aud2-final-dock` (build/debug) | `done fighters=6 outside=6 corvettes=2 RUs=1000 result=PASS`, 0 FAIL lines, ASan 0, no leak; coverage gap F8 |
| no HW1 dock path, bitmask or dock CAT-PROVISIONAL | section 4 | met |
| two 6,000-tick mission streams byte-identical | `mission_test.sh --label r5ld-aud2-mission --ticks 6000 --perturb 3000 --copy-tick 4000`, again after the fixes (`r5ld-aud2-fix2-mission`) | **MISSION TEST PASS** both times: 1,476,370 B, sha256 `215ec310c01a4abf`, ASan 0; perturbed run caught at 3000 / rng10, flipped copy at 4000 / `ships_health`; `hwo_compare.py` IDENTICAL against R4's `r4-integrate-mission-a` and the producer's `r5ld-final-mission-a` (only the build identity differs) |
| sp as before, ASan 0 | `r5ld-aud2-sp` (`--timeout 340`) | universe running, `univUpdateCounter` 1,360 at the timeout (rc 124 by design), ASan 0, no leak |

## 6. Findings

| # | finding | evidence | fix |
|---|---|---|---|
| F1 (code) | `cat_dockGetDockClass`'s launch purpose returned -1 for a monster / component class as a "CAT-FIX (the exe would fault)"; the exe does not fault | jump table 0x004641dc: entries 9 and 10, and the `ja` for classes > 11, go to 0x00463fcf, the purpose-3 code (`0x58` -> 0x1A, flag 0x1 -> 0xE, 0x200 -> 0xD, else -1) | `Dock.c`: the default now breaks out and falls into the purpose-3 code, `CAT:` + `[audit:]` comment; the report's CAT-FIX list annotated |
| F2 (code) | `CustShipLaunchCommand` was declared and called with one argument | `clLaunchShip` 0x0044a220: `(*pcVar2)(ship, receiverShip)` | `SpaceObj.h` slot 23 takes `(ship, receiverShip)`, `CommandLayer.c` passes both (no ship fills the slot yet) |
| F3 (doc) | "CET's 40 `ASSERT_OFFSET`s for `Ship`" | `grep -c "^ASSERT_OFFSET(Ship,"` = 66; three of them contradict the exe (section 2) | report annotated; the `SpaceObj.h` comment and the `m2_layout32.sh` header corrected |
| F4 (convention) | four NULL / -1 guards the exe does not have carried no `CAT-FIX` marker (`RemoveShipFromDocking`, dock states 6 and 12: `madBindings`; `dockPutShipOutside`: the point index) | the exe bodies of section 3 rows 4, 17, 20 | `CAT-FIX` comments added |
| F5 (delta map) | 7 edited IDENTICAL functions without a review entry: `aimProcessDock`, `ShipIsRefuelingAtCarrierMother`, `SaveDockInfo` (all three really MODIFIED), `aimProcessResourceVolume`, `kasfShipsDockSupportWith`, `DockCleanup`, `LaunchCleanup` | the hunk-to-function map; the exe bodies 0x00409350, 0x00467a90, 0x004ff900 (saves the plain 0x24 + 4n block) | review entries added (the three now MODIFIED), plus review entries for the 12 dock bodies compared here |
| F6 (convention) | purpose-constant conversions without a `CAT:` comment: `aimProcessResourceVolume` x2, `kasfShipsDockSupportWith`, `mrRetire`, `mouseDoubleClickCheck`, and `mrDockingOrders` (whose exe namesake 0x00567750 is the slipgate order, purpose 10, the gate as host) | `clWrapDock` call sites in the decompile (0x0040ad00 passes 3, 0x00494610 0, 0x005653b0 7, `mrObjectClick` / `cat_mrReleaseRight` 0 with a host) | `CAT:` comments; `mrDockingOrders` a `CAT-TODO(stage 3 UI)` |
| F7 (delta map) | the producer's reviews were written into `delta_map.json` only: `delta_map_review.json` lacked 72 of them and held other notes for 34 (28 of which the new notes had replaced), so `delta_map.py merge` would have reverted them and a re-classify dropped them; `counts` still said 3,065 / 1,278 / reviewed 1,615 | `git diff --stat 1434b90 -- ghidra/delta_map_review.json` empty; a field-by-field compare | the review file synced with the map (90 new and 35 updated entries counting this audit's 19, the 28 old notes kept in front), `counts` refreshed (3,060 / 1,283 / 77, reviewed 1,705), `delta_map.py merge` verified a no-op, `docs/m2-r1-function-deltas.md` regenerated with `delta_map.py report` |
| F8 (DONE WHEN, open) | the self-test covers less than its DONE WHEN: no carrier host (the host is the sMothership), the sProcessor is created but no order targets it (the depositor picks the mothership at an equal distance), and the two ACVs and the harvesting Worker are logged but not judged | `r5ld-aud2-dock/run.log`: the depositor `host=sMothership`; `sACV#172 state=8 ... inside=1` at the end; the report's own "open questions" | not fixed: recorded for M2-R5 integrate (add an sCarrier / sProcessor leg and judge the corvettes) |
| F9 (doc) | the first audit's line-count fix was off: `Dock.c` is 3,098 lines in `f59e2ef`, the self-test block 482 (from line 2,617), not 3,093 / 443 | `git show f59e2ef:src/Game/Dock.c \| wc -l`, the banner line | report annotated (3,105 after this audit's edits) |

Also annotated, not counted as an error: the retire refund's float, which the report called medium confidence, is
confirmed by the disassembly (row 16); the `Dock.c` comment says so.

## 7. Precision and fixes

Sampled checks: 2 builds + 1 `m2_layout32.sh` + 24 offsets + 1 CET claim + 29 bodies + 1 CAT-FIX-marking check + 6
IDENTICAL compares + 4 exe-table checks + 4 greps + 4 run claims (dock self-test, the stream pair with perturb and flip,
the stream against R4 and the producer, sp) + 1 self-test scope + 2 delta-map checks + 1 diff-scope check + 1
CAT-comment check + 5 first-audit fixes = **86 sampled, 9 wrong** (F1, F2, F3, F4, F5, F6, F7, F8, F9); precision
77/86 = **0.895**.

Fixes made in place: **8** (F1 and F2 in code, F4 and F6 as code comments, F5 and F7 in the delta map, F3 and F9 in
the report and comments), plus the refund annotation. Builds green after them (both audit builds and
`tools/build.sh --debug` / `--release`, 0 warnings), `m2_layout32.sh` OK, the dock self-test PASS (ASan 0) and the
mission pair byte-identical again.

Open (owners in the report's "What is left"): F8 (M2-R5 integrate); the `fsqrt` table question (user); the
corvette flight along the cone and the Worker's jack animation (stages 4 / 3); the ConsMgr, FOW and research
CAT-TODOs (M2-R6); the sound / speech ids (M2-R5 sound); `RemoveShipsFromDoingStuff`'s reason codes (M2-R5
command-tactics).

Verdict: PASS-WITH-FIXES - 77/86 checks hold (24 Ship/SpaceObj/object offsets match the exe and CET where CET is right, 29 dock/launch/harvest/experience bodies and 6 IDENTICALs match the exe, layout32 OK, dock self-test PASS ASan 0, two 6,000-tick Mission01 streams byte-identical and unchanged from R4, sp ASan 0); fixed the unjustified cat_dockGetDockClass CAT-FIX, the one-argument CustShipLaunchCommand slot, 7 unreviewed edited IDENTICALs, the unsynced review file and counts, missing CAT markers and two doc counts; the self-test's carrier/processor coverage stays open.
