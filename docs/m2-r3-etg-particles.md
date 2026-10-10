<!-- done: callconv, functable, scripts, particles-runtime, deltamap -->
# M2-R3 producer 2: ETG + particle argument arrays

Status: DONE (2026-10-10). Branch `m2`, worktree `.claude/worktrees/m2`. Scope: docs/m2-r2-summary.md "Plan for
M2-R3 and M2-R4", row 2: the one-pointer callable convention, the per-race ETG scripts (`etg\race_*.script`),
`ETG.c` and `Particle.c`.

Subsystems (the done marker above lists the finished ones):

1. `callconv`: the one-pointer ETG callable convention (`etgFunctionCall` 0x0046e7b0) and every callable.
2. `functable`: the exe's 167-row `etgFunctionTable` (0x00879808) and the Cataclysm-only callables.
3. `scripts`: the per-race script list, the tables they fill, the parser deltas, the parse self-test.
4. `particles-runtime`: the 0x100-byte particle, particle system types 5/6, effect runtime deltas, the effect
   self-test.
5. `deltamap`: delta-map review entries, final builds and harness runs.

Method: the r5 decompile (`ghidra/exports/cataclysm_gog_r5/decompiled.c`) where it is readable, `objdump -d -M intel`
of `original/game/cataclysm.exe` where it is not (most of the particle code: the decompiler drops the jump-table
bodies), and table dumps read straight from the exe. Scratch helpers (untracked, `build/m2r3-etg/`): `fn.py`,
`exe_table.py`, `dis.sh`, `dwords.py`, `fconst.py`, `inventory.py`; disassembly dumps `setters.dis`, `modify.dis`,
`create.dis`, `fill.dis`, `update.dis`, `render.dis`; decompile extracts `exe_etg.c`, `exe_part.c`.

## 1. `callconv`: one argument-array pointer per callable

