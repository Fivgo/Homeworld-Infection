<!-- done: tool, Game a-f, Game g-m, Game n-s, Game t-z, Ships, other, review, write-up, reread:AIAttackMan-AIHandler, reread:AIMoves, reread:AIOrders-AIVar, reread:Alliance-BTG, reread:Camera-ColPick, reread:CommandLayer-Dock, reread:ETG, reread:fastmath-HS, reread:KAS-KASFunc, reread:Key-MEX, reread:MultiplayerGame-NetCheck, reread:NIS-Options, reread:Particle, reread:Physics-ResearchGUI, reread:SaveGame, reread:ScenPick-SinglePlayer, reread:SoundEvent-StringSupport, reread:Tactical-TradeMgr, reread:Tutor-Universe, reread:UnivUpdate-Ships, reread:bink-Light, reread:main-Queue, reread:render-Titan, reread:TitanInterface-utility, audit-reclass-R2, toolfix-R2, m2r3:closeout-review, m2r3:closeout-audit, m2r3:etg-particles-review, m2r3:etg-particles-audit, m2r3:selection-review, m2r4:univupdate-review, m2r4:univupdate-audit, command, m2r4:command-audit, m2r4:combat-review, m2r4:combat-audit, m2r4:ships-b, port-R4-rest-oracle, m2r5:layout-dock, m2r5:command-tactics, m2r5:render, m2r5:sound, m2r5:sound-audit, m2r5:hud, m2r5:hud-audit, m2r6:mainrgn-popup, m2r6:frontend, m2r6:startup-keys, m2r6:managers, m2r6:nis-fow-camera, m2r6:integrate, m2r7:oracle-rung-aiship, m2r7:ai-orders-moves, m2r7:ai-player-skirmish, m2r7:r6-leftovers, m2r7:integrate, m2r7:skirmish-start, m2r8:oracle-rung-physics, m2r8:kasf-library, m2r8:kas-runtime-mission01, m2r8:singleplayer-tutorial -->
# M2-R1 function deltas: Cataclysm vs Homeworld 1

Round M2-R1, item 3 of `cycle/M2_BRIEF.md`. For every function the binary map matched to Homeworld 1 (class `hw1:<File>`, plus CET's `hw1_equivalent` entries, the rule behind the 4,426 HW1 matches of docs/binary-map.md round 5), this compares the decompiled Cataclysm body (`ghidra/exports/cataclysm_gog_r6`, since M2-R4; r5 before) with Relic's original source (gardens-of-kadesh root commit `ad1bb82`) and classifies it IDENTICAL, MODIFIED or REWRITTEN. Per-function data: `ghidra/delta_map.json`. Tool: `tools/delta_map.py`. Hand-review verdicts: `ghidra/delta_map_review.json`.

## Totals: how much of the engine is free

| | functions | share | Cataclysm bytes | share of bytes |
|---|---:|---:|---:|---:|
| IDENTICAL | 2,773 | 62.7% | 500,313 | 40.4% |
| MODIFIED | 1,564 | 35.4% | 722,679 | 58.3% |
| REWRITTEN | 83 | 1.9% | 15,855 | 1.3% |
| **classified** | **4,420** | | **1,238,847** | |
| not classified (no HW1 body found) | 6 | | 4,137 | |

Estimated true split, correcting each automatic (class, confidence) bucket by its hand-review agreement (class-level agreement where a bucket has fewer than 5 reviewed functions); reviewed functions count with their reviewed class: IDENTICAL 2,659 functions / 480,005 bytes (38.7% of bytes), MODIFIED 1,684 / 743,542 (60.0%), REWRITTEN 77 / 15,300 (1.2%).

## Key findings

1. **Most of the matched engine is free.** By function count about three quarters of the HW1-matched functions show no change the tool can see; by bytes a little over half. Adjusted with the hand-review agreement rates (table below), IDENTICAL covers about 56% of the matched bytes, MODIFIED about 43%, REWRITTEN under 1%; the split after M2-R2 is 51% / 48% / 1% (M2-R1: 53% / 46% / 1%). The MODIFIED bytes sit in a few hundred large functions (165 MODIFIED functions are 1,000 bytes or larger): UI/front-end, rendering, AI and the ship/unit code. Small leaf functions are almost all identical.
2. **Many changes come from a handful of systemic Cataclysm changes.** Porting them once, in headers or shared helpers, covers dozens of functions each:
   - **No fuel.** `aieHandlerSetFuelLow` does not exist in Cataclysm, and 18 AI functions drop their call to it (`aiaSendRecon`, `aioCreate*`...).
   - **Selection filters refresh the info overlay.** 30 functions in ShipSelect.c and elsewhere add an `ioUpdateShipTotals()` call after filtering.
   - **AI move set-up macro.** 62 AI move functions reference a `"\naimProcess<Name>"` string. Cataclysm's InitNewMove/FixMoveFuncPtrs-style macro in AITeam.h stores the process-function name. That is one header change, and the tool does not count it per function.
   - **ETG and particle calling convention.** The functions that ETG scripts call (`etgFadd/Fmult/...`, `partSet*`/`partModify*`) read their arguments from one array pointer instead of separate parameters. 147 functions (Particle.c 84, ETG.c 62, SoundEventPlay.c 1) carry this change, many of them with nothing else changed. M2-R1 counted 76; M2-R2 added the one-parameter callables (`etgSin(real32 *ang)`, `partSetScale`...), which take a pointer to their argument slot where HW1 took the value.
   - **KAS keyed by hashes.** KAS timers take `(scopeHash, nameHash)` instead of a name (Timer.c, `timTimerStart(udword scopeHash, udword nameHash)`). KAS variables go through new `cat_kasVar*` functions, and `cat_kasHashString` is called from 15 matched functions. The `kasfShipsSelect*` family collapses into `cat_kasShipsSelectByPredicate` (11 REWRITTEN).
   - **New salvage system.** The SalCapCorvette.c entry points delegate to new `cat_salvage*` code (REWRITTEN), and 29 other functions call `cat_salvage*` [audit: was "37 other"; 37 is the total including the 8 SalCapCorvette.c functions that call cat_salvage*, counted from calls_added/calls_new_cataclysm in delta_map.json].
   - **Renumbered races and ship types.** Race and ship-type tests use Cataclysm's enum values (races 0/1/2/3/5; ship types such as 0x3f/0x40/0x4d/0x4f). Some filters test more types than HW1 did. Where only the enum value differs, the function is IDENTICAL in source. Where a test names other types (per-race Cataclysm types replacing HW1's, added or dropped types), M2-R2 marks it MODIFIED with a change line starting `enum:`.
   - **New features threaded through HW1 code.** 386 non-identical functions call at least one Cataclysm-only (`cat-new`) function. The most frequent are cloaking (`cat_shipIsCloakedFully`, 15 callers), crystals (11), speed multipliers (11), beast weapons (10), disable reasons (9), upgrades (`cat_playerUpdateUpgradesFromResearch`, 8), plus module slots and support units.
   - **Debug output.** HW1 calls `dbgMessagef` directly in 54 matched functions and `dbgMessage` in 44, some of them the same functions, and the Cataclysm binary has neither function. They are treated as compiled out, not as a change.
