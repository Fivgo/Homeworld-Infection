<!-- done: sample, reclass, stage-counts, rerun, claims, fixes, verdict -->
# Audit: M2-R2 re-read of the 1,013 IDENTICAL/low functions

Auditor pass on `docs/m2-r2-reread.md`, `ghidra/delta_map_review.json`, `ghidra/delta_map.json`, `tools/delta_map.py` and `tools/m2_stage_counts.py`, as committed in b4e0f23. These were checked:
- a 40-function sample read by hand;
- whether the R1 audit's reclassifications were applied;
- the stage-count tables;
- a re-run of the classifier and of the report;
- the doc's headline counts.

Ghidra was not run and `src/` was not touched. Where the decompilation was ambiguous, I read the bytes of `original/game/cataclysm.exe` with `objdump -d -M intel`. Scratch files are in `original/scratch-m2/audit-reread/` (gitignored) and `/tmp/audit-reread/`.

## 1. Sample (40 functions, `random.seed(29)`)

**How the sample was drawn.** The population is the 1,013 review entries with `"sample": "reread-R2"`, with the addresses sorted.
- 15 were drawn with `random.sample` from the 143 entries whose class changed.
- 15 were drawn from the 870 entries still IDENTICAL.
- 10 were drawn from the remaining 983 entries.

The sample list is in `original/scratch-m2/audit-reread/sample.json`. Both bodies were dumped with `tools/delta_map.py show` (`show.txt`). Where `show` cuts a body short, I read the rest with `--max-lines 400`.

