# Audit: M2-R4 port "univupdate" (universe update, mesh state, glow, deaths and deletion)

Auditor: adversarial audit, 2026-10-11.

- **Subject:** producer report `docs/m2-r4-univupdate.md` and the uncommitted diff in the `m2` worktree (23 tracked
  files, +3,027 / -665 before the audit: `UnivUpdate.c/.h`, `SpaceObj.h`, `Mesh.c/.h`, `MeshAnim.c`, `Shader.c/.h`,
  `Trails.c/.h`, `SDL/render.c`, `Sensors.c`, `ETG.c`, `Gun.c`, `DefenseFighter.c`, `SaveGame.c`, `Collision.c`,
  `Crates.c`, `SalCapCorvette.c`, `DefaultShip.c`, `SinglePlayer.c`, `ghidra/delta_map_review.json`,
  `ghidra/delta_map.json`).
- **Not run:** Ghidra and the GOG game. No input was injected.
- **Evidence:** the r6 export (`ghidra/exports/cataclysm_gog_r6/decompiled.c`, `datatypes.h`, `call_graph.csv`),
  `original/game/cataclysm.exe` (constants read through its PE section table; `objdump -d -M intel` where Ghidra's
  output is garbled or its field names are HW1 layouts), the tree, the plain harness.
- **Audit helpers** (untracked, `/tmp/audit-uu/`): `fn.sh` (r6 body by address), `dm.py` (delta-map rows by name),
  `counts.py` / `diffmap.py` / `flip2.py` (delta-map counts against `HEAD`), `hunks.py` (diff hunks without a
  `CAT`/`HWI` marker), `mark.txt` (the audit's review lines).

## 1. Clean builds (separate dir, as `tools/build.sh` does)

`original/scratch-m2/build-audit-univupdate/` was removed and recreated, then
`meson setup --buildtype=debug -Db_sanitize=address,undefined debug` / `meson compile` and
`meson setup -Doptimization=2 -Ddebug=false -Db_sanitize=none release` / `meson compile` (`werror=true`,
`warning_level=3` from `meson.build`; checked in `meson-info/intro-buildoptions.json`). 295/295 steps each.

| build | rc | compiler `warning` lines | size |
|---|---|---|---|
| debug (audit dir, producer's tree) | 0 | 0 | 77,473,152 B |
| release (audit dir, producer's tree) | 0 | 0 | 4,838,608 B |
| debug (audit dir, after the audit fixes, incremental) | 0 | 0 | 77,473,152 B |
| release (audit dir, after the audit fixes, incremental) | 0 | 0 | 4,838,608 B |

After the fixes `tools/build.sh --debug` and `--release` also rc 0 with 0 compiler warnings (the only "WARNING" line
is meson's sanitizer notice); `build/debug/hwi-engine` 77,463,176 B, `build/release/hwi-engine` 4,838,608 B.
`tools/m2_layout32.sh`: OK.

## 2. Plain-harness runs (audit debug binary, `--bin` = the audit dir's clean build of the producer's tree)

| run | mode | result | univUpdateCounter | ASan / leaks |
|---|---|---|---:|---|
| `r4u-audit-sp` | sp, `--timeout 340` | universe running; still running at the 340 s timeout (rc 124, as R3); 3,713 file loads | **1,248** (R3 `r3-integrate-sp`: 1,248) | **0 / none** |
| `r4u-audit-etg` | etg (128 ticks) | etg effect self-test done: 618 created, 3,197 deleted, 1,093 left, peak 2,515 effects / 3,012 particle blocks; parse test 9 scripts, 197 effects, 0 missing (identical to `r3-integrate-etg` and `r4u-final-etg`) | | 0 / none |

UBSan (non-fatal): no new report kind against `r3-integrate-sp`; the touched files report only the known
`madanim`/`mhlocalbinding`/`shipbindings` misaligned-member sites (new sites `UnivUpdate.c:1324/1329` in
`cat_univMeshBindingsLinkParents`, as the producer said).

The audit fixes (section 4) change one unused argument and comments, so the runs were not repeated.

## 3. Checks (40)

"OK" = the port or claim matches the evidence; "WRONG" = a factual error in code, comment, report or delta map.

| # | check | evidence | result |
|---|---|---|---|
| 1 | `cat_shGlowUpdate` 0x0050d560: flags 0x31 force dirty, hit glow (flag 1, > 0) clamped to 1.0 then -0.2/s, flag 2 scorch += lost x 0.2, scorch -0.1/s (inside the flag-1 branch), clamp 0..1 (NaN -> 0), colour -0.15 x scorch + signed info bytes / 256, else team colour x trail weight (state 4 breaks, counter 0 -> 1.0, state 2 -> 1 - c/TRAIL_EXPANSION_TICKS), lastUpdate always set | decompile; constants 0x0072b0f0 = 0.2, 0x0072a510 = 0.2, 0x0072b0ec = 0.1, 0x0072b0e8 = -0.15, 0x0072a6b0 = 1/256, 0x0072a548 = double 1.0 (exe bytes); `TRAIL_CONTRACTED` 4, `TRAIL_EXPANDING` 2 (`Trails.h`) | OK |
| 2 | `cat_shGetTeamColour` 0x0050d460: mimic bit +0x18 & 8 (CAT-TODO), colour scheme (+0x359) in sp else owner index, `0x00bb3024 + i*0x30`, 0xFF000000 -> white, scaled to max 1 | decompile; `tecolorscheme` is 0x30 bytes with `TO_STANDARD_COLORS` 1 (`Teams.h`); `CAT_SHIPFLAG_Mimic` = BIT3 | OK |
| 3 | `cat_univMeshStateAllocRecursive` 0x00541b80: 0x28 per vertex zeroed, +0x11 = 1, +0x14 = -1.0, +0x08 = 1.0 for info flag 4 (info +4, 16-byte stride), slot {first dword, nVertices, states}, pre-order | decompile | OK |
| 4 | `cat_univMeshStateFreeRecursive` 0x00541c70: daughters first, then own slot | decompile | OK |
| 5 | `cat_univShipMeshStateCreate` 0x00541cd0: low-memory switch 0x00c5236c, ships only (objtype 0), `4 + 4n` table, LT_Mesh levels (flags & 7 == 1, 0x38 stride) get `nPolygonObjects x 12` slots | decompile | OK |
| 6 | `cat_univMeshStateFree` 0x00541df0: skips objtype 7 (effects), frees per level, clears | decompile (the LOD count from the static info equals the table's for ships, noted in the code) | OK |
| 7 | `cat_univMeshBindingsLinkParents` 0x00541ae0: per mesh LOD, `binding+0x54 = parentByte == 0xFF ? 0 : &local[parent]` (0x58 / 0x18 strides) | decompile | OK |
| 8 | `cat_shipNozzleInfoGet` 0x0053f8c0: (char)index -1 -> 0; `bindings->localBinding[LOD] + index` | decompile (the NULL-list guard is an HWI deviation, marked) | OK |
| 9 | `cat_meshObjectMatrixConcat` 0x0053f900: function ? binding matrix (+0x14) : object local matrix (+0x34); parent chain first | decompile | OK |
| 10 | `cat_meshObjectMatrixGet` 0x0053f970: identity 0x0072aed8 for NULL | decompile | OK |
| 11 | `trailGetNozzleOffset` 0x005a4cc0: nozzle object `staticinfo+0x2E0 [LOD*4 + trail]`, offset (w = 1) through that matrix | decompile; `CAT_ASSERT_OFFSET(ShipStaticInfo, engineNozzleObject, 0x2E0)` | OK |
| 12 | `AddShipToDeleteShipList` 0x00545b80: shiptype 0x12 (`sMothership` = 18) -> `cat_mothershipStartDeathSequence`; -1 damage type reads 0x0087ac63 | decompile; exe byte at 0x0087ac63 = 0x00 = `EDT_AccumDamage` | OK |
| 13 | report: "`AddTargetToDeleteList` ... its callers in `Crates.c` and `SalCapCorvette.c` pass 1 as the exe does" | `cratesPlayerFoundACrate` pushes **0** (0x00460ebc); only `expireCrate` (inlined in `cratesUpdate`) pushes 1 (0x00461f70); `call_graph.csv` has no `salCapHarvestTarget` caller (dormant HW1 code). `AddTargetToDeleteList` forwards a constant 1 (0x00545ded), so behaviour is unaffected | **WRONG** (finding 1) |
| 14 | `univDeleteDeadShip` 0x00547d00 order: force/torque, flags, salvage depot (TODO), `CustShipDied` (+0x144), factory (sp: player 0 only), external build (TODO) + dock removal, repair effect, launch manager by class 0/2, tactics, inside ships, droids (TODO), death effect, wingman chatter, inline bullet/missile clean-up (beam +10000, missile 5 s), sm/bob/ping, command layer, special ships, clamps, AI, camera, sp, tutorial, sound unless -1, ID table, grow-selections, selections, pie, mouse, hot keys | decompile | OK |
| 15 | the big-ship death switch: ids 11/18/32/53/56 mode 2 x0.5, 13/19..22/26..28/30/31/57 mode 1 x0.4, 23/24/29/54/55/98/104 mode 3 x0.75, 109/131 class 9 row; 10 effect parameters (+0x60, +0x64, +0x68, +0x60 + +0x78, +0x64 + +0x7C) | decompile; `ShipDefs.h` ids, `CLASS_MonsterShip` 9 (`ClassDefs.h`; 0x00c40b74 = table + 45 dwords = class 9) | OK |
| 16 | `univDeleteDeadShips` 0x00548e00: consumes the list head, impactable removal, CLASS_Resource dust cloud, `univKillOtherPlayersIfDead` | decompile | OK |
| 17 | `univWipeShipOutOfExistence` 0x005477d0 / `univDeleteWipeInsideShipOutOfExistence` 0x00547900: list removals only if linked, `catWipeShipFreeDirectly` 0x00a44000, the notification order | decompile | OK |
| 18 | `MiningBaseDied` 0x005b0370: no list removals after `univDeleteDeadShip` | decompile | OK |
| 19 | `univUpdate` 0x0054c3f0 per-tick order: frame step, MP autosave (200 s, AS0..10), debug autosave, time + real time, quit/win in real time, RU regeneration, collision blobs, fog, cm, research, injections (speech 0x2001044), first-tick setup, trade, sp update, bullet/missile collisions, crates, AI, command layer, mine walls, bump collisions, minor objects, bullets, missiles, derelicts/resources/ships, blasts (TODO), bullet-effect recreate, effects, the four delete lists, regrow, next level, bounties, KAS save (TODO) | decompile (the GoK `UNIVERSE_UPDATE_RATE_FACTOR` gates are 1 at 16 Hz) | OK |
| 20 | `CheckPlayerWin` 0x005485e0: `multiPlayerGame` test, skirmish ends with nobody alive, sp with player 0 dead, allied victory, real-time times | decompile (recorder global 0x00a751a0: CAT-TODO) | OK |
| 21 | `univFindBackupMothership` 0x00548ad0: race 0 `sCarrier` (12) / race 1 `bCarrier` (47) or class 0, speech 0x200101d on 0x12/0x35, hyperspace ether in MP with flag 0x4000 | decompile | OK |
| 22 | `ApplyDamageToCollidingObjects` 0x00545540: attribute bit 0x200 xor, min health, `ApplyDamageToTarget(.., 0, 0x309, 99, other, 9/10)`, effect 0x00c412c4 on the killer at position + velocity, heading = -velocity, params (damage, 1) | disassembly (Ghidra names +0x64 "force"); `DEATH_Killed_By_Collision` 777 = 0x309 | OK |
| 23 | `cat_meshApplyBulletHit` 0x004b52d0: per level, daughters of every top-level object start at slot 1 and the object itself uses slot 0 (0x004b53d6 `mov eax,1`, 0x004b540b `push 0`), callback(info, state, data) | disassembly | OK |
| 24 | `cat_meshObjectNearestVertexToHit` 0x004b4f80: `GL_MODELVIEW_MATRIX` (0xba6) read back, hit - translation times the 3x3, `rec.info == 0 || d < rec.dist`, object-space position kept; the bindings argument is not read | disassembly | OK |
| 25 | the bullet-hit glow call (0x0054703a): only with `applyDamage` and objtype 0, `cat_rndObjectEffectsSetup(target)`, radius^2 1.0, `cat_meshBulletHitGlowCB` 0x004b5820, data 1.0 | disassembly 0x0054702f..0x00547064 | OK |
| 26 | `InitializeEngineTrails` 0x00541f40: capital style 2 for shiprace 1 or 3 | decompile; `RACE_Beast` 1, `RACE_Taiidan` 3 | OK |
| 27 | `univRemoveAllTargetReferencesFromMissiles` 0x00547570: `min(totallifetime, timelived + 5)` | decompile; 0x008b5c68 = 5.0 | OK |
| 28 | `univRemoveShipFromHotkeyGroup` 0x00547b70: event 0x2002076, timeLastStatus set | decompile | OK |
| 29 | report/delta map: `univupdateInit` 0x0054af40 "no ClampedShipList init; ... the global at 0x00c52d70"; report row also lists "frame-step" globals | 0x00c52d70 **is** `ClampedShipList`: `univUpdateAllPosVelShips` 0x00544f40 adds clamped hosts to it and runs `updateClampedObject` over it, as HW1 does with `ClampedShipList`; the body clears 0x00a6ed68 and 0x00a6ed6c, not the frame-step globals 0x00a6ed74/78 | **WRONG** (finding 2) |
| 30 | `univFreeBulletContents` 0x0054b1d0 / `cat_univRemoveBulletReferencesFromShips` 0x0054b1a0: CustShipHeader +0x140 (slot 14) for every ship, mesh state, +0x84 | decompile | OK |
| 31 | `cat_univShipSelfRepair` 0x005433d0: combat by specialFlags bit 22 / +0x35a / +0x358, upgrade 0x200000 or Beast, damage > 0, not being built, class 10 or full health (not 0x12) only restarts the timer, `health += damage x +0x3dc`, clamp; module repair, droids, Beast system as CAT-TODO | decompile; `CLASS_Component` 10 | OK |
| 32 | `cat_univRemoveObjReferencesFromShips` 0x0054d350: +0x18 \|= 0x100, slot 12 (+0x138) for ShipList and the hyperspace ether | decompile | OK |
| 33 | `univUpdateAllPosVelShips` 0x00544f40: only `SOF_DontApplyPhysics` test, non-moving ships: housekeep + deferred list, madanim rules, clamped -> deferred -> housekeep order, 5 s protected-speed timer | decompile | OK |
| 34 | `univSetupShipForControl` 0x0054a230: no cloak pass, no HW1 self-repair, deferred list (`unk1`) | decompile | OK |
| 35 | `univupdateClose` 0x0054bbd0 reclassed IDENTICAL (folded `star3dClose`) | decompile vs `Star3d.c` (`memFree` only) | OK |
| 36 | `LoadSpaceObj` 0x00501660 clears +0x84/+0x80 of every loaded object | decompile (`m22 = 0; m12 = 0` after the switch) | OK (but see finding 3) |
| 37 | `meshRender` 0x004b4280 / `meshRenderShipHierarchy` 0x004b4520 take the state as a new first argument | decompile | OK (but see finding 3) |
| 38 | delta map: `UnivUpdate.c` 44 / 53 / 1, 98 review entries (87 new + 11 changed), map 3,106 / 1,237 / 77 with 1,461 reviewed, the 21 extra merged records (Universe.c 7, SalCapCorvette.c 3, ...) | `counts.py`, `diffmap.py` against `HEAD` | OK |
| 39 | DONE WHEN: sp >= R3 with ASan 0; etg unchanged | section 2 | OK |
| 40 | builds green, 0 warnings, layout32 OK | section 1 | OK |

**Precision: 38 / 40 = 0.95.**

## 4. Findings and fixes

1. **Wrong caller claim (check 13).** `cratesPlayerFoundACrate` passes 0 as the unused third argument, not 1;
   `salCapHarvestTarget` has no exe counterpart. *Fixed:* `Crates.c` `cratesPlayerFoundACrate` passes 0 with a
   `CAT:` comment (0x00460ebc), `expireCrate` and `SalCapCorvette.c` got `CAT:` comments; the report row is annotated
   `[audit: ...]`. No behaviour change (`AddTargetToDeleteList` 0x00545dd0 never reads the argument).
2. **Wrong `univupdateInit` description (check 29).** "no ClampedShipList init" and "frame-step globals" are false.
   *Fixed:* the review entry in `ghidra/delta_map_review.json` (merged into `ghidra/delta_map.json`) and the report row
   (`[audit: ...]`). The code was already right (it keeps `growSelectInit(&ClampedShipList)` and clears
   `catShipCounterA` 0x00a6ed68 and `catProtectedSpeedLimitTime` 0x00a6ed6c).
3. **Edited IDENTICAL functions without a (correct) review entry.** The port changed `meshRender` 0x004b4280 and
   `meshRenderShipHierarchy` 0x004b4520 (both IDENTICAL, unreviewed; the exe adds the state as a first argument and
   drops the colour-scheme argument) and `LoadSpaceObj` 0x00501660 (reviewed IDENTICAL in R2's re-read, but the exe
   clears +0x80/+0x84 after the switch). *Fixed:* three review entries, class MODIFIED (`LoadSpaceObj` keeps its
   `reread-R2` sample tag), merged with `--done m2r4:univupdate-audit`. The map is now 3,103 / 1,240 / 77,
   1,463 reviewed; the report's count line is annotated.
4. **Cataclysm changes without a `CAT:` comment.** Four caller edits for the new signatures: `Crates.c` (2),
   `SalCapCorvette.c` (1), `Collision.c:1951` (the second `univBulletCollidedWithTarget` call). *Fixed* (comments; the
   `Crates.c`/`SalCapCorvette.c` ones are part of fix 1). The remaining unmarked hunks in `UnivUpdate.c` are signature
   follow-ups inside functions whose header or neighbouring line carries the `CAT:` note (removed locals, `,1` on
   `AddShipToDeleteShipList` calls, the real-time quit compares under the "CAT: the quit and win times" block).

No change outside the producer's scope was found: `Collision.c`, `Crates.c`, `SalCapCorvette.c` and `SinglePlayer.c`
are follow-ups of the changed signatures / real-time quit times.

Observations (no change):
- The exe's mission attribute word is a dword at SpaceObj +0x38 (`kasfShipsAttributesSet` 0x004933d0 writes
  `[esi+0x38]`); the tree's `obj.attributes` is a `uword` at ILP32 +0x36 with `attributesParam` at +0x38. The port is
  consistent by name (the tree's KAS setters write `obj.attributes`, the killer-collision test reads it, bit 0x200
  fits), but M2-R4 #5's header pass should place the field at the exe's offset and width.
- The report did not say that 2 `UnivUpdate.c` MODIFIED entries became IDENTICAL (`univWipeDerelictOutOfExistance`,
  `univupdateClose`); annotated. Check 35 confirms `univupdateClose`.

Fixes in place: 11 (1 code argument, 3 `CAT:` comments, 4 delta-map review entries, 3 report annotations).

Verdict: PASS-WITH-FIXES - 38/40 checks match r6 and the exe (mesh state, glow, bindings, nozzle matrices, death/deletion order, univUpdate tick order, killer collisions, bullet-hit glow); sp reaches universe running with univUpdateCounter 1,248 and ASan 0, the etg self-test is unchanged; fixed the crate caller's unused argument and claim, univupdateInit's delta-map note, 3 unreviewed edited IDENTICAL functions and 4 missing CAT comments
