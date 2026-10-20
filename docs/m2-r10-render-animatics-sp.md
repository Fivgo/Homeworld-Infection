<!-- done: texreg, animatics, intmin, m17log, spgate -->
<!-- subsystems: texreg, animatics, intmin, m17log, spgate -->
# M2-R10 render-animatics-sp: the Mission04 render stop, the animatics and the sp gate (producer 2)

Base commit `36289a8` (producer 1's audited tree); the work is `git diff 36289a8` in the m2 worktree. Five
subsystems: **texreg** (the Mission04 render stop), **animatics** (every caller of 0x00421860 on the port, the headless
skip), **intmin** (Select.c / FOW.c's INT_MIN UBSan sites), **m17log** (Mission17's memAllocFunctionA notes and
is_final_tex warnings), **spgate** (the sp screenshot loop and the gates). `<R>` below is
`original/scratch-m2/m2r2-harness/runs`, `<S>` is `original/scratch-m2/m2r10-render`.

## Headline

- **The Mission04 stop was ours (GoK), not the exe's.** GoK's 64-bit build skipped, in `meshFree` and
  `trTextureUnregister`, every texture handle `>= TR_RegistrySize`. A handle carries its palette index in bits 16 ..
  (`trHandleMake`), so the second .. eighth colour schemes of every team-coloured texture were never unregistered:
  the usage count never reached 0, `trTextureDeleteAllUnregistered` kept the texture **loaded** into the next level, and
  the next level's `meshTextureRegisterAllPlayers` registered its schemes into a loaded texture. A loaded paletted
  texture's colour infos sit at `palettes + 0x400 * nPalettes` (`trColorInfosPointer`), so each append moved that
  pointer one palette further, the next lookup read palette bytes as colour infos (the `81d4ebe0/818181f1` R9 saw),
  matched nothing and appended again, until `nPalettes` reached 8: `RACE_P1/P1IONARRAYFRIGATE/RL0/LOD0/PAGE0`, flags
  0x36, use count 15, loaded since Mission03; `RACE_P1/P1FIGHTER/RL0/LOD0/PAGE0` was mid-way (the same schemes appended
  at nPalettes 2 .. 6). The exe's `meshFree` 0x004af710 unregisters every handle but -1, so the exe never reaches that
  state. Fixed at the cause; the exe's `trTextureUnregister` 0x005860d0 and `trTextureDelete` 0x005854c0 ported; the
  exe's unbounded append CAT-FIXed (the exe's and the release build's behaviour stated in section 1).
- **The sp gate**: the screenshot loop now walks Mission01_NIS01 -> 01 -> 02 -> 03 -> 04_NIS02 -> **04 -> 05 ->
  06_NIS04 -> 06 -> 07_NIS05 -> 07 -> 08 -> 09** and runs to its 900 s timeout: ASan 0, no leak, no fatal, 94
  screenshots, no Select.c / FOW.c report (`r10p2-c2-sp`).
- **The animatics**: `singlePlayerPostInit`, `singlePlayerGameUpdate` (the exe has **one** call there, 0x0051a01a, and so
  has the tree; not two) and `utyVideoPlay` now call `animAviPlay`, as `kasfMissionCompleted` did; a headless run logs
  `animatic: skipped (headless) a=N b=M file=AnimaticNN` and skips the real-time playback. Every transition 0 -> 1 ..
  17 -> 18 is logged in at least one headless run.
- **Select.c:1153/1154**: an object exactly in the camera plane (camera-space z 0, so w 0 and an infinite circle);
  the exe returns on `depth <= 0` before the rectangle (0x005070b8 [audit: was cited 0x0050683f, which is inside the call at 0x0050683e]): ported with the function's other Cataclysm deltas.
- **FOW.c:296-437**: Mission17's INT_MIN cells are NaN positions of KAS-team Taiidan heavy interceptors of player 0
  that attack a target list containing themselves (traced, owned by M3); the cube arithmetic is CAT-FIXed to the exe's
  two's-complement wrap.
- **Mission17's log**: `is_final_tex`'s 98,718 warnings were the 64-bit read of a 4-byte ETG terminator (fixed, 0
  now); the `memAllocFunctionA` lines are growth-heap notes from a 64 MB main heap 94% full after Mission17's load
  (named now, explained, owned by M3); GoK's sound banks were converted into twice their size (16.4 MB; exact size
  now, 8.2 MB back in the main heap of every run with sound).