| # | address | function | producer | mine | evidence |
|---|---|---|---|---|---|
| 1 | 0x005860d0 | trTextureUnregister | MODIFIED/medium | MODIFIED | Disassembly: the pending-palette reset is guarded by `nUsageCount == 0` (`test esi,esi; jne`). HW1 resets on every call. |
| 2 | 0x0046f020 | etgEveryVar | MODIFIED/high | MODIFIED | Adds a zero-period guard. The rest matches HW1. Ghidra's `1U - lVar3` is `ftol(etgTimeElapsed * -65536.0)` (constant at 0x72a828 = 0xc7800000), so it is `timeElapsed + 1`, and `jbe` gives HW1's `<=`. |
| 3 | 0x004e3f40 | partSetAngDeltaDist | MODIFIED/high | MODIFIED | Takes `real32 *` instead of the value. |
| 4 | 0x004e3c90 | partSetOffsetR | MODIFIED/high | MODIFIED | Takes a pointer to its argument slot. |
| 5 | 0x0046fe70 | etgSin | MODIFIED/high | MODIFIED | Takes a pointer to its argument slot. |
| 6 | 0x00532300 | tutShowPrevButton | MODIFIED/high | MODIFIED | Has its own "TutorialPrev" region at MAIN_WindowWidth-0x140 with `cat_tutProcessPrevButton` and `cat_tutDrawPrevButtonFunction`. |
| 7 | 0x004e4240 | partSetDeltaLength | MODIFIED/high | MODIFIED | Pointer argument. It also zeroes 0x00a504b4. `partSetLengthDist` writes a504a8 and `partSetDeltaLengthDist` writes a504b0, so a504b4 is the field after deltaLengthDist, as stated. |
| 8 | 0x004f1730 | AsteroidTakesDamage | REWRITTEN/medium | REWRITTEN | Disassembly: `damagetaken` ([esp+0xc]) is never read. The RU value is recomputed from the volume fraction, and the result is `alive && resourceVolume <= 0`. HW1's computation, its health test and its scaling call are all gone. The doc row was made more precise (fix 2). |
| 9 | 0x00417240 | aitTeamIsDone | MODIFIED/high | MODIFIED | Adds a `team != NULL` guard. MOVE_DONE is 0 (AITeam.h:67). |
| 10 | 0x00428050 | BountyInit | MODIFIED/high | MODIFIED | Clears 8 dwords and stores the raw setting. |
| 11 | 0x00471030 | etgEffOffsetLOF | MODIFIED/high | MODIFIED | Takes a pointer to its argument slot. |
| 12 | 0x00501050 | SaveDerelict | MODIFIED/high | MODIFIED | Read to the end. The body stops after the inlined SaveClampInfo, which adds a selection chunk (type 0x0900) for the new list. There is no SaveSalvageInfo. |
| 13 | 0x004b47b0 | meshBindingListRecurse | MODIFIED/high | MODIFIED | Has a 4th parameter, stored as a byte at +0x14 and passed down as the parent index. |
| 14 | 0x004e4170 | partSetLighting | MODIFIED/high | MODIFIED | Takes `bool32 *`. |
| 15 | 0x0046fd80 | etgDec | MODIFIED/high | MODIFIED | Takes `udword *`. |
| 16 | 0x004f0fe0 | regDrawFunctionSet | IDENTICAL/high | IDENTICAL | regNULLRenderFunction is folded to nullsub. |
| 17 | 0x00496780 | kasfTutResetContextMenuShipTypeTest | IDENTICAL/high | IDENTICAL | The body is empty in `show` because it is a 5-byte thunk: `jmp 0x533fe0` (tutResetContextMenuShipTypeTest). |
| 18 | 0x00525fd0 | scriptSetGunTypeCB | IDENTICAL/high | IDENTICAL | Same body. |
| 19 | 0x00504a00 | spDescriptionFontSet | IDENTICAL/high | IDENTICAL | Same body. |
| 20 | 0x004c6180 | lgChooseScenario | IDENTICAL/high | IDENTICAL | Same body. |
| 21 | 0x005aabb0 | utyStartTutorial | IDENTICAL/high | IDENTICAL | `forceSP` and `tutorial` are stored, then utySinglePlayerGameStart is inlined. That callee has its own record (0x005aaf80), so by the doc's convention its differences count there. |
| 22 | 0x0055d8b0 | glcCursorUnder | IDENTICAL/medium | IDENTICAL | The bounds, the swap, the ±1 widening, the zero-fill rows and the store/restore loop all match. |
| 23 | 0x0046eb90 | etgLessVVI | IDENTICAL/high | IDENTICAL | `var1 <= var0 → codeBytes` is the same as `var0 < var1 → 16`. The offsets match etgconditional. |
| 24 | 0x004d00f0 | nisCameraFOV | IDENTICAL/high | IDENTICAL | Reads `param[0]` (+0x10) into fieldofview. |
| 25 | 0x0046c640 | etgWorldRenderSet | IDENTICAL/high | IDENTICAL | ESO_WorldRender is 0x4 (etg.h:267). |
| 26 | 0x00472ab0 | feAtomFindInScreen | IDENTICAL/high | IDENTICAL | Inlined strcmp; same loop. |
| 27 | 0x004cc980 | nisRotateAboutVector | IDENTICAL/high | IDENTICAL | All nine M(row,col) stores check out (M(0,1) = m[3] ...). |
| 28 | 0x00581640 | SNDchannel | IDENTICAL/high | IDENTICAL | Same body. The `channel < 0` test is dead after `& 0xFF`. |
| 29 | 0x004bb680 | mgJoinGame | IDENTICAL/medium | IDENTICAL | Read to the end. AddressesAreEqual expands to the IP/IPX branches (TitanInterfaceC.h:69). Inlined: mgPrepareMessageBox (twice), and mgRequestJoinGame with its mgDisplayMessage enqueue (record 5) and titanJoinGameRequest. All three listDeleteAll calls are present. |
| 30 | 0x0053c850 | universeInfoNeededSetCB | IDENTICAL/high | IDENTICAL | `bitSetTo(..., 1, bNeededParam)`. |
| 31 | 0x005403a0 | univRotateObjYaw | IDENTICAL/high | IDENTICAL | 9-dword coordsys copy and the multiply. Ghidra's field names come from the HW1 layout. |
| 32 | 0x005216d0 | soundEventStopSound | IDENTICAL/high | IDENTICAL | 0.0 and 0.5 fades. |
| 33 | 0x004e4200 | partSetLength | MODIFIED/high | MODIFIED | Takes a pointer to its argument slot. |
| 34 | 0x004e39d0 | partSetVelocityInWorldSpace | MODIFIED/high | MODIFIED | Takes `bool8 *`. |
| 35 | 0x0046e790 | etgVarAssign | IDENTICAL/high | IDENTICAL | source +4, dest +8, returns 0xc (etgvariablecopy). |
| 36 | 0x004e3e30 | partSetDeltaLOFDist | MODIFIED/high | MODIFIED | Takes a pointer to its argument slot. |
| 37 | 0x0051c530 | soundEventMasterEQ | IDENTICAL/high | IDENTICAL | Copies 8 floats. |
| 38 | 0x0049fc80 | scriptSetUword2CB | IDENTICAL/medium | IDENTICAL | Disassembly confirms the sscanf out-arguments and the `fillin[1] == -1 → fillin[0]` branch that Ghidra dropped. |
| 39 | 0x00492910 | kasfRandom | IDENTICAL/high | IDENTICAL | RAN_AIPlayer is 6 (randy.h:40). |
| 40 | 0x0048e0c0 | hrShutdownBackground | IDENTICAL/high | IDENTICAL | Same body. |

**Precision = 40/40 = 100%:**
- changed-class stratum: 15/15;
- IDENTICAL stratum: 15/15;
- open draw: 10/10 (7 IDENTICAL, 3 MODIFIED).

The seed drew many one-line ETG and particle callables: 13 of the 40. For a harder check outside the sample, I read `nextLog2` 0x00476920 (MODIFIED/medium, from the "other logic" group) down to the bytes. `setg; dec; and 0x101; dec` gives 256 for n ≤ 256 and -1 above, as the change line says. All five of the producer's non-trivial notes that I checked against the bytes were exact (#1, #2, #8, #38 and nextLog2).

