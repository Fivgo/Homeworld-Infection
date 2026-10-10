# Audit: M2-R3 port "etg-particles" (ETG + particle argument arrays)

Auditor: adversarial audit, 2026-10-10.

- **Subject:** producer report `docs/m2-r3-etg-particles.md` and the uncommitted diff in the `m2` worktree (25 files).
- **Not run:** Ghidra and the GOG game. No input was injected.
- **Evidence:** everything below comes from:
  - the r5 export (`ghidra/exports/cataclysm_gog_r5/decompiled.c`, `functions.csv`);
  - `objdump -d -M intel` of `original/game/cataclysm.exe`;
  - table and constant dumps read straight from the exe;
  - the plain harness.
- **Audit helpers:** untracked, in `build/audit-etg/`:
  - `pe.py`: PE reader, table and dword dumps;
  - `cmp_table.py`: source table vs the exe table;
  - `fn.sh`: r5 decompile by name;
  - `edited_fns.py`, `ident_changed.py`: edited functions vs their delta-map class;
  - `review_audit.txt`.

## 1. Clean builds (separate dir, as `tools/build.sh` does)

**Setup.** `original/scratch-m2/build-audit-etg-particles/` was removed and recreated. Then:

- `meson setup --buildtype=debug -Db_sanitize=address,undefined debug`, then `meson compile`;
- `meson setup -Doptimization=2 -Ddebug=false -Db_sanitize=none release`, then `meson compile`.

`werror=true` comes from the project defaults (`meson.build`). `-Werror` appears on 249 ninja rules.

| build | rc | warnings | size |
|---|---|---|---|
| debug, before the audit fix | 0 | 0 | 76,560,072 B |
| release, before the audit fix | 0 | 0 | 4,795,152 B |
| debug, after the fix (incremental) | 0 | 0 | 76,549,512 B (`tools/build.sh --debug` in the worktree) |
| release, after the fix (incremental) | 0 | 0 | 4,795,152 B, sha256 `0a01d54659edb6a1...` (`tools/build.sh --release` in the worktree) |

- **Hashes and sizes against the producer.** The release sha256 before the fix (`0d3702e8...`) differs from the
  producer's `393530f8...` only because the build directory differs: the embedded `__FILE__` paths change. The
  worktree's own build had the producer's hash and size.
- **Layout.** `tools/m2_layout32.sh` reports OK: `particle` 256, `eboltSystem` 36.

## 2. Callable convention and function bodies against the exe

**ETG function table.** `pe.py etgtable` dumps the exe's `etgFunctionTable` at 0x00879808 (rows of 0x1C bytes up to
the NULL name), and `cmp_table.py` compares it with the source table.

- Result: **167 of 167 rows match**: name, order, return type, passThis, resolve callback, nParams, the 9 type bytes,
  and the target function's r5 name.
- The only name differences are `print` and `modifyBackgroundColor`. Both point at the exe's `nullsub` (0x0049ae30),
  which the port calls `cat_etgNullsub`.
- The oddities the producer reports hold:
  - `setFramerate` points at `partSetLoopFlag`;
  - `modifyDeltaLength` takes 3 arguments;
  - `carandom` has nParams 7.

**`etgFunctionCall` 0x0046e7b0 (decompile).**

- The 10-slot array is on the stack, with the effect first when `passThis` (+0x10) is set.
- Parameter types are handled as follows:

  | type | what the slot gets |
  |---|---|
  | 6, 7 | the value as given |
  | 8 | `+ stat->constData` (stat+0x24) |
  | 9 | `+ effect->variable` (effect+0x120) |
  | any other | the variable's value |

- The call is `function(array)`.
- The 32-bit result goes to the return variable.
- The function returns `n*8+0x14`.

The port's version matches. Its array is longer (1 + 12 slots), and `etgSpawnParse` and `etgCreateEffectsParse` place
nParams so that `etgSpawnNewEffect` reads `args+3` and `etgCreateEffects` reads `args+5`. Both are correct.

**Argument-array layout of 25 callables (all correct).** For each one, the order of the argument slots and how each
slot is read were checked against the decompile, the disassembly, or both:

