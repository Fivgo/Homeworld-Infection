# Round-4 symbol reconcile report

Output: `ghidra/symbols_round4.json` (cet_symbols.json schema). It holds **3526 symbols**: **663 new function names**, **1 rename**, **0 removals**, **0 data names**, and **2862 prototype-only entries** (current name repeated, `type` = signature). Generated 2026-10-09.

Inputs:
- Applied: `ghidra/symbols_round1.json`, `ghidra/symbols_round1_data.json`, `ghidra/symbols_round2.json`, `ghidra/symbols_round3.json`, the 105 B1-created names (`create_functions_r3.json`) and the 186 C1-created names (`create_functions_r4.json`).
- Audited sets in `ghidra/proposals/r4/`: `ui-render` (195, precision 1.0), `ai-ships-sim` (330 kept, 0.933), `titan-libs` (270 kept, 0.931), `prototypes-r4` (2,875 kept, 0.933), `quality-recheck` (0 kept, 0.25; see Renames).
- Validation export: `ghidra/exports/cataclysm_gog_r4a` (14,679 functions; functions.csv, symbols.csv, decompiled.c, decompiled_index.csv, call_graph.csv, datatypes.h).
- Binary checks: the round-3 `objdump -d -M intel` disassembly of `original/game/cataclysm.exe` (same bytes). It gives the RET imm of each body and the caller stack cleanup (`add esp,N` / `pop ecx`) after each direct call. The cleanup table was rebuilt against the r4a function starts.
- Scripts (scratch, not committed): `/tmp/r4rec/reconcile.py`, `validate.py`, `mkreport.py` (round-3 `reconcile.py` adapted).

## C1 check

All 186 names in `create_functions_r4.json` are function entries with exactly that name in r4a `functions.csv`, so C1 applied them. They are not re-emitted, and they count as taken names in the uniqueness check. Every round-1/2/3 function name is still present in r4a (6 round-1 entries show only a Ghidra namespace/thunk spelling: `exception::exception__exception_2/_3`, `lpStartAddress_0067a0a0` for `_threadstart`, `lpStartAddress_0071cdc0` for `_threadstartex`, and 2 import-thunk namespaces. These predate round 4 and are left as they are).

## Totals

| item | count |
|---|---|
| input `ui-render.verified.json` | 195 |
| input `ai-ships-sim.verified.json` | 330 |
| input `titan-libs.verified.json` | 270 |
| input `prototypes-r4.verified.json` | 2875 |
| input `quality-recheck.json` (producer) / `.verified.json` (kept) | 4 / 0 |
| distinct naming-proposal addresses | 717 |
| titan-libs class-only placeholders (`hw1_<File>_<addr>`, `cat_new_<addr>`) superseded by a real name from another set | 48 |
| cross-set conflicts between two real names, resolved (table below) | 17 |
| **new function names** (all on `FUN_*` entries in r4a) | **663** |
| **renames** of applied names | **1** |
| **removals** | **0** |
| **deferred to round 5**: function address not an entry in r4a functions.csv | **54** |
| dropped names (collision, not an identifier, data outside sections) | 0 |
| rename proposals rejected (audit verdict wrong) | 3 |
| naming-set signatures kept (binary-checked; explicit convention added) | 433 |
| naming-set signatures dropped (name kept, `type` empty) | 9 |
| **prototype-only entries** | **2862** |
| prototypes dropped | 13 |
| CET anchors overridden | 0 |
| **symbols written** | **3526** |

New names by confidence: high 171, medium 312, low 180. By class: hw1 173, cat-new 337, lib 89, won 64.

By set (`+` = both sets proposed the same name): ai-ships-sim 279, ai-ships-sim+ui-render 12, titan-libs 213, ui-render 159.

Prototype entries: 2862 (high 2216, medium 629, low 17); `__cdecl` 2785, `__fastcall` 77 (the 77 TitanInterface thiscall fixes); 2085 of them are the mission functions (`void __cdecl NAME(void)`).

## Rules as applied