3. **Inlining is common in the Cataclysm build.** It inlines small helpers from the same source file (`partHeaderSize`, `aiuFindDistanceSquared`, `nisNewEvent`, `StrToNum`, `opOptionsCancelHelper`...). The tool counts a dropped same-file callee as inlined and expects that callee's strings, fields and constants in the caller. A port must not mistake this inlining for a source change.
4. **Some binary-map names look wrong.** [M2-R2: settled. 0x00404090 `aieCheckEnemyNearby` is HW1 `aieCheckHealthLow` and 0x00404150 `aieCheckEnemyNotNearby` is `aieCheckHealthHigh` (both IDENTICAL; HW1's enemy-nearby checks are inlined in `aieExecute`); 0x004aeb10 `meshFixupPacoUV` is `meshObjectFixupPacoUV`. Proposals: `ghidra/proposals/m2-r2-name-fixes.json`. `RemoveShipFromFormation` 0x00447950 is named right: the same body is inlined in `RemoveShipsFromDoingStuff` where HW1 calls it, so Cataclysm replaced the body (REWRITTEN).] M2-R1 text: the bodies are unrelated to the HW1 function: `aieCheckEnemyNearby` 0x00404090 and `RemoveShipFromFormation` 0x00447950. `TitanInterface_GetPatch_2` 0x0058c6e0 is a 9-parameter Cataclysm routine [audit: wrong for GetPatch_2: its body is HW1's TitanInterface::GetPatch thread launcher extended to 9 parameters (allocates an argument object, copies the strings into it, calls _beginthread(TitanInterface_GetPatch,0,args) and calls the failure callback passed as parameter 8 when that returns -1), so the binary-map name fits and the function is MODIFIED, not REWRITTEN]. These are flagged REWRITTEN/low with a note.
5. **Limits.** The tool compares features, not semantics. A changed condition or constant that adds no call, string, field or global is invisible to it. Reading found several such changes in functions the tool called IDENTICAL: `lgLaunchLAN` (protocol retry removed), `bsCurveUpdate` (end-of-curve value), `aioCreateMine` (race test), `timTimerCreateSetStart` (hash keys behind inlined callees). That is why the IDENTICAL buckets agree with review only 88-92% of the time (M2-R2; M2-R1: 82-98% on fewer reviews). In M2, IDENTICAL means "port verbatim, then let the checksum oracle catch the rest".

## M2-R2 re-read

M2-R2 read both bodies of every function M2-R1 had left IDENTICAL/low: 971 functions. For each, it read the Cataclysm decompilation and Relic's ad1bb82 source side by side, and the GoK `src/` body where it differs from ad1bb82. A difference that is only a header-level constant (renumbered enum, struct offset or size, macro value), an inlined or linker-folded callee, Ghidra prototype noise or compiled-out debug code counts as IDENTICAL. Review entries carry `sample: reread-R2`.

| result | functions | share | high | medium | low |
|---|---:|---:|---:|---:|---:|
| IDENTICAL | 833 | 85.8% | 809 | 24 | 0 |
| MODIFIED | 137 | 14.1% | 126 | 11 | 0 |
| REWRITTEN | 1 | 0.1% | 0 | 1 | 0 |
| **read** | **971** | | | | |

**138 of 971 changed class (14.2%)**: 137 MODIFIED and 1 REWRITTEN. 6 of the MODIFIED ones differ only in which ship types or races a test names (per-race Cataclysm types replacing HW1's, added or dropped types); their change line starts with `enum:`. **The re-read IDENTICAL agreement for the IDENTICAL/low bucket is 85.8%** (M2-R1's 51-function sample said 86%).

The audit's reclassifications (`docs/audits/m2-r1-function-deltas.md`) were checked by hand and applied to 18 functions (review entries with `sample: audit-R2`), now 10 IDENTICAL, 7 MODIFIED, 1 REWRITTEN.

Tool fixes in `tools/delta_map.py` (M2-R2), for the systemic misses the R1 audit found:

- **Linker-folded callees.** `ICF_FOLDED` lists HW1 functions whose release code is identical to another's, so the linker kept one copy: `SpaceObjRegistryGetShip`/`GetTarget` → `SpaceObjRegistryGetObj`, `aimInterceptShipDied` → `aimCaptureShipDied`, `aimAdvancedAttackClose` → `aimCloseKamikaze`. A call to the folded name now matches the surviving one (`calls_folded` in the features). The tool also prints candidate folds it finds in the data, but does not apply them: the 4 it lists are inlined or reworked callees, not folds.
- **Callee interface changes.** A bound callee whose Cataclysm prototype has real parameter names and another parameter count than its HW1 definition (`timTimer*`, `clCommandMessage`, `spMainScreen`, `hsStaticInit`...), or that the binary map extended with `cat_argN` arguments counted from caller stack cleanup (`clFormation`, `RemoveShipsFromDoingStuff`, `ApplyDamageToTarget`, `tutSetPointerTargetAIVolume`...), makes each caller MODIFIED. Over-declared prototypes are skipped when every call site passes register or stack garbage in the extra slots (`feCurrentScreenDelete(unaff_EDI, unaff_ESI)`), and under-declared ones when the callee reads `in_EAX`/`in_stack_` (`kniTransGrowVertexLists`). `param_N` prototypes are not used, because a double or a by-value struct splits into several slots (`rgluLookAt` 9 -> 12); so `trTextureRegister`'s real 4th argument is still caught only by hand review.
- **One-parameter ETG/particle callables** that take a pointer to their argument slot are MODIFIED (the old rule needed two or more HW1 parameters).
- **KAS hash keys.** A function that calls `kasfScopeName` in HW1 and tests the `0x80000000` hash bit in Cataclysm is MODIFIED.
- **Deterministic output.** The inlined-callee walk was capped at 12 callees in set order, so `PYTHONHASHSEED` changed 10-11 records per run. It now walks in sorted order; repeated `python3 -I` runs give byte-identical JSON.

Effect of the tool fixes alone, before the reviewed classes are applied: 117 functions went from automatic IDENTICAL to MODIFIED and 8 from MODIFIED to IDENTICAL; 48 unreviewed functions changed class (all IDENTICAL to MODIFIED). Every reviewed function keeps its reviewed class.

Totals after M2-R2 (re-read, audit reclassifications, tool fixes), against M2-R1:

| class | M2-R1 functions | M2-R2 functions | M2-R1 bytes | M2-R2 bytes | M2-R2 share of bytes |
|---|---:|---:|---:|---:|---:|
| IDENTICAL | 3,357 | 2,773 | 658,389 | 500,313 | 40.4% |
| MODIFIED | 983 | 1,564 | 569,953 | 722,679 | 58.3% |
| REWRITTEN | 80 | 83 | 10,505 | 15,855 | 1.3% |

**New IDENTICAL agreement estimate.** 2,773 functions are IDENTICAL now: 1,964 by hand review and 809 by the tool alone. Applying each automatic bucket's review agreement to the unread ones, about 665 of those 809 (82.2%) are expected to be truly identical, so about 2,629 of all 2,773 IDENTICAL functions (94.8%) are, against M2-R1's 80% audit estimate. The unread ones sit in the automatic IDENTICAL/high and /medium buckets, which have only 28 and 111 reviewed functions, so this is a rough figure.

## Hand review and calibration

3340 functions were reviewed by hand (both bodies read; for long bodies sometimes only the head, as the review note says). The calibration below uses the 1216 reviews drawn as samples (`--calib-samples calibration`: the M2-R1 buckets, the M2-R2 re-read and the audit reclassifications); the ports' targeted reviews (sample `port-*` or none, 2124 functions) are excluded because a port reads the functions it suspects of change. Rows: the tool's current automatic class; columns: the reviewed class.

| automatic \ reviewed | IDENTICAL | MODIFIED | REWRITTEN | agreement |
|---|---:|---:|---:|---:|
| IDENTICAL | 985 | 94 | 1 | 91.2% |
| MODIFIED | 13 | 108 | 2 | 87.8% |
| REWRITTEN | 3 | 5 | 5 | 38.5% |

Per automatic bucket (functions in the bucket, reviewed, reviewed class agreeing):

| automatic class | confidence | functions | bytes | reviewed | agree |
|---|---|---:|---:|---:|---:|
| IDENTICAL | high | 380 | 119,105 | 28 | 27 (96.4%) |
| IDENTICAL | medium | 1,873 | 450,016 | 111 | 88 (79.3%) |
| IDENTICAL | low | 989 | 75,172 | 941 | 870 (92.5%) |
| MODIFIED | high | 682 | 508,751 | 21 | 19 (90.5%) |
| MODIFIED | medium | 263 | 65,926 | 25 | 22 (88.0%) |
| MODIFIED | low | 152 | 9,066 | 77 | 67 (87.0%) |
| REWRITTEN | high | 43 | 3,342 | 4 | 2 (50.0%) |
| REWRITTEN | medium | 36 | 7,172 | 7 | 3 (42.9%) |
| REWRITTEN | low | 2 | 297 | 2 | 0 (0.0%) |

Confidence per class (after review): IDENTICAL 1,907 high / 866 medium / 0 low; MODIFIED 1,283 high / 215 medium / 66 low; REWRITTEN 71 high / 11 medium / 1 low.

## Per HW1 source file

Grouped as in the round plan (Game a-f, g-m, n-s, t-z, Ships, other = Win32/ and GoK-only). Bytes are Cataclysm function sizes. `matched` counts the classified functions only; `unres` = matched functions with no HW1 body, additional to `matched` (BitIO.c: 2 classified + 2 unresolved bitio functions). `rev` = hand-reviewed functions in the file.

### Game a-f (1,399 functions, 414,570 bytes, 44.5% of bytes IDENTICAL)

| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AIAttackMan.c | 13 | 3 | 10 | 0 | 796 | 5,120 | 0 | 10 | 0 |
| AIDefenseMan.c | 13 | 7 | 6 | 0 | 1,161 | 3,260 | 0 | 8 | 0 |
| AIEvents.c | 15 | 11 | 4 | 0 | 715 | 1,650 | 0 | 13 | 0 |
| AIFleetMan.c | 11 | 5 | 6 | 0 | 1,090 | 4,844 | 0 | 8 | 0 |
| AIHandler.c | 20 | 13 | 7 | 0 | 762 | 3,622 | 0 | 11 | 0 |
| AIMoves.c | 163 | 118 | 45 | 0 | 14,390 | 13,823 | 0 | 95 | 0 |
| AIOrders.c | 22 | 2 | 20 | 0 | 103 | 10,891 | 0 | 20 | 0 |
| AIPlayer.c | 28 | 19 | 9 | 0 | 2,503 | 3,302 | 0 | 17 | 0 |
| AIResourceMan.c | 13 | 3 | 10 | 0 | 302 | 5,276 | 0 | 11 | 0 |
| AIShip.c | 20 | 11 | 9 | 0 | 2,556 | 8,462 | 0 | 20 | 0 |
| AITeam.c | 44 | 28 | 14 | 2 | 2,715 | 3,589 | 169 | 26 | 0 |
| AITrack.c | 19 | 16 | 3 | 0 | 4,562 | 2,254 | 0 | 19 | 0 |
| AIUtilities.c | 85 | 46 | 39 | 0 | 11,515 | 13,066 | 0 | 85 | 0 |
| AIVar.c | 13 | 13 | 0 | 0 | 1,374 | 0 | 0 | 13 | 0 |
| Alliance.c | 6 | 4 | 2 | 0 | 447 | 1,268 | 0 | 3 | 0 |
| Animatic.c | 4 | 3 | 1 | 0 | 662 | 499 | 0 | 4 | 0 |
| Attack.c | 6 | 4 | 2 | 0 | 1,769 | 790 | 0 | 3 | 0 |
| AutoDownloadMap.c | 11 | 11 | 0 | 0 | 2,109 | 0 | 0 | 7 | 0 |
| AutoLOD.c | 5 | 5 | 0 | 0 | 329 | 0 | 0 | 3 | 0 |
| B-Spline.c | 4 | 3 | 1 | 0 | 431 | 594 | 0 | 2 | 0 |
| Battle.c | 19 | 7 | 11 | 1 | 1,101 | 5,591 | 94 | 19 | 0 |
| BigFile.c | 8 | 8 | 0 | 0 | 3,088 | 0 | 0 | 3 | 0 |
| BitIO.c | 2 | 2 | 0 | 0 | 62 | 0 | 0 | 2 | 2 |
| Blobs.c | 21 | 19 | 2 | 0 | 7,094 | 1,325 | 0 | 3 | 0 |
| BMP.c | 2 | 1 | 1 | 0 | 148 | 79 | 0 | 0 | 0 |
| Bounties.c | 4 | 1 | 3 | 0 | 274 | 232 | 0 | 4 | 0 |
| BTG.c | 17 | 12 | 5 | 0 | 1,673 | 3,892 | 0 | 17 | 0 |
| Camera.c | 8 | 5 | 3 | 0 | 925 | 1,531 | 0 | 8 | 0 |
| CameraCommand.c | 28 | 17 | 11 | 0 | 2,642 | 4,670 | 0 | 28 | 0 |
| Captaincy.c | 10 | 8 | 1 | 1 | 1,319 | 1,960 | 85 | 3 | 0 |
| ChannelFSM.c | 12 | 12 | 0 | 0 | 868 | 0 | 0 | 2 | 0 |
| Chatting.c | 3 | 2 | 1 | 0 | 312 | 159 | 0 | 1 | 0 |
| Clamp.c | 3 | 0 | 3 | 0 | 0 | 898 | 0 | 3 | 0 |
| Clipper.c | 7 | 7 | 0 | 0 | 4,805 | 0 | 0 | 7 | 0 |
| Clouds.c | 25 | 21 | 4 | 0 | 6,711 | 7,544 | 0 | 5 | 0 |
| Collision.c | 25 | 12 | 13 | 0 | 3,498 | 11,658 | 0 | 15 | 0 |
| ColPick.c | 40 | 38 | 2 | 0 | 10,397 | 653 | 0 | 40 | 0 |
| CommandLayer.c | 97 | 32 | 64 | 1 | 4,639 | 41,979 | 54 | 97 | 0 |
| CommandNetwork.c | 53 | 49 | 4 | 0 | 10,756 | 4,480 | 0 | 15 | 0 |
| CommandWrap.c | 23 | 11 | 12 | 0 | 1,150 | 2,372 | 0 | 23 | 0 |
| ConsMgr.c | 50 | 15 | 31 | 4 | 1,321 | 13,058 | 3,024 | 50 | 0 |
| Crates.c | 8 | 6 | 2 | 0 | 2,453 | 2,702 | 0 | 3 | 0 |
| CRC32.c | 2 | 2 | 0 | 0 | 104 | 0 | 0 | 2 | 0 |
| Damage.c | 7 | 5 | 2 | 0 | 747 | 1,208 | 0 | 4 | 0 |
| Debug.c | 2 | 2 | 0 | 0 | 183 | 0 | 0 | 2 | 0 |
| Demo.c | 8 | 6 | 2 | 0 | 1,026 | 599 | 0 | 8 | 0 |
| Dock.c | 18 | 3 | 12 | 3 | 273 | 4,374 | 492 | 18 | 0 |
| ETG.c | 205 | 111 | 93 | 1 | 16,601 | 16,700 | 64 | 108 | 0 |
| Eval.c | 6 | 6 | 0 | 0 | 3,298 | 0 | 0 | 1 | 0 |
| fastmath.c | 2 | 2 | 0 | 0 | 227 | 0 | 0 | 2 | 0 |
| FEFlow.c | 38 | 34 | 4 | 0 | 9,551 | 1,771 | 0 | 38 | 0 |
| FEReg.c | 28 | 27 | 1 | 0 | 13,171 | 104 | 0 | 28 | 0 |
| File.c | 19 | 16 | 3 | 0 | 5,850 | 630 | 0 | 4 | 0 |
| FlightMan.c | 45 | 45 | 0 | 0 | 13,987 | 0 | 0 | 45 | 0 |
| FontReg.c | 6 | 6 | 0 | 0 | 935 | 0 | 0 | 6 | 0 |
| Formation.c | 33 | 16 | 17 | 0 | 3,160 | 13,469 | 0 | 33 | 0 |

### Game g-m (821 functions, 160,446 bytes, 47.6% of bytes IDENTICAL)

| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| GameChat.c | 12 | 8 | 4 | 0 | 1,140 | 2,620 | 0 | 4 | 0 |
| GamePick.c | 11 | 10 | 1 | 0 | 1,979 | 404 | 0 | 11 | 0 |
| Globals.c | 2 | 1 | 1 | 0 | 17 | 317 | 0 | 2 | 0 |
| Gun.c | 8 | 1 | 6 | 1 | 152 | 6,739 | 474 | 7 | 0 |
| HorseRace.c | 16 | 7 | 9 | 0 | 3,076 | 5,195 | 0 | 16 | 0 |
| HS.c | 18 | 17 | 0 | 1 | 4,842 | 0 | 39 | 7 | 0 |
| InfoOverlay.c | 2 | 0 | 1 | 1 | 0 | 431 | 220 | 2 | 0 |
| KAS.c | 30 | 7 | 20 | 3 | 382 | 3,855 | 189 | 30 | 0 |
| KASFunc.c | 294 | 208 | 66 | 20 | 8,491 | 7,157 | 925 | 294 | 0 |
| Key.c | 12 | 11 | 1 | 0 | 881 | 39 | 0 | 12 | 0 |
| KeyBindings.c | 4 | 0 | 4 | 0 | 0 | 573 | 0 | 4 | 0 |
| LagPrint.c | 4 | 4 | 0 | 0 | 263 | 0 | 0 | 0 | 0 |
| LaunchMgr.c | 16 | 5 | 11 | 0 | 276 | 4,041 | 0 | 16 | 0 |
| LevelLoad.c | 33 | 16 | 17 | 0 | 3,480 | 11,626 | 0 | 33 | 0 |
| LinkedList.c | 13 | 13 | 0 | 0 | 1,351 | 0 | 0 | 4 | 0 |
| LOD.c | 8 | 6 | 2 | 0 | 676 | 875 | 0 | 8 | 0 |
| LZSS.c | 1 | 1 | 0 | 0 | 148 | 0 | 0 | 1 | 0 |
| MadLinkIn.c | 13 | 1 | 12 | 0 | 52 | 2,813 | 0 | 13 | 0 |
| Matrix.c | 20 | 16 | 4 | 0 | 1,576 | 757 | 0 | 20 | 0 |
| Memory.c | 14 | 13 | 1 | 0 | 4,040 | 203 | 0 | 7 | 0 |
| Mesh.c | 33 | 14 | 19 | 0 | 2,329 | 18,500 | 0 | 33 | 0 |
| MeshAnim.c | 13 | 6 | 7 | 0 | 700 | 1,882 | 0 | 13 | 0 |
| MEX.c | 11 | 2 | 9 | 0 | 178 | 2,977 | 0 | 10 | 0 |
| MultiplayerGame.c | 175 | 167 | 7 | 1 | 31,337 | 6,220 | 19 | 32 | 0 |
| MultiplayerLANGame.c | 53 | 43 | 10 | 0 | 8,169 | 4,781 | 0 | 10 | 0 |
| StubTransform.c | 5 | 4 | 1 | 0 | 880 | 160 | 0 | 4 | 0 |

### Game n-s (1,000 functions, 270,647 bytes, 33.2% of bytes IDENTICAL)

| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| NavLights.c | 2 | 1 | 1 | 0 | 53 | 581 | 0 | 2 | 0 |
| Nebulae.c | 28 | 25 | 3 | 0 | 11,280 | 4,852 | 0 | 11 | 0 |
| NetCheck.c | 17 | 14 | 3 | 0 | 1,887 | 511 | 0 | 10 | 0 |
| NIS.c | 139 | 114 | 25 | 0 | 21,585 | 12,704 | 0 | 139 | 0 |
| Objectives.c | 14 | 8 | 5 | 1 | 560 | 1,595 | 257 | 14 | 0 |
| ObjTypes.c | 29 | 6 | 23 | 0 | 237 | 1,243 | 0 | 7 | 0 |
| Options.c | 89 | 59 | 28 | 2 | 7,134 | 7,753 | 115 | 89 | 0 |
| Particle.c | 132 | 34 | 98 | 0 | 7,171 | 21,638 | 0 | 73 | 0 |
| Physics.c | 7 | 3 | 4 | 0 | 2,075 | 3,571 | 0 | 5 | 0 |
| PiePlate.c | 13 | 3 | 9 | 1 | 452 | 5,348 | 955 | 13 | 0 |
| Ping.c | 18 | 16 | 2 | 0 | 1,922 | 2,286 | 0 | 18 | 0 |
| PlugScreen.c | 8 | 4 | 4 | 0 | 1,233 | 2,052 | 0 | 8 | 0 |
| Randy.c | 8 | 7 | 1 | 0 | 501 | 209 | 0 | 4 | 0 |
| Region.c | 31 | 30 | 1 | 0 | 4,889 | 376 | 0 | 9 | 0 |
| ResCollect.c | 19 | 6 | 12 | 1 | 736 | 4,044 | 109 | 14 | 0 |
| ResearchAPI.c | 21 | 6 | 15 | 0 | 113 | 5,169 | 0 | 21 | 0 |
| ResearchGUI.c | 18 | 1 | 13 | 4 | 90 | 4,782 | 3,190 | 18 | 0 |
| SaveGame.c | 86 | 62 | 24 | 0 | 8,687 | 15,994 | 0 | 54 | 0 |
| ScenPick.c | 15 | 8 | 7 | 0 | 1,097 | 3,362 | 0 | 15 | 0 |
| Scroller.c | 6 | 6 | 0 | 0 | 530 | 0 | 0 | 6 | 0 |
| Select.c | 29 | 14 | 11 | 4 | 1,364 | 10,476 | 1,631 | 19 | 0 |
| Sensors.c | 19 | 9 | 10 | 0 | 1,833 | 6,440 | 0 | 19 | 0 |
| Shader.c | 20 | 18 | 2 | 0 | 3,444 | 1,615 | 0 | 20 | 0 |
| ShipSelect.c | 52 | 20 | 32 | 0 | 1,249 | 3,857 | 0 | 46 | 0 |
| ShipView.c | 4 | 0 | 4 | 0 | 0 | 2,421 | 0 | 4 | 0 |
| SinglePlayer.c | 46 | 10 | 35 | 1 | 1,071 | 15,681 | 134 | 46 | 0 |
| SoundEvent.c | 32 | 22 | 10 | 0 | 2,216 | 15,108 | 0 | 32 | 0 |
| SoundEventPlay.c | 3 | 1 | 2 | 0 | 53 | 3,054 | 0 | 2 | 0 |
| SoundEventStop.c | 2 | 1 | 1 | 0 | 55 | 81 | 0 | 2 | 0 |
| SpeechEvent.c | 19 | 3 | 16 | 0 | 331 | 7,346 | 0 | 19 | 0 |
| Star3d.c | 2 | 2 | 0 | 0 | 669 | 0 | 0 | 2 | 0 |
| Stats.c | 25 | 13 | 9 | 3 | 1,797 | 2,605 | 671 | 15 | 0 |
| StatScript.c | 42 | 32 | 10 | 0 | 3,472 | 6,165 | 0 | 21 | 0 |
| StringSupport.c | 5 | 3 | 2 | 0 | 195 | 685 | 0 | 5 | 0 |

### Game t-z (409 functions, 118,515 bytes, 38.7% of bytes IDENTICAL)

| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Tactical.c | 6 | 4 | 2 | 0 | 244 | 1,985 | 0 | 6 | 0 |
| Tactics.c | 8 | 3 | 4 | 1 | 640 | 315 | 17 | 8 | 0 |
| Task.c | 16 | 13 | 3 | 0 | 1,404 | 1,352 | 0 | 5 | 0 |
| TaskBar.c | 6 | 5 | 1 | 0 | 670 | 679 | 0 | 6 | 0 |
| Teams.c | 6 | 5 | 1 | 0 | 2,142 | 142 | 0 | 6 | 0 |
| Timer.c | 14 | 4 | 10 | 0 | 219 | 1,967 | 0 | 14 | 0 |
| TitanNet.c | 35 | 33 | 2 | 0 | 3,170 | 337 | 0 | 15 | 0 |
| TradeMgr.c | 27 | 24 | 3 | 0 | 2,916 | 1,804 | 0 | 27 | 0 |
| Transformer.c | 2 | 1 | 1 | 0 | 71 | 303 | 0 | 2 | 0 |
| Tutor.c | 63 | 36 | 27 | 0 | 5,562 | 11,099 | 0 | 63 | 0 |
| Tweak.c | 3 | 3 | 0 | 0 | 205 | 0 | 0 | 2 | 0 |
| Twiddle.c | 2 | 2 | 0 | 0 | 55 | 0 | 0 | 2 | 0 |
| UIControls.c | 47 | 42 | 5 | 0 | 15,450 | 1,899 | 0 | 47 | 0 |
| Undo.c | 2 | 2 | 0 | 0 | 62 | 0 | 0 | 2 | 0 |
| Universe.c | 56 | 27 | 29 | 0 | 5,923 | 15,192 | 0 | 41 | 0 |
| UnivUpdate.c | 98 | 43 | 54 | 1 | 5,889 | 34,040 | 386 | 98 | 0 |
| Vector.c | 14 | 2 | 12 | 0 | 247 | 999 | 0 | 14 | 0 |
| Volume.c | 4 | 3 | 1 | 0 | 976 | 154 | 0 | 4 | 0 |

### Ships (137 functions, 27,321 bytes, 29.7% of bytes IDENTICAL)

| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AdvanceSupportFrigate.c | 2 | 1 | 1 | 0 | 32 | 109 | 0 | 1 | 0 |
| Carrier.c | 8 | 1 | 6 | 1 | 19 | 845 | 38 | 8 | 0 |
| DefaultShip.c | 4 | 2 | 2 | 0 | 112 | 287 | 0 | 3 | 0 |
| DefenseFighter.c | 10 | 7 | 2 | 1 | 653 | 127 | 12 | 1 | 0 |
| DFGFrigate.c | 3 | 1 | 2 | 0 | 41 | 957 | 0 | 2 | 0 |
| FloatingCity.c | 1 | 1 | 0 | 0 | 51 | 0 | 0 | 1 | 0 |
| GenericDefender.c | 3 | 1 | 2 | 0 | 580 | 1,150 | 0 | 2 | 0 |
| GenericInterceptor.c | 13 | 3 | 8 | 2 | 376 | 5,751 | 26 | 13 | 0 |
| GravWellGenerator.c | 18 | 8 | 10 | 0 | 385 | 2,913 | 0 | 12 | 0 |
| HeavyCorvette.c | 6 | 5 | 1 | 0 | 1,153 | 79 | 0 | 3 | 0 |
| HeavyCruiser.c | 1 | 0 | 1 | 0 | 0 | 115 | 0 | 1 | 0 |
| IonCannonFrigate.c | 1 | 0 | 1 | 0 | 0 | 99 | 0 | 1 | 0 |
| MinelayerCorvette.c | 13 | 10 | 3 | 0 | 2,750 | 1,389 | 0 | 5 | 0 |
| MissileDestroyer.c | 5 | 4 | 1 | 0 | 855 | 96 | 0 | 2 | 0 |
| Mothership.c | 8 | 0 | 5 | 3 | 0 | 1,400 | 272 | 8 | 0 |
| MultiGunCorvette.c | 2 | 1 | 1 | 0 | 17 | 86 | 0 | 1 | 0 |
| P1IonArrayFrigate.c | 3 | 0 | 3 | 0 | 0 | 267 | 0 | 3 | 0 |
| P1MissileCorvette.c | 5 | 3 | 2 | 0 | 265 | 232 | 0 | 2 | 0 |
| P1Mothership.c | 2 | 2 | 0 | 0 | 101 | 0 | 0 | 2 | 0 |
| P1StandardCorvette.c | 3 | 0 | 3 | 0 | 0 | 380 | 0 | 3 | 0 |
| P2Swarmer.c | 1 | 1 | 0 | 0 | 41 | 0 | 0 | 1 | 0 |
| ProximitySensor.c | 4 | 2 | 2 | 0 | 272 | 544 | 0 | 2 | 0 |
| RepairCorvette.c | 9 | 6 | 3 | 0 | 320 | 1,623 | 0 | 5 | 0 |
| ResourceCollector.c | 2 | 1 | 1 | 0 | 31 | 54 | 0 | 1 | 0 |
| SalCapCorvette.c | 8 | 0 | 3 | 5 | 0 | 153 | 74 | 8 | 0 |
| StandardDestroyer.c | 1 | 0 | 1 | 0 | 0 | 121 | 0 | 1 | 0 |
| StandardFrigate.c | 1 | 1 | 0 | 0 | 68 | 0 | 0 | 1 | 0 |

### other (654 functions, 247,348 bytes, 38.5% of bytes IDENTICAL)

| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| bink.c | 1 | 1 | 0 | 0 | 42 | 0 | 0 | 1 | 0 |
| Color.c | 7 | 7 | 0 | 0 | 2,270 | 0 | 0 | 7 | 0 |
| dct.c | 2 | 1 | 1 | 0 | 496 | 113 | 0 | 2 | 0 |
| ddraw.cpp | 5 | 3 | 2 | 0 | 512 | 148 | 0 | 2 | 0 |
| debugwnd.c | 9 | 7 | 2 | 0 | 1,524 | 1,360 | 0 | 9 | 0 |
| font.c | 21 | 18 | 3 | 0 | 4,045 | 1,697 | 0 | 21 | 0 |
| fqcodec.c | 3 | 0 | 3 | 0 | 0 | 1,372 | 0 | 3 | 0 |
| fqeffect.c | 6 | 3 | 3 | 0 | 67 | 490 | 0 | 6 | 0 |
| fquant.c | 3 | 0 | 3 | 0 | 0 | 1,270 | 0 | 3 | 0 |
| glcaps.c | 8 | 8 | 0 | 0 | 1,472 | 0 | 0 | 8 | 0 |
| glcompat.c | 27 | 25 | 2 | 0 | 8,933 | 2,313 | 0 | 27 | 1 |
| Light.c | 11 | 11 | 0 | 0 | 1,967 | 0 | 0 | 11 | 0 |
| main.c | 28 | 11 | 17 | 0 | 1,273 | 15,298 | 0 | 28 | 0 |
| mainrgn.c | 32 | 9 | 22 | 1 | 1,054 | 15,526 | 960 | 32 | 0 |
| Md52.cpp | 4 | 4 | 0 | 0 | 2,568 | 0 | 0 | 0 | 0 |
| mixfft.c | 10 | 10 | 0 | 0 | 4,916 | 0 | 0 | 10 | 0 |
| mouse.c | 20 | 13 | 7 | 0 | 737 | 7,699 | 0 | 20 | 0 |
| prim2d.c | 26 | 25 | 1 | 0 | 7,673 | 425 | 0 | 26 | 0 |
| prim3d.c | 12 | 12 | 0 | 0 | 3,409 | 0 | 0 | 12 | 0 |
| Queue.c | 8 | 8 | 0 | 0 | 446 | 0 | 0 | 3 | 0 |
| render.c | 32 | 23 | 9 | 0 | 6,615 | 9,254 | 0 | 32 | 0 |
| rglu.c | 3 | 3 | 0 | 0 | 976 | 0 | 0 | 3 | 0 |
| rinit.c | 3 | 1 | 2 | 0 | 40 | 653 | 0 | 3 | 0 |
| rinit.cpp | 5 | 4 | 1 | 0 | 236 | 1,822 | 0 | 5 | 0 |
| screenshot.c | 1 | 0 | 1 | 0 | 0 | 710 | 0 | 1 | 0 |
| smixer.c | 8 | 5 | 3 | 0 | 494 | 3,285 | 0 | 6 | 0 |
| soundlow.c | 25 | 19 | 6 | 0 | 3,185 | 1,486 | 0 | 17 | 0 |
| sstream.c | 13 | 8 | 5 | 0 | 1,582 | 1,712 | 0 | 9 | 0 |
| Subtitle.c | 22 | 17 | 5 | 0 | 3,430 | 2,433 | 0 | 22 | 0 |
| texreg.c | 44 | 33 | 11 | 0 | 11,848 | 8,248 | 0 | 6 | 0 |
| TimeoutTimer.c | 4 | 4 | 0 | 0 | 233 | 0 | 0 | 4 | 0 |
| Titan.c | 4 | 3 | 1 | 0 | 256 | 71 | 0 | 4 | 0 |
| TitanInterface.cpp | 141 | 46 | 84 | 11 | 7,359 | 55,986 | 1,160 | 36 | 3 |
| Trails.c | 32 | 25 | 7 | 0 | 8,197 | 5,230 | 0 | 32 | 0 |
| utility.c | 74 | 60 | 14 | 0 | 7,455 | 11,317 | 0 | 74 | 0 |

## Largest MODIFIED functions

The 60 largest by Cataclysm bytes; `change` is the hand-written line where reviewed, else the tool's delta list.

| address | function | HW1 file | bytes | conf | reviewed | change |
|---|---|---|---:|---|---|---|
| 0x0051cd10 | soundEventUpdate | SoundEvent.c | 11,044 | high | yes | Cataclysm's object sounds: derelict ambients only updated; engine limits spare the mothership class and the Sentinels; idle engines fade over 5 s; upgraded engine loop 0x404; heading filter on every ship; per-type special loops (mimic, Leech, rammers, Workers, Sentinel field x3, big gun, cloakers, g |
| 0x004e0640 | partFillGenericParticles | Particle.c | 6,718 | high |  | adds calls cat_rndLensFlareSourceAdd, memAllocFunction, ranRandomFn; body 2.4x larger than predicted; fields +m11, m21 -additiveBlends, ang, angDelta, angDeltaDist (+81) |
| 0x004b1d40 | meshSpecObjectRender | Mesh.c | 6,038 | high | yes | the fade's material switch (default / alternative, cat_meshCurrentMaterialAlt), a forced switch at polygon 0x00c32c0c (0 in .bss: the first), and with enableCPV each vertex lit with its info entry and state (meshSpecColour inlined) |
| 0x0044b950 | clProcess | CommandLayer.c | 5,547 | high | yes | the waypoint moves per ship, the saved looping move after an attack, the force-field sphere around a protected ship, orders 11-16 (link, unlink, point, mimic, research), no assertion in the default case |
| 0x00414610 | aishipFlyToPointAvoidingObjsFunc | AIShip.c | 5,298 | high | yes | adds calls cat_mimicGetAvoidRadius, cat_salvageGetSalvagerInfo, cat_shipGetSpeedMultiplier, cat_shipIsCloakedFully, sub_53F9F0; drops calls to attackSimple, proximityCanPlayerSeeShip (absent from Cataclysm); adds strings "\nWarning: ship %s %s flying with no ..."; fields +attack, needFix, next, objl |
| 0x005905b0 | TitanInterface_PeerHandleChallenge | TitanInterface.cpp | 4,942 | medium |  | adds calls DirHandleGetHWDirectory, DirHandleGetTitanServers, DirStartPeerLogin, RequestDirectory, cat_TitanInterface_ContestStartPeerLogin, cat_TitanInterface_ProfileStartPeerLogin (+2); drops calls DirFailOver; adds strings "NOTE: TitanInterface::RequestDirector..."; "TitanInterface::ContestFailOv |
| 0x005197d0 | singlePlayerGameUpdate | SinglePlayer.c | 4,883 | high | yes | KAS globals G_PlayNis / G_PlayNislet / Traders.PopUpGUI; the black fade; HYPERSPACE_LOADDONE starts the mission's kas2c script (missions 1..17); states 7 (wait) and 9 (the fleet arrives, G_ShipsHaveArrived); the rest of the hyperspace flow |
| 0x005764e0 | rndMainViewRenderFunction | render.c | 4,440 | high | yes | Cataclysm's main view: cat_rndRenderObjectList with the deferred depth-sorted pass, render-time LOD timing, the fog layer, blasts, lens flares, the Sentinel fields, motion cues (cat_mcRender); rndMainViewRenderFunction: the motion cues (cat_mcRender at 0x00577360 with catMotionCues) ported |
| 0x00561f10 | WindowProc | main.c | 4,248 | high | yes | adds calls cat_consoleKeyPress, cat_glideIsLoaded, cat_glideShutdown, cat_overlayIconsDiscardGL, cat_rndFogLayerTextureUnload, cat_rndLensFlareTextureUnload (+4); drops calls to mouseLClick (absent from Cataclysm); adds strings "couldn't initialize default rendering..."; globals +MAIN_WindowDepth, M |
| 0x005b2620 | GenericInterceptorAttack | GenericInterceptor.c | 4,198 | high | yes | bombing run only kAttackBomber/tAttackBomber and never at ships; KILL bullet speed via cat_gunGetBulletSpeed; break formation from the ship command; no R1-mothership case |
| 0x0058c8e0 | TitanInterface_GetPatch | TitanInterface.cpp | 3,601 | medium |  | adds calls GetPatch, WONMisc__EasySocket__stopWinsock, titanDebug; drops calls to titanGetPatchFailedCB (absent from Cataclysm); adds strings "Content-Length: %d"; "ET /"; "FAIL TitanInterface::GetPatch PATCHFA..." (+16); parameters 2 -> 1: void TitanInterface_GetPatch(void * theArgs); body 26.6x la |
| 0x004dc6f0 | partRenderBillSystem | Particle.c | 3,471 | high | yes | separate X/Y half sizes (scale, scaleY at +0x54); rndBillboardEnable/Disable; colour bias through glPixelTransferf when the 0x9004 capability exists; per-particle fog toggle; one glColor per particle |
| 0x0056d670 | mouseStartup | mouse.c | 3,458 | high | yes | 31 cursor bitmaps (13 new), each mirrored; salvage.LiF for the salvage cursor |
| 0x00436aa0 | ellipsoid_seq | Clouds.c | 3,418 | high | yes | DUSTCLOUD_RAN_MULTIPLIER random vertex offset against ad1bb82 |
| 0x00569260 | mrRegionDraw | mainrgn.c | 3,355 | high | yes | cat_moRender, cat_shipPrintDraw, cat_ioUpdateHover, cat_shipPrintDragUpdate after the tick text; no toLegendDraw / mrCommandMessageDraw |
| 0x005a9290 | gameStart | utility.c | 3,241 | high | yes | (not reviewed in full) a new game calls cat_mimicInitDisguiseTables; in a non-tutorial new game the start-position table 0x00ab77e0 is set to 0..7 and shuffled with 20 swaps of two ranRandomFn(0) % spScenarios[spCurrentSelected].maxplayers picks; the start-position shuffle: 20 swaps of two ranRandom |
| 0x00424110 | battlePingEvaluate | Battle.c | 3,177 | high | yes | runs at most every 8 s per ping (new battleping +0x28) and only with a non-exempt local ship; battle assessments are the fleet's lines with ship answers carrying the speaker's type; retreat marks Ship.catRetreating; damaged-capital report by the fleet or the PrinceOfSpace line 0x200135e; loss-depend |
| 0x005312b0 | tutDrawTextPointers | Tutor.c | 3,147 | medium | yes | the pointers' colour (red a quarter, green the pulse, blue two thirds); the box's top under the NIS letterbox; an objective pointer drawn only in the sensors manager (not zooming, the option on, no letterbox) and its line starting at the objective list |
| 0x00568100 | toDrawMoveToLine | mainrgn.c | 3,141 | high | yes | first draws the move order's waypoint path legs still to fly (once / loop / patrol out / patrol back, colours 0x0088d78c/90/94, a circle at each point); the end circle inline (primCircleOutline3) |
| 0x005209f0 | soundEventPlay | SoundEventPlay.c | 3,032 | high | yes | Cataclysm's player: no out-of-view exception; the 92-row LUT layout; UI sounds per race and 0x1c8 -> ship 0x821; gun sounds by cat_gunGetSoundType with the Beast weapon on row 9 and GunSpecEvents.lut for the multibeam frigates, dAcolyte, Dreadnought and destroyers; the burst remembers its gun index; |
| 0x00483620 | FormationCalculateOffsets | Formation.c | 3,017 | high | yes | force-field sphere through formationSpecificsSphere with disable reason 5; force-field walls (SetFormationOffsetsForForceFieldWall); other formations clear reason 5 |
| 0x00566b60 | mrObjectClick | mainrgn.c | 2,983 | high | yes | the modifier keys first (force attack, multiple select, guard, special popup, focus, attack); then the context order of the Homeworld CSA option; else the formation select |
| 0x00506720 | selCircleCompute | Select.c | 2,968 | high |  | adds calls cat_mimicGetSelectRadius; drops calls fmathSqrtDouble; calls with a changed parameter count: selShipUnderMouse 2->1; fields +belongto, cameraDistanceSquared, flashtimer, health (+6) -collPosition, collspheresize, corner, currentLOD (+20) |
| 0x004b34f0 | meshMorphedObjectRender | Mesh.c | 2,964 | high | yes | the fade's material switch; shSpecularColour's two new arguments 0, 0 |
| 0x0048aac0 | missileShoot | Gun.c | 2,790 | high | yes | no firing by or at a ship being handed to a salvage depot; the missile's damage type is the gun's current one; damage = ship multiplier x weapon vs armour; mesh state; a second effect parameter by launcher type; no cloak-field decloak; the KAS damage factor is Ship +0x3E0 (catDamageFactor) |
| 0x0053d2a0 | universeStaticInit | Universe.c | 2,755 | high | yes | nebulae never closed; 7 mine statics, the Derelict race's always loaded; maxhealthBase and the single-player difficulty health; no Mothership immobile |
| 0x004cd2b0 | nisStart | NIS.c | 2,733 | high | yes | ported: catSmHideViewport cleared, cat_rndLensFlareNISSourceAdd, cat_rndFogLayerOff, the fadeSet flag 0x00a4cb4c cleared with the fades; the RGL hook (0x00b3a6d8/0x00b3a610) is GoK-backend; nisFocusMaxRadius is inlined (same formula) |
| 0x00441c10 | processAttackToDo | CommandLayer.c | 2,670 | high | yes | adds calls battleChatterFleetAttempt, cat_fowCanPlayerSeeShip, cat_gunPickMothershipModuleTarget, cat_shipIsCrystal, clWrapHalt; drops calls battleCanChatterAtThisTime, battleChatterAttempt; adds strings "\nBad Stuff Going On!!! Ship with no ..."; calls with a changed parameter count: processMoveToD |
| 0x0054c3f0 | univUpdate | UnivUpdate.c | 2,605 | high | yes | frame step, multiplayer autosave, real-time quit/win, Cataclysm speech ids, no tacticsGlobalUpdate, blasts and bullet-effect recreation after the ship update, inline dead-object loops, KAS save |
| 0x004e7a70 | piePointSpecDraw | PiePlate.c | 2,501 | high | yes | big-gun range circle, sensors depth test, cat_smHeightCircleColorGet, the readout only with the mouse visible, the plane distance |
| 0x0057f850 | isoundmixerprocess | smixer.c | 2,494 | high | yes | no effect, q-noise, delay or acoustic-model stage and no effect scale; volume clamps and the SFX stop test rounded with rint; a channel without a patch is released; the decode inlined |
| 0x004a13f0 | scriptSetShipsCB | LevelLoad.c | 2,450 | high | yes | cat_kasHashString teams; no harvest/carrier gates; RacesAllowedForGivenShip before the race swap; cat_shipInitResearchUpgrades per ship; cat_rmResearchModuleActivated for Sect; clFormation 4 args |
| 0x0053bab0 | InitStatShipInfo | Universe.c | 2,432 | high | yes | adds calls ShipTypeToNiceStr, cat_univInitShipTypeFlags, dockFindDockStaticPoint; drops calls precalcStaticInfoGuidance; drops calls to dockInitializeCustomFunctions, mexGetTractorBeamStatic (absent from Cataclysm); adds strings "\nSHP file loaded:  %s"; "\nWARNING: could not set gunbindings ..."; " |
| 0x00587a50 | trRegistryRefresh | texreg.c | 2,421 | medium | yes | size-sort qsort + trCramRAMScaleCompute/trCramIntoRAM replaced by one call to trCramRAMComputeAndScale(); trMeshSortList nulled after free |
| 0x00542320 | univCreateShip | UnivUpdate.c | 2,417 | high | yes | 7 arguments, adds the ship to the universe itself, 0x63C-byte ship with gun/mad/precise blocks, droids, damage points, upgrades, dock info, external build, mothership modules, experience, stealth, dissolve, names, FOW, mesh state; univCreateShip: the repair-droid array (droidCreateArray at 0x0054265 |
| 0x00413240 | airResourceManager | AIResourceMan.c | 2,349 | high | yes | research requests only in multiplayer with research on; collectors with harvesting on (or single player) by race; one processor (Sect with a docking bay, Beast); a multiplayer carrier near the support-unit cap (0x00413929) |
| 0x00460ed0 | cratesRewardPlayer | Crates.c | 2,334 | high | yes | gamerand()/2^32 probabilities; per-race 12-step ship ladders and 20-step technology ladders (double thresholds 0x0072a470..0x0072a7b0); up to 5 tech draws; no mothership gift, no SHIP_PROBS pick |
| 0x00444810 | RemoveShipsFromDoingStuff | CommandLayer.c | 2,321 | high | yes | a third argument (the exe's unread reason code); per order as the exe's jump table 0x00445124: move/hyperspace clear moveTo/moveFrom, link/unlink break the link, the point and build orders keep their ships, an emptied command is freed; a formation drops to a plain command at one ship |
| 0x005053a0 | selRectDragFunction | Select.c | 2,312 | high | yes | fourth argument centrePoint (the centre-point test); untargetable ships skip the selectable/targetable/hidden tests; KAS derelicts out except the big gun's units; enemy mimics posing as allied; mines (kasfShowMinesInSensors) not hidden by FOW; sensors-manager enemy selectability; fully cloaked undet |
| 0x005969e0 | TitanInterface_HandleCloseCmd | TitanInterface.cpp | 2,215 | high |  | adds calls FirewallDetect, PrintAddress, RequestDirectory, cat_TitanInterface_ContestStartPeerLogin, cat_TitanInterface_ProfileStartPeerLogin, mgDisplayMessage (+1); drops calls DirFailOver, FirewallFailOver; adds strings "TitanInterface::ContestFailOver"; "TitanInterface::ContestFailOver FAILE..."; |
| 0x00515010 | svShipViewRender | ShipView.c | 2,203 | high | yes | the automatic turn (cat_cameraSetAngle, one turn in 6 s), the look-at point with only z negated, near plane 5.0, the clip-plane fade-in (solid then wireframe pass, cat_rndObjectEffectsSetup(NULL)), name/cost/hot key at 10 px, the CM_ExtendedInfo backdrop and cat_svShipDescriptionDraw; HW1's six stat |
| 0x004c9430 | nebDrawTendril | Nebulae.c | 2,191 | high | yes | shSpecularColour's two new arguments 0, 0 |
| 0x00439130 | cloudUpdateSystem | Clouds.c | 2,161 | high |  | adds calls cloudRealDist, fmathSqrtDouble, vecNormalize; fields +x, y, z; globals +CLOUD_LIGHTNING_MAXDIST |
| 0x0046bb00 | etgEffectCodeLoad | ETG.c | 2,152 | high | yes | forward-reference label lookups case-insensitive (_stricmp) |
| 0x004045b0 | aifBuildRequestedShips | AIFleetMan.c | 2,132 | high | yes | a dead mothership builds nothing; external non-Beast, non-Component ships need a docking bay; the resource request is mapped to (or dropped for) the support module; overflow tests on the unrounded double and NaN-safe compares; the resource request first only when affordable |
| 0x005631f0 | WinMain | main.c | 2,117 | high | yes | adds calls cat_glCapWin2K; adds strings "CataclysmPatch.exe"; "CloseCataclysm"; "Emergence" (+2); drops strings "Homeworld"; library calls +glCapLoadOpenGL -strcmp, strlen; loops 1 -> 3 |
| 0x004b8690 | mgShowScreen | MultiplayerGame.c | 2,106 | high |  | adds strings "COPPA_Message"; "Registration"; body 2.6x larger than predicted; loops 1 -> 6; switch cases 41 -> 43 |
| 0x005abe70 | utyGameSystemsInit | utility.c | 2,053 | high | yes | cat_shipPrintStartup after mrStartup; cat_overlayIconsStartup, cat_shipNamesInit, cat_gameStatsStartup after prim3dStartup |
| 0x004427a0 | delegateCommand | CommandLayer.c | 2,050 | high | yes | the interceptor leader's flip-turn state is read through ship->command (no Ship.formationcommand); a force-field sphere's followers always passive attack, as a sphere's; the minelayer test is tMinelayerCorvette (84) |
| 0x00591980 | TitanInterface_PeerHandleComplete | TitanInterface.cpp | 2,047 | medium |  | adds calls DirHandleGetTitanServers, DirStartPeerLogin, EncryptAndSendRoutingMsg, cat_TitanInterface_ContestSendRequest, cat_TitanInterface_ContestStartPeerLogin, cat_TitanInterface_ProfileSendRequest (+3); drops calls DirFailOver, RegisterRoutingServer; adds strings "Couldn't send RegisterRequest m |
| 0x004b0170 | meshObjectRender | Mesh.c | 2,024 | high | yes | the fade's material switch (default or cat_meshCurrentMaterialAlt), the forced switch at polygon 0x00c32c0c, per-vertex colour with enableCPV (vertex state + info entry, shColourSet0's two new arguments) |
| 0x00450910 | clProcessSyncPacket | CommandNetwork.c | 2,019 | high | yes | adds calls cat_clExternalBuildUpdate, cat_clLink, cat_clMimic, cat_clPointOrder, cat_clResearch, cat_clSetBeastSystem (+7); adds strings "\cataclysm\exe"; calls with a changed parameter count: clFormation 3->4; fields +ShipPtr, TargetID, curPlayerIndex, objNumber (+3) -AllianceMsgType, CurAlliance,  |
| 0x0042f270 | TransferCaptaincyStateMachine | Captaincy.c | 1,960 | high |  | adds strings "Cap:Proposing new captain %d"; fields +HighPart, LowPart, s -enabled; loops 6 -> 8; switch cases 21 -> 14 |
| 0x00482b90 | processFormationToDo | Formation.c | 1,956 | high | yes | the formation is the leader's command; the leader's collision centre; a force-field sphere centred on the protected ship's collision centre and turned with it; cat_formationShipMoveToSteady / NotToObscuredPoints |
| 0x004c1110 | authReceiveReply | MultiplayerGame.c | 1,929 | medium |  | adds calls Enqueue, LockQueue, UnLockQueue; body 3.0x larger than the file's size ratio predicts, with more branches; constants 200 not in body; loops 0 -> 57; switch cases 12 -> 9 |
| 0x004c8b00 | nebDrawChunk2 | Nebulae.c | 1,923 | high | yes | shSpecularColour's two new arguments 0, 0 |
| 0x00402010 | aiaInit | AIAttackMan.c | 1,891 | high | yes | feature tests over both races' types (huge: k/t/bHeavyCruiser, sDreadnought; capture: k/tSalCapCorvette; cloak, grav well: k/t); the resourcing-class test by class; the kamikaze threshold drawn before the roll |
| 0x004d76d0 | opOptionsAcceptHelper | Options.c | 1,879 | high | yes | no auto-LOD read-back of the detail threshold; the device switch also compares the HardwareTransformLighting word; the Win32 RGL / 3dfx device paths |
| 0x005a7310 | trailDraw | Trails.c | 1,835 | high | yes | cat_trailShouldDraw 0x005a7240 decides whether a trail is drawn (clamped Leech, dissolving, SOF_DontDrawTrails / SOF_Disabled, being built, the ramming frigate's inactive trail; moving or pushing); the glow sprite and the segments take the ship's team colour |
| 0x0057e860 | rinEnumerateDevices | rinit.cpp | 1,822 | high | yes | Glide boards through cat_glideQueryBoards and rinMemAlloc |

## REWRITTEN functions

| address | function | HW1 file | bytes | conf | reviewed | change |
|---|---|---|---:|---|---|---|
| 0x004592a0 | cmDeterministicBuildProcess | ConsMgr.c | 1,239 | high | yes | the external build (slot wait, ship created, attach wait, mode-2/0 updates, cat_cmBuildJobFinished), Beast factor; ported (CAT-FIX: NULL entry) |
| 0x004f7140 | rmResearchGUIBegin | ResearchGUI.c | 1,197 | high | yes | gates, the Beast screen, the race list, the lab buttons, sounds 0x12f/0x106, the default picture; ported |
| 0x004f6d30 | rmUpdateTechList | ResearchGUI.c | 1,036 | high | yes | the exe's tech-set states (held, researching, tutorial / mission allowance 0x00ba4680, multiplayer requirements and the crystals option (technology 0x22 TECH_CRYSTAL_HARVEST) [audit: M2-R6 managers: said the infection op |
| 0x00567930 | mrRegionProcess | mainrgn.c | 960 | high | yes | selSelectionClick's seventh argument, piePointModeOnOff(mode, NULL) |
| 0x004e76b0 | pieMovePointClipToLimits | PiePlate.c | 955 | high | yes | ellipse-intersection clipping (sizeX/Y/Z params) replaced by clamping both points to the box +-(smUniverseSize - margin) |
| 0x004f5100 | rmDrawLabButton | ResearchGUI.c | 946 | high | yes | the module icons by lab (off/on), the empty-slot box, the progress bar with the exe's chop rounding and the pulse line; ported (no glcompat path) |
| 0x00458390 | cmConstructionBegin | ConsMgr.c | 809 | high | yes | piePointModeOnOff(0, NULL) |
| 0x00525640 | statsGatherFightStats | Stats.c | 635 | high | yes | per type pair, fights 1..14 ships through cat_statsSetupShipsForFight / cat_statsGetPostCombatStats into the 60x60x5 table |
| 0x00455410 | cmUpdateShipAvailable | ConsMgr.c | 616 | high | yes | kind-2 rows, rmCanBuildShip's 3rd argument, caps (stat 4), the carrier support module, support units, the factory-off / hyperspace test; ported |
| 0x0048b5b0 | gunShoot | Gun.c | 474 | high | yes | returns the bullet made by cat_gunCreateBullet 0x0048bb00; RKO, salvage-depot and line-of-sight checks; damage = ship multiplier x DamageMult and weapon tables; a Leech shot also does 10 damage; the KAS damage factor is  |
| 0x00507f10 | selStatusDraw0 | Select.c | 470 | high | yes | Cataclysm's status bars: cat_selStatusRectCompute + cat_selStatusBarsDraw (health, ACV EMP, resources, beast, dreadnought, big gun, cloak, gravwell, sentinel, upgrade rows, the disabled X, experience stars, hot key, ammo |
| 0x005080f0 | selStatusDraw1 | Select.c | 449 | high | yes | as selStatusDraw0 at the LOD 1 size |
| 0x005082c0 | selStatusDraw2 | Select.c | 446 | high | yes | as selStatusDraw0 at the LOD 2 size |
| 0x0058dd30 | TitanInterface_ConnectTo | TitanInterface.cpp | 404 | medium |  | drops calls BuildAddress, ConnectTo; body 2.5x larger than predicted; fields -AddrPart, IP, Port, etherAddr; globals +CONNECT_TIMEOUT; library calls +WONMisc__OpenCmd__OpenCmd, WONMisc__RecvPrefCmd__RecvPrefCmd, WONMisc_ |
| 0x00545540 | ApplyDamageToCollidingObjects | UnivUpdate.c | 386 | high | yes | no collision damage except a killer collision (attribute bit 0x200 on exactly one object): both take the smaller health, plus a hit effect (etgGunEventTable[0][16][1]) on the killer |
| 0x00457e50 | cmCanBuildShipType | ConsMgr.c | 360 | high | yes | the factory race's list, external build only at a mothership-class factory with a free slot, rmCanBuildShip's 3rd argument, kind-2 enable test; ported |
| 0x00465360 | RemoveShipFromDocking | Dock.c | 266 | medium | yes | adds calls ShipTypeToStr, madAnimationStart; drops calls madLinkInPostDockingShip, memFree; adds strings "\nDOCKING CODE: A %s is clamped to so..."; body 3.3x smaller than predicted; constants 19, 26, 41, 67 not in body; |
| 0x00508480 | selStatusDraw3 | Select.c | 266 | high | yes | as selStatusDraw0 at the LOD 3 size |
| 0x004d66f0 | poPopupFleetIntelligence | Objectives.c | 257 | high | yes | tutorial: the global KAS variable hash('G_CLICK_<label>') (scope 0) set to 1, then the objective's fleet intelligence shown unless it has no text (then subMessageEnded = 0); mission: shown if it has fleet intelligence (H |
| 0x004ab4b0 | ioStartup | InfoOverlay.c | 220 | high | yes | Cataclysm's overlay: MainOverlay.script print options (cat_moOptionSetCB), cat_ioReset; no HW1 list window region |
| 0x00594d10 | TitanInterface_SendPing | TitanInterface.cpp | 200 | medium |  | drops calls titanDebug; drops strings "EXCEPTION: TitanInterface::SendPing: %s"; params 2 -> 4 (prototype); fields +AddrPart, IP, Port -Pack, what; library calls +Ordinal, WONMisc__EasySocket__getSockAddrIpx, WONMsg__MMs |
| 0x0058d700 | TitanInterface_GetPatchArgs_dtor | TitanInterface.cpp | 179 | low | yes | compiler-generated destructor of a patch-arguments object; no HW1 source counterpart |
| 0x005bba70 | MothershipRemoveShipReferences | Mothership.c | 178 | high | yes | big-gun module loss stops the big gun; launch-manager module slots; Beast weapon and depot |
| 0x00465590 | RemoveShipFromLaunching | Dock.c | 156 | medium | yes | drops calls battleLaunchWelcomeExchange, madLinkInPostDockingShip; body 4.5x smaller than predicted; constants 19, 26, 41, 67 not in body; loops 3 -> 0 |
| 0x0058ded0 | TitanInterface_ConnectAndSend | TitanInterface.cpp | 136 | high |  | adds calls ConnectTo, SendMsg, titanDebug; drops calls BuildAddress, ConnectAndSend; adds strings "FAIL: TitanInterface::SendMsg!"; params 4 -> 6 (prototype); fields -AddrPart, IP, Port, etherAddr; library calls +WONMisc |
| 0x0051bdb0 | singlePlayerStartGame | SinglePlayer.c | 134 | high | yes | a new game plays nis\n01.nis in the staging level Mission01_NIS01 (NIS-first) |
| 0x004f1730 | AsteroidTakesDamage | ResCollect.c | 109 | medium | yes | asteroid damage model replaced: a value converted with ftol is stored in an asteroid field and the function returns TRUE when targetWasAlive and resourceVolume <= 0; HW1's RU damage ratio, resourcevalue decrement, health |
| 0x00417e60 | aitMsgReceived | AITeam.c | 96 | high | yes | a receive compares the message pointer and moves the last entry into the freed slot (no string compare, no free); the message is matched by its kas2c hash word |
| 0x00424ef0 | battleLaunchWelcomeExchange | Battle.c | 94 | high | yes | takes (shipclass, variable): the fleet's launch / welcome lines, else the build report 0x200103d; called by cat_cmBuildJobFinished |
| 0x0042ec90 | captaincyLog | Captaincy.c | 85 | high | yes | vsprintf, two stripped debug calls, clCommandMessage(buffer, 0xffffffff) when echoing; no log file |
| 0x005bb730 | MothershipStaticInit | Mothership.c | 82 | medium | yes | one block, ranges only, no special-door key table; Beast-weapon tweaks for bMothership |
| 0x00495950 | kasfShipsSelectIndex | KASFunc.c | 78 | high | yes | the new selection is cleared and set only for a valid index (kasGrowSelectionClear, growSelectAddShip) |
| 0x00490aa0 | kasPathPtr | KAS.c | 75 | high | yes | drops calls dbgFatalf; adds strings "\nKAS: unresolved reference to path %u"; constants 47 not in body |
| 0x00495a50 | kasfShipsSelectNearby | KASFunc.c | 75 | high | yes | through cat_kasShipsSelectByPredicate (also in place), the squared distance rounded once to float |
| 0x00417e10 | aitMsgSend | AITeam.c | 73 | high | yes | the queue holds 64 message pointers and a count; a send stores the pointer (no copy, no sender); the message is its kas2c hash word |
| 0x00464610 | dockReserveDockPoint | Dock.c | 70 | high | yes | drops calls madLinkInCloseSpecialShip, madLinkInOpenSpecialShip; body 5.0x smaller than predicted; fields -dockInfo, dockpoints, dockstaticpoint, dockvars (+6); constants 26, 41, 67 not in body; loops 2 -> 0 |
| 0x0058f626 | TitanInterface_AuthHandleLogin_tail | TitanInterface.cpp | 67 | high |  | drops calls ReadLoginKey, mgDisplayMessage, titanDebug; drops strings "EXCEPTION: AuthHandleLogin unable to ..."; "EXCEPTION: TitanInterface::AuthHanleL..."; "EXCEPTION: Unable to build AuthLogin ..." (+8); body 14.9x sm |
| 0x0046ae90 | etgLoadErrorf | ETG.c | 64 | high |  | drops strings "\n%s (%d): Non-fatal error - %s"; "Non-Fatal Error (cancel to debug)"; params 4 -> 3 (prototype); body 2.4x smaller than predicted; globals -dbgFatalErrorString, dbgInt3Enabled, ghMainWindow; library calls |
| 0x00491d30 | kasfVarCreateSet | KASFunc.c | 61 | high | yes | the name is a cat_kasHashString hash (bit 31: global, scope 0), scoped by the current scope hash; the hashed KAS variable store (cat_kasVar*) replaces HW1's AIVars |
| 0x00491d70 | kasfVarValueSet | KASFunc.c | 61 | high | yes | the name is a cat_kasHashString hash (bit 31: global, scope 0), scoped by the current scope hash; the hashed KAS variable store (cat_kasVar*) replaces HW1's AIVars |
| 0x004907e0 | kasAITeamShipsPtr | KAS.c | 60 | high | yes | the team's ships are found by its label hash (script teams only); no MsgSender name, no fatal |
| 0x00491e30 | kasfVarValueGet | KASFunc.c | 60 | high | yes | the name is a cat_kasHashString hash (bit 31: global, scope 0), scoped by the current scope hash; the hashed KAS variable store (cat_kasVar*) replaces HW1's AIVars |
| 0x00491e70 | kasfVarDestroy | KASFunc.c | 59 | high | yes | the name is a cat_kasHashString hash (bit 31: global, scope 0), scoped by the current scope hash; the hashed KAS variable store (cat_kasVar*) replaces HW1's AIVars |
| 0x004db400 | opCPUDifficulty | Options.c | 59 | high | yes | a radio button set on tpGameCreated.aiplayerDifficultyLevel (HW1: a 0..100 slider) |
| 0x00491db0 | kasfVarValueInc | KASFunc.c | 57 | high | yes | the name is a cat_kasHashString hash (bit 31: global, scope 0), scoped by the current scope hash; the hashed KAS variable store (cat_kasVar*) replaces HW1's AIVars |
| 0x00491df0 | kasfVarValueDec | KASFunc.c | 57 | high | yes | the name is a cat_kasHashString hash (bit 31: global, scope 0), scoped by the current scope hash; the hashed KAS variable store (cat_kasVar*) replaces HW1's AIVars |
| 0x004db440 | opCPUAttacks | Options.c | 56 | high | yes | a radio button set on tpGameCreated.aiplayerBigotry (HW1: a slider) |
| 0x00447950 | RemoveShipFromFormation | CommandLayer.c | 54 | high | yes | HW1's ship->formationcommand = NULL replaced: sets disable reason 5 when a ship state field is 1, and unclamps sSentinel/bSentinel (types 17/52) |
| 0x004907a0 | kasAITeamPtr | KAS.c | 54 | high | yes | the team is found by its label hash (script teams only); no MsgSender name, no fatal |
| 0x00491cf0 | kasfVarCreate | KASFunc.c | 54 | high | yes | the name is a cat_kasHashString hash (bit 31: global, scope 0), scoped by the current scope hash; the hashed KAS variable store (cat_kasVar*) replaces HW1's AIVars |
| 0x00492960 | kasfGuardShips | KASFunc.c | 53 | high | yes | the team's ships get clWrapProtect on the ships (HW1: a guard-ships move on a copy), only for a non-empty selection |
| 0x00495640 | kasfShipsSelectClass | KASFunc.c | 40 | high | yes | through cat_kasShipsSelectByPredicate (in place: the loop does not advance after a removal) |
| 0x00495690 | kasfShipsSelectType | KASFunc.c | 40 | high | yes | through cat_kasShipsSelectByPredicate |
| 0x00590309 | TitanInterface_AuthHandleRefresh_tail | TitanInterface.cpp | 40 | high |  | drops calls ConnectAndSend, ReadLoginKey, titanDebug; drops strings "AuthHandleRefresh: Login key not found."; "EXCEPTION: TitanInterface::AuthHanleR..."; "FAIL: Couldn't encrypt block with aut..." (+2); params 0 -> 1 (p |
| 0x004901b0 | hsStaticInit | HS.c | 39 | high | yes | no argument: frees the static gate table, zeroes the count |
| 0x005af3c0 | Carrier_Fix | Carrier.c | 38 | medium | yes | Beast weapon and depot fix-up |
| 0x0058e990 | TitanInterface_PingHandleReply_tail | TitanInterface.cpp | 33 | high |  | drops calls titanDebug; drops strings "EXCEPTION: TitanInterface::PingHandle..."; "TitanInterface::PingHandleReply"; params 1 -> 0 (prototype); body 6.1x smaller than predicted; fields -Enter, GetLag, Leave, what |
| 0x004957d0 | kasfShipsSelectDamaged | KASFunc.c | 31 | high | yes | health <= percent * maxhealth * 0.01f (x87, the percent zero-extended); through cat_kasShipsSelectByPredicate |
| 0x00495b00 | kasfShipsSelectSpecial | KASFunc.c | 31 | high | yes | through cat_kasShipsSelectByPredicate; flag 1 (returning technology) is always FALSE; armed: not the cloak generators / salvage corvettes |
| 0x00495560 | kasfShipsSelectEnemy | KASFunc.c | 28 | high | yes | player 0's ships that player 1 has detected (cat_shipDetectedByPlayer) and that do not pass for his (cat_mimicAppearsAlliedTo), not targeted by salvagers (specialFlags2 0x2000000), not swarmers; through cat_kasShipsSelec |
| 0x00495620 | kasfShipsSelectFriendly | KASFunc.c | 28 | high | yes | the computer players' ships, not targeted by salvagers, not swarmers or crystals; through cat_kasShipsSelectByPredicate |
| 0x004956e0 | kasfShipsSelectCapital | KASFunc.c | 28 | high | yes | the static's capital-ship word (+0x2A8) instead of the class test; through cat_kasShipsSelectByPredicate |
| 0x00495730 | kasfShipsSelectNonCapital | KASFunc.c | 28 | high | yes | the negated capital-ship word; through cat_kasShipsSelectByPredicate |
| 0x00495790 | kasfShipsSelectNotDocked | KASFunc.c | 28 | high | yes | through cat_kasShipsSelectByPredicate |
| 0x004958b0 | kasfShipsSelectMoving | KASFunc.c | 28 | high | yes | a velocity component above 2 in magnitude (HW1: the isMoving flags); through cat_kasShipsSelectByPredicate |
| 0x0059a410 | TitanInterface_HandleGameMsg | TitanInterface.cpp | 26 | medium |  | drops calls titanDebug; drops strings "EXCEPTION: TitanInterface::HandleGame..."; params 1 -> 3 (prototype); body 3.9x smaller than predicted |
| 0x0058e562 | TitanInterface_FirewallDetect_tail | TitanInterface.cpp | 25 | high |  | drops calls ConnectAndSend, titanDebug; drops strings "EXCEPTION: TitanInterface::FirewallDe..."; "FAIL: TitanInterface::FirewallDetect!"; "NOTE: TitanInterface::FirewallDetect ..." (+1); body 14.5x smaller than predicte |
| 0x0058e8c3 | TitanInterface_PingFactServer_tail | TitanInterface.cpp | 25 | high |  | drops calls ConnectAndSend, titanDebug; drops strings "EXCEPTION: TitanInterface::PingFactSe..."; "TitanInterface::PingFactServer"; body 11.7x smaller than predicted; fields -AddOutgoingCmd, Leave, Pack, what |
| 0x0058eb1b | TitanInterface_AuthGetPubKeyBlock_tail | TitanInterface.cpp | 25 | high |  | drops calls ConnectAndSend, titanDebug; drops strings "EXCEPTION: TitanInterface::AuthGetPub..."; "FAIL: No auth server to talk to."; "NOTE: TitanInterface::AuthGetPubKeyBl..." (+1); body 13.3x smaller than predicted; fi |
| 0x00524eb0 | statsNumShipsNeededToKillFleet | Stats.c | 21 | high | yes | 3 x the class factor |
| 0x004bdce0 | mgUnitCapsEnabled | MultiplayerGame.c | 19 | medium |  | drops calls feToggleButtonSet, mgGameTypesOtherButtonPressed; body 5.6x smaller than predicted; fields +flags, region -flag, status; globals -tpGameCreated |
| 0x005c5a60 | SalCapCorvetteSpecialTarget | SalCapCorvette.c | 19 | high | yes | cat_salvageSpecialTarget |
| 0x005c5aa0 | SalCapRemoveShipReferences | SalCapCorvette.c | 19 | high | yes | cat_salvageRemoveShipReferences |
| 0x00529130 | tacticsReportMove | Tactics.c | 17 | high | yes | an empty walk of the command list (17 bytes): HW1's retreat registration is gone |
| 0x00524ed0 | statsGetRelativeFleetStrengths | Stats.c | 15 | high | yes | fleet2->numShips / fleet1->numShips |
| 0x005b3720 | CloakedFighterSpecialActivate | GenericInterceptor.c | 14 | high | yes | cat_cloakActivate |
| 0x005b0490 | DefenseFighterHouseKeep | DefenseFighter.c | 12 | high |  | drops calls fmathSqrtDouble, listDeleteNode, matCreateCoordSysFromHeading, matMultiplyMatByVec, ranRandomFn, soundEventBurstStop (+3); adds strings "\n@@@@@@@@ Defense Fighter Doesn't Wo..."; params 1 -> 0 (prototype); b |
| 0x005b3840 | CloakedFighterHouseKeep | GenericInterceptor.c | 12 | high | yes | cat_cloakUpdate |
| 0x005bcdb0 | Mothership_PreFix | Mothership.c | 12 | medium | yes | depot pre-fix only |
| 0x005c5a90 | SalCapCorvetteHouseKeep | SalCapCorvette.c | 12 | high | yes | cat_salvageHouseKeep |
| 0x005c5ac0 | SalCapDied | SalCapCorvette.c | 12 | high | yes | body replaced by a call to the new cat_salvageDied(ship) (Cataclysm salvage system) |
| 0x005c5ad0 | SalCapCorvette_PreFix | SalCapCorvette.c | 12 | medium | yes | cat_salvage_PreFix |
| 0x004f4bd0 | rmClearAllLabs | ResearchGUI.c | 11 | high | yes | the selected lab cleared (0x00891ec0 = -1) and nothing else; ported |

## Not classified

Matched functions for which neither Relic's tree nor the GoK working tree has a definition the brace matcher found:

- `0x00425be0` bitioFileInputBit (`bitioFileInputBit`, 70 bytes): no definition of bitioFileInputBit in ORIG@ad1bb82 src/{Game,Win32,Ships,SinglePlayer} (cpp -DHW_Release) nor in the GoK working tree
- `0x00425c30` bitioFileInputBits (`bitioFileInputBits`, 94 bytes): no definition of bitioFileInputBits in ORIG@ad1bb82 src/{Game,Win32,Ships,SinglePlayer} (cpp -DHW_Release) nor in the GoK working tree
- `0x0055bfd0` glcDisplayExtents (`glcDisplayExtents`, 1192 bytes): no definition of glcDisplayExtents in ORIG@ad1bb82 src/{Game,Win32,Ships,SinglePlayer} (cpp -DHW_Release) nor in the GoK working tree
- `0x0058b8a0` TitanInterface_TitanInterface (`TitanInterface::TitanInterface`, 1629 bytes): no definition of TitanInterface::TitanInterface in ORIG@ad1bb82 src/{Game,Win32,Ships,SinglePlayer} (cpp -DHW_Release) nor in the GoK working tree
- `0x0058bf00` TitanInterface_deleting_dtor (`TitanInterface::~TitanInterface`, 30 bytes): no definition of TitanInterface::~TitanInterface in ORIG@ad1bb82 src/{Game,Win32,Ships,SinglePlayer} (cpp -DHW_Release) nor in the GoK working tree
- `0x0058c110` TitanInterface_dtor (`TitanInterface::~TitanInterface`, 1122 bytes): no definition of TitanInterface::~TitanInterface in ORIG@ad1bb82 src/{Game,Win32,Ships,SinglePlayer} (cpp -DHW_Release) nor in the GoK working tree

## Method

tools/delta_map.py. HW1 side: Relic's source at GoK root commit ad1bb82 (gitlab archive of ad1bb82ca3c62dcc8be9d35f2e836d0bb7cf7caa, src/{Game,Win32,Ships,SinglePlayer}); each unit run through cpp -DHW_Release -DWIN32 -D_WIN32 -D_X86_ -D_MSC_VER=1100 -D_M_IX86=600 -D__MSC__=yes -DHW_English -D_MT with a case-insensitive include dir (system headers stubbed), so debug-only code is compiled out as in a retail build and macros are expanded; definitions found by a brace matcher on the token stream. Names ORIG does not define fall back to the GoK working tree (hw1_source=GoK-HEAD). Cataclysm side: ghidra/exports/cataclysm_gog_r6 decompiled.c body (comments stripped), call_graph.csv callees, strings.csv xrefs. Target set: binary-map class hw1:* (symbols_round1-5 notes, then the audited r2-r5 proposal sets) plus CET entries with an hw1_equivalent whose name is current (the docs/binary-map.md R5 rule, 4,426 functions). Features: game-callee sets by canonical HW1 name (HomeworldSDL names mapped through ORIG macro aliases, e.g. clRemoveShipFromSelection -> clRemoveTargetFromSelection); a dropped callee whose own callees appear instead, or a small one from the same unit, counts as inlined and its strings/fields/constants are expected in the caller; C++ member calls, thunk targets and tail calls named in the body count; the map's cat_<hw1name> spellings map back; library/CRT callees compared separately (soft); string literals of >= 4 chars incl. char[] globals (an untyped string behind a DAT_ reference is excused; AI move-name strings \naim* are one header change); struct fields (-> and .) against every identifier in Relic's tree; named globals from symbols.csv; distinctive integer constants (not -2..16, not masks/powers of two); float constants; for/while loop counts; switch/case counts; parameter count; body size vs the HW1 file's median log(bytes/tokens). Hard deltas: game callee added or dropped (not inlined), HW1 callee absent from Cataclysm, string added or dropped, struct field HW1 never names, parameter count from an evidence-named prototype, argument-array calling convention, body > 2.7x larger with more branches. IDENTICAL = no hard delta (soft deviations only); REWRITTEN = a hard delta, under half of HW1's callees+strings survive, and weighted score < 0.4 or (>= 4 game callees, callee Jaccard < 0.25, string Jaccard < 0.34); else MODIFIED. 'change' is the automatic delta list unless reviewed=true (hand review of both bodies, recorded in ghidra/delta_map_review.json, which also holds the write-up's findings under _notes).

Reproduce (the ad1bb82 tree is the gitlab archive `https://gitlab.com/gardens-of-kadesh/gardens-of-kadesh/-/archive/ad1bb82c/gardens-of-kadesh-ad1bb82c.tar.gz?path=src`, extracted to `original/scratch-m2/delta-orig/`; its pax header names commit ad1bb82ca3c62dcc8be9d35f2e836d0bb7cf7caa; the reference clones are depth 1 and do not hold that commit):

```
python3 -I tools/delta_map.py prep       # cpp + index -> original/scratch-m2/delta-work/hw1_index.json
python3 -I tools/delta_map.py classify   # -> ghidra/delta_map.json (merges ghidra/delta_map_review.json)
python3 -I tools/delta_map.py show 0x00401000   # both bodies side by side, for review
python3 -I tools/delta_map.py report     # -> this file
```

