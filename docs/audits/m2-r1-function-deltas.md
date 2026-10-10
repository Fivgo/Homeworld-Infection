<!-- done: sample, totals, rerun, claims, fixes, verdict -->
# Audit: M2-R1 function deltas

This is the adversarial audit of M2-R1 item 3 ("function-deltas"). It covers these producer outputs:
- `tools/delta_map.py`
- `ghidra/delta_map.json`
- `ghidra/delta_map_review.json`
- `docs/m2-r1-function-deltas.md`

The HW1 side is the producer's ad1bb82 archive in `original/scratch-m2/delta-orig/` and its index in `original/scratch-m2/delta-work/`. The Cataclysm side is the final r5 export. Audit scripts live in `/tmp/audit-fd/` (`sample.py`, `totals.py`, `claims.py`, `sys.py`, `fix.py`); they are not part of the repo. Nothing was committed.

## What was checked

1. **A 40-function sample, read by hand.** The sample was drawn with `random.seed(23)` from the functions sorted by address, using `random.sample` once per class in the order IDENTICAL (15), MODIFIED (15), REWRITTEN (10). That gave 36 unreviewed and 4 hand-reviewed functions. For every function I read the HW1 body (the ad1bb82 source, via `delta_map.py show`, and the raw source where needed) and the r5 decompiled body, then gave my own class.
2. **Per-file totals.** A script recomputed every row of the six per-file tables and every group header from `delta_map.json`: matched, identical, modified, rewritten, the three byte columns, rev and unres. It did the same for the totals table, the calibration and bucket tables, the confidence line and the "estimated true split".
3. **Re-running the tool.** I ran `tools/delta_map.py classify --fresh --out /tmp/audit-fd/dm_rerun.json` (2.3 s) and compared the result with the committed JSON. I compared all records, then three functions in detail.
4. **Key-finding counts.** I checked the counts in the write-up's Key findings against the JSON and the r5 `strings.csv`.

## 1. Sample: my class against the delta map