- **ETG math:**
  - `etgInc` 0x0046fd70;
  - `etgFmult` 0x0046fd90;
  - `cat_etgFdegsin` 0x0046fdb0: the constants are the doubles 6.283185307 and 1/360 at 0x0072a830/38.
- **Particle modify:**
  - `partModifyScale` 0x004e2ac0: writes +0x50 and +0x54;
  - `cat_partModifyScaleY` 0x004e2bc0;
  - `partModifyDeltaLength` 0x004e2cc0: deltaLength +0x6c, arg 2 goes to +0x70;
  - `partModifyTumble` 0x004e25f0: any system type, +0xcc;
  - `partModifyTexture` 0x004e29b0;
  - `cat_partModifyAlphaValue` 0x004e30d0;
  - `partModifyColorC` 0x004e3170.
- **Particle set:**
  - `partSetScale` 0x004e3c20;
  - `partSetDeltaLength` 0x004e4240: clears 0x00a504b4, which is `pat.lengthClamp`;
  - `partSetColorScheme` 0x004e4310: a char store;
  - `partSetOffsetArray` 0x004e3cb0.
- **Random and colour:**
  - `etgCRandom` 0x004714e0;
  - `etgCARandom` 0x00471580: the exe draws blue, green, red (C) and alpha, green, blue, red (CA). Green uses hiB,
    and a high bound becomes low+1 when low <= high;
  - `etgFRandom` 0x00471420: stream 0x1a = `RANDOM_ETG` 26;
  - `etgFloats2Color` 0x00471730: the constants are 256.0 and 1.5*2^52;
  - `cat_etgColGreen` 0x00471680: 0x0072a6ac = 1/255f.
- **Effect and other:**
  - `cat_etgParentShipOmitFromRenderList` 0x00470190;
  - `etgSetAnimation` 0x00471370: passes a pointer to the loop flag;
  - `soundEffect` 0x005216b0;
  - `etgCreateCallbackSetup` 0x0046e390;
  - `etgChatterEventPlay` 0x004701b0;
  - `etgEffOffsetDrag` 0x004710e0.

**Ten ETG and particle bodies against the exe:**

| function | address | result |
|---|---|---|
| `etgFunctionCall` | 0x0046e7b0 | ok |
| `etgEffectDelete` | 0x0046a6f0 | ok. Order: ship references, attachments, free, null. The +0x84 block is deferred, as documented |
| `cat_univRemoveEffectReferencesFromShips` | 0x005475e0 | ok. Walks `ShipList`, then `ShipsInHyperspace` (`InsideShip`+0x10). Calls `staticinfo`(+0x20)+0x13C |
| `cat_partSystemFreeAttachments` | 0x004e2500 | ok |
| `etgLoadErrorf` | 0x0046ae90 | ok. Calls `dbgFatal` |
| `etgNumberEffectsParse` | 0x004691c0 | ok. `scriptSetUdwordCB`, no assert |
| `partSetWorldVel` | 0x004e3a00 | ok. Copies into +0x00 and +0x0c |
| `partUpdateSystem` | 0x004dffaa-0x004e000b | ok. The lens flare source copies the position (+0x24). scale and scaleY are both inside the deltaScale != 0 test |
| `partRenderSystem` cases 5/6 | 0x004df3b2 / 0x004df3c5 | ok. The bolt renderer is called. For lens flares, `[p+0x7c]+0x11 = 0` and hits = 1 |
| template defaults | `partSetDefaults` data 0x0088d300, 0x72 dwords | ok. Every value matches the field at its ILP32 offset in the 0x1C8-byte `particleAttribs`, including turbulance 0.8 at +0xa8, segments 16 at +0xac, additiveBlends 1 at +0x1bc, loopCount and tex -1 |

**The `SpaceObjHeader` growth is supported by the exe.** Both `etgEffectDelete` and 0x005475e0 read `staticinfo` at
+0x20.

**Effect self-test under ASan (debug).** All runs used the audit build after the fix; full results are in section 4.

**Parse coverage.**

