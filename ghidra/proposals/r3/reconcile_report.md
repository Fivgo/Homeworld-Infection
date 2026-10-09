# Round-3 symbol reconcile report

Output: `ghidra/symbols_round3.json` (cet_symbols.json schema). It holds **3087 symbols**: **1080 new names** (1075 function, 5 data), **0 renames**, and **2007 prototype-only entries** (current name repeated, `type` = signature). Generated 2026-10-09.

Inputs:
- Applied: `ghidra/symbols_round1.json` (8,312), `ghidra/symbols_round1_data.json` (925), `ghidra/symbols_round2.json` (962), and the 105 B1-created names in `ghidra/create_functions_r3.json`.
- Audited sets in `ghidra/proposals/r3/`: `table-targets` (556, precision 1.0), `game-ai-ships` (261, 0.967), `game-render-ui` (444, 1.0), `game-sim-mp` (760, 0.967), `prototypes` (2,007 kept of 2,084, 0.967).
- Validation export: `ghidra/exports/cataclysm_gog_r3a` (14,490 functions; functions.csv, symbols.csv, strings.csv, decompiled.c, call_graph.csv).
- Binary checks: `objdump -d -M intel -j .text original/game/cataclysm.exe`. This gave the RET imm of each function body and the caller stack cleanup (`add esp,N` / `pop ecx`) after each direct call. The cleanup logic is the prototypes producer's `stack.py`, re-run on that disassembly.

## B1 check

All 105 names in `create_functions_r3.json` are function entries with exactly that name in r3a `functions.csv`, so B1 applied them. They are not re-emitted, and they count as taken names in the uniqueness check.

## Totals

| item | count |
|---|---|
| input `table-targets.verified.json` | 556 |
| input `game-ai-ships.verified.json` | 261 |
| input `game-render-ui.verified.json` | 444 |
| input `game-sim-mp.verified.json` | 760 |
| input `prototypes.verified.json` | 2007 |
| distinct naming-proposal addresses | 1448 |
| class-only proposals (empty name: WON/CryptoPP template slots, msvc-stl, ICF-ambiguous); nothing to apply | 182 |
| cross-set conflicts at one address, resolved (table below) | 21 |
| proposals whose name is already applied (no-op) | 0 |
| **new function names** (all on `FUN_*` entries in r3a) | **1075** |
| **new data names** (all in `.data`) | **5** |
| **renames of round-1/round-2/B1 names** | **0** |
| **deferred to round 4**: function address not an entry in r3a functions.csv | **186** |
| dropped names (collision, not an identifier, data outside sections) | 0 |
| naming-set signatures kept (after binary checks; explicit `__cdecl` added) | 420 |
| naming-set signatures dropped (name kept, `type` empty) | 13 |
| **prototype-only entries** | **2007** |
| prototypes dropped | 0 |
| CET anchors overridden | 0 |
| **symbols written** | **3087** |

New names by confidence: high 768, low 51, medium 261. By class (`hw1` = all `hw1:<File.c>`): hw1 756, cat-new 256, won 67, crt 1.

By set (a `+` means those sets proposed the same name): game-ai-ships 171, game-ai-ships+game-render-ui+game-sim-mp 2, game-ai-ships+game-sim-mp 70, game-ai-ships+table-targets 18, game-render-ui 125, game-render-ui+game-sim-mp 45, game-render-ui+game-sim-mp+table-targets 107, game-render-ui+table-targets 39, game-sim-mp 175, game-sim-mp+table-targets 78, table-targets 250.

Prototype entries by confidence: high 831, low 76, medium 1100.

## Rules as applied