- **Scope.** The file holds only what to apply now: new names on `FUN_*` entries, the audit-upheld rename, and prototypes. No proposal named an address whose current name equals the proposal (no no-ops).
- **Validity.** A function address must be an entry in r4a `functions.csv`; the rest go to `reconcile_deferred_functions.json`. No round-4 set proposed a data name. Every name matches `^[A-Za-z_][A-Za-z0-9_]*$`.
- **Uniqueness.** Each new name was checked against the CET names (patch sites included), rounds 1, 1-data, 2, 3, B1, C1, every non-default function and label name in r4a, the other round-4 names, and the deferred names (which reserve theirs). No collision occurred, so no `_N` suffix was added and nothing was dropped for a name clash.
- **Placeholders.** titan-libs gave class-only FUN_* a low fallback `hw1_<File>_<addr>` / `cat_new_<addr>` and said "a real name from another r4 set wins". Where ui-render or ai-ships-sim proposed a real name at the same address, the real name was taken (48 addresses, list below).
- **Cross-set conflicts** (two real names). Rules in order: (1) evidence read in this reconcile (r4a body, callers); (2) higher confidence; (3) audit precision (ui-render 1.0 > ai-ships-sim 0.933 > titan-libs 0.931); (4) keep one naming family per cluster. A merged entry takes the highest confidence among agreeing sets; only the winning name's signature is used.
- **Signatures** on naming proposals go through the round-3 binary check: RET imm decides `__cdecl`/`__stdcall`, caller cleanup must match the argument bytes, Ghidra's parameter count may not exceed the declared count. `__fastcall(pThis, edx_unused, ...)` must have RET = 4*(n-2). Types outside `/Cataclysm` (r4a `datatypes.h`) become `void *` for pointers (e.g. `FightStats *` in GetPostCombatStats, the battle ping and blast records); an unknown by-value type drops the signature.
- **Prototypes.** Same name as in r4a, parseable, explicit convention, no `__thiscall` (CParser has no token for it), `__cdecl` only with RET 0, `__fastcall` RET = 4*(n-2). Audit caveat applied: low entries whose producer evidence itself flags an arity/cleanup mismatch were dropped (list below).
- **Notes.** `notes` starts `class=<class>; confidence=<c>; sets=<sets>`, then optional `resolved=placeholder|cross-set (rejected: ...)`, `renames=<old>; why=...`, `sig-checked:...` / `sig-dropped:...` (`; types:X->void*`), then `evidence=<=200 chars`. Prototype entries read `sets=prototypes-r4; prototype-only (name unchanged)`.
- **Apply order.** The only rename releases `aiuGenerateRandomVector`, and no round-4 name takes it, so the order does not matter. ImportSymbolsJson sorts functions first (ascending).

## Renames (1)

| address | old name (applied) | new name | reason |
|---|---|---|---|
| 0x00419E80 | `aiuGenerateRandomVector` (round 3, low) | `cat_MoveReachedDestinationRadius` (medium) | audit verdict correct; reconcile re-check: r4a body = squared distance (ship+0x58 - *dest) < r^2 returning 0/1, 3 stack args (ship, vector*, real32); HW1 AIUtilities.c:90 aiuGenerateRandomVector(void) returns a random vector, so the applied round-3 low name (slot-0 guess) is wrong. Signature applied: `bool32 __cdecl cat_MoveReachedDestinationRadius(Ship *ship, vector *destination, real32 radius)`. |

quality-recheck.verified.json kept 0 entries: its precision 0.25 is under 0.7, so policy kept only verified high entries, and its only high entry was judged wrong. The auditor also wrote that 0x00419E80 is verified correct and recommended applying it ("applied name aiuGenerateRandomVector is demonstrably wrong"). The reconcile re-read the r4a body and HW1 AIUtilities.c:90 and agrees, so the rename is applied. The project rule (rename only on high-confidence evidence) is met, because the evidence that the old name is wrong is conclusive: a 3-argument distance test is not `vector aiuGenerateRandomVector(void)`. The new name stays medium. Apply-stage note: the old `[CET] aiuGenerateRandomVector ... hw1_equivalent=AIUtilities.c` plate comment is not removed by ImportSymbolsJson (it only appends), so the apply stage may want to clear it.

### Rename proposals rejected (3)