- The harness list is generated from the `cataclysm.big` TOC:
  - 264 files: 9 `.script` and 255 `.ebg`;
  - an unrestricted grep of the TOC finds the same 264 `etg*.script|ebg` entries;
  - `update.big` has 0 `etg` entries.
- In the run (`audit-r3-etg-parse`):
  - `FileLoads.log` shows each of the 9 etg scripts loaded once (`general`, `RACE_SECT`, `RACE_BEAST`, `RACE_KUSHAN`,
    `RACE_TAIDAN`, `RACE_P1`, `RACE_TRADERS`, `RACE_DERELICT`, `derelict`);
  - `run.log` has 8 "startup script" lines and 1 "extra script" line;
  - 197 "ok" `.ebg` lines match 197 distinct `.ebg` loads;
  - 58 "unused" lines account for the rest of the 255.

## 3. The diff

**Scope of the read.** The full `git diff` was read: ETG.c (2,716 diff lines), Particle.c (1,696), Particle.h, ETG.h,
the 4 deleted GoK wrapper files and every small file.

**Edited functions against the delta map.** `ident_changed.py` compares each delta-map function's body at HEAD and in
the working tree. 192 functions changed; 10 of them are still IDENTICAL:

- **Edits to debug-only code (`#if ETG_ERROR_CHECKING`).** These are compiled out of the retail build the map
  compares against, so IDENTICAL stands:
  - `etgBetweenDefine`, `etgAtDefine`: the `time` upper bounds;
  - `etgAtVar`, `etgBeforeVar`, `etgAfterVar`: the variable range checks.
- **Callables with no arguments, or that ignore them.** The behaviour does not change: `etgEffDefaults`,
  `etgSetDefaults`, `etgThisOffsetXYZ`.
- **`etgCallbackOpen`:** stores the function pointer without the GoK wrapper; the behaviour is the exe's.
- **`partSetOffsetArray` 0x004e3cb0:** edited for the argument array (the exe dereferences the slot), but still
  IDENTICAL/medium with no review. Every sibling `partSet*` was marked MODIFIED/high in reread-R2. This is **finding
  F3**.

**CAT comments.** Every Cataclysm change in the diff carries a `CAT:` comment with an exe address, with one exception:
`partModifyTexture` now writes `((meshSystem *)psys)->tex` for mesh systems where HW1 wrote `m->tex` through a
`billSystem *`. That change has no comment, but it is not a Cataclysm delta: both fields are at +0x1c, and the exe
writes +0x1c (`piVar1[7]`). It is a cosmetic edit, so it is noted here and not counted.

**HW1 aliases.** No HW1 alias or dormant id (the 59 names of the `ShipDefs.h` alias block) appears in ETG.c,
Particle.c, ETG.h or Particle.h. The only `R1` is in a comment.

**Changes outside the ETG.c/Particle.c scope (noted; all justified, none reverted):**

- `SpaceObj.h`: `SpaceObjHeader` gains two dwords, so every object grows by 8 bytes. The exe evidence is above, and
  the oracle self-test still passes.
- `SoundEventDefs.h`: `NUM_HIT_EVENTS` is 8. This came from an ASan global-buffer-overflow found by the producer's
  effect test.
- `render.c`/`render.h`: the lens flare source list.
- `ObjTypes.c`/`ObjTypes.h`: the gun sound and special-effect tables.
- The two ship files: callers of the changed ETG functions.
- `main.c`/`utility.c`: the self-test hooks. They run only when `/catEtgParseTest` is given.
- `m2_harness.sh`: an `etg` mode. The stop and ASan detection now also read the `ubsan.*` logs.

## 4. DONE WHEN, re-checked on the plain harness (audit build after the fix)