**The exe** (`etgFunctionCall` 0x0046e7b0, 169 bytes): fills a 10-slot array on its stack (the effect first when
`passThis` is set, then every parameter resolved by type: constant and label as given, `EVT_ConstLabel` as
`constData + param`, `EVT_VarLabel` as `variable + param`, anything else as the variable's value), calls
`function(args)` with that one pointer, stores the 32-bit result into the return variable, returns
`nParameters * 8 + 0x14`. Every callable reads its arguments through the pointer (`etgInc(udword *value)`,
`etgFmult(real32 *args)`, `partSetScale(real32 *n)`, `partModifyScale(udword *args)` ...). HW1 pushed the
values on the stack and called `function()` without a prototype (x86 only); GoK replaced that with one generated
wrapper per callable (`wrap.h`, `functions.h`, `wrapped_unlisted_functions.h`, `wrap_types.h`, a meson
`custom_target`).

| file | change |
|---|---|
| `src/Game/ETG.h` | `etgarg` (= `memsize`: a slot holds a pointer on 64-bit hosts; floats sit in its low 4 bytes, read with `MemsizeToReal32`), `etgcallable` = `memsize (*)(etgarg *)`, accessors `etgArgInt/Udword/Float/Ptr/Color`; `etgfunctioncall.function` and `opfunctionentry.function` are `etgcallable`, the GoK `wrap_function` members are gone; `etgEffectCodeStart(stat, effect, nParams, etgarg *params)` (the exe passes the user parameters as an array: 0x0046a68e reads them through its 4th argument); `etgFRandom` and `cat_univRemoveEffectReferencesFromShips` prototypes |
| `src/Game/ETG.c` | `etgFunctionCall` rewritten as the exe's (the GoK `GENERIC_ETGCALLFUNCTION` / `HW_PTR_64` / x86 variants are gone); the array holds 1 + 4 + `ETG_NumberParameters` slots (the exe's 10 would overrun on a createEffects call with more than 5 user parameters; none in the data); type 7 (`EVT_Label`) passes its value as the exe does (HW1 asserted). Every ETG-side callable takes `etgarg *args` and returns `memsize` (the exe's void ones return 0: a well-defined call through `etgcallable`): the math functions (`etgInc` 0x0046fd70 ... `etgCos` 0x0046fe90), the special-case ones (`etgParentShipDelete` 0x0046ff30 ... `etgDeathCriesPlay` 0x004701f0), the create* functions (one shared `etgCreateParticleSystem`), `etgCreateEffects` 0x004704e0 (args: effect, static, number, distribution, nParams, user parameters), `etgSpawnNewEffect` 0x00470c50 (effect, static, nParams, user parameters), `etgCreateCallbackSetup` 0x0046e390, the `etgEff*`/`etgThis*`/`etgSet*` functions (0x00471030-0x004713e0), the random and conversion functions (0x00471420-0x00471810); the parsers that build calls (`etgSpawnParse`, `etgCreateEffectsParse`, `etgCallbackOpen`, `etgSetTextureParse`, `etgSetMeshParse`) store the callable itself; `etgEffectCreate` keeps its variable arguments (callers all over the tree) and copies them into an array as the exe does (0x00470b16) |
| `src/Game/Particle.h`, `src/Game/Particle.c` | the 91 table targets and the three internal callables (`partSetTrHandle` 0x004e3b90 for setTexture, `partSetMeshdata` 0x004e3b80 for setMesh, `partModifyTexture` 0x004e29b0) take `memsize *args`; the value-argument functions the exe keeps (`partSetColor`, `partSetColorA`, `partSetDrag`, `partSetExponent`, `partSetAnimation`, `partSetMorph*`, `partSetFramerate`, `partSetCoordSys`, `partSetPosition`, `partSetSpecular`, `partSetNoDepthWrite`, `partModifyDrag`, ...) keep their values, as the exe's 10-byte bodies show (`mov eax,[esp+4]; mov [x],eax; ret`); `partSetTexFromAnimation` builds the argument array for `partModifyTexture` (partUpdateSystem 0x004dfbf9 does) |
| `src/Game/SoundEvent.h`, `src/Game/SoundEventPlay.c` | `soundEffect` (the playSound callable) is `memsize soundEffect(memsize *args)` = `soundEventPlay(args[0], args[1], NULL)` (0x005216b0) |
| `src/Ships/DefenseFighter.c`, `src/Ships/GravWellGenerator.c` | the two direct callers outside ETG.c pass arrays (`etgEffectCodeStart`, `etgFRandom`) |
| `src/Game/functions.h`, `wrap.h`, `wrap_types.h`, `wrapped_unlisted_functions.h` (deleted), `src/Game/meson.build` | GoK's generated wrappers and their `custom_target` removed |

## 2. `functable`: the exe's function table and the Cataclysm-only callables

`etgFunctionTable` is now the exe's table (0x00879808, 167 rows of 0x1C bytes: name, function, return type,
passThis, resolve, nParams + 9 type bytes; dumped with `exe_table.py`), in its order and with its argument types.
Against HW1/GoK: 17 new rows; `modifyDeltaLength` takes a third (int) argument; HW1's duplicate
`setSpriteDeltaRot`/`modifyAlphaMode` rows are gone; `carandom` keeps HW1's `nParams` 7 with 8 types (the exe's row
has the same). Two oddities kept as the exe has them: `setFramerate` points at `partSetLoopFlag` 0x004e3aa0 (as
`setLoopFlag` does; no Cataclysm effect uses setFramerate), `setEffSpin`/`setEffDeltaSpin` are typed int but read a
float.