| address | applied name (kept) | proposed | audit reason |
|---|---|---|---|
| 0x005169F0 | `cat_saveUniverseList5e288` | `cat_blastSaveList` | Facts hold (0xa5e288 is the list cat_blastCreate fills, cat_blastUpdateAll walks), but the applied name cat_saveUniverseList5e288 is accurate (it saves the universe-save list at 0xa5e288); the rename is only better, not a fix of a wrong name, and is medium (project rule: applied names renamed only on high-confidence evidence). |
| 0x00516A10 | `cat_loadUniverseList5e288` | `cat_blastLoadList` | Same as 0x5169F0: body LoadLinkedListOfStuff(0xa5e288)+sets 0xa67010; applied name cat_loadUniverseList5e288 is accurate, so the rename is merely less-good -> more-good, medium confidence. |
| 0x0053C7F0 | `CloseStatInfo` | `CloseStatDerelictInfo` | r4a body = HW1 Universe.c:2352 CloseStatInfo verbatim (free LOD/mex unless default 0xa6c5bc/0xa6c5c0, memset 0x29 dwords). HW1 CloseStatDerelictInfo first memFrees salvageStaticInfo; no such free here or in nisDelete. Old name not shown wrong; ICF fold/direct call possible; high unsupported. |

## Removals (0)

No audited set upheld a removal (quality-recheck proposed none; its four entries were renames). For the record, `ghidra/scripts/ImportSymbolsJson.py` cannot reset a name: an entry with an empty `name` is reported `bad-address-or-name` and skipped, and `set_function_name` only renames. A future removal must be done by the apply stage with a post-script, e.g. `func.setName(None, SourceType.DEFAULT)`, which gives the function back its `FUN_*` name.

## CET differs (0)

None. No emitted name, rename, prototype or deferred entry sits on a `cet_symbols.json` address, and no round-4 name reuses a CET name. `cet_symbols.json` is unchanged.

## Cross-set conflicts (17)

| address | proposals (set: name, confidence) | chosen | reason |
|---|---|---|---|
| 0x00513920 | ui-render: `cat_salvageTargetAllowed` low; ai-ships-sim: `cat_uiShipIsSalvageable` low; titan-libs: `hw1_ShipSelect_00513920` low | `cat_salvageTargetAllowed` | same reading (mission 10 type 0x6e exception, else cat_salvageCheckTargetReasons); equal low, tie-break precision 1.0 |
| 0x005AD540 | ai-ships-sim: `cat_acolyteLinkedAttack` low; titan-libs: `cat_acolyteGroupAttack` low | `cat_acolyteLinkedAttack` | same reading in both (Acolytes 1/0x22 of the linked group +0x370 flag 1 sharing partner +0x688 -> clAttack); equal low confidence; tie-break audit precision 0.933 vs 0.931 and consistent with its ACV twin 0x5ae720 |
| 0x005AE1D0 | ai-ships-sim: `cat_acvCollectEMPTargets` low; titan-libs: `cat_acvGroupSelectLinkable` low | `cat_acvCollectEMPTargets` | body adds the ship or its linked ACVs (upgrade 0x200) to a GrowSelection and returns the member nearest the centroid; callers ACVSpecialActivate/ACVHouseKeep; equal low confidence, tie-break precision, keeps the cat_acv* family of one set |
| 0x005AE720 | ai-ships-sim: `cat_acvLinkedAttack` low; titan-libs: `cat_acvGroupAttack` low | `cat_acvLinkedAttack` | twin of 0x5ad540 for ACVs 4/0x26 (state 3, shared target +0x6f4 -> clAttack); equal low confidence, tie-break precision and consistency with 0x5ad540 |
| 0x005AF460 | ai-ships-sim: `cat_carrierCountSupportModules` medium; titan-libs: `cat_shipCountCarrierSupportModules` medium | `cat_carrierCountSupportModules` | both read the body the same (count shiptype 0xd not flagged +0x1cc&8); equal medium, tie-break precision 0.933 vs 0.931 |
| 0x005AF4C0 | ai-ships-sim: `cat_moduleShipGetStatics` medium; titan-libs: `cat_moduleShipTypeDataGet` low | `cat_moduleShipGetStatics` | switch on module shiptype returning per-type static blocks 0xa762xx; medium beats low |
| 0x005AFD70 | ai-ships-sim: `cat_moduleIterate` low; titan-libs: `cat_shipModuleIterate` low | `cat_moduleIterate` | same function, naming variant; equal low; tie-break precision, keeps the cat_module* family (cat_moduleShipGetStatics, cat_moduleShipEject) |
| 0x005AFED0 | ai-ships-sim: `cat_moduleIterFirst` low; titan-libs: `cat_shipModuleIterFirst` low | `cat_moduleIterFirst` | same function, naming variant; equal low; follows 0x5afd70 |
| 0x005AFEE0 | ai-ships-sim: `cat_moduleIterNext` low; titan-libs: `cat_shipModuleIterNext` low | `cat_moduleIterNext` | same function, naming variant; equal low; follows 0x5afd70 |
| 0x005B9FF0 | ui-render: `cat_mimicItemCount` low; ai-ships-sim: `cat_spGetMenuItemIndex` low | `cat_mimicItemCount` | returns 0xa77ab0[p] (the counter cat_mimicItemAdd bumps) or 0xa762e8[p]; callers are the popup-menu functions 0x4eac30/0x4ed8b0 that build the Mimic submenus; equal low, ui-render precision 1.0 |
| 0x005BA020 | ui-render: `cat_mimicItemGet` medium; ai-ships-sim: `cat_spGetMenuItemEntry` low | `cat_mimicItemGet` | medium beats low; same Mimic item list |
| 0x005BA0D0 | ui-render: `cat_mimicItemAdd` medium; ai-ships-sim: `cat_spSetCurrentMimicItem` medium | `cat_mimicItemAdd` | body appends param_2 to 0xa76310[player] and bumps 0xa77ab0[player] (item 8 resets fixed entries); only caller kasfSetCurrentMimicItem; equal medium, ui-render precision 1.0 |
| 0x005C5B20 | ui-render: `cat_sentinelFieldSegmentAdd` low; ai-ships-sim: `cat_sentinelFieldQueueAdd` low | `cat_sentinelFieldSegmentAdd` | same function; equal low, ui-render precision 1.0 (tie-break), keeps one naming family for the 0x5c5b20-0x5c67b0 field renderer |
| 0x005C5C20 | ui-render: `cat_sentinelFieldsRenderBehind` low; ai-ships-sim: `cat_sentinelFieldRenderNear` low | `cat_sentinelFieldsRenderBehind` | body renders queued 0xa78d18 segments farther than the object (list sorted by distance +0x30); equal low, tie-break precision |
| 0x005C5CB0 | ui-render: `cat_sentinelFieldsRenderRest` low; ai-ships-sim: `cat_sentinelFieldRenderAll` low | `cat_sentinelFieldsRenderRest` | body renders all remaining queued segments then resets list/pool; equal low, tie-break precision |
| 0x005C60A0 | ui-render: `cat_sentinelFieldSegmentDraw` low; ai-ships-sim: `cat_sentinelFieldRender` low | `cat_sentinelFieldSegmentDraw` | per-segment draw called from both render loops; equal low, tie-break precision |
| 0x005C67B0 | ui-render: `cat_sentinelFieldQueue` low; ai-ships-sim: `cat_sentinelFieldQueueFormation` low | `cat_sentinelFieldQueue` | same function; equal low, tie-break precision |

