# M2-R5 audit: sound and speech (producer 4)

Auditor: adversarial audit of `docs/m2-r5-sound.md` (fq codec, mixer, sound events, speech, battle chatter, subtitles,
`/catSoundTest`). Port work = `git diff af90827` in the m2 worktree (working tree; commit 1ad47c6 is the user's
oracle-script commit, `tools/oracle/record_reference.sh` only, not port work). Reference: the r6 decompile
(`ghidra/exports/cataclysm_gog_r6`), exe bytes read with `tools/m2_exedata.py` and disassembled with `objdump -b binary
-m i386`. Ghidra was not run. Every engine run went through `tools/m2_harness.sh` (or `tools/oracle/mission_test.sh`,
which calls it), with `SDL_AUDIODRIVER=dummy` forced by the harness (`tools/m2_harness.sh` line 198; `HWI_AUDIBLE` unset
in this environment); nothing reached the speakers.

## 1. Clean builds

`rm -rf original/scratch-m2/build-audit-r5-sound`, then `meson setup` with tools/build.sh's arguments (debug:
`--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`) and
`meson compile`.

- **First attempt: both configurations FAILED** (finding F1). kas2c stopped on all 18 mission / tutorial scripts (Mission01-16, Mission05_OEM, Tutorial1):
  `Mission01.kas(148) error : at '|', expecting parameter list for SpeechEvent (syntax error)`. Cause: the port made
  the HW1 actor flags in `src/Game/SpeechEvent.h` `(0x... | HW1_SPEECH_ID)`; the `.kas` files `#include` that header
  through cpp (`cc -E`), the preprocessed `Mission01.kp` line 140 became `SpeechEvent ((376 + (0x00001000 |
  0x00000400) + 0x10000000),0);`, and kas2c's grammar (`src/tools/kas2c/parser.y`) has `+` / `-` but no `|`. The
  producer's incremental `build/debug` never saw it: the `.kp` custom targets have no dependency on the header, so its
  `Mission01.kp` was the Oct 11 one (without the 0x400 marker). The report's "green, 0 warnings" was true only for
  that incremental tree.
