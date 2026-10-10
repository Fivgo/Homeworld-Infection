# Round-5 symbol reconcile report

Output: `ghidra/symbols_round5.json` (cet_symbols.json schema). It holds **1106 symbols**: **457 new function names**, **25 renames** (8 of real applied names, 17 of class-only placeholders), **18 confidence raises**, **9 data names** and **597 prototype-only entries** (current name repeated, `type` = signature). It also carries a top-level `removals` list with **2 removals** (off-cut function objects), which ImportSymbolsJson does not read (see Removals). Generated 2026-10-09.

Inputs:
- Applied: `ghidra/symbols_round1.json`, `symbols_round1_data.json`, `symbols_round2.json`, `symbols_round3.json`, `symbols_round4.json`, and the created-function lists `create_functions_r3.json` (105), `create_functions_r4.json` (186), `create_functions_r5.json` (54, D1).
- Audited sets in `ghidra/proposals/r5/`: `ui-screens` (157 kept, precision 0.9667), `render-combat` (142 kept, 0.9333), `bands-files` (317 kept, 0.9333), `lowconf-r4` (38 kept, 0.9667), `prototypes-r5` (631 kept, 0.9).
- Validation export: `ghidra/exports/cataclysm_gog_r5a` (14,692 functions; functions.csv, symbols.csv, decompiled.c, datatypes.h, program.json memory blocks).
- Binary checks: the `objdump -d -M intel` disassembly of `original/game/cataclysm.exe` used in rounds 3-4 (same bytes). It gives the RET imm of every body and the caller stack cleanup after each direct call. The cleanup table was rebuilt against the r5a function starts.
- Scripts (scratch, not committed): `/tmp/r5rec/reconcile.py`, `validate.py`, `mkreport.py` (the round-4 reconcile adapted: rename, raise and removal handling added).

## D1 check

All 54 names in `create_functions_r5.json` are function entries with exactly that name in r5a `functions.csv`, so D1 applied them. They are not re-emitted, and they count as taken names in the uniqueness check. The 42 one-byte `.data` artefacts D1 removed have no proposal. r5a also has one function created by D1's guarded analysis (`FUN_00528580`); no set proposed a name for it.
Latest applied function name per address (rounds 1-4 and the created lists) vs r5a: 6 differ (`exception__exception_2` -> `exception::exception__exception_2`, `exception__exception_3` -> `exception::exception__exception_3`, `_threadstart` -> `lpStartAddress_0067a0a0`, `WONMisc__EasySocket__ESGetLastError` -> `WSOCK32.DLL::WONMisc__EasySocket__ESGetLastError`, `WONMisc__EasySocket__GetTickCount` -> `KERNEL32.DLL::WONMisc__EasySocket__GetTickCount`, `_threadstartex` -> `lpStartAddress_0071cdc0`), 0 addresses are not function entries in r5a. These match round 4's note (namespace/thunk spellings and import thunks that predate round 5) and are left as they are.

## Totals

| item | count |
|---|---|
| input `ui-screens.verified.json` | 157 |
| input `render-combat.verified.json` | 142 |
| input `bands-files.verified.json` | 317 |
| input `lowconf-r4.verified.json` | 38 |
| input `prototypes-r5.verified.json` | 631 |
| distinct naming-proposal addresses | 528 |
| cross-set conflicts between two real names, resolved (table below) | 68 |
| **new function names** (all on `FUN_*` entries in r5a) | **457** |
| **renames** of real applied names (all high, audit-sampled correct) | **8** |
| **renames** of class-only placeholders (`hw1_<File>_<addr>`, `cat_new_<addr>`, `lib_<x>_<addr>`) | **17** |
| **removals** (off-cut function objects, top-level `removals` list) | **2** |
| **confidence raises** (same name, higher confidence; notes `confidence-raise=`) | **18** |
| **data names** (.data) | **9** |
| no-op confirmations (same name, same confidence; not emitted) | 2 |
| **deferred** (next CreateFunctions pass): function address not an entry in r5a | **15** |
| dropped names (collision, not an identifier, data outside sections) | 0 |
| rename proposals rejected | 0 |
| naming-set signatures kept (binary-checked; explicit convention added) | 235 |
| naming-set signatures dropped (name kept, `type` empty) | 8 |
| **prototype-only entries** (596 from prototypes-r5 + 1 audit-supplied fix) | **597** |
| prototypes dropped (34 audit-flagged int-for-pointer, 1 superseded by a naming entry) | 35 |
| CET anchors overridden | 0 |
| **symbols written** | **1106** |

New names by confidence: high 140, medium 124, low 193. By class: hw1 130, cat-new 292, won 26, lib 9.

By set (`+` = both sets proposed the same name): bands-files 207, bands-files+render-combat 17, bands-files+ui-screens 24, render-combat 102, render-combat+ui-screens 12, ui-screens 95.

Prototype entries: 597 (high 51, medium 405, low 140, plus 1 audit fix at medium); `__cdecl` 580, `__fastcall` 13, `__stdcall` 3 (prototypes-r5 only).

## Rules as applied