- **Scope.** The file holds only new names and prototypes. A proposal whose name Ghidra already carries would be a no-op. There were none: every named round-3 proposal that resolves to an r3a function targets a `FUN_*` entry.
- **Validity.** A function address must be an entry in r3a `functions.csv`; the rest go to `reconcile_deferred_functions.json`. A data address must lie in `.rdata` 0x72a000-0x749fff or `.data` 0x74a000-0xc54197 (r3a `symbols_summary.txt`); all 5 pass. Every name matches `^[A-Za-z_][A-Za-z0-9_]*$`.
- **Uniqueness.** Each name was checked against the CET names (patch sites included), round 1, round-1 data, round 2, B1, every non-default function and label name in r3a, the other round-3 names, and the deferred names (which reserve theirs). One pair needed resolving: `aiuFindResourceControllerDestination` was proposed for 0x0041BAA0 (game-ai-ships) and 0x0041BB80 (game-sim-mp). The wrong one was fixed rather than suffixed (conflict table). No `_N` suffix was added.
- **Renames.** There are none. No round-3 set proposed a different name for an address that already has a round-1, round-2 or B1 name, and no round-3 name collided with an applied name. So no earlier name had to be replaced.
- **CET.** No emitted or deferred address is a CET address, and no round-3 name reuses a CET name. `cet_symbols.json` is unchanged.
- **Cross-set conflicts.** Rules in order: (1) evidence read in this reconcile (r3a body or call graph, HW1 source, raw table bytes); (2) higher confidence; (3) the name the function gives itself or the binary's keyword string; (4) higher audit precision, so table-targets/game-render-ui (1.0) beat game-sim-mp/game-ai-ships (0.967). A merged entry takes the highest confidence among the sets that agree on its name.
- **Notes.** `notes` starts `class=<class>; confidence=<c>; sets=<sets>`. Optional fields follow: `resolved=cross-set (rejected: ...)`, `sig-checked:...` / `sig-dropped:...`, `section=` (data), then `evidence=<=200 chars`. Prototype entries read `sets=prototypes; prototype-only (name unchanged)`. `source` names the verified file of the highest-ranked agreeing set.
- **Apply order.** No rename releases a name, so the order does not matter. ImportSymbolsJson sorts functions first (ascending), then data.

## Renames (0)

None. Every round-3 naming proposal targets an address whose r3a name is a default `FUN_*`, or a data address with no label. The candidates for "an earlier name is wrong" were all inside round 3 and are settled in the conflict table.

## CET differs (0)

None. No round-3 entry touches a CET anchor address, and no prototype entry falls on a CET function.

## Cross-set conflicts (21)