- Fix: the flags add the marker with `+` (the bits are disjoint, so the value is the same), annotated in the header.
- After the fixes: clean debug and release builds green, 0 compiler warnings under `-Werror` (the only "WARNING" is
  meson's sanitizer notice). Sizes: debug `hwi-engine` 81,236,848 B, release 4,967,704 B. `tools/build.sh --debug` /
  `--release` in the worktree re-run green after deleting the stale `.kp` files. `tools/m2_layout32.sh`: OK.

## 2. Bodies compared with the exe (r6 decompile, disassembly where precision matters)

| # | function | result |
|---|---|---|
| 1 | fqDequantBlock 0x00559030 | as the exe: header fields at bits 0-1 / 2-4 / 5-6 / 7-10, start at bit 11, byte band counter with the `0x19 < count` and `++i > 0x19` stops, byte spectrum counter, `0x7f < count` stop inside the store test, gSDBlock[e] read before fqSUnpack, both spectrum loops at most 0x100 |
| 2 | fqSUnpack 0x00559470 / fqUnpack | as the exe (unsigned, `& 0x1f` shift masks, `0x21 - bits` sign step) |
| 3 | fqInitDequant 0x00558fd0 | as the exe (10.0f = 0x41200000, mask factor 1.0f (0x0072a4c4) / (float)(2^n - 1)) |
| 4 | Initdct 0x005558d0 | as the exe (1.0f / (float)len, 2PI float 0x40c90fdb) |
| 5 | fqWriteTBlock 0x00558bc0 | as the exe (no limiter, fChopT + 1.5*2^52 (0x0072a458), clamp, one source index, `0x200 < src` fails, `i < n - 1` loop) |
| 6 | fqDecOver 0x00558800 | as the exe (0x200-float copy) |
| 7 | fqDecBlock 0x005588c0 | as the exe (PI/128 0x3cc90fdb, PI/256, PI/512; Initdct failures fail; `(r != -1) - 1`) |
| 8 | fqMix 0x00558dc0 | as the exe (return at 0.0, plain add at 1.0) |
| 9 | fqPitchShift 0x00558e50 | as the exe (x87 position, __ftol, FQ_SIZE buffer) |
| 10 | fqEqualize 0x00558f20 | as the exe (`(min(end, n) - start) & 0x3fffffff` floats zeroed) |
| 11 | batRandomWeightSetCB 0x004232c0 | as the exe ((double)(float), +0x08 of the 0x58 entry) |
| 12 | batMaxDistanceSetCB 0x00423330 | as the exe (float square, +0x10) |
| 13 | batMinRepeatProximitySetCB 0x00423500 | as the exe (float square, +0x30) |
| 14 | batGlobalFrequencyModifierSet 0x00423710 | as the exe (raw float at 0x0075074c) |
| 15 | battleChatterFrequencySet 0x00424d80 | as the exe (freq100 * 0.01 double 0x0072a560) |
| 16 | cat_soundEventLetterboxVolume 0x00521770 | **differs** (F2): the exe multiplies by the doubles 0.6 / 0.3 (`dc 0d 80 a7 72 00` / `dc 0d 90 a7 72 00`, fmul qword), the port by the floats 0.6f / 0.3f; over volSFX = 0.000 .. 1.000 in 0.001 steps the float product differs by one ulp in 479 of 1,001 cases. Fixed |
| 17 | speechEventActorStop 0x00522a00 | as the exe (3 queues of 10 x 0x30-byte slots, slot = 0 with handle -1, timeout -1.0f, event -1; the playing line faded) |
| 18 | SEequalize 0x0051ff50 | as the exe (LUT order, volSFX product, SFX_MIN_PERCEPTABLE_VOL, clamp, default EQ past the ranges, rint) |
| 19 | ssSubtitleRead 0x00582610 | as the exe (actor bits 0x1000 .. 0x200000, 0x8000 by mission 2 / 1 / 16 / 11, 13, 14, else 12; animatic 0x40000000 -> 9) |
| 20 | battleCanChatterAtThisTime 0x004238b0 (port: battleRepeatOK) | **differs** (F3): the exe loads each coordinate with `flds`, subtracts with `fsubs`, squares and sums on the x87 stack and stores only the double sum (`fstl`, 0x00423969..0x004239af; battleChatterFleetAttempt 0x00423b54.. the same); the port rounded the differences to float first (`vecSub`). Fixed: the differences are taken in double. The wavelength test, the `minRepeatProximity > 0.0` (0x0072a4e8) guard, the camera fallback and `dist2 / sqrt(proximity)` are as the exe |
| 21 | speechEventAttack 0x00523040 | as the exe (own ship 0x4000807, resources only (objtype 2..5) 0x4000808, else 0x4000805, leader's type) |
| 22 | univMissileCollidedWithTarget 0x00547080, mine-field site | as the exe: battleChatterFleetAttempt(-1, 0x34 / 0x35, -1 / group, NULL); the tree's BCE_STAT_Grp_InMineField / BCE_STAT_AssGrp_InMineField are 52 / 53 (compiled with the tree's headers) |
| 23 | isoundmixerprocess 0x0057f850, the SFX channel loop | as the exe (NULL patch released; speech mixed by volfactorL/R without a scale; only EQ, no effect / delay calls) |
| 24 | pingAttackPingsCreate 0x004e9350 | as the port (`local_20[10] = 0`, ship list at +0x2c): battleping +0x28 holds |
| 25 | tutHideText 0x00530ef0 | as the port (speechEventActorStop(0x1000, 0.1)) |

Data: the 88-entry chatter table at 0x0074e8d0 (stride 0x58, end 0x00750710) has the port's names and event ids in
every slot (86 parsed directly plus the two `(sdword)0x81000804` docking entries; ids 0 at 76 / 77 / 81 / 87 as the
report says); the entry defaults (weight 1.0, maxDistance 1e6, expDistance 1.0, minWavelength 10.1, expWavelength 1.0,
proximity 0, exponent 1.0) match `Battle.h`'s BAD_* values. Cataclysm's BattleChatter.script (read from cataclysm.big
into scratch-m2) has 82 maxDistance, 72 expWavelength, 18 minRepeatProximity rows.

## 3. The diff (`git diff af90827`)

- Files: only the sound / speech / codec scope, the CAT-TODO call sites (Blast, ResCollect, UnivUpdate, CommandLayer,
  CommandWrap, Tutor, ShipSelect, LevelLoad, Ping, MeshAnim comment), the self-test hooks (main.c, utility.c,
  m2_harness.sh) and m2_layout32.sh. No change outside the producer's scope.
- A hunk-to-function map (each changed line to the function around it in the old and new file, then the delta map
  by name) found 100 edited functions. Edited functions the map calls IDENTICAL without a review entry from this port
  (finding F4): **pingAttackPingsCreate** 0x004e9350 (new field reset), **tutHideText** 0x00530ef0 (actor 0x1000),
  **soundbankadd** 0x00580d40 and **SNDgetpatch** 0x00581a90 (the HWI LP64 aligned patch table). Compared (rows 24,
  25; soundbankadd / SNDgetpatch as HW1 apart from the 64-bit table); entries added (sample `audit-R5-sound`; the
  SNDgetpatch reread-R2 entry keeps its sample and gets a note), `tools/delta_map.py merge --done m2r5:sound-audit`,
  `docs/m2-r1-function-deltas.md` regenerated: IDENTICAL 3,014, MODIFIED 1,329, REWRITTEN 77, reviewed 2,079.
- IDENTICAL bodies not verbatim (finding F5): the port deleted the debug `dbgAssertOrIgnore` lines of
  batDistanceExponentSetCB, batMinWavelengthSetCB, batWavelengthExponentSetCB (the last without a review entry) and
  batRepeatProximityExponentSetCB. Cataclysm's script satisfies them (minimums expDistance 0, minWavelength 1,
  expWavelength 0.001, expRepeatProximity 1), so they are restored; the four bodies are verbatim again.