## 2. R1 audit reclassifications

Every item in `docs/audits/m2-r1-function-deltas.md` is applied in the current `ghidra/delta_map.json`, with `reviewed=true`.

**Sample reclassifications (9 of 9):**

| function | class now |
|---|---|
| kasfTimerRemaining | MODIFIED |
| meshTextureRegisterAllPlayers | MODIFIED |
| psCurrentScreenDelete | MODIFIED |
| aimProcessMoveAttack | IDENTICAL |
| aimFix_Intercept | IDENTICAL |
| TitanInterface_GetPatch_2 | MODIFIED |
| GroupShip | MODIFIED |
| ccGetShipCollCenter | MODIFIED |
| P1MissileCorvetteSpecialTarget | MODIFIED |

**SpaceObjRegistryGetShip callers (10):**
- 7 are IDENTICAL: aimFix_Intercept, aimFix_AdvancedAttack, aimLoad_MoveAttack, aimFix_HarassAttack, aimFix_Dock, LoadRequestShipsCB and GravWellGenerator_Fix.
- 3 stay MODIFIED: LoadThisAIPlayer, LoadConsMgr and SalCapCorvette_Fix. Each has a stated non-GetShip reason. The R1 audit had marked all 10 as only "probably IDENTICAL".

**kasfTimer\* family:** all 9 are MODIFIED (Create, Set, Start, CreateSetStart, Remaining, Expired, ExpiredDestroy, Destroy and Stop). kasfTimerStop moved from REWRITTEN.

**audit-R2 entries:** there are exactly 20 review entries with `sample: audit-R2`: 10 IDENTICAL, 9 MODIFIED and 1 REWRITTEN, as the doc says.

**Name-fix proposals** (`ghidra/proposals/m2-r2-name-fixes.json`):
- I re-read 0x00404090 against HW1 `aieCheckHealthLow` in decompiled.c:2566-2614. The loop over the selection, the watchIndividual byte at +0x100, health (+0x170) × 100 < watchPercentage (+0xfc) × maxhealth (+0xa4), and the summed branch all match.
- I did not re-read the other two proposals.

## 3. Stage counts

`tools/m2_stage_counts.py --md` on the current map matches the table in `docs/m2-r1-summary.md` ("Delta counts per M2-R2 stage") byte for byte.

On the M2-R1 map (`git show 13723dd:ghidra/delta_map.json`), it matches byte for byte the stage table in `docs/m2-r1-summary.md` as of e099efa. That table is 3,357 / 983 / 80 HW1-matched and 3,789 cat-new.

## 4. Classifier and report re-runs

| run | result |
|---|---|
| `delta_map.py classify`, in place on a copy of the committed map | byte-identical to `ghidra/delta_map.json` |
| `classify --fresh` and `classify` into an empty `--out` | all 4,420 function records identical. Only the `done_subsystems` metadata list differs, because a new output file does not carry the merge labels. |
| reviewed classes after the re-run | 0 mismatches over the 1,191 reviewed functions, class and confidence |
| `delta_map.py report` | byte-identical to `docs/m2-r1-function-deltas.md` |

## 5. Doc claims checked

All of the following were recomputed from the JSON (`/tmp/audit-reread/claims.py`) and match the doc:
- The 1,013 re-read targets are exactly the 1,013 M2-R1 IDENTICAL/low functions.
- The final classes are 870 / 140 / 3.
- The confidence split is 969 high, 42 medium and 2 low; the 2 low are fqMix and fqEqualize.
- The 143 class changes break down as 59 ETG/particle, 10 KAS hash, 8 `enum:`, 4 callee-argument and 62 other.
- 1,191 functions are reviewed, 984 of them IDENTICAL.
- The totals are 3,175 / 1,168 / 77.

## Fixes made (2, in `docs/m2-r2-reread.md`, each marked `[audit: ...]`)

1. The `kasfShipsDockStay` row was missing its closing parenthesis. This is cosmetic. The same text in `delta_map_review.json` was left alone, so that the JSON and the report stay byte-reproducible.
2. The `AsteroidTakesDamage` row now gives the new model as read from the disassembly:
   - `asteroid+0x190 = ftol(static int +0xb4 × (resourceVolume > 0 ? resourceVolume / static float +0xa4 : 0))`;
   - `damagetaken` is never read.

   The producer's text was vague but not wrong, and the class is unchanged.

No class, count or tool output was wrong, so no review entry or table was changed.

Verdict: PASS - 40/40 sampled classes agree (15 changed, 15 IDENTICAL, 10 open draw; the trickiest checked down to the exe bytes). The R1 audit's reclassifications are all applied (20 audit-R2 entries). `m2_stage_counts.py` reproduces both stage tables byte for byte. `classify` re-runs keep all 1,191 reviewed classes, and `report` reproduces the function-deltas doc exactly. The only changes are 2 doc annotations: 1 typo and 1 refinement.
