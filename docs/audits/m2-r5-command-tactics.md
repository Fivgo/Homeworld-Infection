# Audit: M2-R5 producer 2 "command-tactics" (orders 11-16, force-field formations, Tactics.c)

This audit checks `docs/m2-r5-command-tactics.md` (marker `<!-- done: layout-move, orders, formation, attack-misc,
tactics, final -->`) on branch `m2`, base `6f64a21`. There are no commits after the base: the whole port is the
working tree, 53 files, +6,486 / -4,220 lines.

How the checks were run:
- Every engine run went through `tools/m2_harness.sh` or `tools/oracle/mission_test.sh`, the plain harness, silent and
  headless.
- Ghidra was not run. The exe was read only as data and never executed:
  - `tools/m2_fdec.py` (the `cataclysm_gog_r6` decompile);
  - `tools/m2_fdis.py` (`objdump` of `.text`);
  - `tools/m2_exedata.py` (`.rdata` / `.data`).

## 1. Builds (from clean)

Steps:
1. `rm -rf original/scratch-m2/build-audit-r5-command-tactics`.
2. `meson setup` with `tools/build.sh`'s arguments:
   - debug: `--buildtype=debug -Db_sanitize=address,undefined`;
   - release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`.
3. `meson compile`.

Result: both builds **rc 0, 0 warnings, 0 errors under `-Werror`**. The only "WARNING" line is meson's sanitizer
notice.

Sizes (`size`):

| build | text | data | bss | file |
|---|---|---|---|---|
| debug | 43,369,241 | 25,133,384 | 2,842,944 | 80,279,904 B |
| release | 3,887,613 | 267,832 | 2,268,768 | 4,909,496 B |

After this audit's fixes, four builds rebuild with rc 0 and 0 warnings: both audit builds, and `tools/build.sh
--debug` and `--release` (the worktree's `build/`).

`tools/m2_layout32.sh`: **OK**.
- TacticsInfo 2,500 = 0x9C4, CommandToDo 816 = 0x330, FormationCommand 96, Ship 1,596.
- The 45 ship specifics asserts hold.

## 2. Bodies compared with the exe (29 bodies, 3 AIShip sites, 3 IDENTICALs, 2 tables)

Notes on reading the decompile:
- Ghidra's `SelectCommand` is 12 bytes, so `selection[k].numShips / .timeLastStatus / .ShipPtr[0]` = `ShipPtr[3k-2 /
  3k-1 / 3k]`.
- `Node` is 16 bytes, so `toadd[k].next/prev/belongto/structptr` = `+16k + 0/4/8/0xC`.

| # | function (exe) | result |
|---|---|---|
| 1 | `cat_clLink` 0x0044d3b0 | as the exe. Reuse branch: protect cleared, then passive attack, then formation (`catClearCommandForNewOrder` = the inline code), then `FreeLastOrder`. Otherwise reason 0xf and `memAlloc(...,0)`. Order 0xb, `cat_clLinkCleanup` per ship. Pairing loop (disasm 0x0044d5cb..0x0044d6c5): a partner at +0x650 or no partner drops the ship (command NULL, reason-5 disable off, Sentinels unclamped = `RemoveShipFromFormation`). The swap when `partner.z < ship.z` gives `cat_linkPairShips(lower, other)`. Then Prepare / InitShipsForAI. |
| 2 | `cat_clUnlink` 0x0044d7a0 | as the exe: type-flag 0x40 filter (static +0x1EC), reason 0x10, order 0xc, `cat_unlinkPrepareShip` |
| 3 | `cat_clFindLinkPartner` 0x0044d2e0 | as the exe (upgrade 8, in selection, same type, not self, +0x650 free, nearest by `fsqrt`) |
| 4 | `cat_clPointOrder` 0x0044dd70 | as the exe (disasm read: reason 0x12, order 0xe at +0x14, attributes 0, point at +0x2DC; the per-ship loop is `PrepareShipsForCommand`; the byte store at Ship+0x638 is `LeechSpec.state`, a `ubyte`) |
| 5 | `cat_clMimic` 0x0044da80 | as the exe (reason 0x11, order 0xf, +0x2E8..+0x2F4) |
| 6 | `cat_clResearch` 0x0044dba0 | as the exe (empty 8-byte selection, order 0x10, +0x2F8 uword player, +0x2FC lab, +0x300 tech, +0x304 mode; not run through RemoveShipsFromDoingStuff) |
| 7 | `cat_clUpgradeShips` 0x0044de80 | as the exe (clHalt first; start / end times; disable reason 0; upgrade 0x400 makes the cannon; instant upgrades applied at once; speech 0x2000894 if any upgrade is timed) |
| 8 | `SetFormationOffsetsForForceFieldWall` 0x00481ef0 | **all 25 places** of the three rings checked against the exe's switch cases: offsets in units of 0x0087f960 = 300.0 (3x = 0x0072a534 = 3.0) and both link pairs (+0x644/+0x648, +0x64C/+0x650). Stops after the 25th place, as the exe does. |
| 9 | `formationSpecificsSphere` 0x00481880 | as the exe: centre ship (protect / attack with `enders` / own leader); proximity-sensor case, both races; the force-field 13-ship entry 0x00a436f8; the radius cases incl. enders + force field -> field formula; no minimum radius for the field; poles; declinations; the frame 0x0072af18 (identity, read); the last ship to the bottom pole |
| 10 | `cat_formationSentinelFieldLevel` 0x004817d0 | as the exe (0x4000 / 0x2000 = `CAT_UPGRADE_Ability3/2`, Sentinel-only fatal, level to Ship+0x964) |
| 11 | `cat_formationSphereSentinelLinks` 0x004815d0 | all 10 cases match the tree's `CATLINK` table |
| 12 | `clFormation` 0x00484c80 | as the exe. Per-ship header veto; caps 12 / 24 with `clRemoveShipFromSelection`; passive attack cleared for 9/10; same type and level means no change; inline re-sort (call 0x00481200 with 2 arguments, = `formationBiggestThenClosestOptimum`); move/protect/attack conversion. New command: reason 0x20; `ChangeOrderToMove` with 5 arguments, from `points[curPoint-1]` or the last (disasm 0x00485002..0x0048503c); attack reason 4. The 3rd slot argument (level) is not passed: the only implementer, `SentinelFormationCommand` 0x005c6df0, reads 2. Documented in the comment, behaviour-equal. |
| 13 | `cat_tacticsFindResponsibleShip` 0x005294c0 | as the exe (field emitter -> moon, component host, leader) |
| 14 | `cat_tacticsShipAttacked` 0x00529c20 | as the exe |
| 15 | `cat_tacticsRetaliate` 0x00529cb0 | Code as the exe: SOF_Hide 0x8000; order cases 2,3,4,5,7,8,10,0xb,0xc,0xf,0x11; linked Acolyte (types 1/0x22) / sMimic (2) at +0x650; Pyrophoric (4) copy; Evasive / Neutral / Aggressive; the targetable filter; constraint 0x0052a210. **The comment was wrong (F3).** |
| 16 | `tacticsStartUp` 0x005290b0 + `TacticsInfotable` 0x008a80a0 | `tacticsStartUp` as the exe (EnemyNearZone 0; 0x244 dwords of DodgeInfo; paddings 1.0; guard square; `oneOverburstSpeed` from 0x0072a4c4 = 1.0; universe+0x180). The table's **198 rows** match in order, name and callback (0x005259d0, 0x00525b90, 0x00525c20, 0x00525e30, 0x00525fa0). **145 TacticsInfo row offsets** were checked with ILP32 `_Static_assert(offsetof(...))` against the exe's pointers, and 37 `kamikazeDamage` / `burstDamage*` indexes against the enums. All hold; the probe's deliberate sanity assert failed, as it should. |
| 17-21 | `cat_clWrapLink` 0x00453940, `cat_clWrapPointOrder` 0x00453a70, `cat_clWrapMimic` 0x00453b30, `cat_clWrapSetLeechAction` 0x00453730, `cat_clWrapSetBeastSystem` 0x004535f0 | as the exe (packets: stage-5 CAT-TODOs; misc 5 / 7 / 10) |
| 22 | `clHalt` 0x00449e80 | as the exe (speech 0x4000802 / 0x4000806, the passive-attack cancel timer for orders 0/8/9, the `CustShipHaltCommand` filter, the halt loop) |
| 23 | `ChangeOrderToHalt` (inline in clHalt) | as the exe (protect, passive attack, bit 0x10, parade keeps its order) |
| 24 | `ChangeOrderToAttack` 0x00448460 | as the exe (passive attack cleared, hyperspace refused, same targets: only bit 0x10 cleared, attached modules, `doneInitialAttack` +0x27A, the two AI-reset variants) |
| 25 | `cat_clAddAttachedModulesToTargets` 0x004482f0 | as the exe (sCarrier 0xc kept plus its components, s/bMothership 0x12/0x35 replaced by them, others kept; input freed) |
| 26 | `ClearProtecting` 0x004447b0 | as the exe (sphere >= 2 ships -> content changed; force-field sphere -> wall) |
| 27 | `clAttackThese` 0x00448620 | as the exe (Leech-on-ship filter, target test before the empty test, reason 6 after the copies, modules, reason 1) |
| 28 | `clScuttle` 0x0044a150 | as the exe: tutorial sMothership only unmimicked; Bit7 + infection timer skipped; `deathtime` = (max-min) x 2^-32 (0x0072a4e0) x rand + min + time = `frandombetween` |
| 29 | `univCheckShipState` 0x0054a650 | as the exe: deaths modes 2/3/4; autostabilise mask 0x2a08000 + 0x80 = the five SOF flags; `nisIsRunning`; moving / idle retaliation; collector dock 3. The out-of-world block **is dead in the exe**, as the report says: `pieMovePointClipToLimits` 0x004e76b0 writes only its second argument (the zero vector), and the exe compares the unchanged position copies. The `memFree` of `getShipAndItsFormation`'s copy is a marked CAT-FIX for a real leak. |
| AIShip | 0x004149c6 (+0x5C4 > 0 -> limit), 0x00414cb9 (types 1/0x22/2, order 0xb, blob +0x94, partner +0x650), 0x0041546c (Sentinel 0x11/0x34, type 10, selection, protect +0x2B4) | all three as the tree |
| IDENTICAL | `ShipsInPassiveAttackRange` 0x00441960, `clSetResearch` 0x0044a710, `clWrapResearch` 0x00453bb0 (3 of the producer's 42) | identical |

## 3. Delta map, diff scope and conventions

- **Every MODIFIED / REWRITTEN body of the four files.** That is 62 + 1 in CommandLayer.c, 11 in CommandWrap.c, 16 in
  Formation.c, 4 + 1 in Tactics.c; `processLaunchShipToDo` and `clCommandMessage` are mapped to them too.
  - Every one is `reviewed`, and every one has a `CAT` comment naming it or its address in `src/`.
  - The exceptions were checked by hand. `BreakupIntoLeaderWingman` and `LaunchAllInternalShipsOfPlayerThatMustBeLaunched`
    carry M2-R4 ships-b CAT comments. `clSetMilitaryParade` (reason 0xd, read at 0x0044b291) and `clHoldingPattern`
    were ported but unmarked (F2). `createSphereStaticInfo` and `shipHasJustDisappeared` are ported.
  - The leftovers outside stage 2 are listed in the report with owners.
- **The producer's counts are exact** (computed from the base map):
  - 55 unreviewed IDENTICALs: 42 still IDENTICAL, 13 now MODIFIED. The list of 13 is as the report gives it.
  - The unreviewed MODIFIEDs: 29 in CommandLayer.c and 9 in Formation.c.
  - Review file vs base: 93 entries added, 10 updated.
  - The map and the review file are in sync: `delta_map.py merge` on a copy changed 0 records, and the copy was
    byte-equal to the map.
- **Hunk-to-function map of the full diff.** A script mapped each `git diff -U0` hunk to its enclosing function and its
  delta-map class.
  - There is no change outside the producer's scope beyond the files the report names: the caller updates for the
    new signatures, `Ship.formationcommand`'s retirement, AIShip, the self-test plumbing and the helpers.
  - Three edited IDENTICAL functions had no review entry (F1).
  - Ten Cataclysm call-site changes had no `CAT:` comment (F2).
- **Owners of CAT-TODO / CAT-PROVISIONAL.** None in the four files names an M2-R4 owner or R5-2. The remaining tags
  name M2-R5 sound or hud, M2-R6 FOW, stage 3 or stage 5.
  - Two `UnivUpdate.c` CAT-TODOs still name "M2-R4 #3/#4/#5": the asteroid, univCreateShip and level-load parts. These
    are not this producer's (it removed its #2), but the R4 owners no longer exist. This is noted for M2-R5 integrate,
    not counted as a finding.
- **`CAT-FIX` markers for stack fields left uninitialised.** `selectone.timeLastStatus = 0.0f` is set in clHalt,
  cat_tacticsRetaliate, univCheckShipState and cat_clWrapSetBeastSystem. Strictly this is neither a fault nor a leak.
  One of these selections goes into a network packet (clSendMisc), so zeroing it keeps the packet deterministic.
  Accepted, not counted.

## 4. DONE WHEN, on the plain harness (audit debug build)

| claim | run | result |
|---|---|---|
| every MODIFIED body of the four files ported or owned outside stage 2 | section 3 | met |
| `/catCommandTest` (mode `command`): link, unlink, point, mimic, research, force-field wall and sphere, ASan 0 | `r5c-aud-command` (3,000 ticks, rc 0, 15.8 s); after the fixes `r5c-aud-fix-command` (`build/debug`) | `done tick=2998 link=258 mcv=1 unlink=291 research=1/1 mimic=1 point=1/1 result=PASS`, 0 FAIL lines, ASan 0, no leak. Further results: point ordered at 18, gun state 1 at 19; sMCV link at 247; wall 4/4 at level 1 with 2 links; sphere 6/6 at level 1 with 5 links and 6 clamped. These are the producer's numbers exactly. The "exe's end states" are taken from the exe's code paths; the exe itself cannot be run. |
| two 6,000-tick Mission01 streams byte-identical | `mission_test.sh --label r5c-aud-mission --ticks 6000 --perturb 3000 --copy-tick 4000` | **MISSION TEST PASS**: 1,476,370 B, sha256 `3359dba35cd4c93d` (the producer's `r5c-fin2-a`), ASan 0. The perturbed run was caught at 3000 (rng10), and the flipped copy at 4000 (`ships_health`). |
| sp as before, ASan 0 | `r5c-aud-sp` (`--timeout 340`) | universe running; `univUpdateCounter` 1,248 at the timeout (rc 124 by design); ASan 0, no leak |

Observation (not a finding): this port changes the Mission01 stream. It differs from R5 layout-dock's
(`r5ld-aud2-mission-a`, `215ec310c01a4abf`) from **tick 0**, in `ships_pos` and `univcheck`. A change to the command
layer, formation centre and tactics can do this. The report records no stream hash before and after, so the exact
cause was not isolated here.

## 5. Findings

| # | finding | evidence | fix |
|---|---|---|---|
| F1 (delta map) | 3 edited IDENTICAL functions had no review entry: `kasfSelectedShipsInFormation` 0x00495cc0, `kasfShipsInFormation` 0x00495d10, `toDrawMoveCircle` 0x00568d60 (all edited for the `Ship.formationcommand` retirement) | the hunk-to-function map; the map has `reviewed` False; the exe bodies read `ship->command` +0x370, flag +0x16 bit 0, type +0x230 / pulse, as `cat_shipFormationCommand` does | compared (all three are IDENTICAL; `toDrawMoveCircle` only its formation test, the rest is the UI's, stage 3). Entries added to `delta_map_review.json` (sample `audit-R5-command-tactics`), merged (`reviewed` 1,801), `docs/m2-r1-function-deltas.md` regenerated, report annotated |
| F2 (convention) | 10 Cataclysm call-site changes had no `CAT:` comment. Reasons: `kasfTeamHyperspaceOut` 0x13, `SelectionSwitchPlayerOwner` 0x14, `kasfGateShipsOut` 0x15, `nisHaltAttack` 0x21, `clCollectResource` 0x22, `spGoNowHyperspaceCB` 0x23, `TellHyperspacingShipsToReady` 0x24. `clFormation` level 0: `clHoldingPattern` x2, Stats.c `SetupShipsForFight`. | the exe's pushes before `call 0x444810` are 0x00493825, 0x00493f89, 0x00497178, 0x004cf417, 0x004f1376, 0x00517408, 0x0051783f (every value as the tree); before `call 0x484c80` they are 0x0044b6b4 / 0x0044b718 (`push 0`) | `CAT:` comments with the exe address added, tagged `[audit: M2-R5 command-tactics]` |
| F3 (code comment) | `cat_tacticsRetaliate`'s comment lists "building" among the orders that do not retaliate; order 6 (`COMMAND_BUILDING_SHIP`) is not in the exe's case list | the switch at 0x00529d1d: cases 2,3,4,5,7,8,10,0xb,0xc,0xf,0x11 (the code is right) | comment corrected, `[audit: ...]` |

## 6. Precision and fixes

Sampled checks, 53 in all:

| checks | count |
|---|---|
| builds | 2 |
| `m2_layout32.sh` | 1 |
| bodies | 29 |
| AIShip sites | 3 |
| IDENTICAL spot-checks | 3 |
| TacticsInfotable rows, offsets and indexes | 1 |
| exe constants (300.0, 3.0, the identity frame, 1.0) | 1 |
| delta-map checks: owned MODIFIEDs, sync / merge no-op, the 55/42/13 and 29/9 counts, edited IDENTICALs | 4 |
| diff scope | 1 |
| CAT-comment check | 1 |
| retaliation comment | 1 |
| run claims: command self-test, the stream pair with perturb and flip, the stream hash = the producer's, sp | 4 |
| CAT-TODO owner grep | 1 |
| formation-slot arity | 1 |

**3 wrong** (F1, F2, F3). Precision 50/53 = **0.943**.

Fixes made in place: **3**:
- F1 in the delta map, its review file and the generated report;
- F2 as code comments in 6 files;
- F3 as a code comment.

The report was annotated too. After the fixes, the builds are green with 0 warnings (both audit builds and
`tools/build.sh --debug` / `--release`), and the command self-test PASSes again with ASan 0. The fixes are comment and
delta-map only, so the binaries' behaviour is unchanged.

Open (not this audit's to fix):
- the two `UnivUpdate.c` "M2-R4 #3/#4/#5" CAT-TODO owners (M2-R5 integrate);
- `aishipFlyToPointAvoidingObjsFunc`'s remaining differences, which the report names (M2-R5 integrate / M2-R6);
- the stage-3 / stage-5 / M2-R5 sound / hud CAT-TODOs that the report lists.

Verdict: PASS-WITH-FIXES - 50/53 checks hold (29 command/formation/tactics bodies incl. orders 11-16 and the force-field wall, sphere, level and links vs the r6 exe, the 198-row TacticsInfotable with 145 offsets, layout32 OK, command self-test PASS ASan 0, two 6,000-tick Mission01 streams byte-identical, sp ASan 0); fixed 3 unreviewed edited IDENTICALs, 10 call sites without CAT comments and one wrong retaliation comment