## Placeholders superseded (48)

| address | titan-libs fallback (rejected) | name applied |
|---|---|---|
| 0x00412E10 | `hw1_AIResourceMan_00412e10` | `airDumbResourcerRequests` |
| 0x00413070 | `hw1_AIResourceMan_00413070` | `airDumbResearchRequests` |
| 0x00511590 | `cat_new_00511590` | `cat_shipPrintDragStart` |
| 0x00511640 | `cat_new_00511640` | `cat_shipPrintDragEnd` |
| 0x00511690 | `cat_new_00511690` | `cat_shipPrintDragUpdate` |
| 0x00511710 | `cat_new_00511710` | `cat_shipPrintIsDragging` |
| 0x00511CE0 | `hw1_ShipSelect_00511ce0` | `cat_selMakeShipsScuttleable` |
| 0x00511DB0 | `hw1_ShipSelect_00511db0` | `makeShipsRetireable` |
| 0x00511DF0 | `hw1_ShipSelect_00511df0` | `cat_selMakeShipsNotLinkedACVs` |
| 0x00511E50 | `hw1_ShipSelect_00511e50` | `cat_selMakeShipsSameOwnerAsFirst` |
| 0x00511F00 | `hw1_ShipSelect_00511f00` | `cat_selMakeShipsFormationLeaders` |
| 0x00512330 | `hw1_ShipSelect_00512330` | `cat_selMakeShipsHaveAbilityUpgrade` |
| 0x005123A0 | `hw1_ShipSelect_005123a0` | `cat_selMakeShipsLeechCapable` |
| 0x00512410 | `hw1_ShipSelect_00512410` | `cat_selMakeShipsRammingFrigates` |
| 0x00512470 | `hw1_ShipSelect_00512470` | `cat_selMakeShipsRepairCapable` |
| 0x00512530 | `hw1_ShipSelect_00512530` | `cat_selMakeShipsAcolyteLinkFire` |
| 0x00512580 | `hw1_ShipSelect_00512580` | `cat_selMakeShipsNotBigShips` |
| 0x005125F0 | `hw1_ShipSelect_005125f0` | `cat_selMakeShipsNotBusy` |
| 0x00512650 | `hw1_ShipSelect_00512650` | `cat_selMakeShipsLeeching` |
| 0x00512780 | `hw1_ShipSelect_00512780` | `makeShipsFormationCapable` |
| 0x00512B50 | `hw1_ShipSelect_00512b50` | `cat_selMakeTargetsAttackable` |
| 0x00512C40 | `hw1_ShipSelect_00512c40` | `cat_selMakeTargetsBeastAttackable` |
| 0x005133E0 | `hw1_ShipSelect_005133e0` | `cat_growSelectNullShip` |
| 0x005135C0 | `hw1_ShipSelect_005135c0` | `cat_shipCanBeHalted` |
| 0x005136A0 | `hw1_ShipSelect_005136a0` | `cat_shipCanLeech` |
| 0x005136E0 | `hw1_ShipSelect_005136e0` | `cat_shipCanUnleech` |
| 0x00513710 | `hw1_ShipSelect_00513710` | `cat_selMakeShipsDockCapableFor` |
| 0x00513800 | `hw1_ShipSelect_00513800` | `cat_shipTypeCanDockFor` |
| 0x005138B0 | `hw1_ShipSelect_005138b0` | `cat_shipCanRepair` |
| 0x005138F0 | `hw1_ShipSelect_005138f0` | `cat_shipCanSalvage` |
| 0x005139B0 | `hw1_ShipSelect_005139b0` | `cat_selSelectionCopy` |
| 0x00513A00 | `hw1_ShipSelect_00513a00` | `cat_selComputeTargetActions` |
| 0x00515940 | `cat_new_00515940` | `cat_blastSpawnEffect` |
| 0x00515B40 | `cat_new_00515b40` | `cat_blastCreateSlipgateExplosion` |
| 0x00515BD0 | `cat_new_00515bd0` | `cat_blastCreateBigGunExplosion` |
| 0x00515D30 | `cat_new_00515d30` | `cat_blastAffectShip` |
| 0x00516370 | `cat_new_00516370` | `cat_blastInit` |
| 0x00516390 | `cat_new_00516390` | `cat_blastReset` |
| 0x00516A40 | `cat_new_00516a40` | `cat_dreadnoughtChargeEffect` |
| 0x00516BB0 | `cat_new_00516bb0` | `cat_nomadMoonBeamEffect` |
| 0x00516D40 | `cat_new_00516d40` | `cat_bigGunBulletsInFlight` |
| 0x00516D60 | `cat_new_00516d60` | `cat_bigGunBulletRebuildList` |
| 0x00516DA0 | `cat_new_00516da0` | `cat_blastAffectObject` |
| 0x00516F20 | `cat_new_00516f20` | `cat_bigGunBulletTrack` |
| 0x00516F50 | `cat_new_00516f50` | `cat_bigGunBulletUntrack` |
| 0x0054EFF0 | `cat_new_0054eff0` | `cat_kbLeechCB` |
| 0x0054F0D0 | `cat_new_0054f0d0` | `cat_kbMimicShipsCB` |
| 0x0054F130 | `cat_new_0054f130` | `cat_kbMimicPlayersCB` |