| address | proposals (set: name, confidence) | chosen | reason |
|---|---|---|---|
| 0x00417470 | game-ai-ships: `aitTeamIsInMothershipRange` medium; game-sim-mp: `aitTeamIsInRange` medium | `aitTeamIsInMothershipRange` | r3a body takes one arg (team), reads team->aiplayerowner->player->PlayerMothership then runs the aitTeamIsInRange test inline; caller 0x403070 (HW1 AIDefenseMan.c:630 caller of aitTeamIsInMothershipRange) |
| 0x00417F20 | game-ai-ships: `cat_aitMsgQueueFlush` medium; game-sim-mp: `aitMsgQueueFree` low | `cat_aitMsgQueueFlush` | r3a body only zeroes team+0x50 (19 bytes, no memFree); HW1 aitMsgQueueFree frees every msg and the queue, so the HW1 name does not fit; medium beats low |
| 0x0041BAA0 | game-ai-ships: `aiuFindResourceControllerDestination` high; game-sim-mp: `aiuFindResourceCollectors` medium | `aiuFindResourceControllerDestination` | callers aimProcessControlResources + aimProcessMothershipMove = the two HW1 AIMoves.c callers (2616, 3246/3263) of aiuFindResourceControllerDestination; HW1 aiuFindResourceCollectors is a one-line accessor (inlined); high beats medium |
| 0x0041BB80 | game-ai-ships: `aiuDestinationNotNearOtherMothership` high; game-sim-mp: `aiuFindResourceControllerDestination` medium | `aiuDestinationNotNearOtherMothership` | only caller aimProcessMothershipMove, (vector,real32) = 4 dword args as r3a param_count 4; HW1 AIMoves.c:3252/3269 calls it right after aiuFindResourceControllerDestination; high beats medium (sim-mp was one slot off) |
| 0x0045B1C0 | table-targets: `cat_conTextDraw` medium; game-render-ui: `cat_consoleDraw` medium | `cat_conTextDraw` | name follows the FE callback string "CON_TextDraw" in the {fn,name} row @0x754fa8 (binary-sourced); equal confidence, table-targets audit precision 1.0 |
| 0x004D0110 | table-targets: `cat_nisCameraClipNear` high; game-render-ui: `cat_nisCameraClipNear` medium; game-sim-mp: `cat_nisCameraNearClip` medium | `cat_nisCameraClipNear` | two sets (table-targets high, game-render-ui medium) agree; follows the Cataclysm NEO keyword order |
| 0x004D0140 | table-targets: `cat_nisCameraClipFar` high; game-render-ui: `cat_nisCameraClipFar` medium; game-sim-mp: `cat_nisCameraFarClip` medium | `cat_nisCameraClipFar` | two sets (table-targets high, game-render-ui medium) agree; follows the Cataclysm NEO keyword order |
| 0x004DBDA0 | table-targets: `cat_feOverlayIconTextureSetCB` medium; game-sim-mp: `cat_overlayIconTextureLoadCB` medium | `cat_feOverlayIconTextureSetCB` | same function, spelling variant; equal confidence, table-targets audit precision 1.0 (tie-break) |
| 0x004EA200 | table-targets: `psCrossFadeLinkSet` high; game-render-ui: `psImageSet` medium | `psCrossFadeLinkSet` | PlugScreen script row "CrossFadeLink" @0x88dcf0; body RemoveCommasFromString + sscanf("%f %s") + psImageLoad = HW1 PlugScreen.c:424 psCrossFadeLinkSet verbatim (psImageSet has no sscanf); high beats medium |
| 0x00510940 | game-ai-ships: `cat_shipPrintKeyHelp` medium; game-sim-mp: `cat_shipPrintDrawTechHelp` low | `cat_shipPrintKeyHelp` | same function, naming variant; medium beats low |
| 0x00510CA0 | game-ai-ships: `cat_shipPrintToggle` high; game-sim-mp: `cat_shipPrintToggleKey` medium | `cat_shipPrintToggle` | same function, naming variant; high beats medium |
| 0x00517060 | table-targets: `cat_spWarpLevelSetCB` medium; game-sim-mp: `cat_spSetWarpLevelCB` medium | `cat_spWarpLevelSetCB` | same function (warp script "Level" row), spelling variant; equal confidence, table-targets precision 1.0 (tie-break) |
| 0x00539E40 | table-targets: `cat_madNumberOfAnimsSetCB` medium; game-sim-mp: `cat_univSetNumberOfAnims` medium | `cat_madNumberOfAnimsSetCB` | same function ("NUMBER_OF_ANIMS" row), spelling variant; equal confidence, table-targets precision 1.0 (tie-break) |
| 0x00539F70 | table-targets: `cat_madAnimSetCB` medium; game-sim-mp: `setMadAnim` medium | `setMadAnim` | the function names itself: r3a strings.csv 0x008b5738 "setMadAnim(): Animation %s not found in %s" is referenced only by 0x539f70; the original identifier beats a constructed cat_ name |
| 0x0053AC50 | table-targets: `cat_scriptSetBeastAttackConvertToCB` medium; game-sim-mp: `cat_univSetBeastAttackConvertTo` medium | `cat_scriptSetBeastAttackConvertToCB` | same function ("beastAttackConvertTo" row), spelling variant; equal confidence, table-targets precision 1.0 (tie-break) |
| 0x0054D080 | table-targets: `cat_spConsoleAddResources` medium; game-render-ui: `cat_consoleAddResources` medium | `cat_spConsoleAddResources` | same function (console row "addResources"), spelling variant; equal confidence; table-targets precision 1.0 (tie-break) and matches the applied r2 sibling cat_spConsoleWarpMission |
| 0x0057B820 | table-targets: `cat_lfTextureFileSetCB` medium; game-render-ui: `cat_rndLensFlareTextureSetCB` medium | `cat_lfTextureFileSetCB` | LensFlares script row "TextureFile" @0x8babd4 (read from cataclysm.exe); keyword-exact name; equal confidence, tie-break |
| 0x0057B970 | table-targets: `cat_lfElementSetCB` medium; game-render-ui: `cat_rndLensFlareElementSetCB` medium | `cat_lfElementSetCB` | LensFlares script row "Element" @0x8babb0; keyword-exact name; equal confidence, tie-break |
| 0x0057BA00 | table-targets: `cat_lfTextureSetCB` medium; game-render-ui: `cat_rndLensFlareColourSetCB` low | `cat_lfTextureSetCB` | LensFlares script row "Texture" @0x8babc8 -> 0x57ba00 (read from cataclysm.exe); medium beats game-render-ui low (cat_rndLensFlareColourSetCB; body does store 4 ints/255 as colour) |
| 0x0057CB70 | table-targets: `cat_scriptSetFogCB` medium; game-render-ui: `cat_rndFogLayerSetCB` medium | `cat_scriptSetFogCB` | MissionScriptTable row "Fog"; equal confidence, table-targets precision 1.0 (tie-break) |
| 0x005AA0F0 | game-render-ui: `cat_utyMultiplayerQuitNotify` medium; game-sim-mp: `cat_utyGameEndNetworkQuit` medium | `cat_utyMultiplayerQuitNotify` | same function, naming variant; equal confidence, game-render-ui audit precision 1.0 vs 0.967 |