| condition | run | result |
|---|---|---|
| every `etg\*.script` parses, through a self-test outside the default start-up path | `audit-r3-etg-parse` (debug ASan) | met: 9 of 9 scripts, rc 0, ASan 0, no leak. The hook runs only with `/catEtgParseTest` |
| ASan 0 in an effect self-test | `audit-r3-etg-effect128`, `-effect1024` (debug ASan), `-effect512-rel` (release) | met, and reproduces the producer's numbers exactly (see the notes below) |
| fe and sp unchanged or further | `audit-r3-etg-fe`, `audit-r3-etg-sp` | met: unchanged. fe reaches `Main_game_screen` and is still running at the 40 s timeout. sp reaches level load and stops at the same `GenericInterceptor.c (205)` assert, with the same 44-byte leak |

Notes on the effect self-test runs:

- All three create 618 effects and end with 0 lens flare sources on the list.
- The 128-tick run deletes 3197 effects and leaves 1093. Ticks by type: points 998, lines 6056, meshes 71122,
  billboards 171657, lens flares 144, ebolts 0.
- The 1024-tick run deletes 4177 effects and leaves 133.
- Peak counts are 2515 effects and 3012 particle blocks.
- ASan reports 0 errors and LeakSanitizer finds no leak.

The other self-tests are unchanged:

| run | result |
|---|---|
| `audit-r3-etg-statics` | 143 of 145 ship types |
| `audit-r3-etg-meshes` | 1758 files, 0 bad |
| `audit-r3-etg-textures` | 12403 of 12403 found, 10661 loaded |
| `audit-r3-etg-oracle` | 512 ticks, 0 sync errors |
| `tools/oracle/selftest.sh --label audit-r3-etg-oracle-st` (audit release build) | SELFTEST PASS |

All of these have ASan 0. All run dirs are under `original/scratch-m2/m2r2-harness/runs/`.

## 5. Findings

**F1 (code, fixed): `PART_ALPHA` is not "always set".**

- **The claim.** The port added `catPartSetSystemAlphaFlagAlways` and called it from `cat_partModifyAlphaValue` and
  `partModifyColorC`. The report (section 2) and the delta-map entry for 0x004e3170 say the exe ends the alpha test with
  an unconditional `or 2`.
- **The disassembly says otherwise.** In `cat_partModifyAlphaValue`:

  ```
  4e310e  fcomp dword [0x72afc0]   ; 0.996
  4e3116  test  ah,0x41
  4e311c  jne   0x4e3123
  4e311e  and   eax,0xfffffffd
  4e3121  jmp   0x4e3126            ; skips the or
  4e3123  or    eax,0x2
  ```

  `partModifyColorC` has the same pattern at 0x004e3212-0x004e3227, with the `jmp` at 0x004e3225.
- **What the exe really does.** The flag is set when alpha <= 0.996 and cleared otherwise. That is exactly HW1's
  `partSetSystemAlphaFlag` (`a > 0.996f` clears it). The decompile shows the same if/else.
- **Effect.** Every `modifyColor`/`modifyColorA`/`modifyAlphaValue` call with an opaque colour would have drawn the
  system alpha-blended.
- **Fix.** The helper is removed, and both functions call `partSetSystemAlphaFlag(psys, a)`.

**F2 (code comment, fixed): wrong template default addresses.** `Particle.c` gave the turbulance and segments defaults
as 0x0088d3b8 and 0x0088d3bc. They are at 0x0088d3a8 and 0x0088d3ac (0x0088d300 + 0xa8/0xac). The values at
0x0088d3b8/bc are 1.0 (icolor).

**F3 (delta map, fixed): `partSetOffsetArray` left IDENTICAL.** 0x004e3cb0 was edited for the argument array but left
IDENTICAL/medium with no review. It is now marked MODIFIED/high.

**F4 (delta map and doc, fixed): wrong change text for 0x004e3170.** The delta-map change text and the report's
`modifyAlphaValue` row repeated F1's wrong claim. Both are corrected.

**F5 (doc, fixed): wrong HW1 particle size.** The report gave the HW1 particle stride as "0xC8". HEAD's `particle`
measures 224 = 0xE0 under the `m2_layout32.sh` compiler settings, which matches the `Particle.h` comment (0x100 minus
the 0x20 bytes of new fields).

**F6 (tool comment, fixed): split sentence in `m2_layout32.sh`.** The etg-particles header lines were inserted inside
the closeout-headers sentence, which split its "(docs/m2-r3-closeout-headers.md)" reference.