- **No stream moved.** The headless Mission01 stream is IDENTICAL to producer 1's (debug and release); every per-mission
  self-test, the replays and the other guard rails are IDENTICAL in every simulation field (section 7).

## 1. texreg: the Mission04 render stop

### 1.1 Diagnosis (temporary prints, removed; runs kept)

- `r10p2-diag-sp1` (the base tree + prints in texreg.c): the stop reproduced at Mission04's load (`texreg.c (587)`);
  the overflowing texture `RACE_P1/P1IONARRAYFRIGATE/RL0/LOD0/PAGE0` idx 1463 flags 0x36 (Paletted | Allocated |
  TeamColor0 | TeamColor1, **not pending**) nPalettes 8 nUsageCount 15 not shared, the new scheme `ff808096/ff0000c8`,
  its 8 "colour infos" `81d4ebe0/818181f1, 81313181/f181fbe0, ...` (palette bytes); `REGISTER-NEWCOLOR` lines show
  `RACE_P1/P1FIGHTER/RL0/LOD0/PAGE0` (loaded, flags 0x36) taking the same schemes again at nPalettes 2, 3, 4, 5, 6.
  No `FREE-IN-USE` or `SHARE-ZEROCOUNT` line (the palette sharing by CRC is not the cause: the sharer keeps the owner's
  layout, both in one sort list with the same colour infos).
- Cause: GoK's `meshFree` (`if (handle < TR_RegistrySize)` under HW_PTR_64) and `trTextureUnregister` (`if (handle >=
  TR_RegistrySize) return`) compared the whole handle; every handle of palette index > 0 is >= 0x10000.
- `r10p2-diag2-sp` (meshFree / trTextureUnregister fixed): Mission04 passed (13 screenshots), no ninth scheme; the run
  then died in Mission05's load on HW1's memFree pool assertion (`Memory.c (1714)`, a completely full pool), with 3,488
  leaked `dupe trcolorinfo list` blocks in the heap: `trTextureDelete` (section 1.2).

### 1.2 Ported (file by file)

- `src/Game/Mesh.c` `meshFree` (0x004af710, IDENTICAL): HWI fix: GoK's `< TR_RegistrySize` guard removed; every
  handle but -1 (GoK's sign-extended -1 still skipped) is unregistered, as the exe.
- `src/SDL/texreg.c`:
  - `trTextureUnregister` (0x005860d0, MODIFIED): `CAT:` the pending slot is reset only when the last usage goes
    (`flags & TRF_Pending` and `nUsageCount == 0` after the decrement; HW1 reset it on every unregister); GoK's guard
    now tests `trIndex(handle)` (and a non-zero high dword), not the whole handle (HWI).
  - `trTextureDelete` (0x005854c0, IDENTICAL on the map, found modified): `CAT:` the entry is cleared in every build and
    its colour-info list (+0x14) freed when non-NULL (0x005855b5 .. 0x005855e8); HW1 only cleared the pointer under
    TR_ERROR_CHECKING and leaked the 64-byte list of every deleted texture.
  - `trColorsIndexAlloc` (inlined in trTextureRegister 0x00585760 from 0x005858ef and trRegisterAddition 0x00585b60):
    `CAT-FIX`. **The exe's behaviour**: the append has no bound; a ninth scheme is written past the 64-byte colour block
    of a pending texture (`memAllocFunction(0x40)` at registration) or past the palettes + infos block of a loaded one,
    and `nPalettes` (a signed byte at +0xd) becomes 9. **The release build before this round** (no assertion) did the
    same out-of-bounds write into the game heap (ASan cannot see it: the game's own pool). **Now**: a texture keeps its
    8 schemes, the request gets palette 0 and a `dbgWarningf` line. With the unregister fix no ninth scheme arrives in
    the campaign: no `trColorsIndexAlloc:` line in any run of this round.
- Compared and found as HW1 (reviews in section 6): trTextureRegister (4th argument at +0x30 only; the exe's
  `__strupr` of the name is HW1's, GoK's tree does not uppercase: not changed), trRegisterAddition, trRegisterRemoval,
  trTextureColorsUpdate, trInternalTexturesDelete, trTextureDeleteAllUnregistered.

## 2. animatics

- `src/Game/SinglePlayer.c` `singlePlayerPostInit` (0x005193d0: animBinkPlay(0, 1) at 0x005193e5) and
  `singlePlayerGameUpdate`'s white-out (0x005197d0: animBinkPlay(cur, cur + 1) at 0x0051a01a, the function's only call
  of 0x00421860; the tree had one call too) -> `animAviPlay`.
- `src/SDL/utility.c` `utyVideoPlay` (0x005a81a0: animBinkPlay(0, 1) at 0x005a81ba) -> `animAviPlay`. No .fib of
  either archive binds `VideoPlay` (checked over every .fib of cataclysm.big and update.big): unreachable in the retail
  front end.
- `kasfMissionCompleted` (0x00491c70) already called `animAviPlay`. `animBinkPlay` (GoK's `#if 0` stub) now has no
  caller in the built tree (utility_mac.c is not built).
