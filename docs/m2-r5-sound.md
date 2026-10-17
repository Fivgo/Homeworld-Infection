<!-- done: fqcodec, mixer, soundevent, speech, selftest -->
# M2-R5 producer 4: sound and speech (fq codec, mixer, sound events, speech, battle chatter, subtitles)

Status: **done** (DONE WHEN met, see "Runs"). Branch `m2`, worktree `.claude/worktrees/m2`, base commit `af90827`
(HEAD later moved to `1ad47c6`, the user's oracle-script commit, which touches `tools/oracle` only; nothing committed
by this producer: the audit checkpoints). Scope: docs/m2-r4-summary.md "Plan for M2-R5 and M2-R6" row 4.

Every Cataclysm change in the source carries `CAT: <what> (<exe function> 0x00xxxxxx)` (165 new `CAT:` lines); the
three deliberate deviations from the exe carry `CAT-FIX` (listed under "Open questions"). Reference: the Cataclysm
decompile r6 (`ghidra/exports/cataclysm_gog_r6`), exe constants read with `tools/m2_exedata.py`. Ghidra was not run.

Subsystems (done-marker names):
- `fqcodec`: `fqcodec.c`, `fqeffect.c`, `fquant.c`, `dct.c`, `mixfft.c` at the exe bodies (decision 11: follow the
  binary); the `fquant.c` UBSan kinds.
- `mixer`: `smixer.c`, `soundlow.c`, `sstream.c`, `soundcmn.h`; the `struct PATCH` misaligned accesses.
- `soundevent`: `SoundEvent.c`, `SoundEventPlay.c`, `SoundEventStop.c` (stop macros) and the sound cat-new
  functions (blast, asteroid break, level reset, mesh-animation sound table).
- `speech`: `SpeechEvent.c` (3 speech queues, alerts, race voice, Easter eggs, letterbox, music), `Battle.c`,
  `Subtitle.c`; the sound/speech CAT-TODOs of the R3/R4 ports turned into calls.
- `selftest`: `/catSoundTest`, harness mode `sound`, builds and runs.

## Summary

- The fq codec decodes at the exe: `fquant.c`'s byte counters, band/index caps, the unaligned load through `memcpy`
  and the unsigned shifts remove both GoK UBSan kinds (misaligned `int` load, `3 << 31`); `fqWriteTBlock` drops GoK's
  soft limiter (`compress()` is not in the exe) and rounds with Relic's rint (`fqRint`).
- The `struct PATCH` misaligned accesses are gone: the 64-bit converted bank keeps its patch table at an aligned
  +0x10 (`SNDbankpatches`); every run had 17 + 12..15 reports in `SoundEvent.c` / `soundlow.c`, now 0, and the
  harness counts them again (removed from the benign filter).
- The speech runtime is Cataclysm's: 3 speech queues/streams + 1 music stream, the exe's id layout (index 0x7ff,
  actors 0x3ff800, order by the top four type bits), per-race voice (`SectSpeech.vce` / `BeastSpeech.vce`), the
  stealth-ship alerts, the player-name Easter eggs, the letterbox, the 62-track music table and its state machine.
  HW1-named ids are muted by a marker bit (0x400) so no HW1 call site plays an unrelated Cataclysm line.
- The sound events follow the exe's 92-row LUT layout, `GunSpecEvents.lut`, the derelict / engine / ambient update
  rules and the Cataclysm UI sounds; the battle chatter keeps raw weights, squared distances and the status-event
  spacing, and `battlePingEvaluate` runs at most every 8 s per ping.
- 13 sound/speech CAT-TODOs of the R3/R4 ports are now calls (Blast, ResCollect, UnivUpdate, CommandLayer x7,
  CommandWrap, Tutor, ShipSelect); the remaining ones are re-targeted to their owners ("What is left").
- New `/catSoundTest` (harness mode `sound`): PASS, ASan 0, 0 UBSan in `fquant.c`/`fqcodec.c`, 0 PATCH reports;
  sp, fe and the Mission01 streams are unchanged (streams IDENTICAL to base). Delta map: 84 review entries.

## What changed and why, file by file

### `fqcodec` (codec)