## Naming-set signatures dropped (9; names kept)

| address | name | set | proposed | reason |
|---|---|---|---|---|
| 0x004F4130 | `cat_rmTechSetOr` | ai-ships-sim | `void cat_rmTechSetOr(sdword *out, sdword *a, sdword *b)` | sig-dropped:every caller cleans more ([24]) than 12 arg bytes |
| 0x004F7AB0 | `cat_salvageBeamEffect` | ai-ships-sim | `void cat_salvageBeamEffect(Ship *salvager)` | sig-dropped:every caller cleans more ([8]) than 4 arg bytes |
| 0x004F86E0 | `cat_salvageAlignToVelocity` | ai-ships-sim | `void cat_salvageAlignToVelocity(Ship *ship)` | sig-dropped:every caller cleans more ([28]) than 4 arg bytes |
| 0x004F90E0 | `cat_salvageStaticInit` | ai-ships-sim | `void cat_salvageStaticInit(char *directory, char *field, ShipStaticInfo *statinfo)` | sig-dropped:every caller cleans more ([28]) than 12 arg bytes |
| 0x00516370 | `cat_blastInit` | ai-ships-sim | `void cat_blastInit(void)` | sig-dropped:no RET in body and r4a cc __stdcall |
| 0x00516BB0 | `cat_nomadMoonBeamEffect` | ai-ships-sim | `void cat_nomadMoonBeamEffect(vector *pos, real32 frac)` | sig-dropped:every caller cleans more ([16]) than 8 arg bytes |
| 0x0051C980 | `cat_soundLevelReset` | ai-ships-sim | `void cat_soundLevelReset(void)` | sig-dropped:no RET in body and r4a cc __stdcall |
| 0x00524EB0 | `statsNumShipsNeededToKillFleet` | ai-ships-sim | `sdword statsNumShipsNeededToKillFleet(ShipStaticInfo *shipstatic)` | sig-dropped:every caller cleans more ([8, 20]) than 4 arg bytes |
| 0x00585760 | `trTextureRegister` | ui-render | `trhandle trTextureRegister(char *fileName, trcolorinfo *info, void *meshReference)` | sig-dropped:every caller cleans more ([16, 20]) than 12 arg bytes |