- Delta-map counts (finding F6, doc): the report says 84 entries "(73 new, 11 updated)"; the review file at af90827
  vs the port has 84 `port-R5-sound` entries, 62 MODIFIED / 21 IDENTICAL / 1 REWRITTEN, **72 new, 12 updated**
  (consistent with reviewed 2,005 -> 2,077). Annotated. The other totals (3,040 -> 3,016, 1,303 -> 1,327) hold.
- `CAT:` comments: 165 new `CAT:` lines and 3 `CAT-FIX` as the report says. One new line without its own comment
  (finding F7, minor): `/catSoundTest` in main.c sat under the `/catTexturesTest` comment; it now has its own.
- CAT-TODOs: the 11 sound / speech CAT-TODO markers of af90827 (Blast, ResCollect, UnivUpdate, CommandLayer x7,
  CommandWrap) are calls or exe-id comments now; with ShipSelect's CAT-PROVISIONAL stub and Tutor's constant the
  report's 13 items hold. No `CAT-TODO(M2-R5 sound)` is left; the re-targets (M2-R5 hud, M2-R6 camera / mesh
  animation / front end / main region) are as the report lists.

## 4. DONE WHEN on the plain harness (final build, after the fixes)

| claim | run | result |
|---|---|---|
| `/catSoundTest` decodes one stream of each bank into the dummy device, 88 chatter events | `r5s-aud2-sound` (`--mode sound`; first build: `r5s-aud-sound`) | `audio driver dummy`; Music4Cat.wxd 62 streams, track 0 43 blocks; SectSpeech.vce event 0 124 blocks; BeastSpeech.vce event 26 281 blocks; 403 audible output blocks; 88 events / 88 named / 84 with an id; `result=PASS`; ASan 0; UBSan in fquant.c/fqcodec.c 0; PATCH 0; UBSan kinds: none |
| fe unchanged | `r5s-aud2-fe` (and `r5s-aud-fe`) | Main_game_screen, ASan 0, UBSan kinds only `fescreen` (92); base `r5r-aud-fe` had in addition 17 + 12 PATCH and 2 + 1 fquant.c reports; 233 file loads (base 232 + GunSpecEvents.lut) |
| sp unchanged | `r5s-aud2-sp` (and `r5s-aud-sp`), `--timeout 340` | universe running, univUpdateCounter 1,248, ASan 0, no unsuppressed leak, first non-benign UBSan BTG.c:388 (as base); kinds as the producer's `r5s-sp1`, base minus the 17 + 15 PATCH reports; 3,726 file loads |
| no PATCH report in any run | the sound, fe, sp and both mission runs | 0 `struct PATCH` and 0 fquant.c / fqcodec.c reports in each |
| Mission01 streams | `mission_test.sh --ticks 6000`: `r5s-aud-m-*` (after F1/F2) and `r5s-aud2-m-*` (final) | MISSION TEST PASS both times: A = B byte-identical (1,476,370 B, sha256 c1a701c32a193248), ASan 0, perturb found at 1500 (rng10), flipped copy at 2000; vs the base `r5r-fin-a`: IDENTICAL, 6,001 ticks (only the build identity differs). The kas fix (mission scripts now carry the 0x400 marker) and the chatter precision fix leave the stream unchanged |