| new row | target (exe) | port |
|---|---|---|
| `setRandomOffsetArray(6 floats)` | `cat_partSetRandomOffsetArray` 0x004e3cc0 | orders each (lo, hi) pair, draws 100 offsets `lo + rand * (hi - lo) / 2^32` from RNG stream 0 (x, y, z per entry, `ranRandomFn(0)` 0x004e3d8b) into a static array (exe 0x00a505e8) that becomes `pat.offsetArray` |
| `setTurbulance(float)`, `setSegments(int)` | `cat_partSetTurbulance` 0x004e4210, `cat_partSetSegments` 0x004e4220 | template fields `turbulance` (+0xa8, default 0.8), `segments` (+0xac, word, default 16) |
| `print(float)`, `modifyBackgroundColor(RGB)` | the exe's empty `nullsub` 0x0049ae30 | `cat_etgNullsub` |
| `modifyAlphaValue(sys, float)` | `cat_partModifyAlphaValue` 0x004e30d0 | sets every particle's alpha; [audit: the system's `PART_ALPHA` follows the alpha as HW1's `partSetSystemAlphaFlag` does: set when alpha <= 0.996 (0x0072afc0), cleared otherwise. The clear at 0x004e311e is followed by `jmp 0x004e3126`, which skips the `or 2` at 0x004e3123, so the `or 2` is not unconditional. `partModifyColorC` (0x004e3222/0x004e3225) works the same way. The port's "always PART_ALPHA" helper was removed and both now call `partSetSystemAlphaFlag`.] |
| `modifyRotAngle(sys, float)` | `cat_partModifyRotAngle` 0x004e2fd0 | `particle.rot` (+0x84) |
| `modifyScaleX/Y(sys, float)` | `cat_partModifyScaleX` 0x004e2b40, `cat_partModifyScaleY` 0x004e2bc0 | `particle.scale` (+0x50) / the new `particle.scaleY` (+0x54); `partModifyScale` 0x004e2ac0 sets both |
| `modifyTumble(sys, 3 floats)` | `partModifyTumble` 0x004e25f0 | sets the tumble of every particle of any system type (HW1 took a vector and refused all but `PART_MESH`) |
| `createEBolts(n, dist)`, `createLensFlares(n, dist)` | `cat_etgCreateEBolts` 0x00470420, `cat_etgCreateLensFlares` 0x00470480 | particle system types 5 and 6 (section 4) |
| `fdegsin(float)` | `cat_etgFdegsin` 0x0046fdb0 | `sin(x * 6.283185307 / 360)` |
| `omitParentShipFromRenderList()` | `cat_etgParentShipOmitFromRenderList` 0x00470190 | sets the owner's new header word +0x1c (below) to 1 |
| `colRed/colGreen/colBlue(RGBA)` | `cat_etgColRed/Green/Blue` 0x00471650/0x00471680/0x004716b0 | one channel as a float, byte x 1/255 |

Other deltas found in the callable bodies (all against the exe disassembly):

| function | delta |
|---|---|
| `etgFRandom` 0x00471420 | `lo + rand * (hi - lo) / 2^32` in x87 precision, then clamped to [lo, hi]; HW1 computed in float and divided by `UDWORD_Max` |
| `etgCRandom` 0x004714e0, `etgCARandom` 0x00471580 | a high bound becomes low + 1 whenever low <= high (HW1's test reversed, as GoK has it); the green high bound is never read (green uses the blue one); draws in the order blue, green, red (alpha, green, blue, red); `etgCARandom` ORs the channels unmasked |
| `etgFloats2Color(A)` 0x00471730/0x00471810 | round-to-nearest of x * 256 (0x0072a6a8 = 256.0 and the 1.5 * 2^52 rounding constant), clamped to 255 from above, low byte kept; HW1 truncated x * 255 |
| `etgSetColor(A)`, `partModifyColorC`, `partModifyAddColor`, `cat_etgColRed/Green/Blue` | colour bytes times the float 1/255 (0x0072a6ac); HW1 divided by 255.0f |
| `etgSetAnimation` 0x00471370 | passes the loop flag to `partSetLoopFlag` through a pointer (that function is a table callable) |
| `partSetDeltaLength` 0x004e4240 | also clears `pat.lengthClamp` (0x00a504b4) |
| `partSetWorldVel` 0x004e3a00 | copies the vector into `pat.worldVel` and `pat.effectVelocity`; HW1 kept a pointer (`partEffectVelocity`) that the pseudo-billboard renderers dereferenced at draw time; they now read `particle.effectVel` (0x004dd69a, 0x004dd890, 0x004ddbd0) |

New object header words (`src/Game/SpaceObj.h` `SpaceObjHeader`): the exe's objects have `staticinfo` at +0x20 (HW1
+0x18); `etgEffectCreate` clears +0x18 (0x004709ea), `omitParentShipFromRenderList` sets +0x1c, world-rendered effects
clear +0x1c (0x00470ba3). Added as `catHeader18` and `catOmitFromRenderList`; their readers (render list builder) are
not ported, so both are inert for now.