- **Scope.** The file holds only what to apply now: new names on `FUN_*` entries, audit-upheld renames, confidence raises, data names and prototypes. Same-name proposals at the same confidence (`opResItemDraw`, `opRenderItemDraw`) are not emitted.
- **Validity.** A function address must be an entry in r5a `functions.csv`; the rest go to `reconcile_deferred_functions.json`. Data addresses must lie in `.rdata` (0x0072a000-0x00749fff) or `.data` (0x0074a000-0x00c54197) per r5a `program.json`; all 9 do and none has a non-default label yet. Every name matches `^[A-Za-z_][A-Za-z0-9_]*$`.
- **Uniqueness.** Each name was checked against the CET names, rounds 1, 1-data, 2, 3, 4, the three created-function lists, every non-default function and label name in r5a, the other round-5 names and the deferred names (which reserve theirs). A rename releases its old name first (this is how `tutNextButtonClicked` moves from 0x00532A90 to 0x00532A80). No collision occurred, so no `_N` suffix was added and nothing was dropped.
- **Renames.** A real applied name (round 1-4, not a placeholder) is replaced only when the winning proposal is high, its `replaces` equals the r5a name, the address is not a CET anchor, and the audit sampled that address correct: the 7 lowconf-r4 renames (the re-sample's purpose; all 7 were in its 30-address sample, which found only 0x0048F8B0 wrong) and 0x00532A90 (sampled correct by the ui-screens audit and re-checked here in the disassembly). Class-only placeholders from round 4 are filled at any confidence, because they carry no claim to keep.
- **Cross-set conflicts.** Rules in order: (1) evidence read in this reconcile (2 cases); (2) higher confidence; (3) audit precision (ui-screens = lowconf-r4 0.9667 > render-combat = bands-files 0.9333); (4) for render-combat vs bands-files at equal confidence, the subsystem producer (render-combat) over the residual band sweep, which also keeps one naming family per cluster (the `cat_shipDissolve*`, `cat_gun*Sound*`, `cat_collSentinelField*` runs). A merged entry takes the highest confidence among agreeing sets; only the winning name's signature is used.
- **Signatures on naming proposals** go through the round-3/4 binary check: RET imm decides `__cdecl`/`__stdcall`, caller cleanup must match the argument bytes (dropped when 2+ sites all clean more, or all sites clean less), Ghidra's parameter count may not exceed the declared count, and `__fastcall(pThis, edx_unused, ...)` must have RET = 4*(n-2). Types outside `/Cataclysm` (r5a `datatypes.h`) become `void *` for pointers; an unknown by-value type drops the signature.
- **Prototypes.** Same name as in r5a, parseable, explicit convention, no `__thiscall`, `__cdecl` only with RET 0, `__stdcall` RET = argument bytes, `__fastcall` RET = 4*(n-2). The prototypes-r5 audit (precision 0.9) found that all 3 wrong samples had the same defect, an int-typed (`sdword`/`udword`) parameter that the body dereferences as a pointer, and listed 34 more kept entries with that pattern (24 low, 10 medium). Those 34 are dropped here, not applied "with care". A caller-cleanup difference on a prototype is only flagged (MSVC often merges the `add esp` of consecutive calls), not dropped (list below).
- **Confidence raises.** A naming proposal with the applied name and a higher confidence than the applied entry is emitted again with `confidence-raise=<old>-><new>` in its notes. ImportSymbolsJson appends the new plate line and applies the checked signature. The name does not change.
- **Notes.** `notes` starts `class=<class>; confidence=<c>; sets=<sets>`, then optional `resolved=cross-set (rejected: ...)`, `renames=<old>; why=...`, `confidence-raise=...`, `sig-checked:...` / `sig-dropped:...` (`; types:X->void*`), then `evidence=<=200 chars`. Prototype entries read `sets=prototypes-r5; prototype-only (name unchanged)`.
- **Apply order.** ImportSymbolsJson sorts functions ascending, so 0x00532A80 gets `tutNextButtonClicked` before 0x00532A90 gives it up. Ghidra allows a duplicate function name for that moment, and the name is unique once both entries are applied. The apply stage should run `DropStaleComments.py` for the 25 renames (list below). ImportSymbolsJson only appends, so the old `[CET] <old name> ...` plate line would otherwise stay.

## Renames (25)

### Real applied names (8, all high)

| address | old name (applied) | new name | set | reason | signature applied |
|---|---|---|---|---|---|
| 0x004F4790 | `cat_rmShipTypeToModuleSlot` (low) | `cat_rmBeastPrintIndexToShipType` | lowconf-r4 | Old name wrong: maps 10..0x1d to Beast ShipTypes 33..59 (bRecon..bMissileCorvette, CET ShipDefs); result is compared with ship->shiptype and indexes player+0x62b0[shiptype] | `ShipType __cdecl cat_rmBeastPrintIndexToShipType(sdword index)` |
| 0x004F8F60 | `cat_salvageShipReleaseTarget` (low) | `cat_shipIsSalvager` | lowconf-r4 | Old name wrong: pure predicate, no side effects. Bytes: shiptype 14/49/72/89/97 -> mov eax,1; 102 trJunkYardDawg -> debug msg 0x753660 + 0; else 0. Callers test EAX (extraout_EAX) | `bool32 __cdecl cat_shipIsSalvager(Ship *ship)` |
| 0x004FA050 | `cat_salvageReleaseClampedShips` (low) | `cat_salvageDeliverToDepot` | lowconf-r4 | Old name wrong (nothing released): puts carried ship in a free slot of depot's 16x0x14 transfer table, ETA via twSalTransSpeed/MaxTime, sets carried+0x3ec=depot, disable reason 2 on its salvagers | `void __cdecl cat_salvageDeliverToDepot(Ship *salvager, Ship *depot)` |
| 0x00511F00 | `cat_selMakeShipsFormationLeaders` (low) | `cat_selMakeShipsProtectCapable` | lowconf-r4 | Old name wrong: no formation/leader test; drops ships whose command->ordertype.order (+0x14, as in makeShipsControllable) is 4/6/10/11-17; all 3 callers run it on key 0x4a before clWrapProtect | `void __cdecl cat_selMakeShipsProtectCapable(SelectCommand *selection, bool32 check)` |
| 0x00513800 | `cat_shipTypeCanDockFor` (low) | `cat_shipClassCanUseSlipgate` | lowconf-r4 | Old name wrong on its key argument: mrDockingOrders passes staticinfo->shipclass (+0x1e8), not a ShipType; 2nd arg is the slipgate's shiptype 0x70-0x72 (Small/Medium/Large) | `bool32 __cdecl cat_shipClassCanUseSlipgate(ShipClass shipclass, ShipType slipgateType)` |
| 0x00532A90 | `tutNextButtonClicked` (high) | `cat_tutNextTextButtonClicked` | ui-screens | verified in the disassembly: kasfTutNextButtonClicked 0x496600 = "jmp 0x532a80" and kasfTutNextTextButtonClicked 0x496630 = "jmp 0x532a90"; 0x532a90 returns and clears 0xa6c4f8 (NextText flag), 0x532a80 clears 0xa6c4ec. Round-1 name was on the wrong one of two twin getters; ui-screens audit sampled it correct | `sdword __cdecl cat_tutNextTextButtonClicked(void)` |
| 0x005A0BB0 | `std__multimap_Profile2Reply__lower_bound` (low) | `std__multimap_Profile2Reply__find` | lowconf-r4 | Old name wrong: after the _Lbound walk it returns end() (head) unless node.key <= key, i.e. MSVC find(); lower_bound would return the bound node itself | - |
| 0x005C6E80 | `cat_shipIsSentinelInWall` (low) | `cat_shipIsSentinelInSphere` | lowconf-r4 | Old name wrong: tests formation +0x230 == 10; cat_uiProtectFormation issues 10 around a protected target and cat_uiProtectFormationCancel reverts 10 to SPHERE_FORMATION 5 (9 -> WALL 4) | `bool32 __cdecl cat_shipIsSentinelInSphere(Ship *ship)` |

`tutNextButtonClicked` (round 1, high) is not removed: it moves to 0x00532A80, the function `kasfTutNextButtonClicked` really jumps to (new-name entry there, high).

### Class-only placeholders filled (17)

| address | placeholder (applied r4) | new name | confidence | set(s) |
|---|---|---|---|---|
| 0x00415AF0 | `hw1_AIShip_00415af0` | `cat_shipGetAimPoint` | low | bands-files |
| 0x00511EA0 | `hw1_ShipSelect_00511ea0` | `cat_selMakeShipsHaltable` | medium | bands-files+lowconf-r4 |
| 0x00511F90 | `hw1_ShipSelect_00511f90` | `cat_selMakeShipsMobileNotBusy` | medium | lowconf-r4 |
| 0x00513540 | `hw1_ShipSelect_00513540` | `cat_shipCanMove` | medium | lowconf-r4 |
| 0x00513570 | `hw1_ShipSelect_00513570` | `cat_shipCanDock` | medium | bands-files+lowconf-r4 |
| 0x005135E0 | `hw1_ShipSelect_005135e0` | `cat_shipCanSetTactics` | medium | lowconf-r4 |
| 0x00513620 | `hw1_ShipSelect_00513620` | `cat_shipCanCollectResources` | medium | lowconf-r4 |
| 0x005163B0 | `cat_new_005163b0` | `cat_blastDrawLightning` | medium | lowconf-r4 |
| 0x00516610 | `cat_new_00516610` | `cat_blastRandomPolarAngle` | medium | lowconf-r4 |
| 0x005166B0 | `cat_new_005166b0` | `cat_blastInitRandomTables` | medium | bands-files+lowconf-r4 |
| 0x005A1330 | `lib_msvc_stl_005a1330` | `std__deque_uchar__pop_front` | medium | lowconf-r4 |
| 0x005A14F0 | `lib_msvc_stl_005a14f0` | `std__wstring__max_size` | medium | lowconf-r4 |
| 0x005A3960 | `lib_msvc_stl_005a3960` | `std__deque_uchar__pop_back` | medium | lowconf-r4 |
| 0x005A4400 | `lib_msvc_stl_005a4400` | `std__wstring__deleting_dtor` | medium | lowconf-r4 |
| 0x005AF4A0 | `cat_new_005af4a0` | `cat_carrierVariantStaticInit` | low | bands-files |
| 0x005B3850 | `cat_new_005b3850` | `cat_dreadnoughtVariantStaticInit` | low | bands-files |
| 0x005BE5D0 | `cat_new_005be5d0` | `cat_p1MothershipVariantStaticInit` | low | bands-files |

DropStaleComments items for the apply stage: `drop=0x00415af0:hw1_AIShip_00415af0,0x004f4790:cat_rmShipTypeToModuleSlot,0x004f8f60:cat_salvageShipReleaseTarget,0x004fa050:cat_salvageReleaseClampedShips,0x00511ea0:hw1_ShipSelect_00511ea0,0x00511f00:cat_selMakeShipsFormationLeaders,0x00511f90:hw1_ShipSelect_00511f90,0x00513540:hw1_ShipSelect_00513540,0x00513570:hw1_ShipSelect_00513570,0x005135e0:hw1_ShipSelect_005135e0,0x00513620:hw1_ShipSelect_00513620,0x00513800:cat_shipTypeCanDockFor,0x005163b0:cat_new_005163b0,0x00516610:cat_new_00516610,0x005166b0:cat_new_005166b0,0x00532a90:tutNextButtonClicked,0x005a0bb0:std__multimap_Profile2Reply__lower_bound,0x005a1330:lib_msvc_stl_005a1330,0x005a14f0:lib_msvc_stl_005a14f0,0x005a3960:lib_msvc_stl_005a3960,0x005a4400:lib_msvc_stl_005a4400,0x005af4a0:cat_new_005af4a0,0x005b3850:cat_new_005b3850,0x005be5d0:cat_new_005be5d0,0x005c6e80:cat_shipIsSentinelInWall`

## Removals (2)

Both are empty-name proposals the audits kept: a `FUN_*` entry that is really the middle of another function. Round 4 found that ImportSymbolsJson cannot reset a name, because an empty `name` is reported `bad-address-or-name` and skipped. Worse, an empty `name` falls back to `hw1_equivalent`. So these are NOT in `symbols`: they are in the top-level `removals` list of `symbols_round5.json` with `action: remove-function-merge-into-parent`. The apply stage removes the function object at the address (`removeFunction`, no name involved) and then re-grows the parent body, as the round-3 body fix does (`FixFunctionBodies.py`) or with `CreateFunctionCmd.fixupFunctionBody`. Both off-cuts are verified in the disassembly:

| address | current name | parent | proposed by | reason |
|---|---|---|---|---|
| 0x0043453B | `FUN_0043453b` | `recievedChatPacketCB` @ 0x00434500 | bands-files.verified.json (low) | off-cut: recievedChatPacketCB 0x434500 "jle 0x43453b" (0x43451f) jumps into it, it has no callers and runs on the parent frame (unaff_EBP = packet); the CHAT_PACKET arm of the parent |
| 0x00571611 | `FUN_00571611` | `primBeveledRectSolid` @ 0x005715E0 | render-combat.verified.json (high) | off-cut: primBeveledRectSolid 0x5715e0 "je 0x571611" (0x5715e7, glcActive() false) jumps into it; the GL fallback branch of the parent, no other reference |

No applied name is removed this round. Related, not applied: `FUN_005549aa` is also an off-cut (lib_crt_00554930 does `jmp 0x5549aa` at 0x00554981), but bands-files proposed the class-only name `lib_crt_005549aa` (low) and not a removal. The name is applied as audited. A later body-fix pass should merge it like the two above.

## Confidence raises (18)

All come from lowconf-r4, the re-sample of round 4's low names. The name is unchanged.

| address | name | old -> new | signature (checked) |
|---|---|---|---|
| 0x004EAB40 | `cat_popupMenuSetupSensorsItems` | low -> medium | `void __cdecl cat_popupMenuSetupSensorsItems(void)` |
| 0x00511710 | `cat_shipPrintIsDragging` | low -> medium | `bool32 __cdecl cat_shipPrintIsDragging(void)` |
| 0x00511CE0 | `cat_selMakeShipsScuttleable` | low -> medium | `void __cdecl cat_selMakeShipsScuttleable(SelectCommand *selection, bool32 check)` |
| 0x00513710 | `cat_selMakeShipsDockCapableFor` | low -> medium | `void __cdecl cat_selMakeShipsDockCapableFor(SelectCommand *selection, ShipType dockTargetType, bool32 check)` |
| 0x00516D40 | `cat_bigGunBulletsInFlight` | low -> medium | `bool32 __cdecl cat_bigGunBulletsInFlight(void)` |
| 0x0051FC60 | `cat_soundEventBlast` | low -> medium | `void __cdecl cat_soundEventBlast(void *blast)` |
| 0x00524D70 | `cat_statsRandomAttackShipStatic` | low -> medium | `ShipStaticInfo *__cdecl cat_statsRandomAttackShipStatic(ShipRace race)` |
| 0x00524EB0 | `statsNumShipsNeededToKillFleet` | low -> high | `sdword __cdecl statsNumShipsNeededToKillFleet(ShipStaticInfo *shipstatic, SelectCommand *targetFleet)` |
| 0x0054EFF0 | `cat_kbLeechCB` | low -> medium | `void __cdecl cat_kbLeechCB(void)` |
| 0x0054F0D0 | `cat_kbMimicShipsCB` | low -> medium | `void __cdecl cat_kbMimicShipsCB(void)` |
| 0x0054F130 | `cat_kbMimicPlayersCB` | low -> medium | `void __cdecl cat_kbMimicPlayersCB(void)` |
| 0x005AFD70 | `cat_moduleIterate` | low -> high | `Ship *__cdecl cat_moduleIterate(Ship *host)` |
| 0x005AFED0 | `cat_moduleIterFirst` | low -> high | `Ship *__cdecl cat_moduleIterFirst(Ship *host)` |
| 0x005AFEE0 | `cat_moduleIterNext` | low -> high | `Ship *__cdecl cat_moduleIterNext(void)` |
| 0x005B94E0 | `cat_mimicGetSelectRadius` | low -> high | `real32 __cdecl cat_mimicGetSelectRadius(Ship *ship)` |
| 0x005B9550 | `cat_mimicGetAvoidRadius` | low -> high | `real32 __cdecl cat_mimicGetAvoidRadius(Ship *ship)` |
| 0x005B95C0 | `cat_mimicGetCameraRadius` | low -> high | `real32 __cdecl cat_mimicGetCameraRadius(Ship *ship)` |
| 0x005BD5F0 | `cat_mothershipStartDeathSequence` | low -> high | `void __cdecl cat_mothershipStartDeathSequence(Ship *ship, sdword reason)` |

## CET differs (0)

None. No emitted name, rename, raise, prototype, removal or deferred entry sits on a `cet_symbols.json` address, and no round-5 name reuses a CET name. `cet_symbols.json` is unchanged.

## Data names (9)

| address | name | confidence |
|---|---|---|
| 0x00892A80 | `selSelectionWidthFactor0` | high |
| 0x00892A88 | `selHotKeyNumberColor` | medium |
| 0x00892A90 | `selHotKeyString` | high |
| 0x00892AC8 | `selStatusDraw` | high |
| 0x00A75290 | `mrDrawTactical` | high |
| 0x00A75294 | `mrSaveTactical` | high |
| 0x00C0BAD0 | `selCentrePoint` | high |
| 0x00C0BAE4 | `selAverageSize` | high |
| 0x00C0CAB0 | `selCameraSpace` | high |

## Cross-set conflicts (68)

| address | proposals (set: name, confidence) | chosen | reason |
|---|---|---|---|
| 0x00435F50 | render-combat: `cat_cloakActivate` medium; bands-files: `cat_cloakSpecialActivate` low | `cat_cloakActivate` | confidence medium beats low |
| 0x00436110 | render-combat: `cat_shipIsCloakedFully` medium; bands-files: `cat_shipIsCloakedOrCloaking` low | `cat_shipIsCloakedFully` | confidence medium beats low |
| 0x00436180 | render-combat: `cat_cloakGetChargeFraction` medium; bands-files: `cat_cloakChargeFraction` low | `cat_cloakGetChargeFraction` | confidence medium beats low |
| 0x0043ABF0 | render-combat: `cat_collLineSentinelFieldIntersect` low; bands-files: `cat_collNearestSentinelFieldHit` low | `cat_collLineSentinelFieldIntersect` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x0043ACC0 | render-combat: `cat_collSentinelFieldExempt` low; bands-files: `cat_collSentinelFieldIgnore` low | `cat_collSentinelFieldExempt` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x004601D0 | render-combat: `cat_vecShipLocalToWorld` medium; bands-files: `cat_shipLocalToWorld` low | `cat_vecShipLocalToWorld` | confidence medium beats low |
| 0x00460220 | render-combat: `cat_shipDissolveInit` low; bands-files: `cat_dissolveInit` low | `cat_shipDissolveInit` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x00460250 | render-combat: `cat_shipDissolveIsActive` medium; bands-files: `cat_shipIsDissolving` low | `cat_shipDissolveIsActive` | confidence medium beats low |
| 0x00460270 | render-combat: `cat_shipDissolveComputePlane` low; bands-files: `cat_dissolveClipPlane` low | `cat_shipDissolveComputePlane` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x004603D0 | render-combat: `cat_shipDissolveStart` low; bands-files: `cat_dissolveStart` low | `cat_shipDissolveStart` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x004604A0 | render-combat: `cat_shipDissolveSetDirection` low; bands-files: `cat_dissolveSetSourcePoint` low | `cat_shipDissolveSetDirection` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x004604D0 | render-combat: `cat_shipDissolveFraction` low; bands-files: `cat_dissolveFraction` low | `cat_shipDissolveFraction` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x00460500 | render-combat: `cat_shipDissolveEdgeIntersect` low; bands-files: `cat_rayPlaneIntersect` low | `cat_shipDissolveEdgeIntersect` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x00460AF0 | render-combat: `cat_shipDissolveEffectCreate` low; bands-files: `cat_dissolveEffectCreate` low | `cat_shipDissolveEffectCreate` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x00460C70 | render-combat: `cat_shipDissolveStop` low; bands-files: `cat_dissolveStop` low | `cat_shipDissolveStop` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x0047FB00 | ui-screens: `cat_fowBoundaryDestroy` medium; bands-files: `cat_fowFreeBuffers` low | `cat_fowBoundaryDestroy` | confidence medium beats low |
| 0x0047FCF0 | ui-screens: `cat_fowBoundaryUpdate` medium; bands-files: `cat_fowRenderLayers` low | `cat_fowBoundaryUpdate` | confidence medium beats low |
| 0x004801A0 | ui-screens: `cat_fowCellCornerGet` medium; bands-files: `cat_fowCellCorner` low | `cat_fowCellCornerGet` | confidence medium beats low |
| 0x00480A90 | ui-screens: `cat_fowLineAdd` medium; bands-files: `cat_fowAddEdge` low | `cat_fowLineAdd` | confidence medium beats low |
| 0x00480BE0 | ui-screens: `cat_fowCornerLinesAdd` low; bands-files: `cat_fowAddTriangleEdges` low | `cat_fowCornerLinesAdd` | equal confidence; audit precision ui-screens 0.9667 > bands-files 0.9333 |
| 0x00488970 | ui-screens: `cat_gameStatsValueGet` medium; bands-files: `cat_gameStatsGetValue` low | `cat_gameStatsValueGet` | confidence medium beats low |
| 0x0048A1C0 | render-combat: `cat_gunCanFire` medium; bands-files: `cat_shipGunCanShoot` low | `cat_gunCanFire` | confidence medium beats low |
| 0x0048B790 | render-combat: `cat_gunStaticSetSoundTypes` low; bands-files: `cat_gunStaticSetSoundIndices` low | `cat_gunStaticSetSoundTypes` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x0048B7C0 | render-combat: `cat_gunSoundTypeLookup` low; bands-files: `cat_gunSoundIndex` low | `cat_gunSoundTypeLookup` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x0048B8C0 | render-combat: `cat_gunGetSoundType` medium; bands-files: `cat_gunGetSoundIndex` low | `cat_gunGetSoundType` | confidence medium beats low |
| 0x0048B950 | render-combat: `cat_gunPickMothershipModuleTarget` low; bands-files: `cat_attackPickModuleTarget` low | `cat_gunPickMothershipModuleTarget` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x0049B570 | ui-screens: `cat_kbKeyCharParse` low; bands-files: `cat_kbCharToKey` low | `cat_kbKeyCharParse` | equal confidence; audit precision ui-screens 0.9667 > bands-files 0.9333 |
| 0x0049BF30 | ui-screens: `transTransformCompletely_intrin` low; bands-files: `transTransformCompletely_xmm` medium | `transTransformCompletely_xmm` | confidence medium beats low |
| 0x004E7690 | render-combat: `cat_smHeightCircleColorGet` low; bands-files: `cat_pieMovePointReset` low | `cat_smHeightCircleColorGet` | body returns cat_smZoomFadeColor(0x892c94, 0) and 0x892c94 is the Sensors.c smHeightCircleColor slot; bands-files named it from callers only ("name from callers only") |
| 0x005074F0 | ui-screens: `cat_selStatusUpgradePendingDraw` medium; render-combat: `cat_selStatusDrawUpgradeIndicator` medium | `cat_selStatusUpgradePendingDraw` | equal confidence; audit precision ui-screens 0.9667 > render-combat 0.9333 |
| 0x00507630 | ui-screens: `cat_selStatusIconsDraw` low; render-combat: `cat_selStatusDrawIcons` low | `cat_selStatusIconsDraw` | equal confidence; audit precision ui-screens 0.9667 > render-combat 0.9333 |
| 0x005076A0 | ui-screens: `cat_selStatusHotKeyDraw` medium; render-combat: `cat_selStatusDrawHotKeyNumber` medium | `cat_selStatusHotKeyDraw` | equal confidence; audit precision ui-screens 0.9667 > render-combat 0.9333 |
| 0x00507740 | ui-screens: `cat_selStatusAmmoCountDraw` low; render-combat: `cat_selStatusDrawCountText` low | `cat_selStatusAmmoCountDraw` | equal confidence; audit precision ui-screens 0.9667 > render-combat 0.9333 |
| 0x00507920 | ui-screens: `cat_selStatusStealthDetectDraw` medium; render-combat: `cat_selStatusDrawAllianceDots` low | `cat_selStatusStealthDetectDraw` | confidence medium beats low |
| 0x00507B70 | ui-screens: `cat_selHealthFactorGet` high; render-combat: `cat_shipHealthFraction` medium | `cat_selHealthFactorGet` | confidence high beats medium |
| 0x00507BC0 | ui-screens: `cat_selStatusBarsDraw` medium; render-combat: `cat_selStatusDrawBars` medium | `cat_selStatusBarsDraw` | equal confidence; audit precision ui-screens 0.9667 > render-combat 0.9333 |
| 0x00511F90 | bands-files: `cat_selMakeShipsOrderable` low; lowconf-r4: `cat_selMakeShipsMobileNotBusy` medium | `cat_selMakeShipsMobileNotBusy` | confidence medium beats low |
| 0x00513540 | bands-files: `cat_shipIsOrderable` low; lowconf-r4: `cat_shipCanMove` medium | `cat_shipCanMove` | confidence medium beats low |
| 0x005135E0 | bands-files: `cat_shipCanBeOrderedToMove` low; lowconf-r4: `cat_shipCanSetTactics` medium | `cat_shipCanSetTactics` | confidence medium beats low |
| 0x005163B0 | bands-files: `cat_blastRenderBolts` low; lowconf-r4: `cat_blastDrawLightning` medium | `cat_blastDrawLightning` | confidence medium beats low |
| 0x00516610 | bands-files: `cat_blastRandomScale` low; lowconf-r4: `cat_blastRandomPolarAngle` medium | `cat_blastRandomPolarAngle` | confidence medium beats low |
| 0x005280A0 | ui-screens: `cat_scriptAsteroidPieceParse` medium; bands-files: `cat_scriptAsteroidBreakInfoParse` low | `cat_scriptAsteroidPieceParse` | confidence medium beats low |
| 0x005292F0 | ui-screens: `tacticsCheckGuardConditionsDuringAttack` high; bands-files: `cat_tacticsTargetsAwayFromSelection` low | `tacticsCheckGuardConditionsDuringAttack` | confidence high beats low |
| 0x005294C0 | ui-screens: `cat_tacticsFindResponsibleShip` low; bands-files: `cat_tacticsGetCommandLeader` low | `cat_tacticsFindResponsibleShip` | equal confidence; audit precision ui-screens 0.9667 > bands-files 0.9333 |
| 0x005295C0 | ui-screens: `cat_tacticsMergeAttackTargets` low; bands-files: `cat_tacticsMergeTargets` low | `cat_tacticsMergeAttackTargets` | equal confidence; audit precision ui-screens 0.9667 > bands-files 0.9333 |
| 0x005296F0 | ui-screens: `cat_tacticsRetaliatePassive` medium; bands-files: `cat_tacticsPassiveAttackTargets` low | `cat_tacticsRetaliatePassive` | confidence medium beats low |
| 0x00529830 | ui-screens: `cat_tacticsAveragePosition` medium; bands-files: `cat_selAveragePosition` low | `cat_tacticsAveragePosition` | confidence medium beats low |
| 0x005298C0 | ui-screens: `cat_shipIsMothership` high; bands-files: `cat_shipIsModularMothership` low | `cat_shipIsMothership` | confidence high beats low |
| 0x00529B70 | ui-screens: `cat_tacticsSelectionHasFlag80Ship` low; bands-files: `cat_tacticsSelectionHasSpecial80` low | `cat_tacticsSelectionHasFlag80Ship` | equal confidence; audit precision ui-screens 0.9667 > bands-files 0.9333 |
| 0x00529C20 | ui-screens: `cat_tacticsShipAttacked` medium; bands-files: `cat_tacticsDelegateAttackViaLeader` low | `cat_tacticsShipAttacked` | confidence medium beats low |
| 0x00529F00 | ui-screens: `cat_tacticsNotRetreatingToDockCB` low; bands-files: `cat_selShipNotDockingForRepair` low | `cat_tacticsNotRetreatingToDockCB` | equal confidence; audit precision ui-screens 0.9667 > bands-files 0.9333 |
| 0x00529F70 | ui-screens: `cat_tacticsRemoveRetreatingShips` medium; bands-files: `cat_selRemoveShipsDockingForRepair` low | `cat_tacticsRemoveRetreatingShips` | confidence medium beats low |
| 0x00529F90 | ui-screens: `cat_tacticsEvasiveRetreatToDock` medium; bands-files: `cat_shipAutoDockWhenDamaged` low | `cat_tacticsEvasiveRetreatToDock` | confidence medium beats low |
| 0x0052A060 | ui-screens: `cat_shipIsUntargetable` low; bands-files: `cat_shipIsSlipgateClass` low | `cat_shipIsUntargetable` | equal confidence; audit precision ui-screens 0.9667 > bands-files 0.9333 |
| 0x0052A080 | ui-screens: `cat_shipIsTargetableBy` medium; bands-files: `cat_shipIsValidTarget` low | `cat_shipIsTargetableBy` | confidence medium beats low |
| 0x0054D6A0 | ui-screens: `cat_kbCommandsCanShareKey` medium; bands-files: `cat_kbKeysConflict` low | `cat_kbCommandsCanShareKey` | confidence medium beats low |
| 0x0054E940 | ui-screens: `cat_kbCommandNameGet` medium; bands-files: `cat_kbCommandName` low | `cat_kbCommandNameGet` | confidence medium beats low |
| 0x00561E40 | ui-screens: `mainReinitRGL` high; bands-files: `cat_mainReinitRGL` low | `mainReinitRGL` | confidence high beats low |
| 0x00564780 | ui-screens: `cat_uiReleaseHiveSwarm` medium; bands-files: `cat_uiSpecialLaunchSwarm` low | `cat_uiReleaseHiveSwarm` | confidence medium beats low |
| 0x00564890 | ui-screens: `cat_uiCloakShips` medium; bands-files: `cat_uiSpecialCloakOn` low | `cat_uiCloakShips` | confidence medium beats low |
| 0x00564920 | ui-screens: `cat_uiDecloakShips` medium; bands-files: `cat_uiSpecialCloakOff` low | `cat_uiDecloakShips` | confidence medium beats low |
| 0x005663F0 | ui-screens: `cat_uiFireDreadnoughtCannon` medium; bands-files: `cat_uiSpecialUpgradeActivate` low | `cat_uiFireDreadnoughtCannon` | confidence medium beats low |
| 0x0056A080 | ui-screens: `cat_mrTacticalOverlayGet` medium; bands-files: `cat_mrGetSelectState` low | `cat_mrTacticalOverlayGet` | confidence medium beats low |
| 0x005732F0 | ui-screens: `WONCommon__RegKey__RegKey` medium; render-combat: `WONCommon__RegKey__OpenNewKey_2` medium | `WONCommon__RegKey__RegKey` | r5a body: SEH frame, stores vtable 0x72be20 at [this], zeroes members +8..+0x28, opens/creates the key and returns this = the RegKey constructor; OpenNewKey is a separate CET-named method at 0x006e3410 (render-combat called it a copy) |
| 0x005C5EA0 | render-combat: `cat_sentinelFieldChargeFraction` low; bands-files: `cat_sentinelChargeFraction` low | `cat_sentinelFieldChargeFraction` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x005C6E50 | render-combat: `cat_shipIsSentinelFieldActive` medium; bands-files: `cat_sentinelFieldIsOn` low | `cat_shipIsSentinelFieldActive` | confidence medium beats low |
| 0x005C81E0 | render-combat: `cat_shipPointOutsideBoxDistance` low; bands-files: `cat_workerHullPointFromTarget` low | `cat_shipPointOutsideBoxDistance` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |
| 0x005C9010 | render-combat: `cat_workerTurnTowardTarget` low; bands-files: `cat_workerFaceTarget` low | `cat_workerTurnTowardTarget` | equal confidence and precision; subsystem producer render-combat over the band sweep bands-files (keeps one naming family per cluster) |

## Deferred to the next CreateFunctions pass (15)

These are audited names at code addresses that are not function entries in r5a: button process/draw callbacks passed to `regFunctionSet`/`regDrawFunctionSet`, the colour-picker process callbacks, and two `selStatusDraw[]` table slots. None of them is inside an existing function body. They are written to `ghidra/proposals/r5/reconcile_deferred_functions.json` (address, name, reason, class, confidence, evidence, hw1_equivalent, signature, inside_function) and reserve their names. They need a CreateFunctions pass like D1's.

| address | name | set | confidence |
|---|---|---|---|
| 0x0043e690 | `cpValueProcess` | ui-screens | medium |
| 0x0043e8f0 | `cpHueSaturationProcess` | ui-screens | medium |
| 0x00508480 | `selStatusDraw3` | render-combat+ui-screens | high |
| 0x00508590 | `selStatusDraw4` | render-combat+ui-screens | high |
| 0x00532750 | `tutProcessNextButton` | ui-screens | medium |
| 0x00532780 | `cat_tutProcessNextTextButton` | ui-screens | medium |
| 0x005327b0 | `cat_tutProcessPrevButton` | ui-screens | medium |
| 0x005327e0 | `cat_tutProcessQuitButton` | ui-screens | medium |
| 0x00532820 | `tutDrawNextButtonFunction` | ui-screens | medium |
| 0x005328c0 | `cat_tutDrawNextTextButtonFunction` | ui-screens | medium |
| 0x00532940 | `cat_tutDrawPrevButtonFunction` | ui-screens | medium |
| 0x005329e0 | `cat_tutDrawQuitButtonFunction` | ui-screens | medium |
| 0x00532c90 | `tutProcessBackButton` | ui-screens | medium |
| 0x00532d80 | `tutDrawBackButtonFunction` | ui-screens | medium |
| 0x00533080 | `tutDrawImageFunction` | ui-screens | medium |

## Naming-set signatures dropped (8; names kept)

| address | name | set | proposed | reason |
|---|---|---|---|---|
| 0x00480650 | `cat_fowBoundaryDrawList` | ui-screens | `void cat_fowBoundaryDrawList(LinkedList * faceList)` | sig-dropped:every caller cleans more ([8]) than 4 arg bytes |
| 0x00489470 | `cat_gameStatsButtonDraw` | ui-screens | `void cat_gameStatsButtonDraw(rectangle * rect, char * label, sdword hotkeyIndex)` | sig-dropped:r5a uses 12 arg dwords > 3 declared |
| 0x0048A2F0 | `gunCanShoot` | render-combat | `bool32 gunCanShoot(Ship * ship, Gun * gun)` | sig-dropped:every caller cleans more ([12]) than 8 arg bytes |
| 0x0050DB50 | `shGammaGet` | render-combat | `sdword shGammaGet(void)` | sig-dropped:no RET in body and r5a cc __stdcall |
| 0x0050DC60 | `shUpdateLighting` | render-combat | `void shUpdateLighting(void)` | sig-dropped:every caller cleans more ([16]) than 0 arg bytes |
| 0x00529C20 | `cat_tacticsShipAttacked` | ui-screens | `void cat_tacticsShipAttacked(Ship * ship, CommandToDo * command, AttackCommand * attackers)` | sig-dropped:every caller cleans more ([20, 28]) than 12 arg bytes |
| 0x005556E0 | `idct` | render-combat | `int idct(float * spectra, float * samples, float * window, udword size)` | sig-dropped:r5a uses 10 arg dwords > 4 declared |
| 0x00571440 | `primLineThick2` | render-combat | `void primLineThick2(sdword x0, sdword y0, sdword x1, sdword y1, sdword thickness, color c)` | sig-dropped:no RET in body and r5a cc __stdcall |

Signature variants among agreeing sets (same name; the first set's was used): 0x004A3E10 `AnyShipOverlapsAsteroid`: ui-screens `bool32 AnyShipOverlapsAsteroid(vector * position, sdword asteroidtype)` vs bands-files `bool32 AnyShipOverlapsAsteroid(vector * position, AsteroidType asteroidtype)`; 0x004B7240 `mexGetResNozzleStatic`: ui-screens `void * mexGetResNozzleStatic(void * mex)` vs bands-files `ResNozzleStatic * mexGetResNozzleStatic(void * mex)`; 0x004B7330 `mexGetRepairNozzleStatic`: ui-screens `void * mexGetRepairNozzleStatic(void * mex)` vs bands-files `RepairNozzleStatic * mexGetRepairNozzleStatic(void * mex)`; 0x004B7560 `mexGetDockStaticInfo`: ui-screens `void mexGetDockStaticInfo(void * dockStaticInfo, void * mex)` vs bands-files `void mexGetDockStaticInfo(DockStaticInfo * dockstaticinfo, void * mex)`; 0x004B7680 `mexGetNAVLightStaticInfo`: ui-screens `void mexGetNAVLightStaticInfo(void * navLightStaticInfo, void * mex)` vs bands-files `void mexGetNAVLightStaticInfo(NAVLightStaticInfo * navlightstaticinfo, void * mex)`.

## Prototypes dropped (35)

| address | name | reason |
|---|---|---|
| 0x00524EB0 | `statsNumShipsNeededToKillFleet` | a round-5 naming entry (rename or confidence raise) at this address already carries a checked type |
| 0x0042C0C0 | `cat_mcpReset` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x0042C630 | `cat_mcRender` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x0042CC80 | `cat_ccChaseCamFromShip` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x0042CD40 | `cat_ccShipIsLegalToFocusOn` | medium; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x0042E180 | `cat_ccSetDistanceScripted` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x0042E1A0 | `cat_ccZoomScripted` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x004324A0 | `cat_aisShipIsBusy` | medium; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x00434AB0 | `cat_updateClampedObjectNoPhysics` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x00464910 | `cat_dockHasFreeSlotForShip` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x00464B30 | `cat_dockFlyToConeOriginLatch` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x004671D0 | `cat_dockFlyToConeInside` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x004673C0 | `cat_dockFlyToConeOrigin` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x004675C0 | `cat_linkFlyIntoPosition` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x00468F80 | `cat_dockShipIsInsideHost` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x0048BB00 | `cat_gunCreateBullet` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x004B4A60 | `cat_meshObjectRenderEdgeEffect` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x004B5190 | `cat_meshObjectForVerticesInRadius` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x004B7090 | `cat_madAnimationFinished` | medium; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x004EB0B0 | `cat_popupMenuLayout` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x004F4130 | `cat_rmTechSetOr` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x004F90E0 | `cat_salvageStaticInit` | medium; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x0053C4A0 | `cat_univSetShipMaxHealth` | medium; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x00541CD0 | `cat_univShipMeshStateCreate` | medium; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x005420B0 | `cat_univCreateMothershipModules` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x0056F8F0 | `cat_mouseGetContextAction` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x0057BAA0 | `cat_rndLensFlareSourceAdd` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x005A7240 | `cat_trailShouldDraw` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x005AD300 | `cat_shipIsAcolyte` | medium; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x005AD320 | `cat_acolyteCanLink` | medium; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x005AD360 | `cat_shipIsAcolyteOrDerelict` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x005AE1B0 | `cat_shipIsACV` | medium; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x005AFFA0 | `cat_shipIsCrystal` | medium; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x005C2FD0 | `droidIsValidPredockPos` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |
| 0x005C3070 | `droidCreateArray` | low; audit warning: int-typed parameter the r5a body dereferences as a pointer (the defect of all 3 sampled-wrong entries) |

## Audit-supplied prototype fix (1)

- 0x0048F8B0 `cat_hsGateRectangle`: `void __cdecl cat_hsGateRectangle(vector *origin, real32 width, real32 height, ubyte alpha, bool32 outline)`. It replaces the applied round-4 four-argument prototype, which the lowconf-r4 audit showed is wrong (Signature wrong: 5 cdecl args, not 4. hsStaticGateRender pushes origin,w,h,0x5a,1 then add esp,0x14; body tests [esp+0x34] (5th arg = outline) before the outline loop; alpha is an int (ubyte 90), n...). Binary check: RET 0, the caller's cleanup is 0x14 = 5 arguments.

## Prototype flags kept (40)

These prototypes passed the RET check, but no direct caller cleans exactly the declared bytes. MSVC folds the `add esp` of consecutive calls into one, so a larger cleanup is weak evidence. They are applied and listed here so that a later pass can re-check them.

| address | name | flag |
|---|---|---|
| 0x00404040 | `aieHandlerSetTeamDied` | cleanup [24, 28, 68] vs 8 declared bytes (kept: medium) |
| 0x00404050 | `aieHandlerSetInterrupt` | cleanup [76] vs 20 declared bytes (kept: medium) |
| 0x00407770 | `aimInsertMove` | cleanup [32, 40] vs 8 declared bytes (kept: medium) |
| 0x00436AA0 | `ellipsoid_seq` | cleanup [40, 64, 80] vs 20 declared bytes (kept: medium) |
| 0x004479F0 | `RemoveShipReferencesFromExtraAttackInfo` | cleanup [12] vs 8 declared bytes (kept: medium) |
| 0x00486DF0 | `gcProcessGameTextMessage` | cleanup [24] vs 8 declared bytes (kept: medium) |
| 0x004ACFD0 | `matGetMatFromHMat` | cleanup [16] vs 8 declared bytes (kept: medium) |
| 0x004AE910 | `meshTextureRegisterAllPlayers` | cleanup [12] vs 8 declared bytes (kept: medium) |
| 0x004E0640 | `partFillGenericParticles` | cleanup [28] vs 20 declared bytes (kept: medium) |
| 0x005129F0 | `MakeTargetsOnlyNonForceAttackTargets` | cleanup [16, 44] vs 8 declared bytes (kept: medium) |
| 0x00586830 | `trRGBTextureUpdate` | cleanup [36] vs 16 declared bytes (kept: medium) |
| 0x00586AA0 | `trLIFFileLoad` | cleanup [64] vs 8 declared bytes (kept: low) |
| 0x0058AD80 | `titanRefreshRequest` | cleanup [8, 12] vs 4 declared bytes (kept: medium) |
| 0x0058ADE0 | `titanReplaceGameInfo` | cleanup [16, 28] vs 12 declared bytes (kept: medium) |
| 0x0058B2F0 | `chatConnect` | cleanup [8] vs 4 declared bytes (kept: medium) |
| 0x005BA440 | `SetAIVecHeading` | cleanup [16, 40] vs 12 declared bytes (kept: medium) |
| 0x00463840 | `cat_matCoordSysAligned` | cleanup [40] vs 12 declared bytes (kept: low) |
| 0x00463D00 | `cat_dockFindDockStaticPoint` | cleanup [24] vs 8 declared bytes (kept: medium) |
| 0x004A9A00 | `cat_moResearchDraw` | cleanup [36] vs 8 declared bytes (kept: low) |
| 0x004B64C0 | `cat_madAnimSoundTableInit` | cleanup [104] vs 0 declared bytes (kept: low) |
| 0x004C18C0 | `cat_mgProfileSetReplyCB` | cleanup [12] vs 4 declared bytes (kept: low) |
| 0x00501E70 | `cat_saveLoadMaxSelection` | cleanup [12] vs 4 declared bytes (kept: low) |
| 0x0051B9C0 | `cat_spWarpToLevel` | cleanup [16] vs 0 declared bytes (kept: low) |
| 0x005418F0 | `cat_univMeshBindingsDupeNoGuns` | cleanup [8, 12, 16] vs 4 declared bytes (kept: medium) |
| 0x0058B420 | `cat_titanContestRecordResult` | cleanup [116] vs 12 declared bytes (kept: low) |
| 0x005C5B20 | `cat_sentinelFieldSegmentAdd` | cleanup [40] vs 32 declared bytes (kept: low) |
| 0x0040C280 | `aimCreateDock` | cleanup [24] vs 20 declared bytes (kept: medium) |
| 0x004250C0 | `bigTOCFileExistsByCRC` | cleanup [28] vs 12 declared bytes (kept: medium) |
| 0x00425BC0 | `bitioFileInputStop` | cleanup [20] vs 4 declared bytes (kept: medium) |
| 0x00425D90 | `FillInCollisionBlobSpecificLists` | cleanup [20] vs 4 declared bytes (kept: medium) |
| 0x00427B20 | `AddObjToObjectListsOfBlob` | cleanup [16] vs 8 declared bytes (kept: medium) |
| 0x00476EE0 | `ferDrawCorner` | cleanup [32] vs 20 declared bytes (kept: medium) |
| 0x0047F750 | `fowIsValidW` | cleanup [16] vs 12 declared bytes (kept: medium) |
| 0x004AD1A0 | `matMultiplyMatByMat` | cleanup [24, 28, 40, 48] vs 12 declared bytes (kept: medium) |
| 0x004F0D40 | `regMoveLinkChild` | cleanup [20] vs 8 declared bytes (kept: medium) |
| 0x004F75F0 | `rmSetPrintList` | cleanup [36] vs 8 declared bytes (kept: medium) |
| 0x0050B090 | `cat_smBoundsEdgeDraw` | cleanup [72] vs 12 declared bytes (kept: medium) |
| 0x005388B0 | `uicListWindowSetCurItem` | cleanup [16] vs 8 declared bytes (kept: medium) |
| 0x005418C0 | `univDeleteEffect` | cleanup [8] vs 4 declared bytes (kept: medium) |
| 0x00573120 | `InitQueue` | cleanup [64] vs 8 declared bytes (kept: medium) |

## Audit drops already removed by the auditors (for the record)

- ui-screens: 0x0054d3d0 `cat_kbDebugKeyGet` (returns MessageStrings[0xbb4c34..] entries (0xbb4c34 lies inside MessageStrings@0xbb3620, no other symbol between); caller passes result as fontPrintf format st)
- render-combat: 0x0043a980 `cat_collBeastInfectionCheck` (Body is the per-ship half of HW1 Collision.c kamikazeCheck (SPECIAL_Kamikaze test, damage=kamikazeDamage[shiptype] 0xBB32C0, self damage maxhealth*k, killer typ); 0x00569030 `cat_toFieldPulseDraw` (Body is HW1 mainrgn.c toDrawRadialIndicator4 (circle4 timers/delay, reset past 2*radius, newrad=fsqrt(r^2-(r-d)^2), primModeClear2, hmatMakeHMatFromMat, primCir)
- bands-files: 0x00513620 `cat_shipCanCollectSalvage` (Body is the harvest predicate, not salvage: static shipclass(+0x1e8)==7 CLASS_Resource && harvestAmount(+0x260)>0 (= ShipCanHarvest test) gated on SP or tpGameC); 0x00515930 `cat_blastTotalTime` (Returns _DAT_0089400c + _DAT_00893e4c; 0x893e4c is field [0x11] (max blast radius, compared with current radius [0xf]) of the SP big-gun blast template 0x893e08)
- lowconf-r4: 0x0048f8b0 `cat_hsGateRectangle` (Signature wrong: 5 cdecl args, not 4. hsStaticGateRender pushes origin,w,h,0x5a,1 then add esp,0x14; body tests [esp+0x34] (5th arg = outline) before the outlin)
- prototypes-r5: 0x0058da00 (TitanInterface_PrintAddress: arg is SOCKADDR_IN& (body reads +2,+4..+7; HW1 TitanInterface.h:411) typed sdword; sites set ecx=this (thiscall) but proposal says ); 0x005c67b0 (cat_sentinelFieldQueue: both args are pointers (p2 deref +0x10, p1 passed as void* to cat_sentinelFieldQueueSegment) but typed sdword); 0x0042c0b0 (cat_mcResetRequest: arg is a struct pointer (writes byte [arg+0x7c]; all sites push 0xc49400) but typed sdword)

The auditors' better names for two dropped entries are not applied, because no set proposed them: 0x00515930 (`cat_blastBigGunMaxRadius` was suggested for the dropped `cat_blastTotalTime`) stays `FUN_*`. For 0x00513620, lowconf-r4's `cat_shipCanCollectResources` (medium) is applied. That is the harvest predicate the bands-files auditor described (it suggested `cat_shipCanHarvest`).

