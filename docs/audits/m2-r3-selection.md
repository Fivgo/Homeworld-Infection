# Audit: M2-R3 port "selection" (ShipSelect.c filters, Cataclysm-only selection functions, Select.c)

Auditor: adversarial audit, 2026-10-10.

- **Subject:** producer report `docs/m2-r3-selection.md` and the uncommitted diff in the `m2` worktree (10 `src/` files,
  `ghidra/delta_map.json`, `ghidra/delta_map_review.json`).
- **Not run:** Ghidra and the GOG game. No input was injected.
- **Evidence:** the r5 export (`ghidra/exports/cataclysm_gog_r5/decompiled.c`, `decompiled_index.csv`,
  `call_graph.csv`, `symbols.csv`), the tree's headers for every constant (`src/Ships/ShipDefs.h`,
  `src/Game/ClassDefs.h`, `ObjTypes.h`, `CommandDefs.h`, `SpaceObj.h`), the plain harness.
- **Audit helpers** (untracked, `/tmp/audsel/`): `fn.py` prints an r5 body by name or address; `callers.py` lists every
  exe call of the six filters that gained an argument; `edited.py` maps each diff hunk to its function and that
  function's delta-map class and review state; `iou.py` compares the exe block's `ioUpdateShipTotals` callers with the
  source; `nocat.py` lists diff hunks without a `CAT` tag.

## 1. Clean builds (separate dir, as `tools/build.sh` does)

`original/scratch-m2/build-audit-selection/` was removed and recreated, then:

- `meson setup --buildtype=debug -Db_sanitize=address,undefined debug`, then `meson compile`;
- `meson setup -Doptimization=2 -Ddebug=false -Db_sanitize=none release`, then `meson compile`.

`werror=true` comes from `meson.build`; `-Werror` is on 249 ninja rules (249 C units compiled in each build).