| # | address | function | map class / conf | reviewed | my class | verdict | evidence |
|---|---|---|---|---|---|---|---|
| 1 | 0x005a4af0 | mistrailNew | IDENTICAL/medium | no | IDENTICAL | correct | same allocation and field stores; memAlloc expands to memAllocFunction |
| 2 | 0x00492020 | kasfTimerRemaining | IDENTICAL/low | no | MODIFIED | **wrong** | HW1 builds a scoped name string (kasfScopeName) and calls `timTimerRemaining(scopedName)`. Cataclysm calls `timTimerRemaining(0, name&0x7fffffff)` when the high bit is set, else `timTimerRemaining(scopeHash, name)`. This is the hash-keyed KAS API, not an inlined kasfScopeName. |
| 3 | 0x0041b020 | aiuPlayerMothershipCoords | IDENTICAL/low | no | IDENTICAL | correct | returns the struct through a hidden pointer; same logic |
| 4 | 0x004073f0 | aimProcessVarDec | IDENTICAL/high | no | IDENTICAL | correct | same |
| 5 | 0x0051fed0 | SEinrange | IDENTICAL/low | no | IDENTICAL | correct | same lookup-table test |
| 6 | 0x00493d70 | kasfForceCombatStatus | IDENTICAL/medium | no | IDENTICAL | correct | same two loops; bit 0x400000 |
| 7 | 0x004c1d70 | mgUpdateGameInfo | IDENTICAL/medium | no | IDENTICAL | correct | AddressesAreEqual expands to the IP/IPX branch; strcpy and Enqueue are the same |
| 8 | 0x004ae910 | meshTextureRegisterAllPlayers | IDENTICAL/low | no | MODIFIED | **wrong** | Every `trTextureRegister` call passes a 4th argument (`,1`). In Cataclysm `trTextureRegister` takes 4 parameters and stores param_4 in the registry entry (decompiled.c line 342622, +124 lines). In HW1 it takes 3 (texreg.h:314). The tool never compares callee argument counts. |
| 9 | 0x004ea960 | psCurrentScreenDelete | IDENTICAL/medium | no | MODIFIED (cosmetic) | **wrong** | After `memFree(onImage)` and `memFree(offImage)` Cataclysm also sets both fields to 0 (`ptVar1[1].rect.x0 = 0; ...y0 = 0`). The pluglink layout in PlugScreen.h:64 shows these are onImage and offImage. HW1 does not null them. No gameplay effect. |
| 10 | 0x0049fc80 | scriptSetUword2CB | IDENTICAL/low | no | IDENTICAL | correct | Ghidra dropped the sscanf out-arguments; the visible calls are the same |
| 11 | 0x00434430 | cNotifyChatConnected | IDENTICAL/medium | no | IDENTICAL | correct | The CS_RoomConnected case of cChannelStateMachine is inlined: state -1 and mgDisplayMessage (ChannelFSM.c:126) |
| 12 | 0x00570010 | primRectSolidTextured2 | IDENTICAL/high | no | IDENTICAL | correct | same GL sequence; the COORD macro is expanded |
| 13 | 0x00462760 | dmgStopSingleEffect | IDENTICAL/low | no | IDENTICAL | correct | same 3x maxEffects loops |
| 14 | 0x00481190 | StrToTypeOfFormation | IDENTICAL/low | no | IDENTICAL | correct | same |
| 15 | 0x004cb510 | nebChunkPtrToNum | IDENTICAL/low | no | IDENTICAL | correct | same |
| 16 | 0x00408840 | aimProcessMoveAttack | MODIFIED/high | no | IDENTICAL | **wrong** | The "dropped" aimCreateMoveTeamNoAdd is inlined: a 0x170 alloc with type 6, aimProcessMove, NULL, NULL and the destination stored, matching AIMoves1.c:3126. Its "constant 10000 missing" is a float load from `_DAT_0072a488`. Every branch matches HW1. |
| 17 | 0x004703c0 | etgCreateLines | MODIFIED/medium | no | MODIFIED | correct | argument-array calling convention |
| 18 | 0x0051cd10 | soundEventUpdate | MODIFIED/high | no | MODIFIED | correct | HW1 call set kept, plus 12 cat_* calls |
| 19 | 0x004e3340 | partModifyAnimation | MODIFIED/medium | no | MODIFIED | correct | argument-array calling convention |
| 20 | 0x005a6c30 | trailSurpriseColorAdjust | MODIFIED/high | no | MODIFIED | correct | the base colour now comes from cat_shGetTeamColour plus an index gradient |
| 21 | 0x0040e160 | aioCreateGuardShips | MODIFIED/high | no | MODIFIED | correct | race-dependent alternatives and request type; no fuel handler (the change line omits the race tables) |
| 22 | 0x0043ab20 | kamikazeCheck | MODIFIED/high | no | MODIFIED | correct | per-ship logic moved into cat_kamikazeCheckShip; ApplyDamageToTarget takes more arguments |
| 23 | 0x004271d0 | bobBlobItemize | MODIFIED/high | no | MODIFIED | correct | cloak test is cat_shipIsCloakedFully; battleChatter; different flags |
| 24 | 0x00547640 | univRemoveAllShipReferencesFromSpecialShip | MODIFIED/high | no | MODIFIED | correct | 4 extra references cleared; gameIsEnding test gone (the change line understates this) |
| 25 | 0x004f2e90 | rmTechRequiredForShip | MODIFIED/medium | no | MODIFIED | correct | tech bitmask became a 0x2a-dword set; same algorithm |
| 26 | 0x004d8370 | opOptionsCancel | MODIFIED/high | yes | MODIFIED | correct | adds cat_kbReloadKeyBindings |
| 27 | 0x0040bc60 | aimFix_Intercept | MODIFIED/high | no | IDENTICAL | **wrong** | In a release build SpaceObjRegistryGetShip and SpaceObjRegistryGetObj (SaveGame.c:147-171) are the same code once dbgAssert is gone. The same holds for aimInterceptShipDied and aimCaptureShipDied (AIMoves1.c:115/682). The linker folded each pair, so the surviving names differ from HW1 but the source is unchanged. |
| 28 | 0x004f76f0 | rmGUIStartup | MODIFIED/high | no | MODIFIED | correct | per-race lab image list, extra images and fonts |
| 29 | 0x005220e0 | speechPlayQueue | MODIFIED/high | no | MODIFIED | correct | HW1 tail kept (SEselectsentence, volume, queue loop); pilot effects, bookends and actor switch deleted. The change line ("drops calls GetPatch") badly understates this. |
| 30 | 0x0054b1d0 | univFreeBulletContents | MODIFIED/medium | no | MODIFIED | correct | HW1 body is empty; Cataclysm adds a ship-callback loop and frees mesh state (borderline REWRITTEN) |
| 31 | 0x0058c6e0 | TitanInterface_GetPatch_2 | REWRITTEN/low | yes | MODIFIED | **wrong** | The body is HW1's GetPatch launcher with more arguments: `operator_new` for an args object, copy the strings, `_beginthread(TitanInterface_GetPatch,0,args)`, and on -1 call the failure callback (param 8). The tool's automatic class (MODIFIED/low) was right; the review override was not. |
| 32 | 0x004e76b0 | pieMovePointClipToLimits | REWRITTEN/high | yes | REWRITTEN | correct | ellipse intersection replaced by box clamping |
| 33 | 0x0044b320 | GroupShip | REWRITTEN/high | yes | MODIFIED | **wrong** | The research-ship branch is gone. The rest is HW1 with its helpers inlined: AddShipToFormationGroup (realloc of the selection, formationContentHasChanged, AI reset) and GroupShipIntoMilitaryParade (find the group, then AddShipToMilitaryGroup or CreateMilitaryGroupAroundShip), with a new shipclass != 10 guard. |
| 34 | 0x00495560 | kasfShipsSelectEnemy | REWRITTEN/high | no | REWRITTEN | correct | the body is one cat_kasShipsSelectByPredicate call |
| 35 | 0x0042cf30 | ccGetShipCollCenter | REWRITTEN/medium | no | MODIFIED | **wrong** | Cataclysm is exactly HW1's `else` branch (`*pos = collPosition; *rad = collspheresize`). The slave/master branch is deleted, so the port is a deletion, not a rewrite. |
| 36 | 0x00507f10 | selStatusDraw0 | REWRITTEN/medium | no | REWRITTEN | correct | drawing moved into new cat_selStatus* helpers; only the tutorial-rect code is left |
| 37 | 0x00491e30 | kasfVarValueGet | REWRITTEN/high | no | REWRITTEN | correct (borderline) | aivar API replaced by cat_kasVarFind with hash keys |
| 38 | 0x00495620 | kasfShipsSelectFriendly | REWRITTEN/high | no | REWRITTEN | correct | one predicate call |
| 39 | 0x004f4bd0 | rmClearAllLabs | REWRITTEN/medium | no | REWRITTEN | correct | the body is one global store; parameters gone |
| 40 | 0x005be480 | P1MissileCorvetteSpecialTarget | REWRITTEN/medium | no | MODIFIED | **wrong** | the HW1 timing prelude and target-count checks are kept verbatim; the gun/missile loop is deleted (`curTargetIndex = 0; return 1`). A deletion, not a rewrite. |