## 3. `scripts`: the per-race scripts, their tables, the parser deltas

| change | exe evidence |
|---|---|
| `etgScriptFilename` = general, RACE_SECT, RACE_BEAST, RACE_KUSHAN, RACE_TAIDAN, RACE_P1, RACE_TRADERS, RACE_DERELICT `.script` (HW1: general, R1, R2, P1, P2, P3, Traders) | list at 0x0087ac40, read by `etgStartup` 0x0046a260 |
| `etgHistoryScalarMin` key and `etgNumberEffectsParse` use `scriptSetUdwordCB`; no 1..2000 assert on NumberEffects | table row 0x0087ac00 (callback 0x00525c20), `etgNumberEffectsParse` 0x004691c0 |
| `GS_BigGun`, `GS_BeastWeapon` (`NUM_GUN_SOUND_TYPES` 20) in `ObjTypes.h`/`ObjTypes.c` | `gunsoundtypeinfo` 0x00889da0 (20 rows), `etgStartup` clears 700 = 7 x 20 x 5 gun events |
| 23 new special-purpose effects `EGT_WELDING_EFFECT` 20 ... `EGT_MOTHER_EXPLOSION_SHOCKWAVE` 42 (`EGT_NumberOfSpecialEffects` 43) and the name table in the exe's order | `StrToEffectNum` table 0x0088a048 (43 rows; `EGT_LINKING_EFFECT` listed twice), `etgStartup` clears 0x2B entries |
| `ETG_NumberMeshes` 150 | `etgStartup` clears 300 dwords of the mesh registry |
| effect, variable, label and keyword lookups are case-insensitive: `etgEffectStaticFind`, `etgVariableFind` (every inlined variable lookup), `etgTokTheType` keywords, the `NULL` tests of the *EventSet callbacks, `etgDamageEventSet` keywords, `etgSetTextureParse`'s "setTexture" | `_stricmp` (0x00676c90) in `etgEffectStaticFind` 0x0046b090, `etgVarCopyScan`, `etgFunctionCallScan`, `etgSpawnParse`, `etgCreateEffectsParse`, `etgTimeIndexDefine`, `etgTimeBlockOpen`, `etgLabelGoto/Call`, `etgEffectCodeLoad`, `etgDeathEventSet`, `etgDamageEventSet`, the event setters; HW1 used `strcmp` |
| effect names and mesh registry names are `memStringDupe`d (HW1 `memStringDupeNV`) | `etgEffectStaticFind`, `etgSetMeshParse` 0x0046cdf0 |
| `etgLoadErrorf` is always fatal | 0x0046ae90 calls `dbgFatal` directly (HW1 went through the recoverable `etgLoadError`) |
| `etgEvery`/`etgEveryVar`: a zero period runs the block instead of dividing by zero | 0x0046efd6, 0x0046f043 (review entries existed) |
| no range checks on `at`/`before`/`after`/`every` variables, no upper bound on `time`/`at`/`between` constants | the exe has none (`etgAtVar` 0x0046eed0, `etgBeforeVar` 0x0046ef40, `etgAfterVar` 0x0046ef90, `etgEveryVar`); Cataclysm data needs it: `slipgateOpening.ebg` uses `time(86400, 86400)`, effects set an `at` variable to 1e8 for "never". HW1's debug build (`ETG_ERROR_CHECKING`) rejected both; the other debug checks stay |

