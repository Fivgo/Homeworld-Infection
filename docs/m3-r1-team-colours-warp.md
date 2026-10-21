<!-- done: a-repro, b-fix, c-selftest, d-ppm, e-streams, f-review -->
# M3-R1 producer 2: the player's team colours survive the warp (team-colours-warp)

Subsystems: (a) repro: the headless reproduction, the dump and the cause; (b) fix: the CAT-FIX in trRegistryRefresh;
(c) selftest: `/catTeamColourTest` + harness `--teamcolour` / `--player-colours`; (d) ppm: the capture fence;
(e) streams: the guard rails, fe, sp and the heap count; (f) review: delta-map rows, docs, play.sh.

Base 94599c8 (producer 1's audited tree). Work so far: `git diff 94599c8` in the m2 worktree. `<R>` below is
`original/scratch-m2/m2r2-harness/runs`.

All six subsystems are finished.

## Headline

- **The cause**: not the registry and not the colour scheme. At the level change the player's colours are kept
  (levelPreInit restores teColorSchemes[0]; the dump shows the user's ff1bca82/ffff5539 at palette 0 of every Kuun-Lan
  texture, before and after the switch, the Kuun-Lan on scheme 0). Mission01 needs more texture memory than its
  opening NIS's level, so Cataclysm's texture fitting shrinks the textures already loaded and reloads them; the reload
  builds each paletted texture's palette with its mesh-sort list's team flags, and for a list of textures that were
  already loaded those flags are a stale local of trRegistryRefresh (the exe's too): no team bit, so the Kuun-Lan, its
  modules, the Worker and the Recon came back in their textures' own colours. [audit: not the exe's: the exe's
  measurement loop skips every entry that is not pending (flags & 1 tested at 0x00587d24, je 0x00588246, past the
  trMeshSortAdd call at 0x0058823e), so a loaded texture is never listed, never shrunk and never reloaded at a level
  change; the tree kept HW1's loop, which also listed the loaded textures. See "The audit's correction" below.] The user's "warp" is the jump from the
  opening cutscene's level into Mission01 (their log never reached Mission02).
- **The fix**: one line, a CAT-FIX in trRegistryRefresh (a loaded entry passes its own team flags). [audit: replaced
  by the exe's behaviour, a CAT port: trRegistryRefresh lists only pending entries (texreg.c); no CAT-FIX remains.] The whole
  campaign chain (sp, 17 levels, Mission01_NIS01 .. Mission12_NIS07) passes the team-colour check at every level with
  the fix and fails at every level from Mission01 to Mission11 without it.
- **No stream moved**: the headless Mission01 pair, the missiontest pair, Mission02 / 03 / 04 / 09, the loadgame pair
  are IDENTICAL in every field (rng_all included); the motion / nis dumps equal producer 1's audited ones.

## (a) The reproduction and the cause

- The user's log (`original/play-home/logs/play-20261020-200149.log`) never reached Mission02: New Game, the opening
  NIS's staging level Mission01_NIS01 (its ship statics: sRecon .. sMothershipOreCanArmour), then Mission01's load (the
  second batch: sAcolyte, the Kushan and Taiidan ships), then quit. The "warp into the next scene" is the jump from the
  opening NIS's staging level into Mission01: the level after an NIS level.
- Headless reproduction: missiontest Mission01 (NIS-first start) with the user's colours as the picker leaves them
  (Cataclysm.cfg `PlayerBaseColor -14955902` (0xff1bca82), `PlayerStripeColor -43719` (0xffff5539), `ColorsPicked 1`,
  `PlayerRace 0`) against a contrasting picked scheme (0xff2020e0 / 0xff20e020), frames every 96 ticks
  (`m3r1-tcw-scan-user` / `-red`, ce4fab4 + producer 1 = 94599c8's code): in Mission01_NIS01 nearly every frame differs
  between the two schemes (5,502 .. 131,518 pixels); in Mission01 every frame from tick 0 to 2400 is byte-identical
  (0 pixels; 214 .. 717 at 2016 .. 2208: small craft). The Kuun-Lan's hull in Mission01 is drawn in the texture's own
  colours under either scheme.
- The dump (`/catTeamColourTest`, `m3r1-tcw-diag3-user` / `-red`, `m3r1-tcw-diag4`): the registry is right before and
  after the switch: sMothership's textures (e.g. `RACE_SECT/SMOTHERSHIP/RL0/LOD0/SMO_MOTHERSHIP_RIBS` idx 141) keep handle
  0x8d, palette 0, nPalettes 2, the colour info ff1bca82/ffff5539 at palette 0, use 6, not shared; the ships' colour
  scheme is 0; teColorSchemes[0] is the user's. But the PALETTE the texture is drawn with changed during the new level's
  trRegistryRefresh: at the "statics" point (after universeStaticInit's meshRecolorize) idx 141 is still the NIS level's
  palette (CRC 533174dd with the user's colours, 9d25decd with the red scheme); after trRegistryRefresh it is a new
  block with CRC bf3432db under BOTH schemes (GUN_UNDERSIDES idx 144: e3c073dd -> b9e139e6): the palette was rebuilt
  without the team colour.
- The cause (the code path, exe and tree alike [audit: the tree only; see below]): Mission01 has far more textures than its staging level, so
  `trCramRAMComputeAndScale` 0x00589a50 halves textures to fit Cataclysm's texture budget (6 MB at TextureDetail 50)
  and `trPreLoadedTextureScale` 0x00586bd0 makes the already-loaded ones it shrinks pending (the reload branch: the exe
  takes it on any hardware renderer and whenever trNoPalettes, which this engine always is). `trMeshSortListLoad`
  0x00587210 then rebuilds a paletted texture's palette with the team colour only if its mesh-sort list's teamFlags
  carry TRF_TeamColor0/1. Those flags are collected by `trMeshSortAdd` 0x00586190 from the flags trRegistryRefresh
  0x00587a50 passes for each allocated entry; for an entry that is still loaded (not pending) at that point the exe
  passes its local listing-flags word unchanged from the last PENDING texture's measurement (the push at 0x00588231
  before the call at 0x0058823e; only the pending branch writes that word), and the tree's `listFlags` likewise. The
  mothership's list (every member loaded) therefore got whatever team bits another texture had: none here, so every
  reloaded palette of the Kuun-Lan, its modules, the Worker and the Recon (240 textures) came back uncoloured. The
  Acolyte's textures (first registered in Mission01, pending) were right. An exe defect: whether the reload keeps the
  colours depends on an unrelated texture's flags.
  [audit: wrong about the exe. trRegistryRefresh 0x00587a50 at 0x00587d10 .. 0x00587d2a: `test al,0x4; je 0x588246;
  test al,0x1; je 0x588246` (the decompile: `if ((flags & 4) != 0 && (flags & 1) != 0) { measure ...; size list;
  trMeshSortAdd(index, flags); }`): a loaded entry never reaches the push at 0x00588231, so the word pushed is always
  the pending entry's own measurement. The exe's size-sort list (0x00a75fb8) therefore holds only pending entries,
  trCramRAMComputeAndScale never sees a loaded texture and its trPreLoadedTextureScale branch is never reached from a
  level change. The fault is the tree's HW1 loop ("only pending and pre-loaded textures get here"), which listed the
  loaded textures: the cram shrank the Kuun-Lan's (RIBS idx 141: 32x16 -> 16x16 in Mission01) and the reload built
  their palettes with the stale word. The dump evidence above stands; only the attribution to the exe is wrong.]

## (b) The fix

- `src/SDL/texreg.c` `trRegistryRefresh`: `trMeshSortAdd(index, trPending(index) ? listFlags : (udword)reg->flags)`,
  **CAT-FIX**: a texture still loaded from an earlier level gives its mesh-sort list its own team flags (set from its
  .LiF when it was loaded), as a pending one gives its listing's; the exe's behaviour (the stale local) is stated in the
  comment with its addresses. Nothing else in the load path changes; the registry, the handles and the colour infos
  were already right (GoK's / R10's code there matches the exe).
- [audit: replaced. The audit ported the exe's gate instead: `if (trAllocated(index) && trPending(index))` around the
  measurement, the size-sort list and trMeshSortAdd (CAT comment with 0x00587d1c / 0x00587d24 / 0x00588246 /
  0x0058823e), trMeshSortAdd(index, listFlags) as before. A loaded texture keeps its size and palettes into the next
  level (idx 141 stays 32x16 with palette CRC 533174dd in Mission01); 473 fewer file loads in the Mission01 +
  Mission02 run (4,345 -> 3,872, the reloads gone). Evidence: docs/audits/m3-r1-team-colours-warp.md.]

## (c) The self-test

- `src/Game/TeamColourTest.c` / `.h` (new, HWI-TEST; meson.build): `/catTeamColourTest` (src/SDL/main.c). Read only.
  Dumps ("teamcolour: dump before|statics|after": singlePlayerLoadNewLevel's start, after universeStaticInit, after
  trRegistryRefresh: the schemes, player 0's Somtaaw types with their colour schemes, each level-0 texture's registry
  entry for scheme 0 and its palette: block, usage count, palette CRC, the team flags it was built with and those its
  list has, the no-palette entry) and checks ("teamcolour: check after|level|end": every loaded Somtaaw static player 0
  can own, each textured material's scheme-0 handle: loaded, nPalettes > 0, the palette index the one holding the
  player's colours (trColorsEqual), the colour info there the player's base / stripe, a paletted texture's palette
  built with its mesh-sort list's team flags; the Kuun-Lan on scheme 0 and its textures checked; in a new game the
  scheme the picker's), "teamcolour: fail ..." per fault, "teamcolour: heap level=.. dupe=N" after each level's textures
  (debug builds: the memory analysis trRegistryRefresh writes), "teamcolour: done checks=N failed=F ... result=".
- `src/SDL/texreg.c`: `catTrPaletteTeamFlags[]` (HWI-TEST) records the team flags each palette was built with;
  `trTextureRegistry`, `trNoPalRegistry`, `trColorInfosPointer` exported in texreg.h for the test.
- `src/SDL/render.c` (the frame hook next to catShotFrameEnd), `src/Game/SinglePlayer.c` (the three level-change hooks),
  `src/Game/MissionSelfTest.c` (the done hooks; with /catTeamColourTest the missiontest run goes on to the next MISSION
  level: Mission01 to Mission02, mission N past its next level's opening-NIS staging level, and ends 32 ticks after it
  loaded; the load-game test marks the colours as the save's).
- tools/m2_harness.sh: `--teamcolour`, `--player-colours BASE/STRIPE` (documented in the header; summary line
  "teamcolour: N checks, F FAIL ...").
- First results: `m3r1-tcw-st1` (fixed engine, `--teamcolour --player-colours ff1bca82/ffff5539`): 6 checks PASS
  (Mission01_NIS01 after / level, Mission01 after / level, Mission02 after / end; 513 textures in Mission01, 594 in
  Mission02, the Acolyte's 53 among them), missiontest 6/6 PASS. `m3r1-tcw-st1-nofix` (the same tree without the
  fix): 4 of 6 FAIL, 240 textures "palette built without its list's team colour" in Mission01 and Mission02.

## (d) The PPM fence (producer 1's `--shot`, `tools/ppm_region.py`)

Runs `m3r1-tcw-ppm-user` (the user's ff1bca82/ffff5539), `-red` (a contrasting picked scheme ff2020e0/ff20e020) and
`-nofix` (the user's colours on the same tree without the CAT-FIX), missiontest Mission01 NIS-first with
`--teamcolour --shot 900,1500,2016,2208,2400,0-2600/100 -- /catTeamColourTicks 2600` (the run goes on 2,600 ticks into
Mission02): 76 frames each, segment 0 = Mission01_NIS01, s1 = Mission01, s2 = Mission02 (`<R>/m3r1-tcw-ppm-*/shot*.ppm`).

- Pixels differing between the two schemes (fixed engine) / between the fixed and the no-fix engine (user's colours):
  Mission01_NIS01 (where the user saw the colours) every frame from tick 100: 5,502 .. 115,828 / **0** (the first load
  is right either way); Mission01 ticks 0 .. 1700 (the arrival NIS: no player ship in view) 0 / 0, from 1800 (the fleet
  arrived at 1939; the Kuun-Lan in view) 55,930 .. 61,123 / 55,878 .. 60,273; Mission02 from tick 100: 59,241 .. 62,881 /
  59,200 .. 62,203. Before the fix (`m3r1-tcw-scan-user` / `-red`, 94599c8's code) every Mission01 frame was identical
  under the two schemes.
- The region: the 24x24 window where the two schemes' frames differ in every pixel and the red-scheme frame is
  red-dominant (the base colour, not the stripe: a high team-base weight; `original/scratch-m2/m3r1-tcw/pick_region.py`), means by
  `ppm_region.py stats --region`:

| frame | region | user ff1bca82 (base R130 G202 B27) | red ff2020e0 (R224 G32 B32) | no fix, user's colours |
|---|---|---|---|---|
| Mission01_NIS01 tick 900 | 444,450,468,474 | 53.0, 71.2, 35.2 | 72.2, 37.0, 32.1 | 53.0, 71.2, 35.2 (same) |
| Mission01 tick 2016 (after the arrival) | 342,314,366,338 | 95.6, 122.6, 32.0 | 138.5, 48.7, 33.4 | 73.4, 70.1, 47.7 (= both schemes before the fix) |
| Mission02 tick 1000 | 518,418,542,442 | 116.3, 120.5, 58.2 | 153.4, 64.6, 59.9 | 119.1, 99.1, 92.0 |

  The region follows the base colour: green over red and blue with the user's scheme (G > R > B, as 202 > 130 > 27),
  red with the red one; without the fix it is the texture's own grey-brown under either scheme (the NIS level aside).
  Viewed side by side (`original/scratch-m2/m3r1-tcw/ppm3.png`: NIS01 900 and Mission02 1000, user / red / no fix; `fix1.png`): the Kuun-Lan green with blue chevrons / red with
  green chevrons / untinted.

## (e) Streams, dumps and gates (debug ASan build unless stated; `hwo_compare.py`, every field)

Engine: the fixed tree (`original/scratch-m2/m3r1-tcw/hwi-engine-fix-debug` / `-fix2-debug` / `-fix-release`, the
same source but for the test code's later additions); the no-fix comparison binaries are the same tree with the
CAT-FIX line reverted (`hwi-engine-nofix-debug`, `-nofix2-debug`).

| stream | run | against | result |
|---|---|---|---|
| headless Mission01, 6,000 ticks | `m3r1-tcw-mission-a` / `-b` (sha256 565e2e43c4117191, 1,476,370 B, 6,001 T; `cmp` byte-identical pair) | r10i-mission-a (5bd51234405ea0d8) | IDENTICAL, 6,001 ticks, every field, rng_all included |
| headless Mission01, release | `m3r1-tcw-mission-rel` (6e822afc839caaec) | r10i-mission-a | IDENTICAL |
| missiontest Mission01 (NIS-first) | `m3r1-tcw-missiontest-a` / `-b` | r10i-missiontest-a, m3r1-aud-lbf-missiontest-a | IDENTICAL (11,426 ticks); 6/6 PASS, done 11,424 |
| Mission02 self-test | `m3r1-tcw-m2` | m3r1-aud-lbf-m2, r10i-m2-a | IDENTICAL (25,905 ticks); 6/6 PASS, done 25,936 |
| Mission03 self-test | `m3r1-tcw-m3` | r10i-m3-a (producer 1's audit did not run it) | IDENTICAL (17,794 ticks); 5/5 PASS, done 17,825 |
| Mission04 self-test | `m3r1-tcw-m4` | r10i-m4-a | IDENTICAL (20,787 ticks); 5/5 PASS, done 20,818 |
| Mission09 self-test | `m3r1-tcw-m9` | r10i-m9-a | IDENTICAL (20,586 ticks); 6/6 PASS, done 20,617 |
| loadgame "01 - Hiigara", 3,600 ticks | `m3r1-tcw-loadgame-a` / `-b` | r10i-loadgame-a | IDENTICAL (ticks 1984-5586); PASS |
| team-colour runs | `m3r1-tcw-st1` (Mission01 + Mission02), `m3r1-tcw-m3-tc` (Mission03 + Mission04_NIS02 + Mission04), `m3r1-tcw-loadgame-tc` | the default runs above | IDENTICAL over the common ticks (`--to-tick`); the team-colour runs only run longer |

Final binary (the tree as handed over, build/debug): `m3r1-tcw-mission-final` IDENTICAL to r10i-mission-a (every
field, 6,001 ticks; sha256 565e2e43c4117191, 1,476,370 B, ASan 0) and `m3r1-tcw-st1-final` (missiontest Mission01 +
Mission02 with `--teamcolour --player-colours ff1bca82/ffff5539`): 6 checks PASS, 6/6 PASS, ASan 0.

No stream moved in any field; nothing to re-baseline. The texture reload builds the same textures from the same
data, only with the team colour in their palettes: no random draw, no simulation state.

Dumps (whole sha256/16 and with the ` crc=` column stripped), against producer 1's audit:

| dump | this round | m3r1-aud-lbf |
|---|---|---|
| motion /noInterp | `m3r1-tcw-motion-noi` 183681235a3fdc7b / 3016280e9e0dd3c8 | EQUAL |
| motion on | `m3r1-tcw-motion-on` 428fff6c765ab81b / 428ef41479eb5749 | EQUAL |
| nis /noInterp | `m3r1-tcw-nis-noi` 2a7fec4de8cfe716 / 196bab32d244f614 | EQUAL |
| nis on | `m3r1-tcw-nis-on` 46df1d391a86a404 / 7ce8472514254f4f | EQUAL |

(Both modes load one level: no reload, so no pixel moves.)

### The plain-harness runs

| label | mode | milestone | first stop | result |
|---|---|---|---|---|
| m3r1-tcw-fe | fe, 120 s | Main_game_screen | none: still running at the 120 s timeout (as R10) | ASan 0 |
| m3r1-tcw-sp-plain | sp, 1,200 s | universe running | none: still running at the 1,200 s timeout (as R10) | ASan 0, no unsuppressed leak, no fatal, 12 animatics skipped (Animatic01 .. 12), last screenshot golden-12-00128 (R10's r10i-sp: golden-12-00128) |
| m3r1-tcw-sp | sp, 1,200 s, `--teamcolour --player-colours ff1bca82/ffff5539` | universe running | none (timeout, as R10) | ASan 0; 34 checks, 0 FAIL: every level PASS (below) |
| m3r1-tcw-sp-nofix | the same, no-fix engine | universe running | none (timeout) | ASan 0; 34 checks, 30 FAIL, 6,276 fail lines |
| m3r1-tcw-mission-a / -b / -rel, missiontest-a / -b, m2, m3, m4, m9, loadgame-a / -b | as above | oracle mission run done / mission self-test done / save-load self-test done | none (rc=0) | ASan 0 in each |
| m3r1-tcw-st1, -st1-rel (release), -m3-tc, -loadgame-tc, -ppm-user / -red | missiontest / loadgame with `--teamcolour` | mission self-test done / save-load self-test done | none (rc=0) | teamcolour PASS (6, 6, 6, 2, 7, 7 checks), ASan 0 |
| m3r1-tcw-st1-nofix, -ppm-nofix | no-fix engine | mission self-test done | none (rc=0) | teamcolour FAIL (4 of 6, 5 of 7 checks) |

The sp chain with the team-colour check, "after" check per level (textures checked, failed; fix / no fix):

| level | fix | no fix |
|---|---|---|
| Mission01_NIS01 | 248, 0 PASS | 248, 0 PASS |
| Mission01 | 513, 0 PASS | 513, 234 FAIL |
| Mission02 | 594, 0 PASS | 594, 234 FAIL |
| Mission03 | 601, 0 PASS | 601, 234 FAIL |
| Mission04_NIS02 | 311, 0 PASS | 311, 226 FAIL |
| Mission04 | 574, 0 PASS | 574, 214 FAIL |
| Mission05 | 610, 0 PASS | 610, 204 FAIL |
| Mission06_NIS04 / Mission06 | 348 / 741, 0 PASS | 204 / 204 FAIL |
| Mission07_NIS05 / Mission07 | 383 / 741, 0 PASS | 204 / 204 FAIL |
| Mission08 | 784, 0 PASS | 204 FAIL |
| Mission09 | 707, 0 PASS | 204 FAIL |
| Mission10_NIS06 / Mission10 | 194 / 822, 0 PASS | 180 / 194 FAIL |
| Mission11 | 1,005, 0 PASS | 194 FAIL |
| Mission12_NIS07 | 230, 0 PASS | 0 PASS |

### The heap (the M3 soak's input)

"teamcolour: heap" lines (the memory analysis each trRegistryRefresh writes in a debug build; allocated blocks named
"dupe trcolorinfo list"), the sp chain, identical with and without the fix (the fix allocates nothing):
Mission01_NIS01 0, Mission01 473, Mission02 625, Mission03 697, Mission04_NIS02 766, Mission04 1,143, Mission05 1,762
(06_NIS04 .. 08: 1,762), **Mission09 2,437** (R10: 2,438 in r10p2-c1-sp's analysis), Mission10_NIS06 2,437, Mission10
2,997, Mission11 2,997, Mission12_NIS07 3,308. Before the fix and after it: 2,437 at Mission09. The source is the R10
audit's O4, here located: every reload of a loaded texture (trPreLoadedTextureScale 0x00586bd0 at each level's texture
fitting, trTextureColorsUpdate 0x00585fa0, trRegisterRemoval 0x00585e70, and the tree's trRegisterAddition)
allocates a 64-byte copy of the colour infos, calls trInternalTexturesDelete (which stores its own copy in +0x14) and
then overwrites +0x14 with the first copy: one 64-byte block lost per reload (156 KB by Mission09), exe and tree
alike. Not changed here (owner: the M3 soak, below).
[audit: with the exe's pending gate (the audit's replacement of the CAT-FIX) the cram no longer reloads loaded textures,
so the counts fall (m3r1-aud-tcw-sp-tc, the same sp chain, 34 checks 0 FAIL): Mission01_NIS01 0, Mission01 0, Mission02
152, Mission03 224, Mission04_NIS02 / Mission04 293, Mission05 .. Mission08 912, **Mission09 1,587**, Mission10_NIS06 ..
Mission11 1,587, Mission12_NIS07 1,898. trPreLoadedTextureScale is no longer reached in a level change (as in the exe);
the remaining blocks are trTextureColorsUpdate / trRegisterRemoval / trRegisterAddition reloads (O4).]

## (f) Review rows, docs, tags

- ghidra/delta_map_review.json (`[review: M3-R1 team-colours-warp]`, sample `port-M3-R1-team-colours-warp` for the new
  rows): notes on 0x00587a50 trRegistryRefresh (MODIFIED; the stale flags word, the tree's CAT-FIX), 0x00586190
  trMeshSortAdd (IDENTICAL, compared), 0x00587210 trMeshSortListLoad (new row, MODIFIED: the palette coloured iff the
  list's team flags; the RGB branch by the entry's own), 0x00586bd0 trPreLoadedTextureScale (new row, IDENTICAL: HW1's
  two branches; GoK keeps only the reload branch, the one the exe takes without paletted GL; the leak), 0x00589a50
  trCramRAMComputeAndScale (new row, MODIFIED, compared: as the tree), 0x004a3230 levelPreInit (the scheme
  save / restore, as the tree), 0x004af3a0 meshRecolorize, 0x0051ac80 singlePlayerLoadNewLevel (the order of the level
  change), 0x00585fa0 trTextureColorsUpdate, 0x00585650 trColorInfosPointer, 0x004ae910 meshTextureRegisterAllPlayers
  (GoK's utyShipsAlwaysUseOwnerColors condition is not in the exe; the option is FALSE by default and in the user's
  file: noted, not changed). `python3 -I tools/delta_map.py merge --done m3r1:team-colours-warp`: records changed 11;
  IDENTICAL 2,762, MODIFIED 1,575, REWRITTEN 83 (unchanged); `report` rewritten (docs/m2-r1-function-deltas.md).
- Tags: `CAT-FIX` 1 (texreg.c trRegistryRefresh) [audit: now `CAT` 1, `CAT-FIX` 0: the pending gate in
  trRegistryRefresh replaces it]; `HWI-TEST` the test code (TeamColourTest.c / .h, the texreg.c
  record and exports, the hooks in SinglePlayer.c, render.c, MissionSelfTest.c, the switches in main.c). No CAT, no
  HWI-FEATURE (no setting: the exe's behaviour fixed, nothing to opt into), no HWI-FIX.
- CAT-TODOs: none closed, none added, none re-targeted; `/bin/grep -rn "M3-R1" src | /bin/grep -c "CAT-TODO\|CAT-PROVISIONAL"`
  = 0; the 23 stage-5 sites untouched (35 CAT-TODO lines in src, as before).
- Builds: `tools/build.sh --debug` and `--release` green, 0 warnings under -Werror, after every subsystem and at the
  end; `tools/m2_layout32.sh` OK.
- The ninth-scheme warning (R10's trColorsIndexAlloc CAT-FIX): dbgWarningf is unconditional (Debug.c: dbgWarning ->
  dbgMessage, which the release build prints, as the user's log's "WARNING: Could not locate mex" lines show), so its
  absence from the user's log means it never fired; no run of this round logs it.

## Files changed (git diff 94599c8; new files untracked until the audit's checkpoint)

- src/SDL/texreg.c: the CAT-FIX in trRegistryRefresh [audit: replaced by the CAT pending gate]; HWI-TEST `catTrPaletteTeamFlags[]` recorded in
  trMeshSortListLoad (a new palette, and a palette shared from an earlier list member).
- src/SDL/texreg.h: HWI-TEST exports (trTextureRegistry, trNoPalRegistry, catTrPaletteTeamFlags, trColorInfosPointer).
- src/Game/TeamColourTest.c / .h (new), src/Game/meson.build: the self-test.
- src/Game/SinglePlayer.c: the three level-change hooks (singlePlayerLoadNewLevel).
- src/SDL/render.c: the frame hook next to catShotFrameEnd.
- src/SDL/main.c: `/catTeamColourTest`, `/catTeamColourTicks <N>`.
- src/Game/MissionSelfTest.c: the done hooks; with /catTeamColourTest the run goes on to the next mission level; the
  load-game test marks the colours as the save's.
- tools/m2_harness.sh: `--teamcolour`, `--player-colours BASE/STRIPE` (header; summary line). tools/play.sh: a note
  on the team colours and `-- /catTeamColourTest` for a play log that records them.
- ghidra/delta_map_review.json, ghidra/delta_map.json, docs/m2-r1-function-deltas.md (merge / report).

## New self-test, switches and harness flags

- `/catTeamColourTest` (HWI-TEST): the dump and the checks; "teamcolour: done checks=N failed=F afterLoads=N
  levels=<level>:<where>,... result=PASS|FAIL" at the mission self-test's or the load-game test's done line (the sp
  loop has no done line: its summary counts the check lines).
- `/catTeamColourTicks <N>` (HWI-TEST, with /catTeamColourTest in missiontest): the ticks run in the next mission level
  (default 32).
- Harness: `--teamcolour` (any single-player mode), `--player-colours BASE/STRIPE` (any mode; the colour words in hex as
  the teamcolour lines print them; appends PlayerBaseColor / PlayerStripeColor as signed decimals, ColorsPicked 1,
  PlayerRace 0 to the run's fresh home Cataclysm.cfg). Summary: "teamcolour: N checks, F FAIL, L fail lines; <done line>".

## THE USER'S CHECK (for the play-test brief)

1. `tools/play.sh` [audit: `tools/play.sh --rebuild` once after the integrate, so build/release carries the fix]
   (release; the user's play-home, whose Cataclysm.cfg already holds the user's colours: PlayerBaseColor
   -14955902, PlayerStripeColor -43719, ColorsPicked 1). Optionally `tools/play.sh -- /catTeamColourTest` so the log
   records "teamcolour: check ... result=PASS" lines at each level.
2. New Game. Watch the opening cutscene (the Kuun-Lan in the user's green with blue chevrons, as before), then the
   jump into Mission01: after the arrival cutscene, when the fleet comes out of hyperspace (about two minutes in), look
   at the Kuun-Lan: its hull must keep the same green and blue (the M2 build showed it grey-brown there). Check the
   Recons and Workers that launch from it too.
3. Play on into Mission02 (and, if time allows, Mission03 -> the cutscene level before Mission04 -> Mission04): the
   colours must stay on every ship of the player's fleet after each jump and each cutscene level.
4. Report: any level where the player's ships show other colours than the picked ones (the mission and roughly when),
   and the play log's name.

## For STATE.md

- Cause of play-test item 2: trRegistryRefresh 0x00587a50 passes trMeshSortAdd 0x00586190 a stale listing-flags word
  for already-loaded textures (exe and tree); trCramRAMComputeAndScale 0x00589a50 / trPreLoadedTextureScale 0x00586bd0
  reload them at the next level (6 MB texture budget at TextureDetail 50); trMeshSortListLoad 0x00587210 colours a
  paletted palette only with the list's team flags: the Kuun-Lan lost the colours from Mission01 (after its opening
  NIS's level) on. CAT-FIX: a loaded entry passes its own flags. No setting.
  [audit: corrected: the exe lists only PENDING entries (0x00587d24 je 0x00588246), so it never reloads a loaded
  texture at a level change; the tree's HW1 loop listed loaded ones too, the cram shrank them and the reload used the
  stale word. Ported as CAT: trRegistryRefresh lists only allocated + pending entries; no CAT-FIX. No setting.]
- HWI-TEST `/catTeamColourTest`, `/catTeamColourTicks N`; harness `--teamcolour`, `--player-colours BASE/STRIPE`;
  "teamcolour:" lines; texreg.c `catTrPaletteTeamFlags[]`.
- Heap input for the soak: 2,437 live "dupe trcolorinfo list" blocks at Mission09's load, with or without the fix;
  3,308 at Mission12_NIS07; the leak is O4 (one 64-byte list per texture reload). [audit: with the pending gate
  1,587 at Mission09 and 1,898 at Mission12_NIS07 (m3r1-aud-tcw-sp-tc).]
- No re-baseline: every stream and dump of this round equals R10's / producer 1's audited ones.

## What is left (owner)

- O4, the colour-info leak (one 64-byte block per texture reload: trPreLoadedTextureScale, trTextureColorsUpdate,
  trRegisterRemoval, the tree's trRegisterAddition overwrite the list trInternalTexturesDelete stored), exe alike: a
  CAT-FIX candidate for the M3 one-hour soak (owner: M3 soak), with the counts above as its baseline.
- The texture budget itself (Cataclysm's 1..12 MB cram limit, TextureDetail 50 = 6 MB, so every level shrinks the
  ships' textures [audit: only the level's NEW (pending) textures: loaded ones keep their size, as in the exe]): a quality question for producer 3's FULL DETAIL (item 3; CET's UnlimitedVideoMemory option lifts the
  limit). Not touched here.
- GoK's `utyShipsAlwaysUseOwnerColors` condition in meshTextureRegisterAllPlayers is not the exe's (owner: M3 play-test
  fixes, if the option row is ever exposed; harmless while FALSE).