Results:
- **31 correct, 9 wrong, 0 unverifiable. Precision = 31/40 = 77.5%.**
- By stratum: IDENTICAL 12/15 (80%), MODIFIED 13/15 (87%), REWRITTEN 6/10 (60%).
- Unreviewed functions: 29/36 (81%). Hand-reviewed functions: 2/4.

Borderline calls, which I counted as correct: #30, #37. The cosmetic wrong call is #9.

The 9 errors go both ways:
- 3 IDENTICAL should be MODIFIED.
- 2 MODIFIED should be IDENTICAL.
- 4 REWRITTEN should be MODIFIED.

So the headline byte split is roughly right, but per-function classes are estimates.

### Systemic causes found while checking the sample

These come from the misses above. Each was confirmed beyond the sample.

1. **Linker-folded functions counted as dropped calls.** 10 functions are MODIFIED (8 high, 2 medium) because SpaceObjRegistryGetShip is "absent from Cataclysm" (`/tmp/audit-fd/sys.py`): aimFix_Intercept, aimFix_AdvancedAttack, aimLoad_MoveAttack, aimFix_HarassAttack, aimFix_Dock, LoadRequestShipsCB, LoadThisAIPlayer, LoadConsMgr, GravWellGenerator_Fix and SalCapCorvette_Fix. For the first five, that is the only delta listed. In release, GetShip is identical to GetObj, so these are probably IDENTICAL. Other names in `calls_unbound` may be the same artefact. I did not prove that for each.
2. **The kasfTimer\* family.** kasfTimerCreate, Set, Start, CreateSetStart, Remaining, Expired, ExpiredDestroy and Destroy are IDENTICAL/low. kasfTimerStop is REWRITTEN/high. I read Create, Start, Destroy, Remaining and Stop: all of them switch from `kasfScopeName` + `timTimer*(scopedName)` to `timTimer*(scopeHash, nameHash)`, so all should be MODIFIED. kasfTimerStop is REWRITTEN only because HW1 has a bug (KASFunc.c:235 calls `timTimerStart`) that Cataclysm fixed. The tool reads the dropped string building as "kasfScopeName inlined".
3. **Changes in callee arity are invisible** (#8). When an HW1 callee gains a parameter, its callers change but are reported IDENTICAL.
4. **A deleted branch is reported as REWRITTEN** (#35, #40). The size ratio and loss of features push small functions with a deleted branch into REWRITTEN.
5. **An inlined callee is reported as dropped** (#16). That happens when the callee leaves no strings or constants the tool can match.

## 2. Per-file totals (scripted)

Every check below was recomputed from `delta_map.json` (`/tmp/audit-fd/totals.py`) and matches the write-up:
- **Per-file rows:** all 196 rows match, 0 mismatches, and every file in the JSON appears in a table.
- **Group headers:** all six match on functions, bytes and % identical. The values are Game a-f 1,399 / 414,570 / 60.3%, Game g-m 821 / 160,446 / 59.5%, Game n-s 1,000 / 270,647 / 46.1%, Game t-z 409 / 118,515 / 53.9%, Ships 137 / 27,321 / 43.1%, other 654 / 247,348 / 45.5%.
- **Totals table:** 3,357 / 983 / 80 functions; 658,389 / 569,953 / 10,505 bytes; 6 unresolved, 4,137 bytes.
- **Calibration and bucket tables:** the confusion matrix (168 reviewed) and all nine buckets match.
- **Confidence line:** matches.
- **"MODIFIED functions of 1,000 bytes or more":** 164, matches.
- **Estimated true split:** recomputed independently as 3,305 / 719,654 / 58.1%, 1,064 / 512,385 / 41.4% and 50 / 6,808 / 0.5%, an exact match.

One definition was misleading. `matched` excludes the unresolved functions: BitIO.c shows matched 2 and unres 2, but there are 4 matched BitIO functions. This is now fixed inline (see Fixes).

## 3. Re-running the tool

- `classify --fresh` to a temp file reproduces the totals exactly: 3,357 / 983 / 80, 168 reviewed, and the same (class, confidence) counts.
- **4,410 of 4,420 records are byte-identical** to the committed JSON.
- **No class changed.** The other 10 records differ only in features and change text: FreeCommandToDoContents, GroupShip, clProcess, singlePlayerGameUpdate, univUpdate, mainLoadParticularRGL, WindowProc, mrRegionProcess, rndMainViewRenderFunction and SalCapCorvetteSpecialTarget. A second run differed from the first in 11 records.
- The cause is Python's string-hash randomisation (set iteration order). Two runs with `PYTHONHASHSEED=0 python3 -s -P` produced byte-identical files. The documented command `python3 -I` disables PYTHONHASHSEED, so the tool as documented is not bit-reproducible. This is minor.
- **The three detailed functions reproduce exactly.** For each, the rerun's `features` dict equals the committed one:
  - kasfTimerRemaining: inlined `[kasfScopeName]`, score 0.701
  - aimFix_Intercept: unbound `[SpaceObjRegistryGetShip]`, score 0.5
  - aimProcessMoveAttack: dropped `[aimCreateMoveTeamNoAdd]`, inlined `[aimCreateAttackNoAdd]`, score 0.811

## 4. Key-finding counts

| claim | check | result |
|---|---|---|
| 18 AI functions drop aieHandlerSetFuelLow | `calls_unbound` count | correct (18) |
| 30 functions add ioUpdateShipTotals | `calls_added` | correct (27 ShipSelect.c + 3) |
| 62 AI move functions reference `"\naimProcess<Name>"` | strings.csv xref_functions | correct (62, all in the map) |
| 76 argument-array functions (ETG 36, Particle 39, SoundEventPlay 1) | change text | correct |
| cat_kasHashString called from 15 matched functions | features | correct |
| kasfShipsSelect* collapse into the predicate (11 REWRITTEN) | features | correct (11 call the predicate; a 12th REWRITTEN kasfShipsSelectIndex does not) |
| "37 other functions call cat_salvage*" | features | **wrong**: 37 is the total, including 8 SalCapCorvette.c functions; 29 others. Fixed. |
| 386 non-identical functions call a cat-new function; cat_shipIsCloakedFully 15 callers | `calls_new_cataclysm` | correct |
| dbgMessagef 54, dbgMessage 44 | `calls_unbound` | correct |
| TitanInterface_GetPatch_2 "is a 9-parameter Cataclysm routine" whose name looks wrong | read body | **wrong**: it is HW1's GetPatch launcher with more arguments (sample #31). Fixed. |

## Fixes made (5, each marked `[audit: ...]` inline)

1. **`docs/m2-r1-function-deltas.md`, key finding 2, salvage bullet:** "37 other" corrected to 29 others; 37 is the total including 8 SalCapCorvette.c functions. Mirrored in `ghidra/delta_map_review.json` `_notes`, so `report` regenerates it.
2. **Same file, key finding 4:** the GetPatch_2 sentence is annotated as wrong: the name fits and the function is MODIFIED. Mirrored in `_notes`.
3. **Same file, REWRITTEN table, GroupShip row:** "formation group / military parade logic gone" corrected; that logic is inlined and still present. Mirrored in the review entry 0x0044b320 and in the `change` of `delta_map.json`.
4. **Same file, REWRITTEN table, TitanInterface_GetPatch_2 row:** "does not match HW1's launcher" corrected; it does match. Mirrored in the review entry 0x0058c6e0 and in `delta_map.json`.
5. **Same file, per-file section intro:** made clear that `matched` excludes the `unres` functions. This text comes from a string in `tools/delta_map.py` (~line 1251), which I did not edit. **A `report` re-run will drop this one fix.**

I did not change any classes. Doing so would change every table, and a class is a judgment rather than a provable fact. The sample's 9 reclassifications are reported above for the orchestrator to apply or not.

## Notes (not settled here)

- **The calibration table is optimistic.** The producer's 168 reviewed functions were not a random sample, and 2 of the 4 reviewed functions in this random sample have wrong review verdicts (GroupShip, GetPatch_2). This audit's unbiased IDENTICAL agreement is 80% (12/15), against the write-up's 82-98% per bucket. Expect the "estimated true split" to carry more IDENTICAL-to-MODIFIED error than it shows.
- **Possible reclassifications.** The 10 SpaceObjRegistryGetShip functions are probably IDENTICAL. The 7 IDENTICAL kasfTimer* functions, plus kasfTimerStop, are MODIFIED. I proved this for the ones I read (Create, Start, Destroy, Remaining, Stop); Set, CreateSetStart, Expired and ExpiredDestroy were not read.
- **Reproducibility.** `delta_map.py` should iterate in sorted order, or the docs should say to run with a fixed PYTHONHASHSEED (without `-I`).
- **Change lines.** Several are incomplete even where the class is right: speechPlayQueue, univRemoveAllShipReferencesFromSpecialShip, aioCreateGuardShips. Treat the automatic change lines as hints, not specs.

Verdict: PASS-WITH-FIXES - per-file tables, totals and the estimate reproduce exactly from the JSON, and the tool reproduces its classes. But the 40-function sample agrees only 31/40 (77.5%). There are systemic misses (linker-folded names, the hash-keyed kasfTimer* family, callee arity, deletions called REWRITTEN) and two wrong hand-review notes, now corrected inline. Treat per-function classes as estimates.