- `src/Game/Animatic.c` `animAviPlay` (the port of 0x00421860): HWI: after the exe's own `cat_taskTimeCompressionReset`
  and tutorial test, `animHeadlessSkip()` (`univRealTimeVirtual || oracleMissionRequested()` under HWI_ORACLE, or
  `debugScreenshots`) returns FALSE before anything the playback changes (as the earlier /oracleMission skip did) and
  logs `animatic: skipped (headless) a=<from> b=<to> file=<AnimaticNN>`; an attended run logs `animatic: play a=.. b=..
  file=Movies/AnimaticNN.bik` before the playback. The 30 fps rate (decision 45) is animAviDecode's, unchanged.
  `tutorial == 1` returns before the line, as the exe returns before its playback (the tutorial run logs none).
- Transitions reached headlessly (final runs): 0 -> 1 (`r10p2-c2-missiontest-a/b`, the Mission01 stream runs, ai,
  build), 1 -> 2 .. 8 -> 9 (`r10p2-c2-sp`), N -> N + 1 for N = 2 .. 16 (`r10p2-c2-mN-a`: Animatic03 .. Animatic17;
  the Mission01 self-test ends at the jump's start, before the white-out), 17 -> 18 (`r10p2-c2-m17-a`: Mission17's
  MissionCompleted, Animatic18, then the credits).

### 2.1 The user's attended check (for the integrate's play-test brief)

`tools/play.sh` (release build, audio on, a real window). 1. Front end -> Single Player -> New Game: **Animatic01**
must play full screen before Mission01's opening NIS, at its own pace (30 fps; about 246 s; subtitles from
movies\animatic01.script), with its sound and music, the mouse cursor hidden; at its end the cursor returns and the
opening NIS starts. 2. Play Mission01 through its hyperspace jump: at the white-out **Animatic02** must play the same
way, then Mission02 loads. 3. Look in `<home>/logs/play-<timestamp>.log` for `animatic: play a=0 b=1
file=Movies/Animatic01.bik` and `a=1 b=2 ... Animatic02.bik`; no `animatic: skipped` line may appear in an attended run.
Listen for: the animatic's audio starting with the picture and the game's music stopped during it, and the game sounds
back (volumes restored) afterwards. Optional: Mission17's end plays **Animatic18** before the credits.

## 3. intmin: Select.c and FOW.c

### 3.1 Select.c:1153 / 1154 (selCircleCompute 0x00506720, MODIFIED, now ported)

- Found (`r10p2-diag4-m12`, a print in rndShipVisibleUsingCoordSys): Mission12 tick 6464, an sACV at (48417.6, -5747,
  -11492), the camera eye (47010.1, -2838.99, -11497.2) looking at (49659.8, -1546.6, -10539.3) with a valid look-at
  matrix: camera-space (3224.8, 13.28, **0**): the object lies in the camera plane, the projected w is 0, the circle
  infinite, and GoK's truncating `primGLToScreenY` gave `384 - INT_MIN`. A legitimate state, not a fault; the exe
  computes the same infinity but returns on `depth <= 0` before converting it (0x005070b8 [audit: was 0x0050683f]).