**Parse self-test** (`/catEtgParseTest <listfile>`, `tools/m2_harness.sh --mode etg`, not in the default start-up
path): the harness lists every `etg\*.script` and `etg\*.ebg` of the `cataclysm.big` TOC (264 files; `update.big` has
none). The engine starts the effects module under the loading screen as `gameStart` does (`horseRaceInit`,
`etgStartup`: the exe's 8 scripts and every effect they load), then parses the scripts the exe does not load
(`derelict.script`), then reports every `.ebg` with its compiled size; quitting runs `etgShutdown`.

Run `r3-etg-parse4` (plain harness, debug ASan build): rc 0, milestone `etg parse self-test done`, ASan 0, no
unsuppressed leak. **9 of 9 scripts parse**; `etgStartup` loads 197 effects with 0 names referenced (spawn,
createEffects) but not loaded; 58 of the 255 `.ebg` files are loaded by no script (no data file names them;
`bulletbiggunglow.ebg` does not even compile: it uses the undefined label `bgglow_m`; the exe never reads them).
Earlier runs found the deltas above: `r3-etg-parse1` (loading-screen crash: the self-test now starts the horse race),
`r3-etg-parse2` (`time(86400...)`), `r3-etg-parse3` (the uncompilable unused file).

## 4. `particles-runtime`: the 0x100-byte particle, system types 5/6, effect runtime, the effect self-test

**Particle layout** (`src/Game/Particle.h`, static asserts under `__i386__`, checked by `tools/m2_layout32.sh`:
`particle` 256, `eboltSystem` 36, OK). The exe's particle stride is 0x100 (HW1 0xE0 [audit: was "0xC8"; HEAD's `particle` is 224 bytes under clang -m32, as the Particle.h comment says]): `partFillGenericParticles`,
`partUpdateSystem` 0x004df9c0 and the renderers step by 0x100. New fields: `scaleY` +0x54, `lengthClamp` +0x70,
`turbulance` +0x74, `segments` +0x78 (word), `lensFlare` +0x7c (the particle's lens flare source), `tex` +0x80,
`effectVel` +0x98, `wAccel` +0xa4, `bias` +0xb0, `tstruct` +0xbc, `mesh` +0xe4, `exponent` +0xf4, `flags` +0xfc.

**Template** (`Particle.c` `pat`, exe 0x00a50410, 0x1C8 bytes; defaults copied from 0x0088d300, review entry
0x004dc3a0): `effectVelocity` after `worldVel`; `lengthClamp`, `turbulance` (0.8), `segments` (16) after
`deltaLengthDist`. `partFillGenericParticles` sets `scaleY = scale`, copies `lengthClamp`/`turbulance`/`segments`,
and sets `effectVel` from `pat.effectVelocity` for world-space systems (zero otherwise: HWI-FIX, so the field is never
left uninitialised).

| change | exe |
|---|---|
| `PART_EBOLTS` = 5: header `eboltSystem` 0x24 bytes (point header + turbulance, segments); `partCreationHelper`, `partHeaderSize` | `partCreationHelper` 0x004e2150, `cat_etgCreateEBolts` 0x00470420 |
| `cat_partRenderEBoltSystem`: each particle is a `GL_LINE_STRIP` of `segments` steps; interior points jittered from RNG stream 0 with amplitude `abs(turbulance * step) * 0.5`; NaN tests on x, y, z | `cat_partRenderUnk1System` 0x004de880 |
| `PART_LENSFLARES` = 6: point header; each particle owns a `catlensflaresource` (0x40 bytes: position, brightness, enabled/state/visible bytes, visibility +0x18, list link +0x2c), allocated in `partFillGenericParticles`, added with `cat_rndLensFlareSourceAdd`, freed by the new `cat_partSystemFreeAttachments` (called by `etgEffectDelete` for every particle block) | `cat_etgCreateLensFlares` 0x00470480, `cat_rndLensFlareSourceAdd` 0x0057baa0, `cat_rndLensFlareSourceRemove` 0x0057ba90, list 0x00b37880, `cat_partSystemFreeAttachments` 0x004e2500 |
| `src/SDL/render.h`, `render.c`: `catRndLensFlareSourceList`, `cat_rndLensFlareSourceAdd` (link, visibility 0, state 2), `cat_rndLensFlareSourceRemove` (unlink) | 0x0057baa0, 0x0057ba90 |
| `partRenderSystem`: type 5 draws the bolts; type 6 draws nothing itself (the lens flare renderer draws the sources) and marks the first source state 0, hits 1 | `partRenderSystem` jump table |
| `partUpdateSystem`: a clamped length stops at 0; the lens flare source follows the particle position; `scaleY` grows with `deltaScale` and clamps at 0 | 0x004df9c0 |
| `partRenderBillSystem`: separate half sizes `scale * 0.5`, `scaleY * 0.5` | 0x004dc6f0 |
| pseudo-billboard renderers subtract `particle.effectVel` (HW1 global `partEffectVelocity` removed) | 0x004dd650, 0x004dd840, 0x004ddad0 |
| `partModifyDeltaLength` takes a third argument (`lengthClamp`); `partSetDeltaLength` clears it | table row (3 args), 0x004e4240 |

**Effect runtime** (`ETG.c`): `etgEffectCreate` clears header +0x18 (0x004709ea) and, for world-rendered effects,
+0x1c (0x00470ba3); `etgEffectDelete` calls the new `cat_univRemoveEffectReferencesFromShips` (walks `ShipList` and
`ShipsInHyperspace` and calls each ship's `CustShipHeader` slot 13, `CustShipEffectRemoval`) and
`cat_partSystemFreeAttachments` on every particle block, then nulls the freed pointers. Not ported (owner below): the
effect's +0x80 mesh state (`cat_univShipMeshStateCreate`) and +0x84 one-byte block.

**Sound table fix** (`src/Game/SoundEventDefs.h`): the effect self-test hit a global-buffer-overflow in `effectHandle`
on hit event 7; the exe's table has 8 hit events (0x00c0a120-0x00c0a180), so `Hit_Cat7` (7 + `Hit_Flag`) and
`NUM_HIT_EVENTS` 8.

**Effect self-test** (`/catEtgEffectTest <ticks>` after the parse test; `tools/m2_harness.sh --mode etg --etg-ticks N`,
default 128; not in the default start-up path). With a temporary camera (`cameraInit`, `mrCamera`) it creates every
LOD entry of every effect table the scripts fill (618 effects at the origin, each with its declared `eventStart(...)`
defaults read from its `.ebg`: int 1, float 10.0, colour white), then for N ticks runs `etgEffectUpdate` on every live
effect and the update half of `etgEffectDraw` on every particle block (spawned and child effects included), tallies
particle-system ticks by type, then deletes what is left and checks that the lens flare list is empty.

Harness changes (`tools/m2_harness.sh`): mode `etg` (lists the 264 `etg\*` files of the `cataclysm.big` TOC),
milestones `etg parse self-test done` / `etg effect self-test done`, an `etg:` summary line. The ASan count and first
stop also read `ubsan.*` logs (the overflow above landed there), and leak summaries no longer count as a stop.

| run | ticks | created | deleted while running | left at end | peak effects | peak particle blocks | ticks by type (points/lines/meshes/billboards/lensflares/ebolts) | ASan | leak |
|---|---|---|---|---|---|---|---|---|---|
| `r3-etg-final-etg128` (debug ASan) | 128 | 618 | 3197 | 1093 | 2515 | 3012 | 998 / 6056 / 71122 / 171657 / 144 / 0 | 0 | none |
| `r3-etg-final-etg1024` (debug ASan) | 1024 | 618 | 4177 | 133 | 2515 | 3012 | 65174 / 329400 / 236810 / 420560 / 176 / 0 | 0 | none |
| `r3-etg-final-etg512-rel` (release) | 512 | 618 | 4165 | 133 | 2515 | 3012 | 16214 / 83192 / 135978 / 321744 / 176 / 0 | n/a | n/a |

All three runs end with 0 lens flare sources on the list. Ebolt ticks are 0 because no Cataclysm `.ebg` or `.script`
calls `createEBolts` (grep of `original/game_extracted/cataclysm/etg`). `createLensFlares` is used by
`biggunbulletexplodecenter`, `biggunmuzzleflash`, `mothershockwaveexplodecenter`, `slipgateopening` and
`swslipgateopening`. The effects left at the end are long-lived ones; the cleanup deletes them and no leak is reported.
Earlier runs found: `mrCamera` NULL in `getPanAngle` (fixed with the temporary camera), the hit-event overflow (above),
a particle count read from float bits (`weldingSparks`, fixed by the declared defaults), and the `at` variable 1e8
check (section 3).

## 5. `deltamap`: review entries, stage counts

33 review entries (`build/m2r3-etg/review_etg.txt`, sample `port-R3`) were applied with `tools/delta_map.py mark`,
then `merge --done m2r3:etg-particles-review`, into `ghidra/delta_map_review.json` and `ghidra/delta_map.json`:

- 24 changed from IDENTICAL to MODIFIED/high after checking the disassembly: `etgFunctionCall` 0x0046e7b0,
  `etgEffectCodeStart` 0x0046a5b0, the `_stricmp` users 0x004691e0, 0x004694a0, 0x00469610, 0x00469780, 0x004698f0,
  0x00469d90, 0x00469f50, 0x0046b440, 0x0046e150, 0x0046e500, 0x0046daf0, 0x0046d9f0, 0x0046d440, 0x0046d4d0,
  0x0046bb00; `partCreationHelper` 0x004e2150, `partUpdateSystem` 0x004df9c0, `partRenderBillSystem` 0x004dc6f0, the
  three pseudo-billboard renderers 0x004dd650/0x004dd840/0x004ddad0, `partSetTrHandle` 0x004e3b90.
- 2 IDENTICAL/high header-level entries: 0x004dc3a0 (the 0x1C8-byte template) and 0x005209b0 (8 hit events).
- 7 MODIFIED entries confirmed (confidence raised, change described): `etgCRandom` 0x004714e0, `etgCARandom`
  0x00471580, `etgFRandom` 0x00471420, `etgFloats2Color` 0x00471730, `etgFloats2ColorA` 0x00471810,
  `partModifyTumble` 0x004e25f0 (was REWRITTEN), and 0x004e3170.
- [audit: the audit corrected 0x004e3170's change text, since the flag is not always set. It also added `partSetOffsetArray`
  0x004e3cb0 as MODIFIED/high. This port edits that function for the argument array, but the map still called it
  IDENTICAL/medium with no review. Both were added with `tools/delta_map.py mark`, then `merge --done m2r3:etg-particles-audit`.]

`tools/m2_stage_counts.py`, stage 2 (universe+ships), before and after
(`original/scratch-m2/m2r3-etg/stage_counts_{before,after}.txt`):

| class | before | after |
|---|---|---|
| IDENTICAL | 855 | 831 (830 after the audit) |
| MODIFIED | 517 | 542 (543 after the audit) |
| REWRITTEN | 30 | 29 |

Totals over all stages:

| class | before | after |
|---|---|---|
| IDENTICAL | 3161 | 3137 (3136 after the audit) |
| MODIFIED | 1182 | 1207 (1208 after the audit) |
| REWRITTEN | 77 | 76 |

## Builds

`tools/build.sh --debug` and `--release`, clean builds:

- Both rc 0, [292/292]. The count dropped because the generated-wrapper target is gone.
- 0 warnings and 0 errors under `-Werror` (`grep -c warning:` of both logs gives 0).
- Debug `hwi-engine` is 76,550,184 B. Release is 4,795,152 B, sha256 `393530f8d06d3ea8...`.
- [audit: after the PART_ALPHA fix, `tools/build.sh` gives debug 76,549,512 B and release 4,795,152 B, sha256
  `0a01d54659edb6a1...`, with 0 warnings.]
- `tools/m2_layout32.sh`: OK (`particle` 256, `eboltSystem` 36).

## Plain-harness runs (`tools/m2_harness.sh`, debug ASan build unless noted)

| run | milestone | first stop | ASan | leak at exit |
|---|---|---|---|---|
| `r3-etg-final-fe` | Main_game_screen | none: still running at the 40 s timeout | 0 | none |
| `r3-etg-final-sp` | level load (mission file read) | `GenericInterceptor.c (205)`: `interceptorstat != NULL` | 0 | 44 B in 11 allocations (as baseline) |
| `r3-etg-final-statics` | ship statics self-test done | none (rc 0); 143 of 145 ship types | 0 | none |
| `r3-etg-final-meshes` | mesh self-test done | none (rc 0); 1758 files, 0 bad | 0 | none |
| `r3-etg-final-textures` | texture self-test done | none (rc 0); 12403/12403 found, 10661 loaded | 0 | none |
| `r3-etg-final-oracle` | oracle self-test done | none (rc 0); 512 ticks, 0 sync errors | 0 | none |
| `r3-etg-final-etgparse` | etg parse self-test done | none (rc 0) | 0 | none |
| `r3-etg-final-etg128` / `-etg1024` | etg effect self-test done | none (rc 0) | 0 | none |
| `r3-etg-final-etg512-rel` (release) | etg effect self-test done | none (rc 0) | n/a | n/a |

- `tools/oracle/selftest.sh --label r3-etg-oracle`: SELFTEST PASS.
- Baseline for fe/sp is `audit-r3-closeout-final-fe/sp` (the closeout-headers state). It has the same milestones, the
  same sp stop and the same 44-byte sp leak.
- The first non-benign UBSan report of the etg runs is the misaligned `etgconditional` access at ETG.c 4271. It
  comes from GoK's 64-bit packing of the compiled effect code: pre-existing and non-fatal.

## DONE WHEN

| condition | result |
|---|---|
| every `etg\*.script` of the install parses through a self-test mode outside the default start-up path | MET: 9 of 9 scripts (`r3-etg-final-etgparse`), `/catEtgParseTest` only |
| ASan 0 in an effect self-test that creates and runs a sample of effects for a number of ticks | MET: all 618 table effects, 128 and 1024 ticks, ASan 0, no leak |
| fe and sp harness runs unchanged or further | MET: unchanged (Main_game_screen; level load with the same GenericInterceptor stop) |

## Files changed

- `src/Game/`: `ETG.c`, `ETG.h`, `Particle.c`, `Particle.h`, `SpaceObj.h`, `ObjTypes.c`, `ObjTypes.h`,
  `SoundEvent.h`, `SoundEventDefs.h`, `SoundEventPlay.c`, `meson.build`.
- Deleted from `src/Game/`: `functions.h`, `wrap.h`, `wrap_types.h`, `wrapped_unlisted_functions.h`.
- `src/SDL/`: `main.c` (`/catEtgParseTest`, `/catEtgEffectTest`), `utility.c` (the self-test hook), `render.c`,
  `render.h`.
- `src/Ships/`: `DefenseFighter.c`, `GravWellGenerator.c`.
- `tools/`: `m2_harness.sh`, `m2_layout32.sh`.
- `ghidra/`: `delta_map.json`, `delta_map_review.json`.
- This report.

Every Cataclysm change carries a `CAT:` comment with its exe function and address. No new source files were added.
None of the HW1 alias or dormant names appear in the ported files. Nothing was committed.

## What is left, and who owns it

| item | owner |
|---|---|
| billboard renderer deltas: `rndBillboardEnable/Disable`, `glPixelTransferf` colour bias when capability 0x9004 exists, per-particle fog, one `glColor` per particle (0x004dc6f0) | stage 3 renderer |
| fog save/restore around `partRenderSystem` (0x00b3a6d8, `GL_FOG`) | stage 3 renderer |
| mesh, line, point and cube renderers and `partMeshMaterialPrepare` (MODIFIED) not yet compared with the exe | stage 3 renderer |
| effect +0x80 mesh state (`cat_univShipMeshStateCreate`) and the +0x84 block | M2-R4 #1 |
| lens flare renderer `cat_rndLensFlaresRender` (the sources are kept on the list now) | stage 3 renderer |
| the code that reads `catHeader18`/`catOmitFromRenderList` (render list builder) | render list port |
| sound tables: explosion events up to 15 (bounded by `RangeLUT`) | stage 3 sound |
| the slot 13 bodies (`CustShipEffectRemoval`) of each ship's `CustShipHeader` | ship code ports |
| `etgFunctionCallScan` debug strings; the software-renderer effect substitution (GoK has no software renderer) | not planned |
| misaligned `etgconditional`/`etgdecision`/`etgfunctioncall`/`etgbranch` UBSan reports; `etgEffectCreate`'s `va_arg(memsize)` read of int arguments | pre-existing GoK 64-bit artefacts; a later 64-bit cleanup |
| the bolt renderer is ported from the disassembly, but no effect in the data calls `createEBolts`, so nothing tests it | oracle comparison once a reference recording exists |