## 5. Findings

| id | finding | evidence | fix |
|---|---|---|---|
| F1 (build, severe) | clean debug and release builds fail: kas2c cannot parse `|` in the HW1 speech ids that `SpeechEvent.h` now builds; the report's green builds were incremental with stale `.kp` scripts | the first-build log (18 kas2c errors, quoted in section 1; the clean rebuild overwrote the file), `Mission01.kp` line 140, `parser.y` operators | `+` instead of `|` in the 11 HW1 actor flags; clean builds green; worktree `.kp` regenerated; report annotated |
| F2 (exe body) | cat_soundEventLetterboxVolume multiplies by float 0.6f / 0.3f; the exe by double 0.6 / 0.3 | bytes `dc 0d` at 0x005217a0 / 0x005217ad; 479 of 1,001 levels differ | `(real32)((double)volSFX * 0.6)` / `0.3`; annotated |
| F3 (exe body) | battle chatter repeat-proximity distance squared from float-rounded differences; the exe keeps them on the x87 stack | disassembly 0x00423969..0x004239af, 0x00423b54.. | differences in double in battleRepeatOK; annotated |
| F4 (delta map) | 4 edited IDENTICAL functions without a review entry from this port | hunk-to-function map; `delta_map_review.json` | compared, entries added, merged, report regenerated |
| F5 (convention) | debug asserts deleted from 4 IDENTICAL callbacks (not verbatim) | `git diff af90827 -- src/Game/Battle.c` | restored |
| F6 (doc) | "73 new, 11 updated" review entries | review file af90827 vs now: 72 / 12 | annotated |
| F7 (minor) | `/catSoundTest` option without its own CAT comment | main.c | comment added |

Observation (no finding): other float constants the port writes as `real32` products (for example `* 0.01f` in the
random ambience times) were not all checked for the exe's operand width; F2 shows the pattern can differ.

## 6. Precision

46 sampled checks: builds 1 (wrong), bodies 25 (2 wrong: F2, F3), data tables and offsets 3 (chatter table, defaults,
battleping +0x28), CAT-TODO closure 1, runs 7 (sound, fe, sp, mission, dummy driver, PATCH 0, fq UBSan 0), diff and
delta-map checks 9 (scope, edited IDENTICALs F4, verbatim IDENTICALs F5, CAT comments F7, new/updated counts F6, map
totals, 165 CAT lines, 3 CAT-FIX, layout32). Wrong: 7. **Precision = 39 / 46 = 0.85.**

Fixes in the tree: 8 (SpeechEvent.h kas2c-safe flags; letterbox double multiply; chatter distance precision; 4 asserts
restored; 4 delta-map entries + merge + regenerated report; main.c comment; report annotations for the build claim and
for the review counts). Build green after every fix (clean debug and release, 0 warnings).

Verdict: PASS-WITH-FIXES - 39/46 checks hold (25 codec/mixer/speech/chatter bodies and the 88-row chatter table vs r6, sound self-test PASS into the dummy device with ASan 0 and 0 fquant/fqcodec/PATCH reports, fe and sp as base minus the PATCH/fquant reports, two 6,000-tick Mission01 streams identical and IDENTICAL to base); fixed the clean-build break (kas2c cannot parse '|' in the new HW1 speech ids), the letterbox and chatter-distance precision, 4 unreviewed edited IDENTICALs, 4 stripped IDENTICAL asserts, the review counts
