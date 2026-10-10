<!-- done: tool, Game a-f, Game g-m, Game n-s, Game t-z, Ships, other, review, write-up, reread:AIAttackMan-AIHandler, reread:AIMoves, reread:AIOrders-AIVar, reread:Alliance-BTG, reread:Camera-ColPick, reread:CommandLayer-Dock, reread:ETG, reread:fastmath-HS, reread:KAS-KASFunc, reread:Key-MEX, reread:MultiplayerGame-NetCheck, reread:NIS-Options, reread:Particle, reread:Physics-ResearchGUI, reread:SaveGame, reread:ScenPick-SinglePlayer, reread:SoundEvent-StringSupport, reread:Tactical-TradeMgr, reread:Tutor-Universe, reread:UnivUpdate-Ships, reread:bink-Light, reread:main-Queue, reread:render-Titan, reread:TitanInterface-utility, audit-reclass-R2, toolfix-R2 -->
# M2-R1 function deltas: Cataclysm vs Homeworld 1

Round M2-R1, item 3 of `cycle/M2_BRIEF.md`. For every function the binary map matched to Homeworld 1 (class `hw1:<File>`, plus CET's `hw1_equivalent` entries, the rule behind the 4,426 HW1 matches of docs/binary-map.md round 5), this compares the decompiled Cataclysm body (`ghidra/exports/cataclysm_gog_r5`) with Relic's original source (gardens-of-kadesh root commit `ad1bb82`) and classifies it IDENTICAL, MODIFIED or REWRITTEN. Per-function data: `ghidra/delta_map.json`. Tool: `tools/delta_map.py`. Hand-review verdicts: `ghidra/delta_map_review.json`.

## Totals: how much of the engine is free

| | functions | share | Cataclysm bytes | share of bytes |
|---|---:|---:|---:|---:|
| IDENTICAL | 3,175 | 71.8% | 633,417 | 51.1% |
| MODIFIED | 1,168 | 26.4% | 595,973 | 48.1% |
| REWRITTEN | 77 | 1.7% | 9,457 | 0.8% |
| **classified** | **4,420** | | **1,238,847** | |
| not classified (no HW1 body found) | 6 | | 4,137 | |

Estimated true split, correcting each automatic (class, confidence) bucket by its hand-review agreement (class-level agreement where a bucket has fewer than 5 reviewed functions); reviewed functions count with their reviewed class: IDENTICAL 3,176 functions / 699,317 bytes (56.4% of bytes), MODIFIED 1,224 / 536,678 (43.3%), REWRITTEN 21 / 2,852 (0.2%).

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
4. **Some binary-map names look wrong.** [M2-R2: settled. 0x00404090 `aieCheckEnemyNearby` is HW1 `aieCheckHealthLow` and 0x00404150 `aieCheckEnemyNotNearby` is `aieCheckHealthHigh` (both IDENTICAL; HW1's enemy-nearby checks are inlined in `aieExecute`); 0x004aeb10 `meshFixupPacoUV` is `meshObjectFixupPacoUV`. Proposals: `ghidra/proposals/m2-r2-name-fixes.json`; M2-R3 applied all 3 in Ghidra (export `cataclysm_gog_r6`, `docs/binary-map.md` "M2-R3 name fixes"). `RemoveShipFromFormation` 0x00447950 is named right: the same body is inlined in `RemoveShipsFromDoingStuff` where HW1 calls it, so Cataclysm replaced the body (REWRITTEN).] M2-R1 text: the bodies are unrelated to the HW1 function: `aieCheckEnemyNearby` 0x00404090 and `RemoveShipFromFormation` 0x00447950. `TitanInterface_GetPatch_2` 0x0058c6e0 is a 9-parameter Cataclysm routine [audit: wrong for GetPatch_2: its body is HW1's TitanInterface::GetPatch thread launcher extended to 9 parameters (allocates an argument object, copies the strings into it, calls _beginthread(TitanInterface_GetPatch,0,args) and calls the failure callback passed as parameter 8 when that returns -1), so the binary-map name fits and the function is MODIFIED, not REWRITTEN]. These are flagged REWRITTEN/low with a note.
5. **Limits.** The tool compares features, not semantics. A changed condition or constant that adds no call, string, field or global is invisible to it. Reading found several such changes in functions the tool called IDENTICAL: `lgLaunchLAN` (protocol retry removed), `bsCurveUpdate` (end-of-curve value), `aioCreateMine` (race test), `timTimerCreateSetStart` (hash keys behind inlined callees). That is why the IDENTICAL buckets agree with review only 88-92% of the time (M2-R2; M2-R1: 82-98% on fewer reviews). In M2, IDENTICAL means "port verbatim, then let the checksum oracle catch the rest".

## M2-R2 re-read

M2-R2 read both bodies of every function M2-R1 had left IDENTICAL/low: 1,013 functions. For each, it read the Cataclysm decompilation and Relic's ad1bb82 source side by side, and the GoK `src/` body where it differs from ad1bb82. A difference that is only a header-level constant (renumbered enum, struct offset or size, macro value), an inlined or linker-folded callee, Ghidra prototype noise or compiled-out debug code counts as IDENTICAL. Review entries carry `sample: reread-R2`.

| result | functions | share | high | medium | low |
|---|---:|---:|---:|---:|---:|
| IDENTICAL | 870 | 85.9% | 837 | 31 | 2 |
| MODIFIED | 140 | 13.8% | 130 | 10 | 0 |
| REWRITTEN | 3 | 0.3% | 2 | 1 | 0 |
| **read** | **1,013** | | | | |

**143 of 1,013 changed class (14.1%)**: 140 MODIFIED and 3 REWRITTEN. 8 of the MODIFIED ones differ only in which ship types or races a test names (per-race Cataclysm types replacing HW1's, added or dropped types); their change line starts with `enum:`. **The re-read IDENTICAL agreement for the IDENTICAL/low bucket is 85.9%** (M2-R1's 51-function sample said 86%).

The audit's reclassifications (`docs/audits/m2-r1-function-deltas.md`) were checked by hand and applied to 20 functions (review entries with `sample: audit-R2`), now 10 IDENTICAL, 9 MODIFIED, 1 REWRITTEN.

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
| IDENTICAL | 3,357 | 3,175 | 658,389 | 633,417 | 51.1% |
| MODIFIED | 983 | 1,168 | 569,953 | 595,973 | 48.1% |
| REWRITTEN | 80 | 77 | 10,505 | 9,457 | 0.8% |

**New IDENTICAL agreement estimate.** 3,175 functions are IDENTICAL now: 984 by hand review and 2,191 by the tool alone. Applying each automatic bucket's review agreement to the unread ones, about 1,987 of those 2,191 (90.7%) are expected to be truly identical, so about 2,971 of all 3,175 IDENTICAL functions (93.6%) are, against M2-R1's 80% audit estimate. The unread ones sit in the automatic IDENTICAL/high and /medium buckets, which have only 16 and 46 reviewed functions, so this is a rough figure.

## Hand review and calibration

1191 functions were reviewed by hand (both bodies read; for long bodies sometimes only the head, as the review note says). Rows: the tool's current automatic class; columns: the reviewed class.

| automatic \ reviewed | IDENTICAL | MODIFIED | REWRITTEN | agreement |
|---|---:|---:|---:|---:|
| IDENTICAL | 965 | 83 | 3 | 91.8% |
| MODIFIED | 16 | 111 | 2 | 86.0% |
| REWRITTEN | 3 | 6 | 2 | 18.2% |

Per automatic bucket (functions in the bucket, reviewed, reviewed class agreeing):

| automatic class | confidence | functions | bytes | reviewed | agree |
|---|---|---:|---:|---:|---:|
| IDENTICAL | high | 378 | 118,583 | 16 | 14 (87.5%) |
| IDENTICAL | medium | 1,875 | 450,538 | 46 | 42 (91.3%) |
| IDENTICAL | low | 989 | 75,172 | 989 | 909 (91.9%) |
| MODIFIED | high | 682 | 508,751 | 19 | 15 (78.9%) |
| MODIFIED | medium | 263 | 65,926 | 25 | 21 (84.0%) |
| MODIFIED | low | 152 | 9,066 | 85 | 75 (88.2%) |
| REWRITTEN | high | 43 | 3,342 | 3 | 1 (33.3%) |
| REWRITTEN | medium | 36 | 7,172 | 6 | 1 (16.7%) |
| REWRITTEN | low | 2 | 297 | 2 | 0 (0.0%) |

Confidence per class (after review): IDENTICAL 1,285 high / 1,888 medium / 2 low; MODIFIED 835 high / 264 medium / 69 low; REWRITTEN 45 high / 31 medium / 1 low.

## Per HW1 source file

Grouped as in the round plan (Game a-f, g-m, n-s, t-z, Ships, other = Win32/ and GoK-only). Bytes are Cataclysm function sizes. `matched` counts the classified functions only; `unres` = matched functions with no HW1 body, additional to `matched` (BitIO.c: 2 classified + 2 unresolved bitio functions). `rev` = hand-reviewed functions in the file.

### Game a-f (1,399 functions, 414,570 bytes, 58.7% of bytes IDENTICAL)

| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AIAttackMan.c | 13 | 11 | 2 | 0 | 4,978 | 938 | 0 | 2 | 0 |
| AIDefenseMan.c | 13 | 11 | 2 | 0 | 3,701 | 720 | 0 | 2 | 0 |
| AIEvents.c | 15 | 13 | 2 | 0 | 1,203 | 1,162 | 0 | 10 | 0 |
| AIFleetMan.c | 11 | 7 | 4 | 0 | 1,957 | 3,977 | 0 | 3 | 0 |
| AIHandler.c | 20 | 17 | 3 | 0 | 2,529 | 1,855 | 0 | 3 | 0 |
| AIMoves.c | 163 | 156 | 7 | 0 | 23,506 | 4,707 | 0 | 54 | 0 |
| AIOrders.c | 22 | 3 | 19 | 0 | 233 | 10,761 | 0 | 3 | 0 |
| AIPlayer.c | 28 | 20 | 8 | 0 | 3,445 | 2,360 | 0 | 8 | 0 |
| AIResourceMan.c | 13 | 9 | 4 | 0 | 2,277 | 3,301 | 0 | 2 | 0 |
| AIShip.c | 20 | 16 | 4 | 0 | 4,697 | 6,321 | 0 | 3 | 0 |
| AITeam.c | 44 | 39 | 3 | 2 | 5,450 | 854 | 169 | 11 | 0 |
| AITrack.c | 19 | 19 | 0 | 0 | 6,816 | 0 | 0 | 5 | 0 |
| AIUtilities.c | 85 | 69 | 16 | 0 | 18,424 | 6,157 | 0 | 10 | 0 |
| AIVar.c | 13 | 13 | 0 | 0 | 1,374 | 0 | 0 | 7 | 0 |
| Alliance.c | 6 | 4 | 2 | 0 | 447 | 1,268 | 0 | 1 | 0 |
| Animatic.c | 4 | 3 | 1 | 0 | 662 | 499 | 0 | 2 | 0 |
| Attack.c | 6 | 4 | 2 | 0 | 1,769 | 790 | 0 | 2 | 0 |
| AutoDownloadMap.c | 11 | 11 | 0 | 0 | 2,109 | 0 | 0 | 7 | 0 |
| AutoLOD.c | 5 | 5 | 0 | 0 | 329 | 0 | 0 | 3 | 0 |
| B-Spline.c | 4 | 3 | 1 | 0 | 431 | 594 | 0 | 2 | 0 |
| Battle.c | 19 | 8 | 10 | 1 | 1,987 | 4,705 | 94 | 4 | 0 |
| BigFile.c | 8 | 8 | 0 | 0 | 3,088 | 0 | 0 | 3 | 0 |
| BitIO.c | 2 | 2 | 0 | 0 | 62 | 0 | 0 | 2 | 2 |
| Blobs.c | 21 | 20 | 1 | 0 | 7,791 | 628 | 0 | 0 | 0 |
| BMP.c | 2 | 1 | 1 | 0 | 148 | 79 | 0 | 0 | 0 |
| Bounties.c | 4 | 1 | 3 | 0 | 274 | 232 | 0 | 3 | 0 |
| BTG.c | 17 | 13 | 4 | 0 | 3,270 | 2,295 | 0 | 7 | 0 |
| Camera.c | 8 | 6 | 2 | 0 | 1,278 | 1,178 | 0 | 1 | 0 |
| CameraCommand.c | 28 | 20 | 8 | 0 | 3,273 | 4,039 | 0 | 6 | 0 |
| Captaincy.c | 10 | 8 | 1 | 1 | 1,319 | 1,960 | 85 | 2 | 0 |
| ChannelFSM.c | 12 | 12 | 0 | 0 | 868 | 0 | 0 | 2 | 0 |
| Chatting.c | 3 | 2 | 1 | 0 | 312 | 159 | 0 | 1 | 0 |
| Clamp.c | 3 | 0 | 3 | 0 | 0 | 898 | 0 | 1 | 0 |
| Clipper.c | 7 | 7 | 0 | 0 | 4,805 | 0 | 0 | 4 | 0 |
| Clouds.c | 25 | 21 | 4 | 0 | 6,711 | 7,544 | 0 | 3 | 0 |
| Collision.c | 25 | 13 | 12 | 0 | 3,578 | 11,578 | 0 | 3 | 0 |
| ColPick.c | 40 | 39 | 1 | 0 | 10,623 | 427 | 0 | 8 | 0 |
| CommandLayer.c | 97 | 51 | 45 | 1 | 9,102 | 37,516 | 54 | 14 | 0 |
| CommandNetwork.c | 53 | 49 | 4 | 0 | 10,756 | 4,480 | 0 | 13 | 0 |
| CommandWrap.c | 23 | 16 | 7 | 0 | 1,774 | 1,748 | 0 | 2 | 0 |
| ConsMgr.c | 50 | 27 | 23 | 0 | 5,418 | 11,985 | 0 | 11 | 0 |
| Crates.c | 8 | 6 | 2 | 0 | 2,453 | 2,702 | 0 | 0 | 0 |
| CRC32.c | 2 | 2 | 0 | 0 | 104 | 0 | 0 | 2 | 0 |
| Damage.c | 7 | 6 | 1 | 0 | 1,159 | 796 | 0 | 2 | 0 |
| Debug.c | 2 | 2 | 0 | 0 | 183 | 0 | 0 | 0 | 0 |
| Demo.c | 8 | 6 | 2 | 0 | 1,026 | 599 | 0 | 1 | 0 |
| Dock.c | 18 | 4 | 11 | 3 | 342 | 4,305 | 492 | 4 | 0 |
| ETG.c | 205 | 129 | 75 | 1 | 24,764 | 8,537 | 64 | 84 | 0 |
| Eval.c | 6 | 5 | 1 | 0 | 2,137 | 1,161 | 0 | 0 | 0 |
| fastmath.c | 2 | 2 | 0 | 0 | 227 | 0 | 0 | 2 | 0 |
| FEFlow.c | 38 | 35 | 3 | 0 | 9,771 | 1,551 | 0 | 6 | 0 |
| FEReg.c | 28 | 26 | 2 | 0 | 12,976 | 299 | 0 | 5 | 0 |
| File.c | 19 | 16 | 3 | 0 | 5,850 | 630 | 0 | 4 | 0 |
| FlightMan.c | 45 | 45 | 0 | 0 | 13,987 | 0 | 0 | 6 | 0 |
| FontReg.c | 6 | 6 | 0 | 0 | 935 | 0 | 0 | 1 | 0 |
| Formation.c | 33 | 21 | 12 | 0 | 4,668 | 11,961 | 0 | 7 | 0 |

### Game g-m (821 functions, 160,446 bytes, 56.9% of bytes IDENTICAL)

| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| GameChat.c | 12 | 9 | 3 | 0 | 1,386 | 2,374 | 0 | 2 | 0 |
| GamePick.c | 11 | 10 | 1 | 0 | 1,979 | 404 | 0 | 1 | 0 |
| Globals.c | 2 | 1 | 1 | 0 | 17 | 317 | 0 | 1 | 0 |
| Gun.c | 8 | 2 | 5 | 1 | 893 | 5,998 | 474 | 0 | 0 |
| HorseRace.c | 16 | 8 | 8 | 0 | 3,444 | 4,827 | 0 | 2 | 0 |
| HS.c | 18 | 16 | 1 | 1 | 4,541 | 301 | 39 | 3 | 0 |
| InfoOverlay.c | 2 | 0 | 1 | 1 | 0 | 431 | 220 | 0 | 0 |
| KAS.c | 30 | 14 | 13 | 3 | 1,198 | 3,039 | 189 | 6 | 0 |
| KASFunc.c | 294 | 239 | 34 | 21 | 12,149 | 3,439 | 985 | 131 | 0 |
| Key.c | 12 | 11 | 1 | 0 | 881 | 39 | 0 | 8 | 0 |
| KeyBindings.c | 4 | 0 | 4 | 0 | 0 | 573 | 0 | 1 | 0 |
| LagPrint.c | 4 | 4 | 0 | 0 | 263 | 0 | 0 | 0 | 0 |
| LaunchMgr.c | 16 | 8 | 8 | 0 | 1,133 | 3,184 | 0 | 4 | 0 |
| LevelLoad.c | 33 | 22 | 11 | 0 | 6,712 | 8,394 | 0 | 2 | 0 |
| LinkedList.c | 13 | 10 | 3 | 0 | 562 | 789 | 0 | 1 | 0 |
| LOD.c | 8 | 7 | 1 | 0 | 898 | 653 | 0 | 2 | 0 |
| LZSS.c | 1 | 1 | 0 | 0 | 148 | 0 | 0 | 1 | 0 |
| MadLinkIn.c | 13 | 10 | 3 | 0 | 2,044 | 821 | 0 | 0 | 0 |
| Matrix.c | 20 | 20 | 0 | 0 | 2,333 | 0 | 0 | 6 | 0 |
| Memory.c | 14 | 10 | 4 | 0 | 2,476 | 1,767 | 0 | 2 | 0 |
| Mesh.c | 33 | 20 | 13 | 0 | 6,502 | 14,327 | 0 | 9 | 0 |
| MeshAnim.c | 13 | 6 | 7 | 0 | 820 | 1,762 | 0 | 6 | 0 |
| MEX.c | 11 | 3 | 8 | 0 | 579 | 2,576 | 0 | 2 | 0 |
| MultiplayerGame.c | 175 | 167 | 7 | 1 | 31,337 | 6,220 | 19 | 31 | 0 |
| MultiplayerLANGame.c | 53 | 43 | 10 | 0 | 8,169 | 4,781 | 0 | 10 | 0 |
| StubTransform.c | 5 | 4 | 1 | 0 | 880 | 160 | 0 | 4 | 0 |

### Game n-s (1,000 functions, 270,647 bytes, 44.1% of bytes IDENTICAL)

| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| NavLights.c | 2 | 1 | 1 | 0 | 53 | 581 | 0 | 0 | 0 |
| Nebulae.c | 28 | 27 | 1 | 0 | 15,394 | 738 | 0 | 7 | 0 |
| NetCheck.c | 17 | 14 | 3 | 0 | 1,887 | 511 | 0 | 6 | 0 |
| NIS.c | 139 | 122 | 17 | 0 | 24,486 | 9,803 | 0 | 30 | 0 |
| Objectives.c | 14 | 9 | 4 | 1 | 996 | 1,159 | 257 | 2 | 0 |
| ObjTypes.c | 29 | 6 | 23 | 0 | 237 | 1,243 | 0 | 6 | 0 |
| Options.c | 89 | 69 | 18 | 2 | 8,886 | 6,001 | 115 | 7 | 0 |
| Particle.c | 132 | 42 | 89 | 1 | 15,279 | 13,399 | 131 | 62 | 0 |
| Physics.c | 7 | 3 | 4 | 0 | 2,075 | 3,571 | 0 | 2 | 0 |
| PiePlate.c | 13 | 7 | 5 | 1 | 1,784 | 4,016 | 955 | 3 | 0 |
| Ping.c | 18 | 16 | 2 | 0 | 2,521 | 1,687 | 0 | 4 | 0 |
| PlugScreen.c | 8 | 6 | 2 | 0 | 2,849 | 436 | 0 | 3 | 0 |
| Randy.c | 8 | 8 | 0 | 0 | 710 | 0 | 0 | 1 | 0 |
| Region.c | 31 | 28 | 3 | 0 | 4,660 | 605 | 0 | 6 | 0 |
| ResCollect.c | 19 | 9 | 9 | 1 | 1,814 | 2,966 | 109 | 5 | 0 |
| ResearchAPI.c | 21 | 11 | 10 | 0 | 755 | 4,527 | 0 | 5 | 0 |
| ResearchGUI.c | 18 | 6 | 11 | 1 | 546 | 7,505 | 11 | 2 | 0 |
| SaveGame.c | 86 | 65 | 21 | 0 | 9,855 | 14,826 | 0 | 39 | 0 |
| ScenPick.c | 15 | 9 | 6 | 0 | 1,274 | 3,185 | 0 | 3 | 0 |
| Scroller.c | 6 | 6 | 0 | 0 | 530 | 0 | 0 | 1 | 0 |
| Select.c | 29 | 14 | 11 | 4 | 1,364 | 10,476 | 1,631 | 7 | 0 |
| Sensors.c | 19 | 11 | 8 | 0 | 2,525 | 5,748 | 0 | 3 | 0 |
| Shader.c | 20 | 18 | 2 | 0 | 3,444 | 1,615 | 0 | 10 | 0 |
| ShipSelect.c | 52 | 19 | 33 | 0 | 1,046 | 4,060 | 0 | 23 | 0 |
| ShipView.c | 4 | 0 | 4 | 0 | 0 | 2,421 | 0 | 2 | 0 |
| SinglePlayer.c | 46 | 13 | 32 | 1 | 1,584 | 15,168 | 134 | 2 | 0 |
| SoundEvent.c | 32 | 29 | 3 | 0 | 4,099 | 13,225 | 0 | 16 | 0 |
| SoundEventPlay.c | 3 | 1 | 2 | 0 | 53 | 3,054 | 0 | 1 | 0 |
| SoundEventStop.c | 2 | 2 | 0 | 0 | 136 | 0 | 0 | 1 | 0 |
| SpeechEvent.c | 19 | 9 | 10 | 0 | 2,195 | 5,482 | 0 | 3 | 0 |
| Star3d.c | 2 | 2 | 0 | 0 | 669 | 0 | 0 | 0 | 0 |
| Stats.c | 25 | 13 | 9 | 3 | 1,797 | 2,605 | 671 | 9 | 0 |
| StatScript.c | 42 | 33 | 9 | 0 | 3,681 | 5,956 | 0 | 19 | 0 |
| StringSupport.c | 5 | 2 | 3 | 0 | 147 | 733 | 0 | 1 | 0 |

### Game t-z (409 functions, 118,515 bytes, 50.5% of bytes IDENTICAL)

| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Tactical.c | 6 | 4 | 2 | 0 | 244 | 1,985 | 0 | 2 | 0 |
| Tactics.c | 8 | 5 | 2 | 1 | 841 | 114 | 17 | 0 | 0 |
| Task.c | 16 | 14 | 2 | 0 | 2,527 | 229 | 0 | 2 | 0 |
| TaskBar.c | 6 | 5 | 1 | 0 | 670 | 679 | 0 | 2 | 0 |
| Teams.c | 6 | 6 | 0 | 0 | 2,284 | 0 | 0 | 3 | 0 |
| Timer.c | 14 | 5 | 9 | 0 | 274 | 1,912 | 0 | 2 | 0 |
| TitanNet.c | 35 | 33 | 2 | 0 | 3,170 | 337 | 0 | 15 | 0 |
| TradeMgr.c | 27 | 25 | 2 | 0 | 3,466 | 1,254 | 0 | 13 | 0 |
| Transformer.c | 2 | 1 | 1 | 0 | 71 | 303 | 0 | 0 | 0 |
| Tutor.c | 63 | 46 | 17 | 0 | 10,290 | 6,371 | 0 | 22 | 0 |
| Tweak.c | 3 | 3 | 0 | 0 | 205 | 0 | 0 | 2 | 0 |
| Twiddle.c | 2 | 2 | 0 | 0 | 55 | 0 | 0 | 2 | 0 |
| UIControls.c | 47 | 43 | 4 | 0 | 15,659 | 1,690 | 0 | 11 | 0 |
| Undo.c | 2 | 2 | 0 | 0 | 62 | 0 | 0 | 1 | 0 |
| Universe.c | 56 | 34 | 22 | 0 | 8,324 | 12,791 | 0 | 17 | 0 |
| UnivUpdate.c | 98 | 52 | 46 | 0 | 9,337 | 30,978 | 0 | 11 | 0 |
| Vector.c | 14 | 14 | 0 | 0 | 1,246 | 0 | 0 | 7 | 0 |
| Volume.c | 4 | 4 | 0 | 0 | 1,130 | 0 | 0 | 1 | 0 |

### Ships (137 functions, 27,321 bytes, 43.3% of bytes IDENTICAL)

| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AdvanceSupportFrigate.c | 2 | 2 | 0 | 0 | 141 | 0 | 0 | 0 | 0 |
| Carrier.c | 8 | 1 | 6 | 1 | 19 | 845 | 38 | 1 | 0 |
| DefaultShip.c | 4 | 2 | 2 | 0 | 112 | 287 | 0 | 2 | 0 |
| DefenseFighter.c | 10 | 7 | 2 | 1 | 653 | 127 | 12 | 1 | 0 |
| DFGFrigate.c | 3 | 1 | 2 | 0 | 41 | 957 | 0 | 0 | 0 |
| FloatingCity.c | 1 | 1 | 0 | 0 | 51 | 0 | 0 | 0 | 0 |
| GenericDefender.c | 3 | 2 | 1 | 0 | 705 | 1,025 | 0 | 0 | 0 |
| GenericInterceptor.c | 13 | 4 | 7 | 2 | 851 | 5,276 | 26 | 2 | 0 |
| GravWellGenerator.c | 18 | 14 | 4 | 0 | 2,205 | 1,093 | 0 | 2 | 0 |
| HeavyCorvette.c | 6 | 5 | 1 | 0 | 1,153 | 79 | 0 | 2 | 0 |
| HeavyCruiser.c | 1 | 0 | 1 | 0 | 0 | 115 | 0 | 0 | 0 |
| IonCannonFrigate.c | 1 | 1 | 0 | 0 | 99 | 0 | 0 | 0 | 0 |
| MinelayerCorvette.c | 13 | 11 | 2 | 0 | 2,867 | 1,272 | 0 | 2 | 0 |
| MissileDestroyer.c | 5 | 5 | 0 | 0 | 951 | 0 | 0 | 0 | 0 |
| Mothership.c | 8 | 0 | 5 | 3 | 0 | 1,400 | 272 | 1 | 0 |
| MultiGunCorvette.c | 2 | 2 | 0 | 0 | 103 | 0 | 0 | 0 | 0 |
| P1IonArrayFrigate.c | 3 | 1 | 2 | 0 | 93 | 174 | 0 | 0 | 0 |
| P1MissileCorvette.c | 5 | 4 | 1 | 0 | 386 | 111 | 0 | 1 | 0 |
| P1Mothership.c | 2 | 2 | 0 | 0 | 101 | 0 | 0 | 0 | 0 |
| P1StandardCorvette.c | 3 | 0 | 3 | 0 | 0 | 380 | 0 | 0 | 0 |
| P2Swarmer.c | 1 | 1 | 0 | 0 | 41 | 0 | 0 | 0 | 0 |
| ProximitySensor.c | 4 | 3 | 1 | 0 | 352 | 464 | 0 | 0 | 0 |
| RepairCorvette.c | 9 | 8 | 1 | 0 | 752 | 1,191 | 0 | 1 | 0 |
| ResourceCollector.c | 2 | 2 | 0 | 0 | 85 | 0 | 0 | 0 | 0 |
| SalCapCorvette.c | 8 | 0 | 3 | 5 | 0 | 153 | 74 | 3 | 0 |
| StandardDestroyer.c | 1 | 0 | 1 | 0 | 0 | 121 | 0 | 0 | 0 |
| StandardFrigate.c | 1 | 1 | 0 | 0 | 68 | 0 | 0 | 1 | 0 |

### other (654 functions, 247,348 bytes, 43.5% of bytes IDENTICAL)

| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| bink.c | 1 | 1 | 0 | 0 | 42 | 0 | 0 | 1 | 0 |
| Color.c | 7 | 7 | 0 | 0 | 2,270 | 0 | 0 | 4 | 0 |
| dct.c | 2 | 2 | 0 | 0 | 609 | 0 | 0 | 0 | 0 |
| ddraw.cpp | 5 | 3 | 2 | 0 | 512 | 148 | 0 | 2 | 0 |
| debugwnd.c | 9 | 7 | 2 | 0 | 1,524 | 1,360 | 0 | 2 | 0 |
| font.c | 21 | 20 | 1 | 0 | 5,314 | 428 | 0 | 7 | 0 |
| fqcodec.c | 3 | 3 | 0 | 0 | 1,372 | 0 | 0 | 1 | 0 |
| fqeffect.c | 6 | 6 | 0 | 0 | 557 | 0 | 0 | 6 | 0 |
| fquant.c | 3 | 3 | 0 | 0 | 1,270 | 0 | 0 | 2 | 0 |
| glcaps.c | 8 | 8 | 0 | 0 | 1,472 | 0 | 0 | 5 | 0 |
| glcompat.c | 27 | 25 | 2 | 0 | 8,933 | 2,313 | 0 | 6 | 1 |
| Light.c | 11 | 11 | 0 | 0 | 1,967 | 0 | 0 | 5 | 0 |
| main.c | 28 | 11 | 17 | 0 | 1,273 | 15,298 | 0 | 5 | 0 |
| mainrgn.c | 32 | 17 | 14 | 1 | 6,346 | 10,234 | 960 | 5 | 0 |
| Md52.cpp | 4 | 4 | 0 | 0 | 2,568 | 0 | 0 | 0 | 0 |
| mixfft.c | 10 | 9 | 1 | 0 | 4,739 | 177 | 0 | 7 | 0 |
| mouse.c | 20 | 13 | 7 | 0 | 737 | 7,699 | 0 | 13 | 0 |
| prim2d.c | 26 | 25 | 1 | 0 | 7,673 | 425 | 0 | 2 | 0 |
| prim3d.c | 12 | 12 | 0 | 0 | 3,409 | 0 | 0 | 5 | 0 |
| Queue.c | 8 | 8 | 0 | 0 | 446 | 0 | 0 | 3 | 0 |
| render.c | 32 | 24 | 8 | 0 | 7,388 | 8,481 | 0 | 8 | 0 |
| rglu.c | 3 | 3 | 0 | 0 | 976 | 0 | 0 | 3 | 0 |
| rinit.c | 3 | 1 | 2 | 0 | 40 | 653 | 0 | 1 | 0 |
| rinit.cpp | 5 | 4 | 1 | 0 | 236 | 1,822 | 0 | 3 | 0 |
| screenshot.c | 1 | 0 | 1 | 0 | 0 | 710 | 0 | 0 | 0 |
| smixer.c | 8 | 5 | 3 | 0 | 494 | 3,285 | 0 | 3 | 0 |
| soundlow.c | 25 | 19 | 6 | 0 | 2,606 | 2,065 | 0 | 7 | 0 |
| sstream.c | 13 | 11 | 2 | 0 | 2,880 | 414 | 0 | 4 | 0 |
| Subtitle.c | 22 | 20 | 2 | 0 | 4,462 | 1,401 | 0 | 4 | 0 |
| texreg.c | 44 | 33 | 11 | 0 | 11,848 | 8,248 | 0 | 6 | 0 |
| TimeoutTimer.c | 4 | 4 | 0 | 0 | 233 | 0 | 0 | 4 | 0 |
| Titan.c | 4 | 3 | 1 | 0 | 256 | 71 | 0 | 2 | 0 |
| TitanInterface.cpp | 141 | 46 | 84 | 11 | 7,359 | 55,986 | 1,160 | 36 | 3 |
| Trails.c | 32 | 26 | 6 | 0 | 8,436 | 4,991 | 0 | 7 | 0 |
| utility.c | 74 | 60 | 14 | 0 | 7,455 | 11,317 | 0 | 10 | 0 |

## Largest MODIFIED functions

The 60 largest by Cataclysm bytes; `change` is the hand-written line where reviewed, else the tool's delta list.

| address | function | HW1 file | bytes | conf | reviewed | change |
|---|---|---|---:|---|---|---|
| 0x0051cd10 | soundEventUpdate | SoundEvent.c | 11,044 | high |  | adds calls cat_ccIsPilotingShip, cat_ccIsPilotingThisShip, cat_gunGetSoundType, cat_leechGetSpecifics, cat_leechIsLeeching, cat_mimicGetApparentShipType (+6); drops calls soundEventShipRemove; uses fields HW1 never uses: beastSystemType, buildProgress; fields +actorNum, collInfo, collOptimizeDist, c |
| 0x004e0640 | partFillGenericParticles | Particle.c | 6,718 | high |  | adds calls cat_rndLensFlareSourceAdd, memAllocFunction, ranRandomFn; body 2.4x larger than predicted; fields +m11, m21 -additiveBlends, ang, angDelta, angDeltaDist (+81) |
| 0x004b1d40 | meshSpecObjectRender | Mesh.c | 6,038 | high |  | adds calls cat_meshCurrentMaterialAlt, meshCurrentMaterialDefault, shSpecularColour; params 3 -> 0 (prototype); body 3.8x larger than the file's size ratio predicts, with more branches; fields +x, y, z -iFaceNormal, iMaterial, iV0, iV1 (+12); globals +meshCurrentMaterial; constants 0xb52, 0x1d01 not |
| 0x0044b950 | clProcess | CommandLayer.c | 5,547 | high |  | adds calls battleChatterFleetAttempt, cat_bigGunProcessFireCommand, cat_mimicCommand, cat_mothershipBigGunActive, cat_processLinkToDo, cat_processResearchToDo (+2); drops calls to ShipWithinDockRange (absent from Cataclysm); calls with a changed parameter count: processMoveToDo 2->4; fields +attribu |
| 0x00414610 | aishipFlyToPointAvoidingObjsFunc | AIShip.c | 5,298 | high |  | adds calls cat_mimicGetAvoidRadius, cat_salvageGetSalvagerInfo, cat_shipGetSpeedMultiplier, cat_shipIsCloakedFully, sub_53F9F0; drops calls to attackSimple, proximityCanPlayerSeeShip (absent from Cataclysm); adds strings "\nWarning: ship %s %s flying with no ..."; fields +attack, needFix, next, objl |
| 0x005905b0 | TitanInterface_PeerHandleChallenge | TitanInterface.cpp | 4,942 | medium |  | adds calls DirHandleGetHWDirectory, DirHandleGetTitanServers, DirStartPeerLogin, RequestDirectory, cat_TitanInterface_ContestStartPeerLogin, cat_TitanInterface_ProfileStartPeerLogin (+2); drops calls DirFailOver; adds strings "NOTE: TitanInterface::RequestDirector..."; "TitanInterface::ContestFailOv |
| 0x005197d0 | singlePlayerGameUpdate | SinglePlayer.c | 4,883 | high |  | adds calls AddShipToHyperspace, GroupShipIntoMilitaryParade, PutShipOutsideAt, RemoveShipsFromDoingStuff, aitrackForceSteadyShip, cat_kasHashString (+27); drops calls aivarDestroy, aivarFind, aivarValueGet, soundEventPlay, soundEventPlayMusic; drops calls to aivarFindAnyFSM (absent from Cataclysm);  |
| 0x005764e0 | rndMainViewRenderFunction | render.c | 4,440 | high |  | adds calls cat_blastRenderAll, cat_lodLevelGetTimed, cat_mcRender, cat_meshHierarchyCount, cat_rndDepthTestEnable, cat_rndFogLayerRender (+18); drops calls RenderNAVLights, cloudKillLightning, cloudRenderAndUpdateLightning, cloudRenderSystem, etgEffectUpdate, hmatMakeHMatFromMat (+13); drops calls t |
| 0x00561f10 | WindowProc | main.c | 4,248 | high |  | adds calls cat_consoleKeyPress, cat_glideIsLoaded, cat_glideShutdown, cat_overlayIconsDiscardGL, cat_rndFogLayerTextureUnload, cat_rndLensFlareTextureUnload (+4); drops calls to mouseLClick (absent from Cataclysm); adds strings "couldn't initialize default rendering..."; globals +MAIN_WindowDepth, M |
| 0x005b2620 | GenericInterceptorAttack | GenericInterceptor.c | 4,198 | high |  | adds calls cat_gunGetBulletSpeed; fields +belongto, force, prev, renderlink -CustShipFire, ShipSpecifics, aiAIPflightman, aistate (+40); constants 19, 45, 0x168, 0x15f9 not in body |
| 0x0058c8e0 | TitanInterface_GetPatch | TitanInterface.cpp | 3,601 | medium |  | adds calls GetPatch, WONMisc__EasySocket__stopWinsock, titanDebug; drops calls to titanGetPatchFailedCB (absent from Cataclysm); adds strings "Content-Length: %d"; "ET /"; "FAIL TitanInterface::GetPatch PATCHFA..." (+16); parameters 2 -> 1: void TitanInterface_GetPatch(void * theArgs); body 26.6x la |
| 0x0056d670 | mouseStartup | mouse.c | 3,458 | high |  | adds strings "CursorBitmaps//beast_weapon.LiF"; "CursorBitmaps//emp.LiF"; "CursorBitmaps//fire_missile.LiF" (+11); body 3.9x larger than the file's size ratio predicts, with more branches; loops 0 -> 76 |
| 0x00436aa0 | ellipsoid_seq | Clouds.c | 3,418 | high |  | adds calls ranRandomFn; globals +DUSTCLOUD_RAN_MULTIPLIER |
| 0x00569260 | mrRegionDraw | mainrgn.c | 3,355 | high |  | adds calls cat_gameStatsDraw, cat_gameStatsIsActive, cat_ioUpdateHover, cat_moAltPrintOptionsActive, cat_moRender, cat_mrSelectRectDraw (+7); drops calls MothershipOrCarrierIndexInSelection, clWrapFormation, clWrapSetMilitaryParade, fontPrint, makeShipsFormationCapable, primRectOutline2 (+4); drops  |
| 0x005a9290 | gameStart | utility.c | 3,241 | high |  | adds calls cat_aisReset, cat_blastInit, cat_consoleStartup, cat_mimicInitDisguiseTables, cat_playerUpdateUpgradesFromResearch, cat_ranResetGameStreams (+9); drops calls ranStartup, rmAddTechToPlayer, rmEnableShip, universeFlagEverythingNeeded; drops calls to ConvertSliderToAIPlayerDifficulty, tbStar |
| 0x00424110 | battlePingEvaluate | Battle.c | 3,177 | high |  | adds calls battleChatterFleetAttempt, speechEventQueue; globals +singlePlayerGame, singlePlayerGameInfo; loops 3 -> 10 |
| 0x005209f0 | soundEventPlay | SoundEventPlay.c | 3,032 | high |  | adds calls cat_gunGetSoundType; fields +gunheading, x, y, z -approxcollspheresize, burstHandle, burstfiring, cameraDistanceSquared (+20); globals +whichRaceSelected; constants 18, 36, 58, 65 not in body |
| 0x00483620 | FormationCalculateOffsets | Formation.c | 3,017 | high |  | adds calls SetFormationOffsetsForForceFieldWall, cat_shipSetDisableReason; calls with a changed parameter count: formationSpecificsSphere 1->2; constants 22 not in body |
| 0x00566b60 | mrObjectClick | mainrgn.c | 2,983 | high |  | adds calls MakeShipsFriendlyAndAlliesShips, cat_ccShipIsLegalToFocusOn, cat_clWrapLink, cat_mouseGetContextAction, cat_mrMimicTarget, cat_popupMenuOpen (+10); drops calls MakeShipsFriendlyShips, MakeShipsHarvestCapable, allianceIsShipAlly, clWrapCollectResource, clWrapSpecial, tmTradeBegin; drops ca |
| 0x00506720 | selCircleCompute | Select.c | 2,968 | high |  | adds calls cat_mimicGetSelectRadius; drops calls fmathSqrtDouble; calls with a changed parameter count: selShipUnderMouse 2->1; fields +belongto, cameraDistanceSquared, flashtimer, health (+6) -collPosition, collspheresize, corner, currentLOD (+20) |
| 0x004b34f0 | meshMorphedObjectRender | Mesh.c | 2,964 | high |  | adds calls cat_meshCurrentMaterialAlt, meshCurrentMaterialDefault; fields +nFaceNormals, pName -iFaceNormal, iMaterial, iV0, iV1 (+9); globals +meshCurrentMaterial; constants 0xb52, 0xba6 not in body |
| 0x0048aac0 | missileShoot | Gun.c | 2,790 | high |  | adds calls cat_shipGetDamageMultiplier, cat_univShipMeshStateCreate, cat_weaponDamageVsArmour; adds strings "Hobbes call Mike upgraded weapon does..."; fields +attributesPad, burstStartTime, cameraDistanceVector, gimblehandle (+2) -FORCE_DROPPED, MineList, ShipSpecifics, barrelLength (+67) |
| 0x0053d2a0 | universeStaticInit | Universe.c | 2,755 | high |  | adds calls cat_scriptAsteroidBreakInfoLoad, trailDelete; globals +GLOBAL_SHIP_HEALTH_MODIFIER; library calls -memset; constants 19 not in body; loops 11 -> 46 |
| 0x004cd2b0 | nisStart | NIS.c | 2,733 | high |  | adds calls cat_kasHashString, cat_rndFogLayerOff, cat_rndLensFlareNISSourceAdd; fields +currentPoint, nPoints, timeLastStatus -cam, cameraPath, cameraSplines, camerasInMotion (+29) |
| 0x00441c10 | processAttackToDo | CommandLayer.c | 2,670 | high |  | adds calls battleChatterFleetAttempt, cat_fowCanPlayerSeeShip, cat_gunPickMothershipModuleTarget, cat_shipIsCrystal, clWrapHalt; drops calls battleCanChatterAtThisTime, battleChatterAttempt; adds strings "\nBad Stuff Going On!!! Ship with no ..."; calls with a changed parameter count: processMoveToD |
| 0x0054c3f0 | univUpdate | UnivUpdate.c | 2,605 | high |  | adds calls cat_blastUpdateAll, cat_kasSaveLevelNow, cat_kasShipRemovedFromSelections, cat_univRecreateBulletEffectsIfNeeded, cat_univRemoveTargetFromRammingFrigates, cat_utyGameQuitToMainOrStats; adds strings "SavedGames\multiplayer\AS%d"; calls with a changed parameter count: clCommandMessage 1->2, |
| 0x004e7a70 | piePointSpecDraw | PiePlate.c | 2,501 | high |  | adds calls cat_bigGunMaxRange, cat_smHeightCircleColorGet, cat_vecDistance, vecNormalize; calls with a changed parameter count: piePointModeOnOff 0->2; globals +mouseIsVisible -smUniverseSizeX, smUniverseSizeY; constants 0xba7 not in body |
| 0x0057f850 | isoundmixerprocess | smixer.c | 2,494 | high |  | drops calls to fqAcModel, fqAddBreakE, fqAddNoiseE, fqAddToneE (+3) (absent from Cataclysm); library calls -memset, printf; loops 2 -> 15 |
| 0x004a13f0 | scriptSetShipsCB | LevelLoad.c | 2,450 | high |  | adds calls cat_kasHashString, cat_rmResearchModuleActivated, cat_shipInitResearchUpgrades; drops calls memStrncpy; drops calls to addMonkeyResearchShip, rmActivateFreeLab (absent from Cataclysm); calls with a changed parameter count: clFormation 3->4; fields +attributesPad, collInfo, collOffset, ene |
| 0x0053bab0 | InitStatShipInfo | Universe.c | 2,432 | high |  | adds calls ShipTypeToNiceStr, cat_univInitShipTypeFlags, dockFindDockStaticPoint; drops calls precalcStaticInfoGuidance; drops calls to dockInitializeCustomFunctions, mexGetTractorBeamStatic (absent from Cataclysm); adds strings "\nSHP file loaded:  %s"; "\nWARNING: could not set gunbindings ..."; " |
| 0x00587a50 | trRegistryRefresh | texreg.c | 2,421 | medium | yes | size-sort qsort + trCramRAMScaleCompute/trCramIntoRAM replaced by one call to trCramRAMComputeAndScale(); trMeshSortList nulled after free |
| 0x00542320 | univCreateShip | UnivUpdate.c | 2,417 | high |  | adds calls ShipTypeToStr, bobAddObjToNearestBlob, cat_ShipTypeToRace, cat_dmgShipLoadDamagePoints, cat_dockInitShipDockInfo, cat_madAnimClear (+19); adds strings "\nFailed to allocate %i repair droids..."; params 5 -> 7 (prototype); uses fields HW1 never uses: externalBuild; fields +CustShipStaticIn |
| 0x00413240 | airResourceManager | AIResourceMan.c | 2,349 | high |  | adds calls cat_playerHasCompletedShip; drops calls ranRandomFn; drops calls to aitNeedStrikeSupport (absent from Cataclysm); body 2.0x larger than predicted; uses fields HW1 never uses: supportUnitsMax, supportUnitsUsed; fields +ResourceManRequestShips, airResourceCollectors, flags, maxNumShips (+4) |
| 0x00460ed0 | cratesRewardPlayer | Crates.c | 2,334 | high |  | adds calls cat_rmTechSetHas; drops calls GetAppropriateShipTypeForRace; calls with a changed parameter count: clFormation 3->4; body 1.9x larger than predicted; constants 0x270f not in body |
| 0x00444810 | RemoveShipsFromDoingStuff | CommandLayer.c | 2,321 | high |  | adds calls cat_shipSetDisableReason, memFree; drops calls RemoveShipReferencesFromExtraAttackInfo; params 2 -> 3 (prototype); uses fields HW1 never uses: beastSystemType; fields +CustShipAttackPassive, attackevents, attackvars, collInfo (+8) -FULL, MiningStatus, ShipSpecifics, attributes (+23); cons |
| 0x005053a0 | selRectDragFunction | Select.c | 2,312 | high |  | adds calls allianceArePlayersAllied, cat_mimicAppearsAlliedTo, cat_mimicAppearsAsDerelict, cat_mimicAppearsAsResource, cat_selPositionHiddenByFOW, cat_selShipSelectableInFOW (+5); drops calls to proximityCanPlayerSeeShip (absent from Cataclysm); parameters 8 -> 9: void selRectDragFunction(Node * sta |
| 0x005969e0 | TitanInterface_HandleCloseCmd | TitanInterface.cpp | 2,215 | high |  | adds calls FirewallDetect, PrintAddress, RequestDirectory, cat_TitanInterface_ContestStartPeerLogin, cat_TitanInterface_ProfileStartPeerLogin, mgDisplayMessage (+1); drops calls DirFailOver, FirewallFailOver; adds strings "TitanInterface::ContestFailOver"; "TitanInterface::ContestFailOver FAILE..."; |
| 0x00515010 | svShipViewRender | ShipView.c | 2,203 | high |  | adds calls cat_cameraSetAngle, cat_rndDepthTestEnable, cat_rndObjectEffectsSetup, cat_rndSetupObjectEffectParams, cat_svShipDescriptionDraw, fontPrint (+1); drops calls to GetValidRaceForShipType (absent from Cataclysm); calls with a changed parameter count: GetShipStaticInfoSafe 2->1; globals +cat_ |
| 0x00439130 | cloudUpdateSystem | Clouds.c | 2,161 | high |  | adds calls cloudRealDist, fmathSqrtDouble, vecNormalize; fields +x, y, z; globals +CLOUD_LIGHTNING_MAXDIST |
| 0x004045b0 | aifBuildRequestedShips | AIFleetMan.c | 2,132 | high |  | adds calls cat_playerHasCompletedShip; fields +ResourceManRequestShips, firstTurn, flags -NumRUsSpentOnResourceman, buildCost, infoFlags, staticheader |
| 0x005631f0 | WinMain | main.c | 2,117 | high |  | adds calls cat_glCapWin2K; adds strings "CataclysmPatch.exe"; "CloseCataclysm"; "Emergence" (+2); drops strings "Homeworld"; library calls +glCapLoadOpenGL -strcmp, strlen; loops 1 -> 3 |
| 0x004b8690 | mgShowScreen | MultiplayerGame.c | 2,106 | high |  | adds strings "COPPA_Message"; "Registration"; body 2.6x larger than predicted; loops 1 -> 6; switch cases 41 -> 43 |
| 0x005abe70 | utyGameSystemsInit | utility.c | 2,053 | high |  | adds calls cat_gameStatsStartup, cat_kbLoadKeyBindings, cat_overlayIconsStartup, cat_popupMenuStartup, cat_shipNamesInit, cat_shipPrintStartup (+2); drops calls binkCleanup, binkInit, binkPlay; drops calls to bsStartup, dmgStartup, tmStartup, uicStartup (absent from Cataclysm); adds strings "CaptSer |
| 0x004427a0 | delegateCommand | CommandLayer.c | 2,050 | high |  | adds calls processMoveToDo; fields +CustShipClose, command, pulse, salvageNumTagged (+4) -CustShipAttack, CustShipAttackPassive, DodgeTime, aidescend (+31); constants 17, 24 not in body; loops 11 -> 9 |
| 0x00591980 | TitanInterface_PeerHandleComplete | TitanInterface.cpp | 2,047 | medium |  | adds calls DirHandleGetTitanServers, DirStartPeerLogin, EncryptAndSendRoutingMsg, cat_TitanInterface_ContestSendRequest, cat_TitanInterface_ContestStartPeerLogin, cat_TitanInterface_ProfileSendRequest (+3); drops calls DirFailOver, RegisterRoutingServer; adds strings "Couldn't send RegisterRequest m |
| 0x004b0170 | meshObjectRender | Mesh.c | 2,024 | high |  | adds calls cat_meshCurrentMaterialAlt, meshCurrentMaterialDefault, shColourSet0; params 3 -> 2 (prototype); globals +meshCurrentMaterial; switch cases 6 -> 4 |
| 0x00450910 | clProcessSyncPacket | CommandNetwork.c | 2,019 | high |  | adds calls cat_clExternalBuildUpdate, cat_clLink, cat_clMimic, cat_clPointOrder, cat_clResearch, cat_clSetBeastSystem (+7); adds strings "\cataclysm\exe"; calls with a changed parameter count: clFormation 3->4; fields +ShipPtr, TargetID, curPlayerIndex, objNumber (+3) -AllianceMsgType, CurAlliance,  |
| 0x0042f270 | TransferCaptaincyStateMachine | Captaincy.c | 1,960 | high |  | adds strings "Cap:Proposing new captain %d"; fields +HighPart, LowPart, s -enabled; loops 6 -> 8; switch cases 21 -> 14 |
| 0x00482b90 | processFormationToDo | Formation.c | 1,956 | high |  | adds calls cat_formationShipMoveToNotToObscuredPoints, cat_formationShipMoveToSteady; fields +attributesParam, belongto, collInfo, next (+3) -ShipPtr, TargetPtr, attack, attacktarget (+30); constants 24, 26, 28 not in body; switch cases 7 -> 17 |
| 0x004c1110 | authReceiveReply | MultiplayerGame.c | 1,929 | medium |  | adds calls Enqueue, LockQueue, UnLockQueue; body 3.0x larger than the file's size ratio predicts, with more branches; constants 200 not in body; loops 0 -> 57; switch cases 12 -> 9 |
| 0x004d76d0 | opOptionsAcceptHelper | Options.c | 1,879 | high |  | drops calls to alodGetTargetPolys (absent from Cataclysm); constants 22, 65 not in body; loops 4 -> 6 |
| 0x005a7310 | trailDraw | Trails.c | 1,835 | high |  | adds calls cat_shGetTeamColour, cat_shipGetSpeedMultiplier, cat_trailShouldDraw; constants 0xb44 not in body; loops 4 -> 6 |
| 0x0057e860 | rinEnumerateDevices | rinit.cpp | 1,822 | high |  | adds calls cat_glideQueryBoards, rinMemAlloc; drops calls to sstHardwareExists (absent from Cataclysm); library calls -memset; loops 4 -> 15 |
| 0x004ffd70 | SaveShip | SaveGame.c | 1,812 | high |  | adds calls SaveSelection, fileStream, memAllocFunction; drops calls to Save_madBindings (absent from Cataclysm); globals +savefile, savefilestatus; constants 48, 80, 240, 0x300, 0xe00 not in body; loops 8 -> 19 |
| 0x0050be70 | smViewportRender | Sensors.c | 1,806 | high |  | adds calls cat_ioUpdateHover, cat_moButton1Clicked, cat_moButton2Clicked, cat_moButton3Clicked, cat_moButton4Clicked, cat_moRender (+10); drops calls lightSetLighting, primRectOutline2, rndRenderAHomeworld; body 2.8x smaller than predicted; globals +mrDrawTactical -BG_RADIUS, MAIN_WindowHeight, MAIN |
| 0x00598c80 | TitanInterface_HandleChannelList | TitanInterface.cpp | 1,795 | high |  | drops calls BuildAddress, PrintAddress; adds strings "TitanFactoryServer"; "TitanRoutingServer"; params 1 -> 3 (prototype); library calls +Ordinal, WONCommon__WStringToString, operator_delete, sprintf (+3) |
| 0x00454940 | cmBuildTaskFunction | ConsMgr.c | 1,785 | high |  | adds calls cat_beastMothershipBuildFactor, cat_clWrapExternalBuildUpdate, cat_cmBuildJobFinished, cat_cmFactoryHasFreeSlot, cat_cmFactoryMarkDirty, cat_playerHasCompletedShip (+1); drops calls pingNewShipPingCreate; uses fields HW1 never uses: externalBuild; globals +singlePlayerGame; constants 34,  |
| 0x0055b810 | glcPageFlip | glcompat.c | 1,764 | high |  | adds calls glcDisplayExtents, rgluOrtho2D; constants 0xb71, 0xbe2, 0xde1, 0x1401, 0x1908 not in body |
| 0x004460d0 | ChangeOrderToMove | CommandLayer.c | 1,763 | high |  | adds calls cat_clCalculateMoveToOffsets; drops calls vecNormalize; calls with a changed parameter count: RemoveShipsFromDoingStuff 2->3; constants 21 not in body; loops 8 -> 13 |
| 0x00587210 | trMeshSortListLoad | texreg.c | 1,753 | high |  | adds strings "C:\Cataclysm\src\win32\	exreg.c"; constants 0xfdfbfaf5 not in body; loops 6 -> 18 |

## REWRITTEN functions

| address | function | HW1 file | bytes | conf | reviewed | change |
|---|---|---|---:|---|---|---|
| 0x00567930 | mrRegionProcess | mainrgn.c | 960 | medium |  | adds calls cat_gameStatsEnd, cat_gameStatsIsActive, cat_kbCommandExecute, cat_kbKeyPress, cat_kbKeyRelease, cat_mrBandBoxRelease (+13); drops calls MakeShipsAttackCapable, MakeShipsFriendlyAndAlliesShips, MakeShipsGuardC |
| 0x004e76b0 | pieMovePointClipToLimits | PiePlate.c | 955 | high | yes | ellipse-intersection clipping (sizeX/Y/Z params) replaced by clamping both points to the box +-(smUniverseSize - margin) |
| 0x00525640 | statsGatherFightStats | Stats.c | 635 | high |  | adds calls CommandInCommandLayer, GetShipStaticInfo, cat_statsGetPostCombatStats, cat_statsSetupShipsForFight, univUpdate; drops calls ShipRaceToStr, fileLoad, fileSizeGet, logfileClear; drops calls to fileSave (absent f |
| 0x0048b5b0 | gunShoot | Gun.c | 474 | high |  | adds calls ApplyDamageToTarget, cat_gunCreateBullet, cat_shipGetDamageMultiplier; drops calls bobAddObjToNearestBlob, bobAddObjToSpecificBlob, etgEffectCreate, etgFrequencyExceeded, listAddNode, matCreateCoordSysFromHead |
| 0x00507f10 | selStatusDraw0 | Select.c | 470 | medium |  | adds calls cat_salvageGetSalvagerInfo, cat_selStatusBarsDraw, cat_selStatusRectCompute; drops calls fontHeight, fontMakeCurrent, fontPrint, fontPrintf, fontWidth, primRectSolid2; body 3.1x smaller than predicted; fields  |
| 0x005080f0 | selStatusDraw1 | Select.c | 449 | medium |  | adds calls cat_salvageGetSalvagerInfo, cat_selStatusBarsDraw, cat_selStatusRectCompute; drops calls fontHeight, fontMakeCurrent, fontPrint, fontPrintf, fontWidth, primRectSolid2; body 3.0x smaller than predicted; fields  |
| 0x005082c0 | selStatusDraw2 | Select.c | 446 | medium |  | adds calls cat_salvageGetSalvagerInfo, cat_selStatusBarsDraw, cat_selStatusRectCompute; drops calls fontHeight, fontMakeCurrent, fontPrint, fontPrintf, fontWidth, primRectSolid2; body 2.8x smaller than predicted; constan |
| 0x0058dd30 | TitanInterface_ConnectTo | TitanInterface.cpp | 404 | medium |  | drops calls BuildAddress, ConnectTo; body 2.5x larger than predicted; fields -AddrPart, IP, Port, etherAddr; globals +CONNECT_TIMEOUT; library calls +WONMisc__OpenCmd__OpenCmd, WONMisc__RecvPrefCmd__RecvPrefCmd, WONMisc_ |
| 0x00465360 | RemoveShipFromDocking | Dock.c | 266 | medium |  | adds calls ShipTypeToStr, madAnimationStart; drops calls madLinkInPostDockingShip, memFree; adds strings "\nDOCKING CODE: A %s is clamped to so..."; body 3.3x smaller than predicted; constants 19, 26, 41, 67 not in body; |
| 0x00508480 | selStatusDraw3 | Select.c | 266 | medium |  | adds calls cat_selStatusBarsDraw; drops calls fontHeight, fontMakeCurrent, fontPrint, fontWidth; drops calls to primNonAALine2 (absent from Cataclysm); params 1 -> 0 (prototype); constants 0x7fe0 not in body |
| 0x004d66f0 | poPopupFleetIntelligence | Objectives.c | 257 | high |  | adds calls cat_kasHashString, cat_kasVarFindOrCreate, feDrawCallbackAddMultiple, feScreenStart, mrDisable, spMainScreen; drops calls aivarCreate; adds strings "G_CLICK_"; "Single_Player_Objective"; body 2.5x larger than  |
| 0x004ab4b0 | ioStartup | InfoOverlay.c | 220 | medium |  | adds calls cat_ioReset, scriptSet; drops calls fontHeight, fontMakeCurrent, regChildAlloc, regDrawFunctionSet, regFunctionSet, regLinkRemove; adds strings "Arial_b17.hff"; "MainOverlay.script"; "SimplixSSK_13.hff" (+1);  |
| 0x00594d10 | TitanInterface_SendPing | TitanInterface.cpp | 200 | medium |  | drops calls titanDebug; drops strings "EXCEPTION: TitanInterface::SendPing: %s"; params 2 -> 4 (prototype); fields +AddrPart, IP, Port -Pack, what; library calls +Ordinal, WONMisc__EasySocket__getSockAddrIpx, WONMsg__MMs |
| 0x0058d700 | TitanInterface_GetPatchArgs_dtor | TitanInterface.cpp | 179 | low | yes | compiler-generated destructor of a patch-arguments object; no HW1 source counterpart |
| 0x005bba70 | MothershipRemoveShipReferences | Mothership.c | 178 | medium |  | adds calls cat_beastWeaponRemoveShipReferences, cat_lmModuleSlotsRemoveType, cat_salvageDepotRemoveShipReferences, cat_shipHasBeastWeapon, etgEffectDelete, listDeleteNode (+1); drops calls madLinkInCloseDoor; drops calls |
| 0x00465590 | RemoveShipFromLaunching | Dock.c | 156 | medium |  | drops calls battleLaunchWelcomeExchange, madLinkInPostDockingShip; body 4.5x smaller than predicted; constants 19, 26, 41, 67 not in body; loops 3 -> 0 |
| 0x0058ded0 | TitanInterface_ConnectAndSend | TitanInterface.cpp | 136 | high |  | adds calls ConnectTo, SendMsg, titanDebug; drops calls BuildAddress, ConnectAndSend; adds strings "FAIL: TitanInterface::SendMsg!"; params 4 -> 6 (prototype); fields -AddrPart, IP, Port, etherAddr; library calls +WONMisc |
| 0x0051bdb0 | singlePlayerStartGame | SinglePlayer.c | 134 | medium |  | adds calls cat_spWarpToLevel; drops calls ccFocusOnPlayersMothership, levelInit, levelStartNext, soundEventPlayMusic; adds strings "nis\\n01.nis"; "nis\\n01.script"; drops strings "\nUnsupported mission #%d"; "CloakDefen |
| 0x004e25f0 | partModifyTumble | Particle.c | 131 | medium |  | drops strings "\nparticle system is not of type PART..."; takes one argument-array pointer instead of 2 parameters; body 2.0x smaller than predicted; fields -n, t, tumble |
| 0x004f1730 | AsteroidTakesDamage | ResCollect.c | 109 | medium | yes | asteroid damage model replaced: a value converted with ftol is stored in an asteroid field and the function returns TRUE when targetWasAlive and resourceVolume <= 0; HW1's RU damage ratio, resourcevalue decrement, health |
| 0x00417e60 | aitMsgReceived | AITeam.c | 96 | medium |  | drops calls memFree; fields -msgQueue, msgSender, msgSenders, msgs |
| 0x00424ef0 | battleLaunchWelcomeExchange | Battle.c | 94 | high |  | adds calls battleChatterFleetAttempt; drops calls battleCanChatterAtThisTime; parameters 1 -> 2: void battleLaunchWelcomeExchange(sdword shipclass, sdword variable); body 7.9x smaller than predicted; fields -ShipPtr, blo |
| 0x0042ec90 | captaincyLog | Captaincy.c | 85 | medium |  | drops calls logfileLog; drops strings "captaincylog.txt"; params 3 -> 2 (prototype); calls with a changed parameter count: clCommandMessage 1->2; body 1.8x smaller than predicted; fields -wHour, wMinute, wSecond; library |
| 0x005bb730 | MothershipStaticInit | Mothership.c | 82 | medium |  | adds calls cat_beastWeaponStaticInit, cat_staticHasBeastWeapon; drops calls scriptSetStruct; body 2.1x smaller than predicted; fields -custstatinfo, shiprace, specialDoorOffset, specialDoorOrientationHeading (+4); consta |
| 0x00495950 | kasfShipsSelectIndex | KASFunc.c | 78 | medium |  | adds calls growSelectAddShip, kasGrowSelectionClear; drops calls clRemoveTargetFromSelection; body 2.2x smaller than predicted |
| 0x00490aa0 | kasPathPtr | KAS.c | 75 | medium |  | drops calls dbgFatalf; adds strings "\nKAS: unresolved reference to path %u"; constants 47 not in body |
| 0x00495a50 | kasfShipsSelectNearby | KASFunc.c | 75 | high |  | adds calls cat_kasShipsSelectByPredicate; drops calls aiuFindDistanceSquared, growSelectAddShip; body 3.0x smaller than predicted; fields +maxNumShips -ShipPtr, numShips, posinfo, position (+1) |
| 0x00417e10 | aitMsgSend | AITeam.c | 73 | medium |  | drops calls memAllocFunction, memFree; body 2.0x smaller than predicted |
| 0x00464610 | dockReserveDockPoint | Dock.c | 70 | high |  | drops calls madLinkInCloseSpecialShip, madLinkInOpenSpecialShip; body 5.0x smaller than predicted; fields -dockInfo, dockpoints, dockstaticpoint, dockvars (+6); constants 26, 41, 67 not in body; loops 2 -> 0 |
| 0x0058f626 | TitanInterface_AuthHandleLogin_tail | TitanInterface.cpp | 67 | high |  | drops calls ReadLoginKey, mgDisplayMessage, titanDebug; drops strings "EXCEPTION: AuthHandleLogin unable to ..."; "EXCEPTION: TitanInterface::AuthHanleL..."; "EXCEPTION: Unable to build AuthLogin ..." (+8); body 14.9x sm |
| 0x0046ae90 | etgLoadErrorf | ETG.c | 64 | high |  | drops strings "\n%s (%d): Non-fatal error - %s"; "Non-Fatal Error (cancel to debug)"; params 4 -> 3 (prototype); body 2.4x smaller than predicted; globals -dbgFatalErrorString, dbgInt3Enabled, ghMainWindow; library calls |
| 0x00491d30 | kasfVarCreateSet | KASFunc.c | 61 | high |  | adds calls cat_kasVarFindOrCreate; drops calls aivarCreate, aivarFind, aivarValueSet; KAS hash keys: the name arrives as a hash (high bit = global scope); kasfScopeName string building gone; body 2.2x smaller than predic |
| 0x00491d70 | kasfVarValueSet | KASFunc.c | 61 | high |  | adds calls cat_kasVarFind; drops calls aivarFind, aivarValueSet; KAS hash keys: the name arrives as a hash (high bit = global scope); kasfScopeName string building gone; body 2.1x smaller than predicted; library calls -s |
| 0x004907e0 | kasAITeamShipsPtr | KAS.c | 60 | medium |  | drops strings "MsgSender"; body 1.8x smaller than predicted; fields +enemyShipsIAmAwareOf, maxNumShips -kasLabel, msgSender, shipList, teamType (+2); constants 47 not in body |
| 0x00491e30 | kasfVarValueGet | KASFunc.c | 60 | high |  | adds calls cat_kasVarFind; drops calls aivarFind, aivarValueGet; KAS hash keys: the name arrives as a hash (high bit = global scope); kasfScopeName string building gone; body 2.2x smaller than predicted; library calls -s |
| 0x00497290 | kasfSaveLevel | KASFunc.c | 60 | medium |  | drops calls SaveGame, clCommandMessage, collUpdateCollBlobs, collUpdateObjsInCollBlobs; drops strings "SavedGames\SinglePlayer\"; library calls -sprintf; loops 0 -> 3 |
| 0x00491e70 | kasfVarDestroy | KASFunc.c | 59 | high |  | adds calls cat_kasVarDestroy; drops calls aivarDestroy, aivarFind; KAS hash keys: the name arrives as a hash (high bit = global scope); kasfScopeName string building gone; body 2.2x smaller than predicted; library calls  |
| 0x004db400 | opCPUDifficulty | Options.c | 59 | high |  | adds calls feRadioButtonSet; drops calls regFilterSet; body 3.2x smaller than predicted; fields +pData -aiplayerDifficultyLevel, maxvalue, processFunction, value; globals +tpGameCreated_aiplayerDifficultyLevel -tpGameCre |
| 0x00491db0 | kasfVarValueInc | KASFunc.c | 57 | high |  | adds calls cat_kasVarFind; drops calls aivarFind, aivarValueGet, aivarValueSet; KAS hash keys: the name arrives as a hash (high bit = global scope); kasfScopeName string building gone; body 2.6x smaller than predicted; l |
| 0x00491df0 | kasfVarValueDec | KASFunc.c | 57 | high |  | adds calls cat_kasVarFind; drops calls aivarFind, aivarValueGet, aivarValueSet; KAS hash keys: the name arrives as a hash (high bit = global scope); kasfScopeName string building gone; body 2.6x smaller than predicted; l |
| 0x004db440 | opCPUAttacks | Options.c | 56 | high |  | adds calls feRadioButtonSet; drops calls regFilterSet; body 3.4x smaller than predicted; fields +pData -aiplayerBigotry, maxvalue, processFunction, value; globals +tpGameCreated_aiplayerDifficultyLevel -tpGameCreated; co |
| 0x00447950 | RemoveShipFromFormation | CommandLayer.c | 54 | high | yes | HW1's ship->formationcommand = NULL replaced: sets disable reason 5 when a ship state field is 1, and unclamps sSentinel/bSentinel (types 17/52) |
| 0x004907a0 | kasAITeamPtr | KAS.c | 54 | medium |  | drops strings "MsgSender"; body 1.9x smaller than predicted; fields +enemyShipsIAmAwareOf, maxNumShips -kasLabel, msgSender, teamType, teams (+1); constants 47 not in body |
| 0x00491cf0 | kasfVarCreate | KASFunc.c | 54 | high |  | adds calls cat_kasVarFindOrCreate; drops calls aivarCreate; KAS hash keys: the name arrives as a hash (high bit = global scope); kasfScopeName string building gone; body 2.3x smaller than predicted; library calls -sprint |
| 0x00492960 | kasfGuardShips | KASFunc.c | 53 | medium |  | adds calls clWrapProtect; drops calls aimCreateGuardShips, selectMemDupSelection; drops strings "selDup"; globals +universe |
| 0x00495640 | kasfShipsSelectClass | KASFunc.c | 40 | high |  | adds calls cat_kasShipsSelectByPredicate; drops calls clRemoveTargetFromSelection, growSelectAddShip; body 6.8x smaller than predicted; fields +maxNumShips -ShipPtr, numShips, selection, shipclass (+1); loops 2 -> 0 |
| 0x00495690 | kasfShipsSelectType | KASFunc.c | 40 | high |  | adds calls cat_kasShipsSelectByPredicate; drops calls clRemoveTargetFromSelection, growSelectAddShip; body 6.4x smaller than predicted; fields +maxNumShips -ShipPtr, numShips, selection, shiptype; loops 2 -> 0 |
| 0x00590309 | TitanInterface_AuthHandleRefresh_tail | TitanInterface.cpp | 40 | high |  | drops calls ConnectAndSend, ReadLoginKey, titanDebug; drops strings "AuthHandleRefresh: Login key not found."; "EXCEPTION: TitanInterface::AuthHanleR..."; "FAIL: Couldn't encrypt block with aut..." (+2); params 0 -> 1 (p |
| 0x004901b0 | hsStaticInit | HS.c | 39 | high |  | drops calls aivarCreate, aivarValueSet, memAllocFunction, memStrncpy; drops calls to univAddHyperspaceGateAsDerelict (absent from Cataclysm); parameters 1 -> 0: void hsStaticInit(void); body 10.1x smaller than predicted; |
| 0x005af3c0 | Carrier_Fix | Carrier.c | 38 | medium |  | adds calls cat_beastWeaponFix, cat_salvageDepotFix, cat_shipHasBeastWeapon; drops calls SpaceObjRegistryGetObj; fields -ShipSpecifics, droptarget |
| 0x0058e990 | TitanInterface_PingHandleReply_tail | TitanInterface.cpp | 33 | high |  | drops calls titanDebug; drops strings "EXCEPTION: TitanInterface::PingHandle..."; "TitanInterface::PingHandleReply"; params 1 -> 0 (prototype); body 6.1x smaller than predicted; fields -Enter, GetLag, Leave, what |
| 0x004957d0 | kasfShipsSelectDamaged | KASFunc.c | 31 | high |  | adds calls cat_kasShipsSelectByPredicate; drops calls clRemoveTargetFromSelection, growSelectAddShip; body 8.3x smaller than predicted; fields +maxNumShips -ShipPtr, health, maxhealth, numShips (+2); constants 100 not in |
| 0x00495b00 | kasfShipsSelectSpecial | KASFunc.c | 31 | high |  | adds calls cat_kasShipsSelectByPredicate; drops calls clRemoveTargetFromSelection, growSelectAddShip; body 15.3x smaller than predicted; fields +maxNumShips -ShipPtr, attributes, flags, numShips (+5); constants 23, 26, 2 |
| 0x00495560 | kasfShipsSelectEnemy | KASFunc.c | 28 | high |  | adds calls cat_kasShipsSelectByPredicate; drops calls clRemoveTargetFromSelection, growSelectAddShip; body 8.7x smaller than predicted; fields +maxNumShips -ShipPtr, numShips, playerIndex, playerowner (+1); loops 2 -> 0 |
| 0x00495620 | kasfShipsSelectFriendly | KASFunc.c | 28 | high |  | adds calls cat_kasShipsSelectByPredicate; drops calls clRemoveTargetFromSelection, growSelectAddShip; body 8.7x smaller than predicted; fields +maxNumShips -ShipPtr, numShips, playerIndex, playerowner (+1); loops 2 -> 0 |
| 0x004956e0 | kasfShipsSelectCapital | KASFunc.c | 28 | high |  | adds calls cat_kasShipsSelectByPredicate; drops calls clRemoveTargetFromSelection, growSelectAddShip; body 9.2x smaller than predicted; fields +maxNumShips -ShipPtr, numShips, selection, shipIsCapital (+1); loops 2 -> 0 |
| 0x00495730 | kasfShipsSelectNonCapital | KASFunc.c | 28 | high |  | adds calls cat_kasShipsSelectByPredicate; drops calls clRemoveTargetFromSelection, growSelectAddShip; body 9.3x smaller than predicted; fields +maxNumShips -ShipPtr, numShips, selection, shipIsCapital (+1); loops 2 -> 0 |
| 0x00495790 | kasfShipsSelectNotDocked | KASFunc.c | 28 | medium |  | adds calls cat_kasShipsSelectByPredicate; drops calls clRemoveTargetFromSelection, growSelectAddShip; body 9.1x smaller than predicted; fields +maxNumShips -ShipPtr, flags, numShips, selection; loops 2 -> 0 |
| 0x004958b0 | kasfShipsSelectMoving | KASFunc.c | 28 | high |  | adds calls cat_kasShipsSelectByPredicate; drops calls clRemoveTargetFromSelection, growSelectAddShip; body 8.8x smaller than predicted; fields +maxNumShips -ShipPtr, isMoving, numShips, posinfo (+1); loops 2 -> 0 |
| 0x0059a410 | TitanInterface_HandleGameMsg | TitanInterface.cpp | 26 | medium |  | drops calls titanDebug; drops strings "EXCEPTION: TitanInterface::HandleGame..."; params 1 -> 3 (prototype); body 3.9x smaller than predicted |
| 0x0058e562 | TitanInterface_FirewallDetect_tail | TitanInterface.cpp | 25 | high |  | drops calls ConnectAndSend, titanDebug; drops strings "EXCEPTION: TitanInterface::FirewallDe..."; "FAIL: TitanInterface::FirewallDetect!"; "NOTE: TitanInterface::FirewallDetect ..." (+1); body 14.5x smaller than predicte |
| 0x0058e8c3 | TitanInterface_PingFactServer_tail | TitanInterface.cpp | 25 | high |  | drops calls ConnectAndSend, titanDebug; drops strings "EXCEPTION: TitanInterface::PingFactSe..."; "TitanInterface::PingFactServer"; body 11.7x smaller than predicted; fields -AddOutgoingCmd, Leave, Pack, what |
| 0x0058eb1b | TitanInterface_AuthGetPubKeyBlock_tail | TitanInterface.cpp | 25 | high |  | drops calls ConnectAndSend, titanDebug; drops strings "EXCEPTION: TitanInterface::AuthGetPub..."; "FAIL: No auth server to talk to."; "NOTE: TitanInterface::AuthGetPubKeyBl..." (+1); body 13.3x smaller than predicted; fi |
| 0x00524eb0 | statsNumShipsNeededToKillFleet | Stats.c | 21 | high | yes | returns a per-class table value times 3; HW1's kill-ratio computation against the target fleet (statsGetKillRatingAgainstFleet, overkill factor) is gone |
| 0x004bdce0 | mgUnitCapsEnabled | MultiplayerGame.c | 19 | medium |  | drops calls feToggleButtonSet, mgGameTypesOtherButtonPressed; body 5.6x smaller than predicted; fields +flags, region -flag, status; globals -tpGameCreated |
| 0x005c5a60 | SalCapCorvetteSpecialTarget | SalCapCorvette.c | 19 | high |  | adds calls cat_salvageSpecialTarget; drops calls ApplyDamageToTarget, MoveReachedDestinationVariable, RangeToTarget, RemoveShipsFromDoingStuff, ShipInSelection, aishipFlyToPointAvoidingObjsFunc (+24); drops strings "\nSA |
| 0x005c5aa0 | SalCapRemoveShipReferences | SalCapCorvette.c | 19 | high |  | adds calls cat_salvageRemoveShipReferences; drops calls clDock, clHalt, getShipAndItsCommand, madLinkInOpenSpecialShip, unClampObj; body 18.2x smaller than predicted; fields -PlayerMothership, ShipList, ShipPtr, ShipSpec |
| 0x00529130 | tacticsReportMove | Tactics.c | 17 | high |  | drops calls listAddNode, memAllocFunction; body 19.4x smaller than predicted; fields -RetreatGetAwayDistSqr, RetreatList, ShipPtr, attack (+21); globals -universe; loops 5 -> 1 |
| 0x00524ed0 | statsGetRelativeFleetStrengths | Stats.c | 15 | high | yes | returns fleet2->numShips / fleet1->numShips; HW1 sums statsGetKillRatingAgainstFleet over fleet2 |
| 0x005b3720 | CloakedFighterSpecialActivate | GenericInterceptor.c | 14 | high |  | adds calls cat_cloakActivate; drops calls battleCanChatterAtThisTime, battleChatterAttempt, soundEventPlay; body 21.7x smaller than predicted; fields -DeCloakFuelLevel, ReCloak, ShipSpecifics, collspheresize (+15); globa |
| 0x005b0490 | DefenseFighterHouseKeep | DefenseFighter.c | 12 | high |  | drops calls fmathSqrtDouble, listDeleteNode, matCreateCoordSysFromHeading, matMultiplyMatByVec, ranRandomFn, soundEventBurstStop (+3); adds strings "\n@@@@@@@@ Defense Fighter Doesn't Wo..."; params 1 -> 0 (prototype); b |
| 0x005b3840 | CloakedFighterHouseKeep | GenericInterceptor.c | 12 | high |  | adds calls cat_cloakUpdate; drops calls battleCanChatterAtThisTime, battleChatterAttempt, shipHasJustCloaked, soundEventPlay; body 42.8x smaller than predicted; fields -CloakFuelBurnRate, CloakLowWarning, CloakingStatus, |
| 0x005bcdb0 | Mothership_PreFix | Mothership.c | 12 | medium |  | adds calls cat_salvageDepot_PreFix; drops calls SpaceObjRegistryGetID; body 6.3x smaller than predicted; fields -ShipSpecifics, doorCargo, droptarget |
| 0x005c5a90 | SalCapCorvetteHouseKeep | SalCapCorvette.c | 12 | high |  | adds calls cat_salvageHouseKeep; drops calls GroupShipIntoMilitaryParade, RemoveShipsFromDoingStuff; body 7.7x smaller than predicted; fields -ShipPtr, ShipSpecifics, clampInfo, currentLOD (+24); globals -universe; float |
| 0x005c5ac0 | SalCapDied | SalCapCorvette.c | 12 | high | yes | body replaced by a call to the new cat_salvageDied(ship) (Cataclysm salvage system) |
| 0x005c5ad0 | SalCapCorvette_PreFix | SalCapCorvette.c | 12 | medium |  | adds calls cat_salvage_PreFix; drops calls SpaceObjRegistryGetID; fields -ShipSpecifics, dockwith, noDamageTarget, target |
| 0x004f4bd0 | rmClearAllLabs | ResearchGUI.c | 11 | medium |  | drops calls rmClearResearchlab; params 2 -> 0 (prototype); body 3.9x smaller than predicted |

## Not classified

Matched functions for which neither Relic's tree nor the GoK working tree has a definition the brace matcher found:

- `0x00425be0` bitioFileInputBit (`bitioFileInputBit`, 70 bytes): no definition of bitioFileInputBit in ORIG@ad1bb82 src/{Game,Win32,Ships,SinglePlayer} (cpp -DHW_Release) nor in the GoK working tree
- `0x00425c30` bitioFileInputBits (`bitioFileInputBits`, 94 bytes): no definition of bitioFileInputBits in ORIG@ad1bb82 src/{Game,Win32,Ships,SinglePlayer} (cpp -DHW_Release) nor in the GoK working tree
- `0x0055bfd0` glcDisplayExtents (`glcDisplayExtents`, 1192 bytes): no definition of glcDisplayExtents in ORIG@ad1bb82 src/{Game,Win32,Ships,SinglePlayer} (cpp -DHW_Release) nor in the GoK working tree
- `0x0058b8a0` TitanInterface_TitanInterface (`TitanInterface::TitanInterface`, 1629 bytes): no definition of TitanInterface::TitanInterface in ORIG@ad1bb82 src/{Game,Win32,Ships,SinglePlayer} (cpp -DHW_Release) nor in the GoK working tree
- `0x0058bf00` TitanInterface_deleting_dtor (`TitanInterface::~TitanInterface`, 30 bytes): no definition of TitanInterface::~TitanInterface in ORIG@ad1bb82 src/{Game,Win32,Ships,SinglePlayer} (cpp -DHW_Release) nor in the GoK working tree
- `0x0058c110` TitanInterface_dtor (`TitanInterface::~TitanInterface`, 1122 bytes): no definition of TitanInterface::~TitanInterface in ORIG@ad1bb82 src/{Game,Win32,Ships,SinglePlayer} (cpp -DHW_Release) nor in the GoK working tree

## Method

tools/delta_map.py. HW1 side: Relic's source at GoK root commit ad1bb82 (gitlab archive of ad1bb82ca3c62dcc8be9d35f2e836d0bb7cf7caa, src/{Game,Win32,Ships,SinglePlayer}); each unit run through cpp -DHW_Release -DWIN32 -D_WIN32 -D_X86_ -D_MSC_VER=1100 -D_M_IX86=600 -D__MSC__=yes -DHW_English -D_MT with a case-insensitive include dir (system headers stubbed), so debug-only code is compiled out as in a retail build and macros are expanded; definitions found by a brace matcher on the token stream. Names ORIG does not define fall back to the GoK working tree (hw1_source=GoK-HEAD). Cataclysm side: ghidra/exports/cataclysm_gog_r5 decompiled.c body (comments stripped), call_graph.csv callees, strings.csv xrefs. Target set: binary-map class hw1:* (symbols_round1-5 notes, then the audited r2-r5 proposal sets) plus CET entries with an hw1_equivalent whose name is current (the docs/binary-map.md R5 rule, 4,426 functions). Features: game-callee sets by canonical HW1 name (HomeworldSDL names mapped through ORIG macro aliases, e.g. clRemoveShipFromSelection -> clRemoveTargetFromSelection); a dropped callee whose own callees appear instead, or a small one from the same unit, counts as inlined and its strings/fields/constants are expected in the caller; C++ member calls, thunk targets and tail calls named in the body count; the map's cat_<hw1name> spellings map back; library/CRT callees compared separately (soft); string literals of >= 4 chars incl. char[] globals (an untyped string behind a DAT_ reference is excused; AI move-name strings \naim* are one header change); struct fields (-> and .) against every identifier in Relic's tree; named globals from symbols.csv; distinctive integer constants (not -2..16, not masks/powers of two); float constants; for/while loop counts; switch/case counts; parameter count; body size vs the HW1 file's median log(bytes/tokens). Hard deltas: game callee added or dropped (not inlined), HW1 callee absent from Cataclysm, string added or dropped, struct field HW1 never names, parameter count from an evidence-named prototype, argument-array calling convention, body > 2.7x larger with more branches. IDENTICAL = no hard delta (soft deviations only); REWRITTEN = a hard delta, under half of HW1's callees+strings survive, and weighted score < 0.4 or (>= 4 game callees, callee Jaccard < 0.25, string Jaccard < 0.34); else MODIFIED. 'change' is the automatic delta list unless reviewed=true (hand review of both bodies, recorded in ghidra/delta_map_review.json, which also holds the write-up's findings under _notes).

Reproduce (the ad1bb82 tree is the gitlab archive `https://gitlab.com/gardens-of-kadesh/gardens-of-kadesh/-/archive/ad1bb82c/gardens-of-kadesh-ad1bb82c.tar.gz?path=src`, extracted to `original/scratch-m2/delta-orig/`; its pax header names commit ad1bb82ca3c62dcc8be9d35f2e836d0bb7cf7caa; the reference clones are depth 1 and do not hold that commit):

```
python3 -I tools/delta_map.py prep       # cpp + index -> original/scratch-m2/delta-work/hw1_index.json
python3 -I tools/delta_map.py classify   # -> ghidra/delta_map.json (merges ghidra/delta_map_review.json)
python3 -I tools/delta_map.py show 0x00401000   # both bodies side by side, for review
python3 -I tools/delta_map.py report     # -> this file
```