## Signatures on new names

Naming-set signatures had no calling convention. If one were applied as-is, Ghidra would keep its analysed convention, which is often a bogus `__stdcall` or `__thiscall`. Each signature was therefore checked against the binary before it was kept:
- RET imm in the body (objdump): 0 gives `__cdecl`, and the convention is written into the signature. N>0 must equal the declared argument bytes (none occurred). With no RET (tail jump or noreturn), the signature is kept only when r3a already says `__cdecl`, or for CRT `longjmp`.
- Caller cleanup. If every direct caller cleans more than the declared bytes (at least 2 sites), Cataclysm added arguments, so the signature is dropped. If every caller cleans less, the HW1 declaration has too many arguments, so it is also dropped.
- r3a `param_count` (minus ECX/EDX for thiscall/fastcall misreads) larger than the declared dword slots (vector = 3, real64 = 2, plus a hidden pointer for a `vector` return) also drops it.
- Fewer Ghidra params than declared is accepted (unused trailing args such as `dataToFillIn`, or by-value vectors); 29 kept signatures carry a `ghidra-params=` note.
- Kept: 420, all now `__cdecl`. Dropped: 13 (names kept):

| address | name | set | proposed signature | why dropped |
|---|---|---|---|---|
| 0x00404040 | `aieHandlerSetTeamDied` | game-ai-ships | `void aieHandlerSetTeamDied(AITeamMove *move, aieHandlerSimple handler)` | every caller cleans more ([24, 28, 68]) than 8 arg bytes (Cataclysm added args) |
| 0x00404050 | `aieHandlerSetInterrupt` | game-ai-ships | `void aieHandlerSetInterrupt(AITeamMove *move, udword *intvar, udword interval, bool8 oneShot, aieHandlerInt handler)` | every caller cleans more ([76]) than 20 arg bytes (Cataclysm added args) |
| 0x00404090 | `aieCheckEnemyNearby` | game-ai-ships | `bool32 aieCheckEnemyNearby(AITeam *team, SelectCommand **ships)` | every caller cleans less ([4]) than 8 arg bytes |
| 0x0041CF90 | `aiuFindLeadShipInSphereOfInfluence` | game-ai-ships | `SelectCommand *aiuFindLeadShipInSphereOfInfluence(vector centre, real32 radiussq)` | r3a uses 9 arg dwords > 4 declared |
| 0x0041EA60 | `aiuAttack` | game-ai-ships | `SelectCommand *aiuAttack(AITeam *team, SelectCommand *targets)` | r3a uses 3 arg dwords > 2 declared |
| 0x004324A0 | `cat_aisShipIsBusy` | game-ai-ships | `bool32 cat_aisShipIsBusy(Ship *ship)` | every caller cleans more ([12]) than 4 arg bytes (Cataclysm added args) |
| 0x004467C0 | `clMoveThese` | game-ai-ships | `void clMoveThese(CommandLayer *comlayer, SelectCommand *selectcom, vector from, vector to)` | every caller cleans more ([36]) than 32 arg bytes (Cataclysm added args) |
| 0x00448460 | `ChangeOrderToAttack` | game-ai-ships | `void ChangeOrderToAttack(CommandToDo *alreadycommand, AttackCommand *attack)` | every caller cleans more ([12, 24]) than 8 arg bytes (Cataclysm added args) |
| 0x00462340 | `dmgPlayEffect` | game-sim-mp | `void dmgPlayEffect(Ship *ship, etglod *effect, sdword level)` | every caller cleans more ([16]) than 12 arg bytes (Cataclysm added args) |
| 0x00464B30 | `cat_dockFlyToConeOriginLatch` | game-ai-ships | `bool32 cat_dockFlyToConeOriginLatch(Ship *ship, Ship *dockwith)` | every caller cleans more ([12]) than 8 arg bytes (Cataclysm added args) |
| 0x00481200 | `formationArrangeOptimum` | game-ai-ships | `void formationArrangeOptimum(CommandToDo *formationcommand)` | every caller cleans more ([8]) than 4 arg bytes (Cataclysm added args) |
| 0x00484310 | `formationArrageCrazyOptimum` | game-ai-ships | `void formationArrageCrazyOptimum(CommandToDo *formationcommand)` | r3a uses 19 arg dwords > 1 declared |
| 0x005C3070 | `droidCreateArray` | game-ai-ships | `void droidCreateArray(Ship *ship)` | every caller cleans more ([8]) than 4 arg bytes (Cataclysm added args) |

