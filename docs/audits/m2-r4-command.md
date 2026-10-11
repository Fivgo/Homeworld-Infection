# Audit: M2-R4 port "command" (specialFlags renumbering, command layer, command wraps, formations)

Auditor: adversarial audit, 2026-10-11.

- **Subject:** producer report `docs/m2-r4-command.md` and the uncommitted diff in the `m2` worktree (20 tracked files
  before the audit: 18 under `src/` with +659 / -435 lines, `ghidra/delta_map_review.json`, `ghidra/delta_map.json`;
  4 new untracked helpers `tools/m2_specialflags_scan.py`, `m2_fdis.py`, `m2_fdec.py`, `m2_exedata.py`).
- **Not run:** Ghidra and the GOG game. No input was injected.
- **Evidence:** the r6 export (`ghidra/exports/cataclysm_gog_r6/decompiled.c`, `functions.csv`, `symbols.csv`), the
  R2 `objdump -d -M intel` listing of `original/game/cataclysm.exe` (`original/scratch-m2/m2r2-mesh-tools/exe.dis`),
  exe data read through the PE section table, the tree, the plain harness.
- **Audit helpers** (untracked, `/tmp/audit-cmd/`, written for this audit and independent of the producer's tools):
  `fd.py` (one exe function's disassembly by name or address, from `functions.csv` ranges), `dec.py` (one r6
  decompiled body), `pe.py` (exe data by VA: dwords, C strings, `NumStrXlate` tables, floats), `bitscan.py`
  (every `test/or/and/xor [reg+0x1c8..0x1cf],imm` in the listing, filtered to the bits the port calls unused; the
  same scan with all bits finds 38 bit groups, so it does match), `editedfuncs.py` (functions touched by the diff with
  their delta-map class and review state), `hunks.py` (diff hunks without a `CAT` marker), `dmcount.py` / `dm2.py`
  (review-file and map counts against `HEAD`).

## 1. Clean builds (separate dir, as `tools/build.sh` does)

`original/scratch-m2/build-audit-command/` was removed and recreated, then
`meson setup --buildtype=debug -Db_sanitize=address,undefined debug` / `meson compile` and
`meson setup -Doptimization=2 -Ddebug=false -Db_sanitize=none release` / `meson compile`. `meson-info` confirms
`werror=true`, `warning_level=3`, optimization 0 / 2, sanitizers address,undefined / none. 295/295 steps each.

| build | rc | compiler `warning` lines | size |
|---|---|---|---|
| debug (audit dir, producer's tree) | 0 | 0 | 77,357,424 B |
| release (audit dir, producer's tree) | 0 | 0 | 4,838,944 B (the producer's size) |
| debug (audit dir, after the audit fixes, incremental) | 0 | 0 | 77,362,560 B |
| release (audit dir, after the audit fixes, incremental) | 0 | 0 | 4,838,944 B |

The only "WARNING" line is meson's sanitizer notice at setup. After the fixes `tools/build.sh --debug` and
`--release` also rc 0 with 0 compiler warnings (section 6). `tools/m2_layout32.sh`: OK (every ILP32 layout assert
holds, 24 ship-specifics files).

## 2. Plain-harness runs

| run | binary | mode | result | univUpdateCounter | ASan / leaks |
|---|---|---|---|---:|---|
| `r4c-audit-sp` | audit dir, producer's tree | sp, `--timeout 340` | universe running; still running at the timeout (rc 124, as R3); 3,713 file loads | **1,248** (R3 and `r4c-final-sp`: 1,248) | **0 / none** |
| `r4c-audit-fe` | audit dir, producer's tree | fe, `--timeout 40` | `Main_game_screen`; 232 file loads (as `r4c-final-fe`) | | 0 / none |
| `r4c-audit-fixed-sp` | audit dir, after the fixes | sp, `--timeout 340` | universe running; still running at the timeout (rc 124); 3,713 file loads | 1,360 (the counter at the timeout depends on how long the real-time animatic took; `r4c-flags-sp` also had 1,360) | 0 / none |

UBSan (non-fatal) in `r4c-audit-sp`: the known misaligned-member kinds only (`fescreen`, `etgdecision`, `PATCH`,
`etgfunctioncall`, `mhlocalbinding`, `etgconditional`, `etgbranch`, `fquant.c`), no new kind.

## 3. Checks (65)

"OK" = the port or claim matches the evidence; "WRONG" = a factual error in code, comment, report or delta map.
Flag checks name the exe sites read (address of the access where it matters). Ship+0x1C8 = specialFlags,
Ship+0x1CC = specialFlags2.

### specialFlags / specialFlags2 against the exe (26 checks, 61 exe sites)

| # | check | evidence | result |
|---|---|---|---|
| 1 | sf 0x2 Kamikaze, 0x8 CrazyFast, 0x100 LimitSpeed | `tacticsGetShipsMaxVelocity` 0x0052948b: `test al,2` / `test al,8` / `test ah,1`; `kasfShipsSetMaxVelocity` 0x0049391e `or 0x100` | OK |
| 2 | sf 0x1 StopForResearchDocking, 0x40 BurstFiring, 0x80 WithinHyperSpaceRange | `cat_dockShipDocksAtShip` 0x0046612f `test byte,1`; `doBurstFire` 0x005b5269 `or 0x40` / `and 0xffffffbf`; `CalculateRollCall` 0x0051775d `test byte,0x80` | OK |
| 3 | sf 0x1000 STAY_TILL_EXPLICITLAUNCH | `InitializeDockVars` 0x00463c5f `or 0x1000`; `cat_dockShipDocksAtShip` 0x004660e7 `test ah,0x10`; `ShipHasToLaunch` decompile `& 0x1000` | OK |
| 4 | sf 0x2000 new repair modifier | `kasfShipsSetRepairFactor` 0x00493a44 `or 0x2000`; `kasfShipsClearRepairFactor` 0x00493a84 `and 0xffffdfff` | OK |
| 5 | sf 0x4000 KasSpecialDamageModifier (HW1 0x2000) | `kasfShipsSetDamageFactor` 0x004939b4 `or 0x4000`; `gunShoot` 0x0048b71e `test ch,0x40` | OK |
| 6 | sf 0x8000 InParade | `spGoNowHyperspaceCB` 0x00517444 `or 0x8080`; tree `SinglePlayer.c:736/927` set WithinHyperSpaceRange\|InParade | OK |
| 7 | sf 0x10000 rowGettingOutOfWay | `rowFlyShipOutOfWay` 0x004142e5/0x004142f0; `ChangeOrderToMove` 0x0044659d; `clProtect` 0x0044b028; `clLaunchShip` 0x0044a335; `aishipFlyToPointAvoidingObjsFunc` 0x00414fc7 | OK |
| 8 | sf 0x20000 ATTACKMOVECANCEL | `ChangeOrderToMove` 0x00446479 `test 0x20000`, 0x004465ec `and 0xfffdffff` | OK |
| 9 | sf 0x40000 FriendlyStatus | `kasfShipsSetNonRetaliation` 0x00495db9; `selShipUnderMouse` 0x00506606; `CrystalInit` 0x005b0063; `SlipgateInit` 0x005c72a1 | OK |
| 10 | sf 0x80000 CatSalvager | `cat_salvageInit` 0x004f901b `or 0x80000`; `aishipFlyToPointAvoidingObjsFunc` 0x004153dd / 0x00415416 | OK |
| 11 | sf 0x100000 AttackFromAbove | `attackSideStep` 0x00422076 / 0x00422306; `RemoveShipFromAttacking` 0x00447802 `and 0xffeffff9` (= ~0x100006) | OK |
| 12 | sf 0x200000 Hyperspacing | `CalculateHyperspaceTimeForShip` 0x005179f0; `UpdateArrivingShip` 0x00518412 `and 0xffdfff7f`; `cat_dockShipDocksAtShip` 0x00466254 | OK |
| 13 | sf 0x400000 ForcedAttackStatus | `kasfForceCombatStatus` 0x00493d91 / 0x00493dc2 | OK |
| 14 | sf 0x800000 / 0x1000000 LaunchedFromKas / KasCheckDoneLaunching | `kasfLaunch` 0x004947db / 0x004947f0; `kasfTeamFinishedLaunching` 0x004948c0 | OK |
| 15 | sf 0x2000000 Launching | `clLaunchShip` 0x0044a272 `test 0x2000000` | OK |
| 16 | sf 0x8000000 SalvageTargetAlreadyDriven (HW1 sf2 0x1) | `univSetupShipForControl` 0x0054a26d `and 0xf7ffffff` | OK |
| 17 | sf 0x10000000 ShipInGravwell (HW1 sf2 0x2) | `gunShoot` 0x0048b6e8; `GravWellGeneratorRemoveShipReferences` 0x005b479d `and 0xefffffff` | OK |
| 18 | sf 0x20000000 InUnitCaps (HW1 sf2 0x4) | `unitCapCreateShip` 0x0053eacd `mov ecx,0x20000000` + `or` at 0x0053eb18; `unitCapDeleteShip` 0x0053ec12 / 0x0053ecc6 | OK |
| 19 | sf 0x40000000 CircularGuard (HW1 sf2 0x8) | `clProtect` 0x0044b0bb/0x0044b0ff (`mov edi,0x40000000`/`0xbfffffff`); `protectShip` 0x00441313; `protectShipsAvg` 0x0044165d | OK |
| 20 | sf2 0x1 DontPlowThroughEnemyShips, 0x40 CatNoAvoidance | `aishipFlyToPointAvoidingObjsFunc` 0x004151c5 `test byte,1`, 0x00414b00 `test byte,0x40`; `cat_dockShipDocksAtShip` 0x00466298 `or 0x40`; `cat_salvageSpecialTarget` 0x004fadd4; `cat_salvageDetach` 0x004f8909 | OK |
| 21 | sf2 0x8 CatBeingBuilt | `kasfShipUnderConstruction` 0x00493410 `shr 3; and 1`; `univCreateShip` 0x00542aea / 0x00542b0b; `cat_clExternalBuildUpdate` (`& 0xfffffff7`); `cat_univShipSelfRepair` 0x00543465; `cat_suShipProvidesSU` 0x00459a64; `ThisShipCanDockWith` 0x00464462; `univDeleteDeadShip` 0x00547dab | OK |
| 22 | sf2 KAS writers: 0x10, 0x4000, 0x8000, 0x200000, 0x400000, 0x800000, 0x1000000, 0x8000000 | `kasfShipsAttackCapable` 0x0049812a; `kasfShipsParalyze` 0x00493444; `kasfShipsSetBeastInfectionUnlimited` 0x00498a91; `kasfShipsSetBulimia` 0x0049a05a; `kasfEnableNaggarok` 0x0049a23b; `kasfEnableBeastWeaponAutoAttack` 0x0049a433; `kasfShipsSetInvincibleInWidescreen` 0x0049ac03; `kasfSetStayMimicked` 0x0049a87a; readers `CollectResources` 0x004f1803, `cat_rkoCanTarget` 0x005c4054, `ApplyDamageToTarget` 0x00546423, `cat_mimicCleanup` 0x005b8ea5 | OK |
| 23 | sf2 0x800, 0x1000, 0x2000, 0x10000, 0x20000, 0x80000, 0x100000, 0x2000000, 0x4000000, 0x10000000, 0x20000000 | `processCollectResource` 0x004f21cf `and 0xfffff7ff`, 0x004f216e `or 0x4000000`; `SentinelFormationCommand` 0x005c6e0a/0x005c6e33; `cat_salvageIsBeingSalvaged` 0x004f7c0a `shr 0xd`; `cat_salvageDepotInit` 0x004fc806; `cat_univBeastInfectionUpdate` 0x00543929 and the `kasfShipsSelectBeastified` predicate 0x00498b0a; `cat_salvageDepotProcessGeneric` 0x004f95c2; `cat_clStartExternalBuildTimer` 0x0044aa3c; `cat_shipSetSUExempt` (`(exempt & 1) << 0x14`); `cat_salvageSpecialTarget` 0x004fb70d; `rescollectFindNearestResource` 0x004f123b; `cat_suShipProvidesSU` 0x00459a7f; `SlipgateInit` 0x005c72b3, `ThisShipCanDockWith` 0x00464432 | OK |
| 24 | no exe access to the bits the port leaves free (sf 4, 5, 9-11, 26, 31; sf2 1, 2, 5, 7-10, 30, 31) | `bitscan.py`: no immediate test/or/and/xor on those bits in the whole listing (register-immediate sites are the producer's hand-read list) | OK |
| 25 | static checks: the 25 / 23 live flags are distinct and their unions are `0x7BFFF1CF` / `0x3FFFF859` | `~0x84000E30 = 0x7BFFF1CF` and `~0xC00007A6 = 0x3FFFF859`: the masks are exactly the complements of the free bits above; the `_Static_assert`s compile in both builds and in `m2_layout32.sh` | OK |
| 26 | no reader uses an old value | token scan of every `src/` statement (outside `src/reference`) holding a `specialFlags*` word and a `SPECIAL_*` name: 0 uses of a `SPECIAL_2_*` name on `specialFlags` or of a `SPECIAL_*` name on `specialFlags2`; no `SPECIAL_2_{SalvageTargetAlreadyDriven,ShipInGravwell,InUnitCaps,CircularGuard}` left; no literal mask on either word. The flags defined 0 make the inverted tests `!bitTest(.., 0)` TRUE only in dormant code (the `ResearchShip` parade branch of `Formation.c`, HW1 `SalCapCorvette.c`); `univGetChecksum`/`NetCheck.c`'s `& ~(Resourcing\|Finished_Resource)` now keeps the whole word, as the report says (stage 5) | OK |

### Orders and formations (4)

| # | check | evidence | result |
|---|---|---|---|
| 27 | orders 11, 12, 14, 15, 16 and their creators; no 13 or 17 | the only `mov word [reg+0x14],0xb..0x10` stores are `cat_clLink` 0x0044d58b, `cat_clUnlink` 0x0044d9a3, `cat_clMimic` 0x0044daf7, `cat_clResearch` 0x0044dbfb, `cat_clPointOrder` 0x0044ddd3 (a sixth, 0x004c1b73, is a stack local); no store of 0xd or 0x11 | OK |
| 28 | `cat_selMakeShipsProtectCapable` 0x00511f00 order list | decompile: cases 4, 6, 10, 11, 12, 14, 15, 16, 17 | OK |
| 29 | `FORCEFIELD_WALL_FORMATION` 9 / `FORCEFIELD_SPHERE_FORMATION` 10 | `NumStrXlate` 0x0087f998 read with `pe.py`: 0..8 as HW1, 9 `FORCEFIELD_WALL_FORMATION`, 10 `FORCEFIELD_SPHERE_FORMATION`, end; `StrToTypeOfFormation` 0x00481190 pushes 0x87f998 | OK |
| 30 | `clFormation` 0x00484c80 accepted types | `cmp ebx,0xa; ja` + byte table 0x00485084 = {0,0,0,0,0,0,0,1,1,0,0} (1 = `dbgFatalf` "Invalid formation %d" 0x00880d44): 0-6, 9, 10 valid. No tree caller passes 7/8/100 from game data (`NO_`/`SAME_FORMATION` occur only in `aiplayer.script`) | OK |

### CommandLayer / CommandWrap / Formation bodies against the exe (24)

| # | function | evidence | result |
|---|---|---|---|
| 31 | `protectShip` 0x004411c0 | capital range for types 0x40/0x4f (0x00441211/16); follow-behind for 0x10/0x33 (`sProcessor` 16, `bProcessor` 51); CircularGuard at 0x00441313; no DDDFrigate call | OK |
| 32 | `protectShipsAvg` 0x004413f0 | 0x40/0x4f/0x3f/0x4d (64/79/63/77); the dockship loop 0x00441634..0x0044164a has no `canReceiveSomething` gate | OK |
| 33 | `ClearProtecting` 0x004447b0 | `type == 5 && numShips > 1` -> changed; `type == 10` -> 9 then changed | OK |
| 34 | `shipHasJustCloaked` 0x004452f0 / `shipHasJustDisappeared` 0x00445340 | flags 2 / 8, 4th argument 0; `sub_53F9F0` (= `cat_shipDetectedByPlayer`) | OK |
| 35 | `shipHasJustBeenDisabled` 0x00445360 | `ccRemoveShip` only when the 2nd argument is 0; hook for 0x40/0x4f only; three madLinkIn calls unconditional; both exe callers (0x00429a9f, 0x004fb6f4) push 1 | OK |
| 36 | `RemoveShipFromBeingTargeted` 0x004453c0 | decompile, pass by pass: salvaged-player filter, hyperspace pass (dock by dockship only, salvager loop via +0x134, falls through), protect pass with inlined ClearProtecting and free on attributes 0/order 0, passive attack, switch (attack with the crystal exception, dock, launch, building, special with `cat_salvageRemoveShipReferences`, parade's inlined halt), default ignored. All 7 exe callers' arguments match the tree's 7 calls | OK |
| 37 | `cat_shipHasJustBeenSalvaged` 0x0044b450 | `(cl, ship, 0x20, player)` | OK |
| 38 | `cat_shipIsSalvager` 0x004f8f60 | jump table 0x004f8fa8 over type-14: 14, 49, 72, 89, 97 -> 1; 102 -> `nullsubReturnFalse` 0x006fe9a0 ("trJunkYardDawg was thought to be removed") then 0 | OK |
| 39 | `cat_shipIsLiveWorker` 0x005c8e00 | objtype (+0x10) 0 and type 0xe/0x31 | OK |
| 40 | `clScuttle` 0x0044a150 | tutorial gate, sMothership (0x12) skipped in the tutorial, `catShipFlags & 0x80 && +0x4E4 != 0.0` (0x0072a460 = 0.0) not scuttled, unmimic for both: the tree ports the first two and lists the rest as CAT-TODO | OK |
| 41 | `ShipHasToLaunch` 0x0044a780 | player-index compare, 0x2b/9, 0x1000, class 6/11 with fighters, class 5 or live worker with corvettes; same truth table as the tree | OK |
| 42 | `clCreateShip` 0x0044a840 | `PlayerMothership->staticinfo->shipclass == 2`; `univCreateShip(.., 5, creator, 0)` | OK |
| 43 | `clShipDied` 0x0044b470 | parade around the dead ship (order 9, `*(+0x2c0) == deadship`) copied and stopped first; `(cl, ship, 1, 0)`; research calls listed as CAT-TODO | OK |
| 44 | `cat_clStartExternalBuildTimer` 0x0044aa30 | first ship, sf2 `or 0x80000`, only when `numShips != 0` | OK |
| 45 | `cat_clExternalBuildUpdate` 0x0044dfc0 | modes 0/1/2; progress at +0x3E8; health +0x170 += delta x statinfo+0xA4, clamped; mode 0 `< 1.0` (double 0x0072a548), clears 0x8, research call when shiprace (+0x2B4) is 0 (`RACE_Sect`), `cmDirtyShipInfo`; mode 1 clears, `univReallyDeleteThisShipRightNow`, `cmDirtyShipInfo` | OK |
| 46 | `univCreateShip` external-build branch | 0x00542ac4..0x00542b10: `externalBuild` (statinfo+0x1F0) && `built & 1` -> health 0, +0x3E8 = 0, `or 8`; else health = maxhealth, +0x3E8 = 1.0f, `and ~8` | OK |
| 47 | `clFlashShips` 0x00452960 | `cat_taskRealTimeElapsed` (0x00a6b20c) into +0x174; objtype 0, `[ship+0x1bc]` -> `+0x34` list | OK |
| 48 | `clWrapMove` 0x004529e0 | pause gate with opPauseOrders; empty `lastTimeRepaired` scan; no tutorial message | OK |
| 49 | `clWrapMpHyperspace` 0x00452ae0 | pause gate with opPauseOrders; `cat_shipSetDisableReason(ship, 1, 6)` | OK |
| 50 | `clWrapDock` 0x00452d60 | `dockType != 7` -> `ioUpdateShipTotals`; speech 0x400106a / 0x4000801 / 0x4000803 (CAT-TODO) | OK |
| 51 | `clWrapSpecial` 0x004531d0 | flash targets else selection; `cat_shipIsSalvager` result ignored; tutorial bit 0x200000; the copy freed on every path | OK |
| 52 | `clWrapSetTactics` 0x00453500 | returns on an empty selection | OK |
| 53 | `clWrapCollectResource` 0x00453000, `clWrapScuttle` 0x00453390, `clWrapSetKamikaze` 0x00453820 | resource flash in real time; opPauseOrders in both gates | OK |
| 54 | `cat_clWrapStartExternalBuildTimer` 0x00453660, `cat_clWrapExternalBuildUpdate` 0x00453a00 | no pause gate; local path; `clSendMisc(.., 0xb, 0)` / `cat_clSendExternalBuildUpdatePacket` listed as CAT-TODO | OK |

### Builds, runs, report, delta map, conventions (11)

| # | check | evidence | result |
|---|---|---|---|
| 55 | clean debug + release, 0 warnings under `-Werror` | section 1 | OK |
| 56 | `m2_layout32.sh` | section 1 | OK |
| 57 | sp as far as before, ASan 0 | `r4c-audit-sp`: 1,248, ASan 0 | OK |
| 58 | fe unchanged | `r4c-audit-fe`: `Main_game_screen`, 232 loads, ASan 0 | OK |
| 59 | report: "18 files, +659 / -435 lines, 191 added lines carrying CAT" | `git diff --numstat -- src` minus the audit's own edits: 18 files, 659 / 435; 195 `+...CAT` lines now, 4 of them the audit's | OK |
| 60 | report: 33 review entries, 27 new; map 3,097 / 1,246 / 77, 1,490 reviewed | 29 keys added to `delta_map_review.json` and 4 changed; 2 of the added keys (`unitCapCreateShip`, `unitCapDeleteShip`) were already `reviewed` rows in the map, so 27 rows became reviewed; the counts match `HEAD` + 6 class changes | OK |
| 61 | every edited IDENTICAL function has a review entry | `editedfuncs.py`: `GravWellGeneratorRemoveShipReferences` 0x005b4760 is edited (its flag clear moved words), IDENTICAL and unreviewed | **WRONG** (finding 1) |
| 62 | every Cataclysm change carries `CAT:` | `hunks.py`: the unmarked hunks are includes, signature follow-ups (`,NULL`), removed locals, and lines inside functions whose header or neighbouring line carries the `CAT:` note (`RemoveShipFromBeingTargeted`'s header lists every delta, `ClearProtecting`, `shipHasJustBeenDisabled`, `clFlashShips`, `clWrapSpecial`, `clFormation`) | OK |
| 63 | report: "`CommandLayer.c`, `CommandWrap.c` and `Formation.c` hold 87 / 65 / 1 functions" | at `HEAD` the three files hold 97 / 23 / 33 mapped functions; 87 / 65 / 1 are their IDENTICAL / MODIFIED / REWRITTEN totals (51+16+20, 45+7+13, 1) | **WRONG** (finding 2) |
| 64 | the CAT-TODOs that named this port are done or listed | `univDeleteDeadShip` (UnivUpdate.c) had "CAT-TODO(M2-R4 #2): ... the clearing of specialFlags2 bit 0x8 (being built), whose renumbering is M2-R4 #2's"; the port neither did it nor listed it in "What is left" | **WRONG** (finding 3) |
| 65 | no comment still describes the pre-port state | four comments say the renumbering or a caller is still M2-R4 #2's: `UnivUpdate.c` `univSetupShipForControl` ("the flags' renumbering is M2-R4 #2", next to the line the port edited) and `cat_univShipSelfRepair` ("renumbering: M2-R4 #2"); `ShipSelect.c` `MakeSelectionNonResearchShips` "Kept for its unported HW1 caller (CommandWrap.c: M2-R4 #2)", but the port removed that call, so nothing calls it now; `P1StandardCorvette.c` "the specialFlags renumbering is M2-R4 #2" | **WRONG** (finding 4) |

**Precision: 61 / 65 = 0.938** (61 correct, 4 wrong).

## 4. Findings and fixes

1. **Edited IDENTICAL function without a review entry (check 61).** `GravWellGeneratorRemoveShipReferences`
   0x005b4760: the port moved its `SPECIAL_ShipInGravwell` clear from specialFlags2 to specialFlags, as the exe does
   (`and 0xefffffff` on Ship+0x1C8 at 0x005b479d), but the map still said IDENTICAL with no review. Every other edited
   function has a reviewed class, or the map is MODIFIED/REWRITTEN for it, or it is a new or dormant HW1 function
   with no map row. *Fixed:* review entry, class MODIFIED (the body otherwise matches the decompile: find the node,
   clear the two `dont*ever` bits, `listDeleteNode`), merged with
   `python3 tools/delta_map.py merge --done m2r4:command-audit`. The map is now **3,096 / 1,247 / 77, 1,491
   reviewed**. Report annotated `[audit: ...]`.
2. **Misleading per-file count (check 63).** The report reads as if the three files held 87, 65 and 1 functions.
   *Fixed:* report annotated `[audit: ...]` with the real reading (class totals) and the per-file counts.
3. **A CAT-TODO assigned to this port was dropped (check 64).** `univDeleteDeadShip`'s TODO named M2-R4 #2 for
   clearing `SPECIAL_2_CatBeingBuilt` on a dead externally built ship. The exe does it right after
   `cat_cmExternalBuildShipDied` (0x00547da2..0x00547dae: `and eax,0xfffffff7` on Ship+0x1CC when statinfo+0x1F0 is
   set). *Fixed:* `UnivUpdate.c` now clears the flag there, with a `CAT:` comment. The
   `cat_cmExternalBuildShipDied` call stays a CAT-TODO, now pointed at M2-R4 #5 (ConsMgr's external-build driver).
   The report's "What is left" is annotated. This does not change behaviour now: with `CAT_EXTERNAL_BUILD_DRIVER 0`
   the flag is never set.
4. **Stale comments after the port (check 65).** *Fixed:* the two `UnivUpdate.c` comments now name the flag the port
   introduced; `ShipSelect.c` says `MakeSelectionNonResearchShips` has no caller left (`[audit: ...]`);
   `P1StandardCorvette.c` keeps the CAT-PROVISIONAL reason that is still true (mimic mode is M2-R4 #4's) and drops
   the stale renumbering reason (`[audit: ...]`).

Fixes: **4** (1 delta-map entry + merge, 1 code change of one line with its comment, 4 comments in 3 source files,
3 report annotations).

Observations (not counted as errors):
- `Mothership.c:488` still calls the external-build state "CommandLayer's (M2-R4 #2)". That is still provisional
  (the Ship+0x2FC timer is not ported), so it was left alone.
- The flags defined 0 make a few HW1 readers of the retired flags fall through to the exe's behaviour; others
  diverge until their owners port them. The producer lists every such reader with an owner. One example:
  `AIShip.c:1457/1470` tests `SPECIAL_IsASalvager`, which is now FALSE, where the exe tests `SPECIAL_CatSalvager`
  (0x004153dd); that is M2-R4 #3's.
- `RemoveShipFromBeingTargeted` also drops two HW1 `dbgAssertOrIgnore`s (launch `numShips == 1`; passive attack
  `order != COMMAND_ATTACK`). A retail build compiles them out, so this is not a Cataclysm delta.

## 5. DONE WHEN, re-checked on the plain harness

- *Every specialFlags/specialFlags2 bit at the exe's value with a static check, no reader using an old value:* met
  (checks 1-26). The `_Static_assert`s pin distinctness, both used-bit masks and the values M2-R3 quoted.
- *sp as far as before with ASan 0:* met (`r4c-audit-sp` 1,248, ASan 0; after the fixes `r4c-audit-fixed-sp` 1,360, ASan 0).
- *fe unchanged:* met (`r4c-audit-fe`).

## 6. Builds after the fixes

`tools/build.sh --debug` and `tools/build.sh --release` (incremental, 4/4 steps each): rc 0, 0 `warning:` lines;
`build/debug/hwi-engine` 77,348,496 B, `build/release/hwi-engine` 4,838,944 B. The audit-dir incremental builds
are in section 1.

Verdict: PASS-WITH-FIXES - 61/65 checks match the exe and the tree (61 flag sites, orders 11-17, formations 9/10, 24 command/wrap bodies; sp 1,248 ASan 0, fe Main_game_screen); the audit fixed an edited IDENTICAL function with no review entry (GravWellGeneratorRemoveShipReferences), a CAT-TODO of this port that it dropped (the SPECIAL_2_CatBeingBuilt clear in univDeleteDeadShip), a misleading per-file count in the report and 4 stale comments