- Ported: the depth test before the rectangle and before a precise hit (with the radius test, 0x00506fa0 / 0x00506fb7 [audit: was 0x00506f0b, inside the poly loop; the bullet return is 0x005068af, the CAT comment's 0x00506a8d was a face test]); the
  rectangle rounded to the nearest pixel (`catSelScreenX/Y`, the exe's 1.5 * 2^52 trick); the sphere radius: a mimic's
  select radius (`cat_mimicGetSelectRadius` 0x005b94e0, Ship+0x18 bit 3), a bullet 1.0 only, the slipgates
  (dSlipgateSmall / Medium / Large, 0x70 .. 0x72) +200 / +300 / +400 (0x005067a2 ..); a bullet has no precise
  selection. HW1's `distance` sums are the exe's dead stores (dropped). GoK's mouse pixel-centre (decision 29) kept.
- Result: no Select.c report in sp or any mission self-test of this round.

### 3.2 FOW.c:296-437 (fowActivateCube 0x0047eeb0, fowDeactivateCube 0x0047f070, fowCubeHasUnvisited 0x0047f400)

- Found (`r10p2-diag2-m17`, `r10p2-diag3-m17dump`, `r10p2-diag4-m17`, `r10p2-diag5-m17dump`, `r10p2-diag6-m17`, prints
  and `/oracleShipDump`): the cells come from **NaN positions** of player 0's tHeavyInterceptors 27, 28, 30 (the
  KAS-controlled Taiidan fleet). At tick 1 `Init_Mission17` stores the "friendly" (computer-player) ships of volume
  0x7ec6887 in selection 0x6da2da9 (`kasfShipsSelectFriendly`), and the team FSMs `kFighters_Attack` /
  `kIonCannonFrigates_Attack` (Mission17.c 11503 / 12579) give their teams `kasfAttack(0x36980eb)`, built from
  0x6da2da9; the attackers (ships 0, 5, 8, 17, 27, 30, 35, 43, owner 0) are in that list (`clAttack` ->
  `ChangeOrderToAttack`; neither the exe's clAttack 0x00448950 nor clAttackThese 0x00448620 removes the attackers from
  their targets). Ship 27 then attacks itself (`T` lines: target=27, state 3), at tick 328 GenericInterceptorAttack
  flies to its own position with limitvel 0 (`V 328 27 fly ... dest=own position`), the desired velocity is 0,
  `aishipFlyToPointAvoidingObjsFunc` (AIShip.c 1852) normalises it to NaN, `catAitrackTurn` applies a NaN torque, the
  coordsys and later the position become NaN (ship 27 from 976, 30 from 1056, 28 from 979), and `fowToCell(NaN)` gives
  0x80000000 (the exe's `__ftol` gives the same). Whether 0x6da2da9 holds player 0's ships in the exe at that tick (the
  ships' owner when Init_Mission17 runs) is not decidable without the exe: **owner M3 missions / oracle physics**
  (evidence: the runs above; the stream already carries the NaN ships: producer 1's `r10p1g-m17-a` is IDENTICAL).
- CAT-FIX (FOW.c `fowCellSub` / `fowCellAdd`): the box `x - r`, `x + r` in 32-bit two's complement, as the exe's
  `sub` / `add` (0x0047eefa ..): a 0x80000000 cell gives a large positive x0 and a negative x1, an empty clipped box in
  both. No FOW.c report in the Mission17 runs of this round.

## 4. m17log: Mission17's memAllocFunctionA and is_final_tex lines

- **is_final_tex** (`src/Game/Particle.c`, GoK's 64-bit helper of partAdvanceTexAnim 0x004df940): an ETG texture
  animation (`subTextureDWORD` frames, etgSubTextureDWORD) ends with the script's `int 4294967295`, a 4-byte constant;
  the 64-bit frame's handle is 8 bytes, its high half the next constant's bytes (`0x65726628ffffffff`: "(fre"). The
  exe compares the 4-byte handle with -1, so the low 32 bits decide now: Mission17 98,718 warnings -> 0 (Missions 06,
  10, 11 too). Behaviour unchanged (GoK's work-around already ended the animation there).
- **memAllocFunctionA / ANV "failed to find block"** (`src/Game/Memory.c`, debug builds only, MEM_VERBOSE_LEVEL 1): not
  failures. The main heap (GoK's `MEM_HeapSizeDefault` 64 MB for the 64-bit layout; the exe's 0x01800000 = 24 MB at
  .data 0x008b861c, set by HeapSizeSet 0x0055f390) is 87% used after Mission17's load and 94% after the first frames
  (445 ships); each such allocation then comes from growth heap 0 (`memAllocFunction`'s growth heaps, as the exe's);
  none fails, one growth heap for the whole run. The lines now name the block and say so (`... ('MassAllocBlobs'):
  trying the growth heaps`). In the final Mission17 run (1,927 lines): MAB(MassAllocBlobs) 876, MassAllocBlobs 487,
  CatVertexState 239, MAS(MassAllocSel) 128, catMtShips 64, Ship 52, ShipLocalBindings 45, partsys 17, fowLines 8,
  SBHGrow 3, and at the load textures.ll / texreg 64bit conversion / LiFList.
- **Heap pressure found on the way** (sp, the chained campaign): GoK's `loadbankAndConvertTo64Bit`
  (`src/Game/SoundEvent.c`) allocated twice each bank's length: 16.4 MB of the main heap for 8.2 MB of converted banks.
  HWI: the exact converted size (the old length plus the larger patch table and its alignment), checked before the
  copy. Every run with sound has 8.9 MB more main heap (sp's front end 31.8 -> 22.9 MB used). The mission self-tests
  run without sound (banks not loaded), so Mission17's numbers above do not change.
- **Left (owner M3, the one-hour soak)**: in the chained campaign the heap still grows: growth heap 1 at Mission05,
  2 at Mission06, 3 at Mission08 (`r10p2-c2-sp`; before the bank fix: 1 at Mission04 .. 4 at Mission09). Whether a
  cross-level leak remains (2,438 `dupe trcolorinfo list` blocks still live at Mission09's load in `r10p2-c1-sp`'s heap
  analysis, possibly pending lists) or the main heap is simply small for the 64-bit layout is the soak's question; no
  allocation fails.

## 5. Builds

`tools/build.sh --debug` and `--release` from the final tree: rc 0, 0 warnings under `-Werror` (every build of the
round). The gate runs used `<S>/cur2/hwi-engine-{debug,release}`; a comment in SoundEvent.c was shortened after them
(the final tree's binaries `<S>/final/hwi-engine-{debug,release}`, `6100a357abebdcb3` / `33c37f11540be162`: the release
binary byte-identical to cur2's, the debug one differs in line numbers only; `r10p2-f-mission-{a,b,perturb}` on it:
MISSION TEST PASS, a = b `902884367d98025d`, ASan 0). No struct changed (`tools/m2_layout32.sh` not affected).
The diagnostic binaries (`<S>/diag2`, `diag4`, `diag6`) carried temporary prints (`HWI-DIAG-TEMP`), all removed:
`/bin/grep -rn "HWIDIAG\|HWI-DIAG\|hwiDiag" src` is empty.

## 6. Delta-map review entries

15 entries `[review: M2-R10 render-animatics-sp]` in `ghidra/delta_map_review.json` (8 new with sample
`port-R10-render-animatics-sp`, 7 existing with the note appended), merged with `python3 -I tools/delta_map.py merge --done
m2r10:render-animatics-sp` ("records changed 15") and `python3 -I tools/delta_map.py report`. Counts: IDENTICAL 2,760 ->
2,762, MODIFIED 1,577 -> 1,575, REWRITTEN 83, reviewed 3,521 -> 3,529. [audit: the audit added two review entries for memAllocFunctionANV 0x004adae0 / memAllocFunctionA 0x004adc60 (edited only inside `#if MEM_VERBOSE_LEVEL >= 1`, class IDENTICAL), so the merge now says "records changed 17" and reviewed 3,531; the other counts are unchanged]

- **IDENTICAL found modified (now MODIFIED), 1**: trTextureDelete 0x005854c0 (ported).
- **MODIFIED, ported or extended**: trTextureUnregister 0x005860d0 (ported), selCircleCompute 0x00506720 (ported, was
  unreviewed), trTextureRegister 0x00585760 (reviewed: the 4th argument; the unbounded append CAT-FIXed).
- **MODIFIED on the map, compared and as HW1 (now IDENTICAL), 3**: trRegisterAddition 0x00585b60, trRegisterRemoval
  0x00585e70, trTextureColorsUpdate 0x00585fa0 (the auto-class came from the inlined helpers).
- **IDENTICAL, compared (unreviewed before)**: trInternalTexturesDelete 0x00585280, trTextureDeleteAllUnregistered
  0x00586140.
- **Notes appended**: meshFree 0x004af710 (GoK's guard, fixed), partAdvanceTexAnim 0x004df940 (the 4-byte end test),
  singlePlayerPostInit 0x005193d0, utyVideoPlay 0x005a81a0, singlePlayerGameUpdate 0x005197d0 (one call, not two),
  animBinkPlay 0x00421860 (every caller on animAviPlay).
- Not in the map (cat-new): fowActivateCube 0x0047eeb0, fowDeactivateCube 0x0047f070, fowCubeHasUnvisited 0x0047f400
  (CAT-FIX in place).

## 7. Runs (plain harness, final debug binary `<S>/cur2/hwi-engine-debug` unless stated)

### 7.1 The sp gate

| run | mode | rc / wall | milestone | first stop | result |
|---|---|---|---|---|---|
| `r10p2-c2-sp` | sp `--timeout 900` | 124 / 900 s | universe running; levels Mission01_NIS01, 01, 02, 03, 04_NIS02, 04, 05, 06_NIS04, 06, 07_NIS05, 07, 08, 09 | none: running at the timeout (Mission09, its univUpdateCounter 800) | **ASan 0, no leak, no fatal**; screenshots kept per mission (`<R>/r10p2-c2-sp/settings/ScreenShots/golden-NN-*.jpg`): 01 10, 02 8, 03 14, 04 13, 05 8, 06 13, 07 8, 08 13, 09 7 (94); `animatics: 9 skipped (headless): Animatic01 .. Animatic09`; 0 Select.c / FOW.c reports; 0 is_final_tex lines; fescreen reports 0; UBSan kinds: Particle / ETG misaligned members and BTG.c loads only (R9's) |
| `r10p2-c1-sp` (the tree before the bank fix) | sp `--timeout 900` | 124 / 900 s | the same walk to Mission09 | none | ASan 0, no fatal; growth heaps 1 .. 4 by Mission09 |
| `r10p2-diag2-sp` (diagnostic: meshFree fix only) | sp `--timeout 700` | 1 / 441 s | Mission04 passed | `Memory.c (1714)` memFree pool assertion in Mission05's load (the dupe-list leak + the 2x banks) | led to trTextureDelete and the bank fix |
| `r10p2-diag-sp1` (diagnostic: base tree + prints) | sp `--timeout 600` | 1 / 342 s | Mission04's load | `texreg.c (587)` nPalettes < 8 | the stop reproduced and dumped (section 1.1) |

The machine was loaded during the round (a game and an editor's indexer at 10+ cores; load average 21-26), which slows
sp's real-time NIS; the walk depends on the wall clock, the gate on ASan, the fatal and the levels.

### 7.2 Streams (each against producer 1's audited stream with `hwo_compare.py`; "IDENTICAL" = only the header's build identity differs)

| stream | before (producer 1, audited) | after (this producer) | the move |
|---|---|---|---|
| Mission01 headless, debug (`tools/oracle/mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000`) | `44f6c81a6bf3f58d` (= R9 `553845b65e87f3b5` content) | `r10p2-c2-mission-a` = `-b` **`902884367d98025d`**, 1,476,370 B, MISSION TEST PASS, ASan 0 | **IDENTICAL** over ticks 0-6000 (to producer 1 and to R9) |
| Mission01 headless, release (`--no-perturb-run`) | `96267d55360e6f4e` | `r10p2-c2-mission-rel-a` = `-b` **`f00d68e03f244551`** | **IDENTICAL**; IDENTICAL to the debug stream |
| missiontest Mission01 (NIS-first) | `3775f5b80e300357` | a = b `abdb96e858db6f79`, 6/6, done 11424 | IDENTICAL |
| ai 400 | `4250574d4d40fa6b` | a = b `b3497a95ecfef7e0` (content) | IDENTICAL |
| skirmish LiveFireExercise2 | `97744e77a08306ac` | `1a0025fc8481af43`, 91 ships, PASS | IDENTICAL |
| replay 10-17 | `74db522c2b63cf23` | `r10p2-c2-r17` `b0cd45c23ec37bf7` | IDENTICAL over 9601 ticks / 4800 packets |
| Tutorial1 leg | `4a165cb9055e3269` | `d371fd3484bc6280`, 12/12, 10444 | IDENTICAL |
| warp start level 10 | `d45b5ee41af801c2` | `14268c823f389d61`, PASS | IDENTICAL |
| build | `f9144d0c347e9863` | `33d28ad3ba195488`, PASS | IDENTICAL |
| motion / nis dumps `/noInterp` (decision 46) | `183681235a3fdc7b` / `13ca70eea998e5de` | `r10p2-c2b-motion-noi` / `r10p2-c2b-nis-noi`: the same hashes | equal |
| fe 120 s | - | `r10p2-c2-fe`: Main_game_screen, ASan 0, no leak, 0 fescreen reports | - |

### 7.3 The per-mission self-tests (`--mission N --mission-test-ticks 90000 --timeout 3600`; Mission01 = missiontest)

| mission | objectives | done tick | stream (a) | pair | vs producer 1 (`r10p1g-*`) | animatic line |
|---|---|---|---|---|---|---|
| 01 | 6/6 | 11424 | `abdb96e858db6f79` | a = b | IDENTICAL | Animatic01 |
| 02 | 6/6 | 25936 | `31cf665878e5438d` | - | rng_all only from 11035; every simulation field IDENTICAL | Animatic03 |
| 03 | 5/5 | 17825 | `544e57ae1921719f` | - | rng_all only from 6708; simulation IDENTICAL | Animatic04 |
| 04 | 5/5 | 20818 | `198d4a9b590735cb` | a = b | IDENTICAL | Animatic05 |
| 05 | 6/6 | 16384 | `d2553049b8603011` | - | IDENTICAL | Animatic06 |
| 06 | 7/7 | 31011 | `a6093894f8a7d0ac` | a != b: rng_all only from 4980 (b `1d4ec070d0f5ee71`); simulation IDENTICAL | rng_all only from 4980; simulation IDENTICAL | Animatic07 |
| 07 | 4/4 | 16592 | `2031ab0db85ee125` | - | IDENTICAL | Animatic08 |
| 08 | 6/6 | 29361 | `ee0d8f0eada845a2` | - | IDENTICAL | Animatic09 |
| 09 | 6/6 | 20617 | `3d2c2c341f4d1af8` | - | IDENTICAL | Animatic10 |
| 10 | 5/5 | 21939 | `c700d4856cb6de2d` | - | rng_all only from 11118; simulation IDENTICAL | Animatic11 |
| 11 | 5/5 | 8275 | `68589654c29cf14c` | - | IDENTICAL | Animatic12 |
| 12 | 3/3 | 10995 | `12ea97cf6febd47f` | a = b | IDENTICAL | Animatic13 |
| 13 | 3/3 | 19283 | `afabfab9832e6286` | - | IDENTICAL | Animatic14 |
| 14 | 4/4 | 22338 | `bbcbb55e8abfe8fe` | a = b | rng_all only from 10111 (producer 1's run; this pair is byte-identical); simulation IDENTICAL | Animatic15 |
| 15 | 3/3 | 15424 | `54f10604711fa24c` | a = b | IDENTICAL | Animatic16 |
| 16 | 6/6 | 21523 | `a589d57404638a48` | a = b | IDENTICAL | Animatic17 |
| 17 | 4/4 + `game end tick=15584` (the credits) | 15584 | `7f2da769dd02065a` | a = b | IDENTICAL | Animatic18 |

Every run: result=PASS, every objective in order, ASan 0, **0 Select.c / FOW.c UBSan reports**, 0 is_final_tex lines.
The rng_all-only differences are decision 94's wall-clock mixer flake (the same binary's Mission06 pair shows it; the
"simulation fields" compare `dt, total, real, fpcw, univcheck, statcheck, rng10, rng6, rng0, ships_*, missiles,
bullets, derelicts, resources, players`, all ticks). No stream of this producer moved.

## 8. Harness

`tools/m2_harness.sh` (edited with no run in flight): the header documents the animatics (every caller, the headless
skip, the `animatic: skipped (headless) a=.. b=.. file=AnimaticNN` and `animatic: play ...` lines); the summary has a
new line `animatics: N skipped (headless): Animatic01,...` in every mode. No new mode or flag; no new engine switch (the
skip keys on the existing virtual clock and /debugScreenshots).

## 9. CAT-TODOs

None names the render / presentation owner, M2-R9 or M2-R10 (`/bin/grep -rn 'CAT-TODO(' src`: M3 oracle physics 2,
stage 5 23). None closed or added by this producer; the new marks are `CAT:` (trTextureUnregister, trTextureDelete,
selCircleCompute, the call sites) and `CAT-FIX` (trColorsIndexAlloc, the FOW cube boxes).

## 10. For STATE.md

- **The Mission04 render stop closed** (decision 90): GoK's HW_PTR_64 guards in meshFree / trTextureUnregister refused
  every handle with a palette index > 0; fixed (HWI) with the exe's trTextureUnregister 0x005860d0 (slot reset at the
  last usage) and trTextureDelete 0x005854c0 (the colour list freed) ported; trColorsIndexAlloc's ninth scheme is a
  CAT-FIX (palette 0, a warning; the exe writes past the block). The sp gate now walks Mission01 .. 09 in 900 s.
- **The animatics routed** (decision 91 applied): every caller of 0x00421860 reaches animAviPlay; headless = the virtual
  clock or /debugScreenshots; lines `animatic: skipped (headless) a=N b=M file=AnimaticNN` / `animatic: play ...`;
  harness summary `animatics:`. Names: `animHeadlessSkip`. singlePlayerGameUpdate has one call (0x0051a01a), not two.
- **Select.c ported** (selCircleCompute 0x00506720): the depth test, the radius cases, the pixel rounding; decision 85's
  Select.c kind closed.
- **FOW.c CAT-FIX** (`fowCellSub` / `fowCellAdd`, the exe's wrap); decision 85's FOW.c kind closed; the NaN
  interceptors of Mission17 owned by M3.
- **is_final_tex** decides on the low 32 bits; **memAlloc notes** name the block; **the sound banks** are converted into
  their exact size (HWI; 8.2 MB back in the main heap).
- Stream refs (this producer, = producer 1's content): Mission01 debug `902884367d98025d`, release `f00d68e03f244551`;
  the mission table in 7.3.

## 11. What is left (owner)

- Mission17's KAS-team Taiidan interceptors attacking themselves (Init_Mission17's selection 0x6da2da9 holds player 0's
  ships at tick 1; the NaN flight from tick 328): whether the exe's selection holds them and what its attack path does
  with a self-target: **M3 missions / oracle physics** (section 3.2).
- The chained campaign's heap growth (growth heaps 1 .. 3 by Mission08 in sp; a possible cross-level leak, the main heap
  size for the 64-bit layout): **M3 soak** (section 4).
- The attended animatic check (section 2.1): **the user** (the integrate's play-test brief).
- trTextureRegister's name case (the exe uppercases, GoK's tree does not): **M3 render**, only if a texture is found
  registered twice under two cases.