Five type names in kept naming-set signatures are not in the /Cataclysm header catalog: `aieHandlerSimple`, `aieHandlerShips` and `aieHandlerInt` (HW1 handler function-pointer typedefs), plus `pingeval` and `TeamType`. ImportSymbolsJson degrades an unknown by-value type to `undefined4`. These are all 4-byte types, so the argument layout survives.

The game-ai-ships audit lists 48 `signature_changes`. All 48 are already in its verified proposals.

## Prototypes (2007)

Source: `prototypes.verified.json`, 2,007 kept by its audit (77 ETG/particle function-table entries were dropped there). Every entry repeats the current r3a name; none was dropped here (0 with a different r3a name, 0 not a function). A whole-set check against the binary found 0 `__cdecl` prototypes whose function ends in `ret N`. Of the `__thiscall` entries, every callee-cleaned one pops exactly the stack arguments other than `this`. The one exception is 0x0059B1F0 `TitanInterface_HandleRoutingRegister`: it has `ret 4` and also a plain `ret`. The prototypes auditor already noted this as an SEH catch funclet, and the entry is kept.

**Does ImportSymbolsJson.py apply a signature when the name is unchanged? Yes**, with one condition: it must run in phase `post` or `all` (the default). `apply_function()` calls `set_function_name()`, which returns `same-name` and renames nothing. Then, still in phase post/all, it calls `try_signature(func, sym, base)` regardless of that result. `try_signature` parses `type` with `CParserUtils.parseSignature` against the program data types, so /Cataclysm names resolve, and the explicit `__cdecl`/`__thiscall` is kept. It applies the result with `ApplyFunctionSignatureCmd(..., SourceType.IMPORTED)`. The report CSV `detail` column shows `same-name;proto-applied`. Caveats:
- A run with `phase=pre` applies no prototypes at all.
- If the full prototype does not parse, the script falls back to a degraded one (unknown pointers become `void *`, unknown by-value types become `undefined4`). That shows as `proto-applied-degraded`, so the apply stage should count those in its report CSV.
- Each entry appends one more `[CET]` line to the function's plate comment.

## Deferred to round 4 (186)

`ghidra/proposals/r3/reconcile_deferred_functions.json` uses the round-2 deferred format plus `class`, `hw1_equivalent`, `signature` (as proposed, NOT binary-checked) and `inside_function`. By set: r3-deferred:game-render-ui 70, r3-deferred:game-sim-mp 67, r3-deferred:game-render-ui+game-sim-mp 49. None lies inside an existing r3a function body, so all are free code; the producers checked each with objdump. Most are FE/script callbacks and NIS event handlers reached only through data tables (game-sim-mp marks these "create first"). Create them with `CreateFunctions.py` (reason `r3-deferred`), then name them. Their names are reserved: no round-3 name uses them.

## Points the apply stage or round 4 should know

- `0x005A4CC0 trailGetNozzleOffset` (game-sim-mp, medium). The sim-mp auditor asked for a body check. Done: r3a takes `(float *vec, shiptrail *trail)`, reads trail->vship (+4) and trailNum (+0x1a), and copies staticinfo->engineNozzleOffset[trailNum] (+0x2b0 + n*0xc). That is HW1 Trails.c:509, plus a Cataclysm mesh-hierarchy transform. Callers are trailUpdate, trailDrawCapitalGlow and trailDraw. Kept.
- `0x005A6530 trailLineFuzzySheath`: two sets propose it independently (game-render-ui and game-sim-mp, both medium). Kept.
- `0x005A4CB0 trailDelete` (medium, two sets). It is a 12-byte `memFree(p)` wrapper and is ICF-folded: CloseStatInfo also calls it on pMexData, where HW1 calls mexFree. `trailDelete` is only one of its source names.
- `0x005A5E90` is named `trailPositions` (game-render-ui, low). game-sim-mp's `trailStraighten` for that address was dropped by its auditor.
- `0x00539F70 setMadAnim` is the binary's own identifier (its debug string), so it carries no `cat_` prefix even though HW1 does not have it.
- 182 proposal addresses carry only a class (WON/CryptoPP RTTI template slots, msvc-stl instances under TitanInterface, Magick-range targets). Nothing is applied for them.