Signature variants among agreeing sets (same name): 0x00420A20 `allianceSetAlliance`: ui-render `void allianceSetAlliance(udword AllianceType, uword playerone, uword playertwo)` vs ai-ships-sim `void allianceSetAlliance(udword AllianceType, uword curalliance, uword newalliance)`. Only parameter names differ; the higher-precision set (ui-render) was used.

## Prototypes dropped (13)

| address | name | reason |
|---|---|---|
| 0x00404040 | `aieHandlerSetTeamDied` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): (merged?) |
| 0x00412350 | `NumberToAIPlayer` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): push >=3 args > 1 |
| 0x00436AA0 | `ellipsoid_seq` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): (merged?) |
| 0x00438090 | `cloudRenderLightning` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): (merged?) |
| 0x00460ED0 | `cratesRewardPlayer` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): push >=5 args > 2 |
| 0x004CADB0 | `nebRenderNebula` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): push >=2 args > 1 |
| 0x004EA200 | `psCrossFadeLinkSet` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): beyond 12 bytes |
| 0x005511E0 | `volFindRadius` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): push >=4 args > 1 |
| 0x0050B380 | `cat_smMinesDraw` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): push >=3 args > 2 |
| 0x00514080 | `cat_shipUpgradeFlagsTime` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): push >=5 args > 2 |
| 0x00514620 | `cat_shipGetUpgradeTechs` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): push >=4 args > 2 |
| 0x005B4020 | `cat_shipSetSUExempt` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): (merged?) |
| 0x005BD670 | `cat_mothershipModuleSlotGet` | low + producer flagged an arity/cleanup mismatch (audit caveat: both sampled such entries were wrong): push >=6 args > 2 |

Flagged but kept: 0x0059B1F0 `TitanInterface_HandleRoutingRegister` RET [0, 4] incl. plain ret (SEH funclet?) vs 4 stack bytes (the producer documents this one as an SEH funclet plain ret; the stack-arg RET matches).

## Deferred to round 5 (54)

These are audited names at code addresses that are not function entries in r4a (callback assignments and table-referenced code found in gaps). None is inside an existing function body. They are written to `ghidra/proposals/r4/reconcile_deferred_functions.json` in the round-3 deferred format (address, name, reason, class, confidence, evidence, hw1_equivalent, signature, inside_function) and reserve their names. Round 5 needs to create these functions, the same way C1 did with `create_functions_r4.json`.