- `src/SDL/fquant.c`: fqInitDequant 0x00558fd0 computes the mask factor as a float division `1.0f / (2^(i+1) - 1)`;
  fqSUnpack 0x00559470 / fqUnpack extract unsigned bit fields with the x86 shift masks (`& 31`; GoK's signed shift
  overflowed: the `3 << 31` UBSan kind); the 32-bit stream load is a `memcpy` (`fqLoad32`; the exe reads unaligned
  dwords: the misaligned `int` UBSan kind); fqDequantBlock 0x00559030 uses the exe's byte counters (`ubyte`
  count / u / e) and its caps: the band loop stops at `count > FQ_CBNUM-2` and `++i > FQ_CBNUM-2`, the spectrum loops
  at `count > 0x7f`. Why: decision 11 (follow the binary where GoK's reconstruction differs), and both UBSan kinds.
- `src/SDL/fqcodec.c` / `fqcodec.h`: fqWriteTBlock 0x00558bc0 has no soft limiter (GoK's `compress()` is not in the
  exe, removed), each sample is Relic's rint clamped to [-0x8000, 0x7fff], one source index across both output
  buffers, an error past FQ_DSIZE; fqDecOver 0x00558800 copies the whole FQ_DSIZE block; fqDecBlock 0x005588c0 builds
  the windows with the exe's float steps (PI/128, PI/256, PI/512 at 0x0072bdd8/d4/d0) and fails when Initdct fails.
  `fqcodec.h` adds `fqRint(double)`: the exe's `fChopT + 1.5*2^52` low dword (round to nearest even, low 32 bits),
  used wherever the exe rounds with it (mixer clamps, fqWriteTBlock, SEequalize, pan angles, volume compares).
- `src/SDL/dct.c`: Initdct 0x005558d0 computes `step = 1.0f/(float)len` in float and the angle
  `(i + 0.125) * step * 2PI` (float 0x0072a594) at full precision. idct 0x005556e0 compared: as GoK.
- `src/SDL/fqeffect.c`: fqMix 0x00558dc0 returns only for level 0 (negative levels mix in) and adds plainly at 1.0;
  fqPitchShift 0x00558e50 keeps the position in double (x87) and uses an FQ_SIZE work buffer; fqEqualize 0x00558f20
  zeroes a 0.0 band only up to min(band end, nBSize).
- `src/SDL/mixfft.c`: unchanged; transTableSetup 0x0056c290, twiddleTransf 0x0056cc70 and fft 0x0056d3f0 compared
  with r6: IDENTICAL.

### `mixer`

- `src/SDL/smixer.c`: isoundmixerprocess 0x0057f850 without the effect / q-noise / delay / acoustic-model stages and
  without the effect scale (the exe's mixer has none; `isoundmixerdecode` lost its effect argument), clamps and the
  stop test rounded with Relic's rint, a playing channel with a NULL patch is released. Port diagnostics (not in
  the exe, marked so): `catSoundDecodedBlocks[]` per stream and `catSoundAudibleBlocks` (output blocks with sound;
  the size counts shorts), read only by the self-test. isoundmixerinit 0x0057f680: the codec init as the exe; its
  DirectSound / waveOut part is the Win32 device layer (not ported, see "What is left").
- `src/SDL/soundcmn.h`: `SND_BANK_PATCHES_OFFSET` / `SNDbankpatches(bank)`: the 64-bit converted bank's patch table
  starts at the aligned +0x10 instead of HW1's +4 (the source of every `struct PATCH` misaligned report); static
  asserts pin the on-disk `PATCH_DISK` layout (size 0x38, bitrate +0x1C, flags +0x1E, frequency +0x28) and the
  ILP32 `PATCH` size 0x38 (checked by `tools/m2_layout32.sh`, which now includes `soundcmn.h`).
- `src/SDL/soundlow.c`: soundbankadd / SNDgetpatch go through `SNDbankpatches`; soundvolumeF 0x00580f60's "already at
  this level" test rounds with rint.
- `src/SDL/sstream.c`: soundstreamcreatebuffer 0x00581f20 has no fqAcModel reset; soundstreamopenfile 0x00581e90
  prints "Cataclysm CD not detected."; soundstreaminit 0x00581da0 no longer starts the stream thread
  (speechEventInit 0x00521a40 does); soundstreamvolume 0x00582420 rounds with rint; ssSubtitleRead 0x00582610 maps
  Cataclysm's actor bits to the subtitle actor (0x1000 -> 0, 0x2000 -> 1, 0x4000 -> 11, ...).
- `src/Game/SoundEvent.c` loadbankAndConvertTo64Bit: writes the converted patch table at `SND_BANK_PATCHES_OFFSET`.

### `soundevent`

- `src/Game/SoundEvent.c`: SEloadbank 0x005200c0 also loads `SoundFX/GunSpecEvents.lut` (soundEventClose 0x0051c9a0
  frees it); soundEventInit 0x0051c6e0 takes the race (speech through cat_speechEventInitForRace 0x00521d50) and ends
  with cat_madAnimSoundTableInit 0x004b64c0; soundEventRestart 0x0051caf0 plays front-end track 0x11;
  soundEventUpdate 0x0051cd10 is Cataclysm's (derelicts: ambient and random only; engine limits spare the
  motherships; the ship's handles through one helper `catSEupdateHandle`; the stealth alerts through
  cat_speechAlertsNoteShip; the piloting tests through cat_ccIsPilotingShip / ThisShip stubs, see "What is left");
  soundEventInitStruct 0x0051f900 starts lastAmbient at -1; SEequalize 0x0051ff50, getPanAngle 0x005206f0 and
  SEgetAngleToCamera 0x005208e0 round with rint. soundEventHearActor (not in the exe's callers) is a no-op.
  New cat functions: cat_madAnimSoundTableInit 0x004b64c0 / cat_madAnimSoundEvent (the 60 x 6 door / jack sound table
  at .bss 0x00c32660, read by madAnimationUpdate 0x004b6ce0); cat_soundEventBlast 0x0051fc60 (a blast's sound);
  cat_soundEventAsteroidBreak 0x005215d0; cat_soundLevelReset 0x0051c980 (alerts, chatter, letterbox, Easter egg).
- `src/Game/SoundEventPlay.c`: soundEventPlay 0x005209f0 is Cataclysm's player: no out-of-view exception, the 92-row
  Volume / Range / Frequency LUT layout (`SoundEventPrivate.h` row offsets 12 / 24 / 44 / 64 / 68 / 84, explosions
  past 0x100f on row 0x52, Hit_Cat7 on the ricochet row 0x56), the per-race UI sounds and the UI ambients
  0x100..0x107 (`CAT_UI_LAST_AMBIENT`, `SoundEventDefs.h`).
- `src/Game/SoundEvent.h`: the stop macros on Cataclysm's actor bits; prototypes of the new functions;
  `SOUND_FRONTEND_TRACK 0x11`. `SoundEventStop.c` unchanged (its bodies compared earlier).
- `src/Game/SoundEventPrivate.h`: `SE_NUM_ACTORS 3`, `SE_NUM_STREAMS 4`, the LUT rows, `GunSpecEventsLUT`.
- Callers: `Blast.c` calls cat_soundEventBlast (was CAT-TODO), `ResCollect.c` cat_soundEventAsteroidBreak (was
  CAT-TODO), `LevelLoad.c` cat_soundLevelReset after the mission's preload script (levelPreInit 0x004a3230),
  `MeshAnim.c` comment points to cat_madAnimSoundEvent.

### `speech`

- `src/Game/SpeechEvent.h` (latin-1 file): the Cataclysm id block (`CAT_SPEECH_EVENT_MASK 0x7ff`,
  `CAT_SPEECH_LAST_EVENT 0x35e`, actor bits 0x800 pilots / 0x1000 Fleet Command / 0x2000 second voice / 0x200000,
  `CAT_SPEECH_ACTOR_MASK 0x3ff800`, order mask 0xf0000000); every HW1 actor flag carries `HW1_SPEECH_ID` 0x400, so
  speechEventQueue rejects an HW1-named event (index > 0x35e) instead of playing an unrelated Cataclysm line.
- `src/Game/SpeechEvent.c`: rewritten at the exe: speechEventInit 0x00521a40 (3 speech streams at the LUT's bitrate
  + 1 music stream at 0x200, Music4Cat.wxh / .wxd, the stream thread), cat_speechEventLoadRaceVoice 0x005217f0
  (Somtaaw: Sectsentence/Sectphrase.lut + SectSpeech.vce; others: the Beast set), cat_speechEventInitForRace
  0x00521d50, speechEventClose 0x00521e30 (8 buffers), speechEventUpdate 0x00521ea0, speechQueueUpdate 0x00521eb0
  (timeover, the Eegah Easter-egg line 0x20012d1 after 540 s), speechPlayQueue 0x005220e0 (volumes, single-player
  ratio, pilots x0.75, letterbox), SEselectsentence 0x005223c0 (no N01 rotations), speechEventQueue 0x00522560 (ids,
  3 queues by actor bits), speechEventActorStop 0x00522a00 / StopAllSpecific 0x00522a70 / RemoveShip 0x00522b00
  (cleared slots event -1), SEreorderqueue 0x00522e60 (SEcleanqueue inlined, top-four-bit order), speechEventAttack
  0x00523040 (0x4000807 / 0x4000808 / 0x4000805), speechEventUnderAttack 0x00523190; the alerts
  cat_speechAlertsReset 0x00522ba0 / NoteShip 0x00522bc0 / Update 0x00522ce0; cat_speechEasterEggCheck 0x00522dc0
  (player names at 0x008961a4 / 0x00896194, `catSpeechPrinceOfSpace`); cat_soundEventLetterboxVolume 0x00521770 (SFX
  to 60%, 30% in mission 5) [audit: the exe multiplies by the doubles 0.6 / 0.3 (`fmul qword`, bytes `dc 0d`); the
  port multiplied by the floats 0.6f / 0.3f, one ulp off for about half the levels; fixed]; music: musicEventPlay 0x00523480 (one stream, random track 0x44e, 0.5 s fade then the
  next), musicEventStop 0x005235c0, musicEventUpdateVolume 0x00523660 (the single stream's state machine, level by
  nearest ship, tutorial track 0x12, letterbox x0.7, sensors / manager levels, game-stats track 0x28). The
  self-test `catSoundRunSelfTest` is appended (port diagnostic, see `selftest`).
- `src/Game/Battle.c` / `Battle.h`: the script callbacks store the raw weight (batRandomWeightSetCB 0x004232c0) and
  squared distances (batMaxDistanceSetCB 0x00423330, batMinRepeatProximitySetCB 0x00423500), defaults from .data
  0x0074e8d0 (weight 1.0, distance 1e6, wavelength 10.1); batGlobalFrequencyModifier is a float scale
  (0x0075074c); battleChatterAttempt 0x00423790 (local player's ships only, one random draw), battleCanChatterAtThisTime
  0x004238b0 (squared proximity), battleChatterFleetAttempt 0x00423a50 (status events 30 s apart, 60 s after the same,
  suppressed ones silent in single player), battlePingEvaluateNoEnemies 0x00423f80, battlePingEvaluate 0x00424110
  (at most every 8 s per ping: new battleping field `catLastEvaluated` +0x28, reset in `Ping.c` as
  pingAttackPingsCreate 0x004e9350 does), battleChatterFrequencySet 0x00424d80, battleShipDyingWithTimeToScream
  0x00424da0, battleLaunchWelcomeExchange 0x00424ef0 (REWRITTEN: takes class and variable), cat_battleChatterReset
  0x00423760, cat_batStatusUpdateSetCB 0x004235f0 / cat_batStatusSuppressSetCB 0x00423680 with readers.
  [audit: the port had also dropped the debug asserts of four IDENTICAL callbacks (batDistanceExponentSetCB,
  batMinWavelengthSetCB, batWavelengthExponentSetCB, batRepeatProximityExponentSetCB); IDENTICAL bodies stay verbatim
  and Cataclysm's BattleChatter.script satisfies them (minimums 0 / 1 / 0.001 / 1), so they are restored.]
- `src/Game/Subtitle.c`: subTitleAdd 0x00584580 resets the time compression (cat_taskTimeCompressionResetAndPrint);
  subTitlesDraw 0x00584620 read against r6 and left to the HUD port (CAT-TODO(M2-R5 hud) at the function).
- Calls that were CAT-TODOs: `CommandLayer.c` (clRUTransfer 0x00449a5c sound 0x129 + speech 0x4001045/0x4001046,
  clHaltThese 0x00449c67, clHalt 0x00449e80, cat_clHarvestNearestResource speech 0x2001043, soundEventPlay(NULL,0x10a)
  + speech 0x40008a2, soundEventPlay(NULL,0x109) + speech 0x40008a3 + the ship's engine sound), `CommandWrap.c` (dock
  lines 0x400106a / 0x4000801 / 0x4000803), `UnivUpdate.c` (mine field: battleChatterFleetAttempt with
  BCE_STAT_Grp_InMineField / BCE_STAT_AssGrp_InMineField by hot-key group, univMissileCollidedWithTarget 0x005470e2),
  `Tutor.c` (tutHideText 0x00530ef0 stops Fleet Command's bit 0x1000), `ShipSelect.c` (its provisional
  cat_speechAlertsNoteShip stub removed: the real one is in SpeechEvent.c).

### `selftest`

- `src/Game/SpeechEvent.c` catSoundRunSelfTest (port diagnostic, not in the exe): after the normal start-up, queues
  the first track with data of `Music4Cat.wxd`, a `SectSpeech.vce` sentence and a `BeastSpeech.vce` sentence (loading
  each race's voice with cat_speechEventLoadRaceVoice) on the stream code as the game does, waits until the mixer
  has decoded them through fqDequantBlock / fqDecBlock / fqWriteTBlock into the audio device, then looks up the 88
  battle-chatter events in both races' tables. Log lines `soundtest: ...`, last `soundtest: done ... result=PASS|FAIL`.
- `src/SDL/main.c`: option `/catSoundTest` (`catSoundSelfTest`); `src/SDL/utility.c`: runs it after the start-up
  and quits.
- `tools/m2_harness.sh`: mode `sound` (args `/catSoundTest`, milestone "sound self-test done", a summary line with the
  `soundtest:` lines, the UBSan count in `fquant.c`/`fqcodec.c` and the PATCH count); `struct PATCH` removed from the
  benign UBSan filter so any new one is reported. The harness forces `SDL_AUDIODRIVER=dummy`: nothing is audible.
## Builds

- `tools/build.sh --debug` (ASan + UBSan): green, 0 compiler warnings under `-Werror` (the only "WARNING" line is
  meson's sanitizer notice). The final runs below use this build (`build/debug/hwi-engine`, 2026-10-17 07:19).
- `tools/build.sh --release`: green, 0 warnings.
- [audit: these builds were incremental. From a clean directory both configurations FAILED: the HW1 actor flags in
  `SpeechEvent.h` were `(0x... | HW1_SPEECH_ID)`, the `.kas` mission scripts include that header through cpp, and
  kas2c (`src/tools/kas2c/parser.y`) has no `|` operator, so all 18 mission/tutorial scripts stopped with "at '|',
  expecting parameter list for SpeechEvent". The `.kp` custom targets do not depend on the header, so the
  producer's `build/debug` kept the Oct 11 scripts (without the 0x400 marker). Fixed: the flags add the marker with
  `+` (disjoint bits); clean debug and release builds green, 0 warnings; `build/{debug,release}` regenerated.]
- `tools/m2_layout32.sh`: OK (now includes `soundcmn.h`: the ILP32 `PATCH` and `PATCH_DISK` asserts hold).

## Runs (all through `tools/m2_harness.sh`, silent: SDL_AUDIODRIVER=dummy, offscreen; logs under `original/scratch-m2/m2r2-harness/runs/<label>`)

| label | mode | milestone | first stop | ASan | notes |
|---|---|---|---|---|---|
| r5s-sound-fin | sound | sound self-test done | none found in the log (rc=0) | 0 | `result=PASS`: Music4Cat.wxd 62 streams, track 0 (bitrate 512, 2 ch) 43 blocks; SectSpeech.vce event 0 124 blocks; BeastSpeech.vce event 26 281 blocks; 403 audible output blocks (into the dummy device); 88 chatter events, 88 named, 84 with an id (4 have 0, as in the exe), 64 with a Somtaaw and 57 with a Beast sentence; UBSan in fquant.c/fqcodec.c 0; PATCH 0; first non-benign UBSan: none |
| r5s-sound1, r5s-sound2 | sound | sound self-test done | none found in the log (rc=0) | 0 | same result (earlier builds of this round) |
| r5s-fe-fin | fe | Main_game_screen | none: still running at the 120s timeout | 0 | UBSan kinds only `fescreen` (92); base r5r-aud-fe also had 17 + 12 PATCH and 2 + 1 fquant.c reports, now 0; 233 file loads (base 232: + GunSpecEvents.lut) |
| r5s-sp1 | sp | universe running (univUpdateCounter > 0) | none: still running at the 340s timeout | 0 | first non-benign UBSan: BTG.c:388 misaligned `short` (as base); PATCH 17 + 15 of base r5r-aud-sp now 0; other kinds as base; 33 ship statics; 3726 file loads (base 3721: + GunSpecEvents.lut and 2 + 2 extra redraws of the load-screen `arrows.lif` / `ring.lif`, timing) |
| r5s-fin (a, b) | mission (tools/oracle/mission_test.sh, 6000 ticks) | oracle mission run done | none found in the log (rc=0) | 0 | MISSION TEST PASS: a and b byte-identical (sha256 c1a701c32a193248), perturbed copy found at tick 2000 `ships_health`; vs base r5r-fin-a (render port): IDENTICAL, 6001 ticks, no difference (only the build identity differs) |
| r5s-m1, r5s-m2 | mission | oracle mission run done | none found in the log (rc=0) | 0 | earlier builds of this round, PASS, IDENTICAL to base |
| r5s-oracle-rel | release selftest (tools/oracle/selftest.sh, build/release) | oracle self-test done | none found in the log (rc=0) | n/a | SELFTEST PASS |

DONE WHEN: `/catSoundTest` opens Music4Cat.wxd, SectSpeech.vce and BeastSpeech.vce, decodes one stream of each
through the fq codec into the dummy device and looks up the 88 chatter events, ASan 0 and 0 UBSan in
`fquant.c`/`fqcodec.c`; no PATCH misaligned report in any run (sound, fe, sp, mission); sp and fe unchanged apart
from the removed reports and the one new LUT load; nothing audible (dummy driver). **Met.**

## Delta-map review entries

84 entries (sample `port-R5-sound`, notes `[review: M2-R5 sound]`) in `ghidra/delta_map_review.json`, merged with
`python3 -I tools/delta_map.py merge --done m2r5:sound` into `ghidra/delta_map.json` and the report
`docs/m2-r1-function-deltas.md`: 62 MODIFIED, 21 IDENTICAL (compared with r6), 1 REWRITTEN (73 new, 11 updated) [audit: 72 new, 12 updated (the
review file at af90827 vs now; reviewed 2,005 + 72 = 2,077)].
Counts: IDENTICAL 3040 -> 3016, MODIFIED 1303 -> 1327, REWRITTEN 77, reviewed 2005 -> 2077 (4426 targets, 6
unresolved). [audit: the audit added 4 entries for edited functions that had none from this port: pingAttackPingsCreate
0x004e9350 and tutHideText 0x00530ef0 (MODIFIED, sample `audit-R5-sound`), soundbankadd 0x00580d40 (IDENTICAL,
compared) and a note on SNDgetpatch 0x00581a90 (IDENTICAL, its reread-R2 sample kept): IDENTICAL 3,014, MODIFIED
1,329, reviewed 2,079.]

### fqcodec

| exe function | address | class | conf. | what the port follows |
|---|---|---|---|---|
| idct | 0x005556e0 | IDENTICAL | high | compared with r6: as HW1 (idct read against r6: prerotation with 0.5f (0x0072a4c8), factor 8.0f / sqrt(size) (0x0072a558), fft, postrotation and reorder as GoK's reconstruction) |
| Initdct | 0x005558d0 | MODIFIED | low | step = 1.0f/(float)len in float, then (i + 0.125) * step * 2PI (float 0x0072a594) at full precision (GoK rounded the angle to a float) |
| fqDecOver | 0x00558800 | MODIFIED | medium | copies the whole FQ_DSIZE (0x200) block from aTSBlock to aTPBlock whatever nSize is (GoK copied nSize floats) |
| fqDecBlock | 0x005588c0 | MODIFIED | low | the init's window angles are i * the float constants PI/128, PI/256, PI/512 (0x0072bdd8/d4/d0) at full precision, and an Initdct failure fails the init (returns -1); the decode branches are GoK's |
| fqWriteTBlock | 0x00558bc0 | MODIFIED | high | no soft limiter (GoK's compress() is not in the exe): each sample is Relic's rint (fChopT + 1.5*2^52, 0x0072a458) clamped to [-0x8000, 0x7fff]; one source index runs across both output buffers (GoK restarted it at 0 for the second); returns -1 past 0x200 source samples |
| fqMix | 0x00558dc0 | MODIFIED | high | returns only for fLev == 0 (negative levels mix in; GoK: fLev > 0), a plain add at fLev == 1.0 |
| fqPitchShift | 0x00558e50 | MODIFIED | low | the position is kept at x87 precision for the compare, the truncation and the interpolation; the work buffer is FQ_SIZE floats |
| fqEqualize | 0x00558f20 | MODIFIED | medium | a 0.0 band is zeroed only up to min(band end, nBSize) (GoK zeroed the whole band) |
| fqInitDequant | 0x00558fd0 | MODIFIED | low | the mask factor is a float division 1.0f / (float)(2^(i+1) - 1) before the multiply (GoK: a double division) |
| fqDequantBlock | 0x00559030 | MODIFIED | high | follows the binary where GoK's reconstruction differs: the band and spectrum counters and the spectrum step are bytes (wrap at 256), the band loop also stops when the band index passes 25, both spectrum loops stop at an index above 127; the bit-field load is unaligned |
| fqSUnpack | 0x00559470 | MODIFIED | medium | the exe's unsigned bit-field extraction with x86 shift masks (& 31): GoK's signed shift (val << (33 - bits)) overflowed for bits 2 and is undefined for bits 1 |
| transTableSetup | 0x0056c290 | IDENTICAL | high | compared with r6: as HW1 (transTableSetup read against r6: the 'Prime factor of FFT length too large' printf and exit(1) are there (string 0x008ba70a; the automatic 'drops strings' is the decompiler's argument display)) |
| twiddleTransf | 0x0056cc70 | IDENTICAL | medium | compared with r6: as HW1 (twiddleTransf read against r6: the same omega/twiddle recurrence and the radix 2/3/4/5/8/10/odd switch with the file's constants) |
| fft | 0x0056d3f0 | IDENTICAL | high | compared with r6: as HW1 (fft read against r6: pi = 0x40490fdb, transTableSetup, permute, twiddleTransf per factor) |

### mixer

| exe function | address | class | conf. | what the port follows |
|---|---|---|---|---|
| isoundmixerinit | 0x0057f680 | MODIFIED | medium | the device part is DirectSound / waveOut (cat_smixPrepareWaveOut); the codec init (fqInitDequant, fqRand(ranRandomFn, 2), fqSqrt, fqSize, two fqDecBlock inits) as the tree |
| isoundmixerprocess | 0x0057f850 | MODIFIED | high | no effect, q-noise, delay or acoustic-model stage and no effect scale; volume clamps and the SFX stop test rounded with rint; a channel without a patch is released; the decode inlined |
| isoundmixerthreadWAVEOUT | 0x005804f0 | MODIFIED | low | the Win32 waveOut mixer thread |
| soundinit | 0x00580900 | MODIFIED | low | takes one argument; DirectSound / waveOut start-up and cat_isoundmixerStartThread; clears the channels and the master EQ |
| soundreinit | 0x00580a00 | MODIFIED | low | DirectSound re-init and cat_smixPrepareWaveOut |
| soundrestore | 0x00580b40 | MODIFIED | low | stop all with fade 1.0, wait for the mixer, release DirectSound |
| soundpause | 0x00580c40 | MODIFIED | low | stop all with fade 1.0 and wait while the mixer status is above 1 |
| soundstop | 0x00580e50 | IDENTICAL | high | compared with r6: as HW1 (read against r6) |
| soundvolumeF | 0x00580f60 | MODIFIED | medium | the 'already at this level' test rounds the current volume with rint (HW1 truncated) |
| soundpanF | 0x00581090 | IDENTICAL | high | compared with r6: as HW1 (read against r6: the pan fade clamp to +-180 and the +-1 minimum) |
| SNDgetchannel | 0x00581490 | IDENTICAL | high | compared with r6: as HW1 (read against r6) |
| splayFPRVL | 0x00581720 | IDENTICAL | medium | compared with r6: as HW1 (read against r6: SNDgetpatch (0x38-byte patches), SNDcreatehandle, SNDresetchannel, SNDcalcvolpan and soundfrequency inlined; the automatic 'drops calls' is that inlining) |
| soundstreaminit | 0x00581da0 | MODIFIED | high | no streamStartThread (speechEventInit starts it) |
| soundstreamopenfile | 0x00581e90 | MODIFIED | high | 'Cataclysm CD not detected.' |
| soundstreamcreatebuffer | 0x00581f20 | MODIFIED | high | no fqAcModel reset of the delay buffers |
| soundstreamvolume | 0x00582420 | MODIFIED | medium | the 'already at this level' test rounds the current volume with rint (HW1 truncated) |
| ssSubtitleRead | 0x00582610 | MODIFIED | high | Cataclysm's actor bits pick the subtitle actor (0x1000 -> 0, 0x2000 -> 1, 0x4000 -> 11, 0x20000 -> 10, 0x10000 -> 8, 0x40000 -> 5, 0x80000 -> 6, 0x200000 -> 12, 0x8000 by mission, else 9; animatic -> 9) |

### soundevent

| exe function | address | class | conf. | what the port follows |
|---|---|---|---|---|
| soundEventSFXMasterVol | 0x0051c2c0 | IDENTICAL | high | compared with r6: as HW1 (read against r6: SEsetVol clamp and volNIS = 1.0 inlined) |
| soundEventSpeechMasterVol | 0x0051c3a0 | IDENTICAL | high | compared with r6: as HW1 (read against r6: SEsetVol clamp and volNIS = 1.0 inlined) |
| soundEventMusicMasterVol | 0x0051c460 | IDENTICAL | high | compared with r6: as HW1 (read against r6: SEsetVol clamp and volNIS = 1.0 inlined) |
| soundEventInit | 0x0051c6e0 | MODIFIED | high | takes the race (speech through cat_speechEventInitForRace); ends with cat_madAnimSoundTableInit; the music factors, LUT loads, cardioid table and handles as HW1 |
| soundEventClose | 0x0051c9a0 | MODIFIED | high | also frees GunSpecEvents.lut |
| soundEventRestart | 0x0051caf0 | MODIFIED | medium | the front-end track is 0x11 (musicEventPlay(0x11)); soundreinit is DirectSound's (the SDL layer keeps soundpause(FALSE)) |
| soundEventStopSFX | 0x0051cb20 | IDENTICAL | high | compared with r6: as HW1 (read against r6: soundstopallSFX, then soundEventDerelictRemove / soundEventShipRemove inlined per object) |
| soundEventPause | 0x0051ccd0 | IDENTICAL | high | compared with r6: as HW1 (read against r6) |
| soundEventUpdate | 0x0051cd10 | MODIFIED | high | Cataclysm's object sounds: derelict ambients only updated; engine limits spare the mothership class and the Sentinels; idle engines fade over 5 s; upgraded engine loop 0x404; heading filter on every ship; per-type special loops (mimic, Leech, rammers, Workers, Sentinel field x3, big gun, cloakers, gravwell, SalCap, RKO, slipgates, crystals, Nomad moon); EMP-stun / Beast-infection damage loops; ambient by health / being built / converting; single-player hyperspace for the local player x3; bursts by the remembered gun x4; the leech speech alert |
| soundEventInitStruct | 0x0051f900 | MODIFIED | high | lastAmbient starts at -1 (HW1 ShipCmn_Ambient) |
| soundEventShipRemove | 0x0051fb00 | IDENTICAL | high | compared with r6: as HW1 (read against r6: SEstopengine and SEstopsoundhandle inlined) |
| SEequalize | 0x0051ff50 | MODIFIED | medium | the volume is rounded with Relic's rint (HW1 truncated) |
| SEloadbank | 0x005200c0 | MODIFIED | high | also loads GunSpecEvents.lut (after GunEvents.lut); the banks and the other LUTs as HW1 |
| getPanAngle | 0x005206f0 | MODIFIED | medium | the angle is rounded with Relic's rint (HW1 truncated) |
| SEgetAngleToCamera | 0x005208e0 | MODIFIED | medium | the angle is rounded with Relic's rint (HW1 truncated) |
| soundEventPlay | 0x005209f0 | MODIFIED | high | Cataclysm's player: no out-of-view exception; the 92-row LUT layout; UI sounds per race and 0x1c8 -> ship 0x821; gun sounds by cat_gunGetSoundType with the Beast weapon on row 9 and GunSpecEvents.lut for the multibeam frigates, dAcolyte, Dreadnought and destroyers; the burst remembers its gun index; ship events without a range test (Nomad moon nearer in an NIS); explosions 0x1002/4/7 add the 0x100d boom |

### speech

| exe function | address | class | conf. | what the port follows |
|---|---|---|---|---|
| batRandomWeightSetCB | 0x004232c0 | MODIFIED | high | stores the raw script value as the double at +0x08 ((double)(float)value; HW1 scaled it to a udword by BAT_RandomTotal / PNG_TaskPeriod) |
| batMaxDistanceSetCB | 0x00423330 | MODIFIED | high | stores the square of the distance ((float)v * (float)v); battleChatterAttempt compares it with the squared camera distance |
| batDistanceExponentSetCB | 0x004233b0 | IDENTICAL | high | compared with r6: as HW1 (read against r6: the raw double; the automatic 'adds strings' is the inlined battleChatterEventFromName table scan) |
| batMinWavelengthSetCB | 0x00423420 | IDENTICAL | high | compared with r6: as HW1 (read against r6: the raw double (inlined name lookup)) |
| batMinRepeatProximitySetCB | 0x00423500 | MODIFIED | high | stores the square of the proximity ((float)v * (float)v) |
| batRepeatProximityExponentSetCB | 0x00423580 | IDENTICAL | high | compared with r6: as HW1 (read against r6: the raw double (inlined name lookup)) |
| batGlobalFrequencyModifierSet | 0x00423710 | MODIFIED | high | stores the scanned float itself in batGlobalFrequencyModifier (a float at 0x0075074c, 128.0 at start; HW1: (udword)(value * 256)) |
| battleChatterStartup | 0x00423740 | IDENTICAL | high | compared with r6: as HW1 (read against r6: scriptSet(NULL, BattleChatter.script, the 21-row table at 0x00750758)) |
| battleChatterAttempt | 0x00423790 | MODIFIED | high | local player's ships only; one random draw against raw weight x the float frequency scale x 32768 and the squared camera distance against the squared maxDistance (HW1's distance curve and second draw gone); event 0x100080c queued without the ship |
| battleCanChatterAtThisTime | 0x004238b0 | MODIFIED | high | the proximity test compares the squared distance with the squared proximity and takes squared distance / proximity as the curve input (HW1: distance / proximity); the wavelength ratio in float [audit: the squared distance is summed in double from unrounded float differences (flds / fsubs / fstl at 0x00423969..0x004239af); the port rounded the differences to float first (vecSub); fixed in battleRepeatOK] |
| battleChatterFleetAttempt | 0x00423a50 | MODIFIED | high | status-update events spaced (30 s after any, 60 s after the same; last status time 0x00750750, event 0x008df9f4); raw weight x frequency scale; a status-suppressed event is not said in single player |
| battleShipOnMyTail | 0x00423ce0 | IDENTICAL | high | compared with r6: as HW1 (read against r6: the same two transposed-coordsys bogey tests with batBogeyAspect) |
| battlePingEvaluateNoEnemies | 0x00423f80 | MODIFIED | high | no mothership-class ship talks; friendly / allied fire reported by the fleet (group events by hot-key number); victory is the fleet's BCE_BattleWon then the group's; nothing in mission 13 |
| battlePingEvaluate | 0x00424110 | MODIFIED | high | runs at most every 8 s per ping (new battleping +0x28) and only with a non-exempt local ship; battle assessments are the fleet's lines with ship answers carrying the speaker's type; retreat marks Ship.catRetreating; damaged-capital report by the fleet or the PrinceOfSpace line 0x200135e; loss-dependent weights fixed to the standing-value coefficient |
| battleChatterFrequencySet | 0x00424d80 | MODIFIED | high | frequency modifier is now a plain scale of freq100 (freq100 x 0.01) |
| battleShipDyingWithTimeToScream | 0x00424da0 | MODIFIED | high | heavy cruiser / carrier / destroyer / frigate / resource classes: BCE_CapitalShipDies then the fleet's report (or only 0x100080c when the nearest ship is 5000+ away in the main screen); corvette / fighter / micro: the same line, then BCE_WingmanDies; exempt ships silent |
| battleLaunchWelcomeExchange | 0x00424ef0 | REWRITTEN | high | takes (shipclass, variable): the fleet's launch / welcome lines, else the build report 0x200103d; called by cat_cmBuildJobFinished |
| speechEventInit | 0x00521a40 | MODIFIED | high | 3 speech streams at the LUT's bitrate + 1 music stream at 0x200; Music4Cat.wxh / .wxd; the 62-entry music track table (0x00c09f00); the two EQ scripts; 3 queues; ends with streamStartThread (HW1: 5 + 2 streams, effect/delay scripts, the LUT load) |
| speechEventClose | 0x00521e30 | MODIFIED | high | frees 8 buffers: music buffer and header, stream structures, 3 speech buffers, the two LUTs |
| speechEventUpdate | 0x00521ea0 | MODIFIED | high | speechQueueUpdate then cat_speechAlertsUpdate (no soundupdate) |
| speechQueueUpdate | 0x00521eb0 | MODIFIED | high | timeover cleared when a line ends and set to start + timeout when one starts; cleared slots get event -1; the start test with the Bink and tutorial terms; the Eegah Easter egg line 0x20012d1 after 540 s |
| speechPlayQueue | 0x005220e0 | MODIFIED | high | plain speech: volume x volSpeech x 255, single-player ratio or the pilots' 0.75, letterbox ratio; the intel EQ; no effects, delays, radio beeps or background patch; truncated volume for the linked phrases, rint for the last |
| SEselectsentence | 0x005223c0 | MODIFIED | medium | no N01 mid / background chatter rotations: always the random series |
| speechEventQueue | 0x00522560 | MODIFIED | high | Cataclysm's ids (index 0x7ff up to 0x35e, actors 0x3ff800); three queues by actor bits; letterbox rejects chatter/status; enemy mimics that look allied answer the selection with their apparent type; exempt / building ships silent; 25 Relic names (alliance 0x400102b/c -> 0x200102e/f, death 0x200102d); actor 0; variable capped at 60; links to a playing event |
| speechEventActorStop | 0x00522a00 | MODIFIED | high | all three queues (HW1 the first only); cleared slots get event -1 |
| speechEventStopAllSpecific | 0x00522a70 | MODIFIED | medium | cleared slots get event -1 |
| speechEventRemoveShip | 0x00522b00 | MODIFIED | medium | cleared slots get event -1; the playing line fades with -1.0 (HW1 100) |
| SEreorderqueue | 0x00522e60 | MODIFIED | high | SEcleanqueue inlined; order and drop by the top four type bits (0xf0000000): timed-out tutorial lines are dropped too; cleared slots get event -1 |
| speechEventCleanup | 0x00522fb0 | IDENTICAL | high | compared with r6: as HW1 (read against r6) |
| speechEventAttack | 0x00523040 | MODIFIED | high | 0x4000807 for a forced attack with own ships, 0x4000808 resources only, 0x4000805 otherwise, with the leader's type; no random refusal, no per-resource lines |
| speechEventUnderAttack | 0x00523190 | MODIFIED | high | mothership / component class (not the cannon or carrier module) warn 0x2001058; off screen carrier class and heavy cruisers 0x2001066, groups 0x200105b; on screen carrier class, Workers, heavy cruisers below half health 0x100080b; exempt ships silent |
| musicEventPlay | 0x00523480 | MODIFIED | high | one music stream; track 0x44e = random of 17; a playing track fades 0.5 s and the next starts from the update (RESTART); track 0x29 by the NIS volume |
| musicEventStop | 0x005235c0 | MODIFIED | high | one stream; an NIS-type track (table type 4) also stops the NIS speech; no return to the level track |
| musicEventUpdateVolume | 0x00523660 | MODIFIED | high | the single stream's state machine; main-screen level by nearest ship (tutorial 0x12, fixed from 0x19), letterbox x0.7, sensors / managers levels (0x28 its own); the restart after a finished track sets no garbage volume (CAT-FIX of the exe's uninitialised argument); random pilot lines over track 0x28 |
| musicEventCurrentTrack | 0x00523cf0 | IDENTICAL | high | compared with r6: as HW1 (read against r6 (the single stream's track) (was: IDENTICAL)) |
| subTitleAdd | 0x00584580 | MODIFIED | high | a new subtitle resets the time compression (cat_taskTimeCompressionResetAndPrint) |
| subTitlesDraw | 0x00584620 | MODIFIED | high | Cataclysm's subtitle drawing: a translucent backing rectangle with additive blending, cat_fontPrintSubtitle, cat_ioSubtitleNotify, no cards while the construction manager is open (cat_cmIsOpen), cat_tutButtonRowSet |

Cat-new functions without a delta-map row (Cataclysm-only, not HW1 targets), all with their address at the body:
cat_speechEventLoadRaceVoice 0x005217f0, cat_speechEventInitForRace 0x00521d50, cat_speechAlertsReset 0x00522ba0,
cat_speechAlertsNoteShip 0x00522bc0, cat_speechAlertsUpdate 0x00522ce0, cat_speechEasterEggCheck 0x00522dc0,
cat_soundEventLetterboxVolume 0x00521770, cat_madAnimSoundTableInit 0x004b64c0, cat_soundEventBlast 0x0051fc60,
cat_soundEventAsteroidBreak 0x005215d0, cat_soundLevelReset 0x0051c980, cat_battleChatterReset 0x00423760,
cat_batStatusUpdateSetCB 0x004235f0, cat_batStatusSuppressSetCB 0x00423680.

## CAT-TODOs closed / re-targeted

Closed (now calls): Blast.c cat_soundEventBlast; ResCollect.c cat_soundEventAsteroidBreak; UnivUpdate.c the
mine-field chatter; CommandLayer.c the speech/sound ids of clRUTransfer, clHaltThese, clHalt and
cat_clHarvestNearestResource (the "tables that resolve them" now exist); CommandWrap.c the dock lines; ShipSelect.c
the provisional alerts stub; Tutor.c Fleet Command's actor bit. No `CAT-TODO(M2-R5 sound)` is left in `src/`.

Re-targeted: CommandLayer.c clCommandMessage "no resources" -> `CAT-TODO(M2-R5 hud)`; Subtitle.c subTitlesDraw ->
`CAT-TODO(M2-R5 hud)`; SoundEvent.c cat_ccIsPilotingShip 0x0042c2e0 / cat_ccIsPilotingThisShip 0x0042c310 ->
`CAT-TODO(M2-R6 camera)`; madAnimationUpdate's call of cat_madAnimSoundEvent -> `CAT-TODO(M2-R6 mesh animation)`;
speechQueueUpdate's Bink term -> `CAT-TODO(M2-R6 front end: bink.c)`; the Options voice preview ->
`CAT-TODO(M2-R6 front end: Options.c)`; the music next/previous keys -> `CAT-TODO(M2-R6 main region)`.

## What is left (owner)

- Win32 device layer, not applicable to the SDL port (SDL audio replaces it): cat_smixPrepareWaveOut,
  cat_isoundmixerStartThread, isoundmixerthreadWAVEOUT 0x005804f0, the DirectSound parts of soundinit 0x00580900 /
  soundreinit 0x00580a00 / soundrestore 0x00580b40 / soundpause 0x00580c40, cat_glide*. Reviewed MODIFIED low.
- cat_ioMessageAdd's UI sound 0x12a, clCommandMessage, subTitlesDraw: M2-R5 hud.
- The letterbox callers: kasfWideScreenIn 0x00497420, cat_kasWideScreenInHyperspace 0x004974d0,
  kasfWideScreenInDontCloseSensors 0x00497580, kasfWideScreenOut 0x00497630 and singlePlayerGameUpdate 0x005197d0 call
  cat_soundEventLetterboxVolume; the tree's KAS bodies are still HW1's: the KAS / single-player port (M2-R6
  missions); until then only cat_soundLevelReset clears it.
- madAnimationUpdate calling cat_madAnimSoundEvent: M2-R6 mesh animation.
- cat_ccIsPilotingShip / ThisShip (stubs returning FALSE): M2-R6 camera.
- Bink term in speechQueueUpdate, Options voice preview: M2-R6 front end.
- musicEventNextTrack / PrevTrack keys: M2-R6 main region.
- battleLaunchWelcomeExchange's caller cat_cmBuildJobFinished: M2-R6 construction manager.
- About 90 HW1-named speech call sites stay muted (HW1_SPEECH_ID) until each port passes the exe's id: their owners
  (AI, KAS/missions, tutorial, HUD).
- Uncompared bodies still marked IDENTICAL by the auto-classifier: Subtitle.c (about 16), sstream.c
  soundstreamfading / queuePatch / getvol / isoundstreamreadheader, soundlow.c soundPanic / soundclose /
  soundstopallSFX / soundbankadd [audit: compared with r6 by the audit: IDENTICAL] / soundover / soundfrequency / soundequalize / SNDreleasebuffer / SNDcalcvolpan,
  smixer.c isoundmixerrestore / queueWAVEOUT: a later sound review pass (M2-R6 or the audit).

## Open questions

- Names chosen without an exe string: `CAT_SPEECH_ACTOR_FLEET_INTEL` for actor 0x2000 (second voice of stream 1,
  subtitle actor 1), the game-stats track 0x28, `catBatLastStatus*`.
- HW1-id muting: the 0x400 marker on HW1 actor flags silences every HW1-named speech call; acceptable until the
  ports pass exe ids, or should the call sites be converted in bulk now?
- CAT-FIX deviations: (1) musicEventUpdateVolume's restart after a finished track sets the stream volume to an
  uninitialised value in the exe (its own return address word); the port keeps the restarted track at its start level; (2) cat_soundEventBlast and (3)
  cat_soundEventAsteroidBreak test enableSFX because the exe reads the ship LUT, NULL when SFX are off.
- The 4 chatter events with id 0 (as in the exe) never speak: confirm intended.