**Notes (not counted as wrong; no change made):**

- **N1, `etgFRandom` precision.** The exe computes `hi - lo` with an x87 `fsub` (0x00471463). The port computes it in
  float and then widens it. The last bit can differ when the FPU precision control is wider than 24 bits. The
  precision control the exe runs under is not established, so this is left open.
- **N2, the electric-bolt renderer** (`cat_partRenderEBoltSystem`) was not compared with 0x004de880. No data calls
  `createEBolts`, so it is untested, as the producer says.
- **N3, pre-existing UBSan reports.** The misaligned-access reports (`etgconditional`, `etgdecision`,
  `etgfunctioncall`, `particle` after the 0x1c/0x24-byte headers on LP64) are GoK 64-bit artefacts and are not fatal.

## 6. Precision

**Sampled checks: 48.** Each item below counts as one check.

- The 167-row table compare: 1.
- The 25 callable bodies: each counted once. The flag claim of `cat_partModifyAlphaValue` and `partModifyColorC`
  counts as 2.
- The 10 non-callable bodies and data, from `etgFunctionCall` to the template, counted as listed in section 2.
- The template default addresses: 1.
- The HW1 particle size: 1.
- The parse coverage (9/9, 197/58): 1.
- The effect self-test numbers: 1.
- fe/sp unchanged: 1.
- The other self-tests: 1.
- No aliases: 1.
- The build with 0 warnings: 1.
- `m2_layout32` OK, including its comment: 1.
- The delta-map coverage of edited functions: 1.
- The spawn/createEffects argument offsets: 1.
- The `SpaceObjHeader` +0x20 evidence: 1.

**Wrong: 6.**

- the `PART_ALPHA` claim for `cat_partModifyAlphaValue`;
- the `PART_ALPHA` claim for `partModifyColorC`;
- the template default addresses;
- the HW1 particle size;
- the `partSetOffsetArray` delta-map entry;
- the `m2_layout32.sh` comment placement.

**Precision = 42 / 48 = 0.875.**

## 7. Fixes made by the audit (8)

1. `src/Game/Particle.c`, `cat_partModifyAlphaValue`: calls `partSetSystemAlphaFlag` (F1). The
   `catPartSetSystemAlphaFlagAlways` helper is removed.
2. `src/Game/Particle.c`, `partModifyColorC`: calls `partSetSystemAlphaFlag` (F1). The comments are marked
   `[audit: ...]`.
3. `src/Game/Particle.c`: the turbulance and segments default addresses are now 0x0088d3a8 and 0x0088d3ac (F2).
4. `tools/m2_layout32.sh`: the header comment is reordered (F6).
5. `ghidra/delta_map_review.json` and `ghidra/delta_map.json`: the 0x004e3170 change text (F4).
6. `ghidra/delta_map_review.json` and `ghidra/delta_map.json`: the new 0x004e3cb0 MODIFIED/high entry (F3). Entries 5
   and 6 were made with `tools/delta_map.py mark` and `merge --done m2r3:etg-particles-audit`, which reported
   "records changed 2". Stage counts now:

   | scope | IDENTICAL | MODIFIED | REWRITTEN |
   |---|---|---|---|
   | stage 2 | 830 | 543 | 29 |
   | total | 3136 | 1208 | 76 |

7. `docs/m2-r3-etg-particles.md`: `[audit: ...]` notes on the `modifyAlphaValue` row (F1/F4), on the delta-map
   section (F3) and on the stage counts.
8. `docs/m2-r3-etg-particles.md`: the HW1 particle size (F5) and the rebuilt binaries.

After the fixes, `tools/build.sh --debug` and `--release` both succeed with 0 warnings. Every harness run in section 4
used the fixed code.

Verdict: PASS-WITH-FIXES - table (167/167), call convention (25 callables) and every DONE WHEN reproduce exactly on the plain harness; the port wrongly forced PART_ALPHA on in modifyColor/modifyAlphaValue (the exe's clear branch jumps past the or), now fixed, plus one missed delta-map review and four doc/comment corrections