| address | name | set | confidence |
|---|---|---|---|
| 0x004011d0 | `aiaPriorityShipsConstraints` | ai-ships-sim | medium |
| 0x00405f70 | `aihHarassFiringSingleShipHandler` | ai-ships-sim | high |
| 0x00406650 | `aihPatrolEnemyNearbyHandler` | ai-ships-sim | high |
| 0x004066d0 | `aihGravWellEnemyNearbyHandler` | ai-ships-sim | high |
| 0x00406880 | `aihFastDefenseDistressHandler` | ai-ships-sim | high |
| 0x00406af0 | `aihSlowDefenseDistressHandler` | ai-ships-sim | high |
| 0x0040c590 | `aimPreFix_PatrolMove` | ai-ships-sim | high |
| 0x00411bb0 | `SaveRequestShipsCB` | ai-ships-sim | high |
| 0x00411bf0 | `SaveTeamWaitingCB` | ai-ships-sim | high |
| 0x00411c50 | `LoadRequestShipsCB` | ai-ships-sim | high |
| 0x00411c80 | `LoadTeamWaitingCB` | ai-ships-sim | high |
| 0x00411ca0 | `FixTeamWaitingCB` | ai-ships-sim | high |
| 0x00413b70 | `scriptAIShipPrecalculate` | ai-ships-sim | high |
| 0x00417990 | `aitTeamIsntCoopWithAnotherTeam_GravWell` | ai-ships-sim | high |
| 0x0041aa70 | `aiuShipIsFighter` | ai-ships-sim | high |
| 0x0041aa90 | `aiuShipNotGoodAgainstFighters` | ai-ships-sim | high |
| 0x004d7200 | `opHighlightBabyFunction` | ui-render | high |
| 0x004d96f0 | `opMusicVolumeProcess` | ui-render | high |
| 0x004d9770 | `opSFXVolumeProcess` | ui-render | high |
| 0x004d97f0 | `opSpeechVolumeProcess` | ui-render | high |
| 0x004d98c0 | `opMouseSensitivityProcess` | ui-render | high |
| 0x004d98e0 | `opBattleChatterProcess` | ui-render | high |
| 0x004d9900 | `opNumEffectsProcess` | ui-render | high |
| 0x004d9990 | `opEqualizerProcess` | ui-render | high |
| 0x004d9b20 | `opDetailThresholdProcess` | ui-render | high |
| 0x004da690 | `opBrightnessProcess` | ui-render | high |
| 0x004db6d0 | `cat_opTextureDetailProcess` | ui-render | medium |
| 0x004f1cf0 | `DefaultShipHarvestsResource` | ai-ships-sim | high |
| 0x004f1d40 | `R1ResourcerHarvestsAsteroid` | ai-ships-sim | high |
| 0x00501ad0 | `DerelictListAddCB` | ai-ships-sim | high |
| 0x00501af0 | `ImpactableListAddCB` | ai-ships-sim | high |
| 0x00501b10 | `MissileListAddCB` | ai-ships-sim | high |
| 0x00503da0 | `spScenarioItemDraw` | ai-ships-sim | high |
| 0x005040a0 | `compareScenariosCB` | ai-ships-sim | high |
| 0x005170b0 | `cat_spWarpShipsSetCB` | ai-ships-sim | high |
| 0x00517130 | `cat_spWarpResourcesSetCB` | ai-ships-sim | high |
| 0x00517180 | `cat_spWarpTechSetCB` | ai-ships-sim | high |
| 0x0051b430 | `singlePlayerNisStoppedCB` | ai-ships-sim | high |
| 0x00524b10 | `statShipConstraintsFrigatesOrBetterCB` | ai-ships-sim | high |
| 0x00524b30 | `statShipConstraintsFightingShipsCB` | ai-ships-sim | medium |
| 0x00524bd0 | `statShipConstraintsCarrierFightingShipsCB` | ai-ships-sim | medium |
| 0x00534ea0 | `uicScrollBarProcess` | ui-render | high |
| 0x00535160 | `uicScrollBarDraw` | ui-render | high |
| 0x005351b0 | `uicScrollBarButtonDraw` | ui-render | high |
| 0x00535290 | `uicListWindowDraw` | ui-render | high |
| 0x00536c10 | `uicTextEntryProcess` | ui-render | high |
| 0x00537af0 | `uicTextEntryDraw` | ui-render | high |
| 0x00538eb0 | `uicTabProcess` | ui-render | high |
| 0x00538fc0 | `uicRightArrowProcess` | ui-render | high |
| 0x00539230 | `uicLeftArrowProcess` | ui-render | high |
| 0x00555d70 | `dbwWindowProc` | ui-render | high |
| 0x00561f10 | `WindowProc` | ui-render | high |
| 0x00567930 | `mrRegionProcess` | ui-render | high |
| 0x00569260 | `mrRegionDraw` | ui-render | high |