| build | rc | `warning:`/`error:` lines | link | size |
|---|---|---|---|---|
| debug (audit dir, producer's tree) | 0 | 0 | `[292/292] Linking target hwi-engine` | 76,694,640 B |
| release (audit dir, producer's tree) | 0 | 0 | `[292/292] Linking target hwi-engine` | 4,801,952 B |
| `tools/build.sh --debug` (after the audit fixes) | 0 | 0 | yes | 76,684,752 B |
| `tools/build.sh --release` (after the audit fixes) | 0 | 0 | yes | 4,797,856 B |

The `build/` sizes equal the report's (76,684,752 / 4,797,856 B). The audit-dir binaries are a little larger
because the sources sit at a longer relative path from that build dir, and `__FILE__` strings embed that path.
`tools/m2_layout32.sh`: `OK (all ILP32 layout asserts hold)`.

## 2. Filter and selection bodies against the r5 decompile

Each row: the exe body, read in full with `fn.py`, against the ported body. Every constant was resolved against
the tree's enums: `sWorker` 14 / `bWorker` 49, `tResourceCollector` 88, `CLASS_*` 0-11, `OBJ_DustType` 5 /
`OBJ_DerelictType` 6 / `OBJ_MissileType` 8, `HyperspaceGate` 7, `MISSILE_Mine` 1, `RACE_Derelict` 6, the
`COMMAND_*` orders 3/4/6/7/8/10, and `COMMAND_MASK_FORMATION` 1 at `CommandToDo+0x16`.

Struct offsets:

- CustShipHeader slots: static+0x120 is AttackPassive, +0x124 SpecialActivate, +0x128 SpecialTarget, and
  `staticinfo->unk2+4` is CustShipAttack, slot 21. The decompile's own call site at line 60547 compares that slot with
  `GenericInterceptorAttack`.
- `Ship+0x210` is `catDisabled`.
- `selSelected` is at 0x00c0ab20 (`symbols.csv`), so `DAT_00c0ab28` is `selSelected.ShipPtr[0]`.

| # | function | result |
|---|---|---|
| 1 | `isShipPassiveAttackCapable` 0x00511b10 | correct: formation 9/10 under the formation attribute, specialFlags2 bit 3, then AttackPassive |
| 2 | `MakeShipsAttackCapable` 0x00511b60 | correct: CustShipAttack, `cat_shipIsSentinelInField`, arg && +0x210, `ioUpdateShipTotals` |
| 3 | `MakeShipsHarvestCapable` 0x00511be0 + `ShipCanHarvest` 0x00513680 | correct: class 7 and harvestAmount > 0, single player or `DAT_008860ec & 0x20` (`MG_HarvestinEnabled` BIT5), arg: not disabled and type 0xe/0x31 |
| 4 | `MakeShipsGuardCapable` 0x00511c70 (`ShipCanGuard` inlined) | correct: class 0 or 10, `externalBuild` (+0x1F0), arg && disabled |
| 5 | `makeShipsRetireable` 0x00511db0 | correct: type 0x58 |
| 6 | `MakeShipsSpecialActivateCapable` 0x00512250 | correct: +0x124, arg && disabled && type not 3/0x11/0x34 |
| 7 | `MakeShipsSpecialTargetCapable` 0x005122c0 | correct: +0x128, arg && disabled; `bFriendlies` unused (only an empty debug call) |
| 8 | `MakeShipsMobile` 0x005126b0 | correct: `immobile` (+0x31) or arg && disabled; speech for class 0 of the current player |
| 9 | `makeShipsFormationCapable` 0x00512780 | correct: removes disabled ships |
| 10 | `MakeTargetsOnlyEnemyShips` 0x00512970 | correct: specialFlags 0x40000 maps to the tree's `SPECIAL_FriendlyStatus` (renumbering deferred, as the report states); crystals 0x7e/0x7f |
| 11 | `MakeTargetsOnlyNonForceAttackTargets` 0x005129f0 | correct: every branch (derelict race not current player not crystal; derelict type 7; non-ally non-0x67 ship: crystal / `+0x18 & 0x10` removed, mimic test on `universe.curPlayerPtr`; mine kept) |
| 12 | `makeShipsControllable` 0x00512020 | correct: chatter for the selected ship with its type; no research-ship exception; `ioUpdateShipTotals` only after the early-out tests |
| 13 | `makeSelectionHyperspaceCapable` 0x005120d0 | correct: clamped list bit 3, salvaged + depot crystal, type 0x12 with command (order 7 or big gun), then `canMpHyperspace` (a mothership with no command falls through to the table test in both) |
| 14 | `MakeSelectionNotHaveNonVisibleEnemyShips` 0x00512d00 | correct: `cat_shipIsCloakedFully`, `sub_53F9F0(ship, player+0x24)` |
| 15 | `selectAllCurrentPlayersNonHyperspacingShips` 0x00512f00 | correct |
| 16 | `selectAllCurrentPlayersHyperspacingShips` 0x00512f80 | correct |
| 17 | `MothershipOrCarrierIndexInSelection` 0x00513310 | correct: class 0 returns, class 2 remembered |
| 18 | `growSelectAddShip` 0x00513350 | correct: realloc by 8 (`GROWSELECT_ADDBATCH` 8) |
| 19 | `MakeShipsNotIncludeTheseShips` 0x00511940 | correct: `ioUpdateShipTotals` after the loop |
| 20 | `selectMergeTwoSelections` 0x00513130 | correct: `ioUpdateShipTotals` only on the both-non-NULL path, which is where the tree calls `MakeShipsNotIncludeTheseShips` |
| 21 | `MakeSelectionKamikazeCapable` 0x005134e0 | correct: classes 6/5/7, returns `numShips > 0` after `ioUpdateShipTotals` |
| 22 | `makeShipsNotHaveNonCombatShipsForGuardAttack` 0x00512ca0 | correct: 0x3f, 0x40, 0x4d, 0x4f, 0x2d, 0x4e |
| 23 | `selectSelectionIntersection` 0x00513060 (MODIFIED to IDENTICAL) | correct: the swap on `<`, dup of the smaller, reverse scan and remove, free on empty are HW1's |
| 24 | `cat_selMakeShipsScuttleable` 0x00511ce0 | correct: disabled removed silently; types 0xc/0x12/0x2f/0x35 removed with chatter from `selSelected.ShipPtr[0]` |
| 25 | `cat_selMakeShipsProtectCapable` 0x00511f00 | correct: orders 4, 6, 10, 0xb, 0xc, 0xe-0x11 |
| 26 | `cat_selSelectionCopy` 0x005139b0 | correct: at most 255 pointers, source count |
| 27 | `cat_selMakeShipsHaltable` 0x00511ea0 | correct: +0x598 exception |
| 28 | `cat_selSelectionAddShip` 0x00511870 | correct: limit 1000 |
| 29 | `cat_selMakeShipsMobileNotBusy` 0x00511f90 | correct: the `MakeShipsMobile` body with the +0x210 test forced |
| 30 | `cat_selMakeTargetsAttackable` 0x00512b50 | correct: catTypeFlags 0x4000, type 1, +0x570, specialFlags 0x200000 mapped to `SPECIAL_Hyperspacing`, 0x8d, Sentinel field outside single player without force; objtype 5 removed. The exe tests `+0x570 == 1`, the port tests nonzero: no writer is ported, so this has no effect yet |
| 31 | `cat_selMakeTargetsBeastAttackable` 0x00512c40 | correct |
| 32 | `cat_selMakeShipsLeeching` 0x00512650 | correct |
| 33 | `cat_selMakeShipsDockCapableFor` 0x00513710 + `cat_shipClassCanUseSlipgate` 0x00513800 | correct: sizes 1/2/3 for 0x70-0x72; class table {0,9}:3, {1,2,3,4,7}:2, {5,6,8,10,11}:1, else 4 |
| 34 | `cat_selMakeShipsNotLinkedACVs` 0x00511df0 | correct: types 4/0x26, byte +0x6F0 is 1 or 2 (provisional query) |
| 35 | `cat_shipCanSetTactics` 0x005135e0 | correct: `(~flags >> 1) & 1`, type 9 |
| 36 | `cat_shipCanCollectResources` 0x00513620 | correct |
| 37 | `cat_shipCanLeech` 0x005136a0 | correct |
| 38 | `cat_acolyteCanFireLinkedWeapon` 0x005ada50 | correct: types 1/0x22, upgrade 0x4000000, guns[2]/[3] (+0x17C/+0x204 from gunInfo: 4-byte header, 0x88-byte exe Gun, numMissiles at exe Gun+0x68) |
| 39 | `cat_upgradeFlagsBaseTime` 0x00513f40 | correct: bit order and the 0x10000 = 20 s case match the exe's if-chain |
| 40 | `cat_spShipCanHyperspace` 0x00517650 | correct: SOF 0x800 / 0x800000, types 9/0x2b, class 10, 0x8c, mission 5 + 0x3e, 0x11 returns 1 |
| 41 | `cat_mothershipBigGunActive` 0x005bbf60 | correct as provisional: clamped scan for 0x1f not being built ported; byte +0x674 returns FALSE until ship code B |
| 42 | `selCentrePointComputeGeneral` 0x00508be0 | correct: ships and missiles only; the divisor counts every entry; mimic radius on `+0x18 & 8`; scale 1 when 0; missile scale (`moveTo.z` in the decompile = +0x254) provisional 1 |
| 43 | `selStartup` 0x00505300 | correct: fifth font `default.hff` into 0x00c0bae8 |

**`ioUpdateShipTotals` coverage** (`iou.py`): 45 exe functions in 0x00511800-0x00514300 call it. The same 45 in
the source call it, 43 directly; the only extra is `makeShipsNotIncludeSinglePlayerMotherships`, a HW1 function that
is not in the exe. This matches the report exactly.

**Callers of the six widened filters** (`callers.py`): game code passes 0: `aiuAttack`, `aiuWrapMove`,
`clAttackThese`, `clAttack`, `clPostProcess`, `kasfHarvest`, `cat_tacticsRetaliate`. UI code passes 1:
`mrObjectClick`, `mrSelectHold`, `cat_mrBandBoxRelease`, `cat_mrReleaseRight`, `mrHarvestResources`, `mrRetire`,
`cat_kbLeechCB`/`MimicShipsCB`/`MimicPlayersCB`. The report's list is exact. The tree's 10 / 2 / 1 TRUE and 4 / 1 / 1
FALSE call sites were counted in the diff.

**HW1 aliases and dormant ids:** 37 aliases and 22 dormant names were extracted from `src/Ships/ShipDefs.h`
(lines 182-243). A `grep -w` over `Select.c`, `Select.h`, `ShipSelect.c` and `ShipSelect.h` finds one match: the
`#include "GravWellGenerator.h"` line, as the report says.

## 3. Full diff review

- **Scope:** every changed file is in the producer's list. Outside `ShipSelect.c`/`Select.c`, the only edits are the
  argument additions at the call sites in `mainrgn.c`, `mouse.c`, `Sensors.c`, `CommandLayer.c`, `AIUtilities.c` and
  `KASFunc.c`, which the signature changes force. The `SpaceObj.h` additions are new `Ship` fields and flags, appended
  before `sizeofShipSpecifics`. `univCreateShip` zeroes them with `memset(newship,0,sizeship)` (UnivUpdate.c). The two
  specialFlags2 bits, BIT8 and BIT9, are free in this tree (only BIT0-7 are defined).
- **IDENTICAL functions edited** (`edited.py`): none. Every edited function the map knows is MODIFIED or REWRITTEN.
  The functions with no map record (`ShipCanGuard`, `makeShipsDockCapable`, `MakeTargetsSalvageable`,
  `MakeSelectionNonResearchShips`, `makeShipsNotIncludeSinglePlayerMotherships`, `selClickFromArray`, `mrKeyRelease`,
  `mouseLDoubleClick`, `smViewportProcess`) are not in the exe or not matched.
- **Review entries:**
  - `allianceIsShipAlly` 0x00420ec0: the crystal and non-ship early outs are in the decompile.
  - `selectSelectionIntersection`: row 23.
  - `selSelectHotKeyGroup` 0x00509030: see F2.
- **`CAT:` tags** (`nocat.py`): every hunk without a tag is an include removal or continues a function whose `CAT`
  comment is in a neighbouring hunk (`selCentrePointComputeGeneral`, `MakeTargetsSalvageable`), with one exception:
  F1.
- **Counts:** `git diff --numstat -- src/` before the audit gave 10 files, +1,395 / -449; with the audit's two comment
  lines it is +1,397. The added lines that carry `CAT` are 203 before the audit and 204 after.
  `tools/m2_stage_counts.py` stage 2: IDENTICAL 831 (167,690 B), MODIFIED 542 (234,001 B), REWRITTEN 29, cat-new 829.
  Totals: IDENTICAL 3,137, MODIFIED 1,207. All as reported.

## 4. Findings

| # | finding | evidence | fix |
|---|---|---|---|
| F1 | One changed call site had no `CAT:` comment, although the report says "each call carries a `CAT:` comment": `mainrgn.c` `mrObjectClick`, `MakeShipsHarvestCapable(..., TRUE)` (the asteroid/dust branch). | `git diff src/SDL/mainrgn.c` hunk `@@ -3597`. In the exe, `mrObjectClick` has no `MakeShipsHarvestCapable` call; it calls `mrHarvestResources`, which passes 1 (`call_graph.csv`, `callers.py`). | Comment added. Report sentence marked `[audit: ...]`. |
| F2 | The `selSelectHotKeyGroup` review note said the mask 0x2a08000 "is HW1's SOF_Hide". It is four flags: SOF_Hide 0x8000, SOF_Disabled 0x800000, SOF_Hyperspace 0x2000000, SOF_Crazy 0x200000. | The decompile tests `(flags & 0x2a08000) == 0 && (flags & 0x2000)`. The tree's `selSelectHotKeyGroup` (Select.c:2333) tests `SOF_Hide\|SOF_Disabled\|SOF_Hyperspace\|SOF_Crazy` and `SOF_Selectable`; the `SpaceObj.h` values are as listed. The IDENTICAL class itself is right. | Note corrected in `delta_map_review.json` and in the merged `review_note` of `delta_map.json`. Both files still parse. The report's line is marked `[audit: ...]`. |
| F3 | The `clMove` comment ("clAttack 0x00448950 and the other game-code callers pass 0") implies that the exe's `clMove` calls `MakeShipsMobile`. It does not. | `decompiled.c` has 13 `MakeShipsMobile` mentions. The only call is in `aiuWrapMove` (line 26492, argument 0). `clMove` 0x00446d90 uses the ships' own move-command slot instead. | Comment rewritten: argument 0 as game code passes (`aiuWrapMove`); the exe's `clMove` has no such call; its port is M2-R4 #2. |

**Observations, not counted as errors:**

- The exe's `aiuWrapMove` calls `MakeShipsMobile(..., 0)`, and the tree's `aiuWrapMove` has no such call. That is a
  Cataclysm delta in AIUtilities.c, which belongs to the AI stage, not this port.
- `cat_selMakeTargetsAttackable` tests `catDissolving` as nonzero, while the exe tests `== 1` (row 30). These are the
  same unless a writer stores a value other than 0 or 1. Check when the dissolve code (M2-R4 #4) is ported.

## 5. DONE WHEN, re-checked on the plain harness

| condition | result |
|---|---|
| the filters compile against the new flags | **met**: clean audit-dir builds, 0 warnings under `-Werror` (section 1) |
| and match the exe's filter bodies (spot-checked in the report) | **met**: 43 bodies compared (section 2), all correct; `ioUpdateShipTotals` coverage 45/45 |
| no HW1 alias left in Select.c and ShipSelect.c | **met**: only the `GravWellGenerator.h` include name |
| fe and sp unchanged or further | **met**: see the runs below |

Plain-harness runs, on the tree after the audit fixes (`build/debug/hwi-engine`):

| run | rc / wall | milestone | first stop | sanitizers | file loads | baseline |
|---|---|---|---|---|---|---|
| `audit-r3-selection-fe` (fe, 40 s) | 124 / 40.3 s | `Main_game_screen` | none (timeout) | ASan 0, LeakSanitizer clean | 232 | unchanged from `audit-r3-etg-fe` |
| `audit-r3-selection-sp` (sp, 90 s) | 1 / 10.1 s | level load (mission file read) | `Ships/GenericInterceptor.c (205)` | ASan 0; the 44-byte leak summary on the assert exit | 250 | `audit-r3-etg-sp`: same milestone, same stop, same 250 loads and same leak summary |

## 6. Precision

Sampled checks: 50.

- 43 body comparisons (section 2).
- The `ioUpdateShipTotals` coverage claim and the caller-split claim.
- The alias/dormant-id claim.
- The three review entries.
- The diff and stage counts.
- The report's per-call `CAT:` claim.

Wrong: 3 (F1, F2, F3). Correct: 47. **Precision = 47 / 50 = 0.94.**

Fixes in place: 3. They are one `CAT:` comment added in `mainrgn.c`, one comment corrected in `CommandLayer.c`, and one
review note corrected in `delta_map_review.json` and `delta_map.json`, with matching `[audit: ...]` marks in the
report. The tree builds green after them: `tools/build.sh --debug` and `--release` give rc 0 and 0 warnings.

Verdict: PASS-WITH-FIXES - 43 ported bodies match the r5 decompile, ioUpdateShipTotals coverage is 45/45, the caller split matches the exe, no HW1 alias is left, and fe/sp are unchanged on the plain harness; fixed one missing CAT comment, one misleading clMove comment and one inaccurate delta-map review note
