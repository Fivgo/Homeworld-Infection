# Audit: M2-R5 producer 3 "render" (object list, lens flares, fog layers, the ship renderers)

This audit checks `docs/m2-r5-render.md` (marker `<!-- done: objlist, flares-fog, ship-render, gl-files, final -->`)
on branch `m2`, base `84a9d05`. There are no commits after the base: the whole port is the working tree, 54 files,
+7,374 / -1,956 lines (most of `render.c`'s 4,690-line diff is functions moved, see section 3).

How the checks were run:
- Every engine run went through `tools/m2_harness.sh` or `tools/oracle/mission_test.sh` (the plain harness, silent,
  headless, offscreen driver), with the audit's clean debug binary (`--bin`).
- Ghidra was not run. The exe was read only as data: `tools/m2_fdec.py` (the `cataclysm_gog_r6` decompile),
  `tools/m2_fdis.py` (`objdump` of `.text`), `tools/m2_exedata.py` (constants and `.data` defaults).

## 1. Builds (from clean)

`rm -rf original/scratch-m2/build-audit-r5-render`, then `meson setup` with `tools/build.sh`'s arguments (debug:
`--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`) and
`meson compile`. Both **rc 0, 0 warnings under `-Werror`** (the only "WARNING" is meson's sanitizer notice).

| build | text | data | bss | file |
|---|---|---|---|---|
| debug | 43,944,623 | 25,453,376 | 2,855,392 | 81,273,336 B |
| release | 3,936,747 | 268,272 | 2,312,704 | 4,966,888 B |

After the audit's fixes: both audit builds rebuilt (rc 0, 0 warnings), and `tools/build.sh --debug` and `--release`
in the worktree (rc 0, 0 `warning:` lines). `tools/m2_layout32.sh`: **OK** (Ship 1,596, Missile 600, the 45 ship
specifics asserts).

## 2. Bodies compared with the exe (38)

Ghidra's `posinfo` labels are shifted by one vector in several bodies (its "velocity" is the position, "force" the
velocity); offsets were read from the raw `+0x..` accesses or the disassembly where it mattered.

| # | function (exe) | result |
|---|---|---|
| 1 | `cat_rndSetupDissolveFlag` 0x005759f0 | as the exe |
| 2 | `cat_rndSetupFadeFlag` 0x00575a20 | as the exe (2/3 = 0x0072be94, 1.0 = 0x0072a548 double, modf; build mode = 0x00878168 = 1 = `CAT_RND_FADE_CLIP`; 1 - Ship+0x3E8) |
| 3 | `cat_rndShipEMPdAndMoving` 0x00575ae0 | as the exe (0.01 = 0x0072a680) |
| 4 | `cat_kasNaggarokEMPHitShip` 0x00575b30 | a documented stub (FALSE, `CAT-TODO(stage 6 KAS)`): the exe reads the KAS variable `G_NaggarokEMP` against Ship+0x52C; no mission sets it while `CAT_HW1_MISSION_SCRIPTS` is 0 |
| 5 | `cat_rndSetupTimedEffectPhase` 0x00575b70 | as the exe (0.1 / 0.9 / 1.25 / 10 / 9.999998 / pi/2) |
| 6 | `cat_rndSetupMimicFlag` 0x00575c60 | as the exe (+0x18 bit 8, Ship+0x768) |
| 7 | `cat_rndRenderObjectList` 0x005776f0, head to the timed-effect pass | as the exe: the flag block (+0x4D0..+0x4D8, +0x2B4 race 1, type 0x6d, +0x4E4 with `nisIsRunning`), the deferral rules (Beast stun, fade, mimic with 0x00c32c98, timed phase, crystal, a slipgate-docked ship by its clamp host), LOD NULL -> selCircleRadius -1, the cloak test, ACV link lightning (4 / 0x26), Nomad 0x83, Dreadnought charge, the Sentinel field queue nodes {formation, Node, queued} at 0x00b378a0, the module spin, the scaleCap rescale, plane {0,0,-1,-1}, the ssinfo hyperspace test, the effect params (+0x75C, +0x70C, +0x710..+0x718, +0x760 == 1), fade channels 6 / 2 (0x008bab20 = 0.001) / 7 (dock states 0x10/0xd/0x11/0xe, Ship+0x620), cloak pass logic, the build clip plane (+0x80 * +0x3E8 + +0x68), the Beast overlay (1.01 scale), the dissolve plane |
| 8 | `cat_rndConversionBoxDraw` (inlined at 0x005786xx) | as the exe: the pulse fold (mod 0x24, 0x25 - n), phase 2 bands (0x10 + i * 0xe, bounce at 0x14 / 0), phase 3 bands from **random stream 0** (`ranRandomFn(0)`, as the exe), scale / tick / green per phase |
| 9 | `rndMainViewRenderFunction` 0x005764e0, the object passes | as the exe (disasm 0x00576b41..0x00576b90: ships with type > 0x8d or header +0x1c set are skipped, bullets (1) and effects (7) go in untested, the rest through `rndShipVisibleUsingCoordSys`; LOD budget loop over types 0/2/4/5/6/8, `opNoLODVal`, at most 11 passes; sort mode 1, the opaque pass, minor list, sort mode 2 with `cat_sentinelFieldsRenderBehind`, `RenderRest`, mode 0) |
| 10 | `lodLevelGet` 0x004a4d70 | as the exe (bands 0.4 / 0.12 / 0.045 / 0.017 / 0.0065 = 0x0072a674, 0x0072ab84..78; bOn +8, bOff +0x10; lodScaleFactor 0x00884868 = 0.6; the detail clamp; the time tail split into `cat_lodLevelTime`, documented in LOD.h) |
| 11 | `cat_lodLevelGetTimed` 0x004a5000 | as the exe; the pass table 0x0072be84 = 99,3,2,1,0,3,2,1,3,2,3 |
| 12 | `cat_lodTimingStart/Stop` 0x004a4cf0 / 0x004a4d20 | as the exe (SDL's counter for QueryPerformanceCounter) |
| 13 | `cat_rndLensFlareSourceVisibility` 0x0057bad0 | as the exe: 8 slots, the replacement loop, occlusion on mode 2 (slipgates 0x70..0x72 skipped, type 0x90 = `nisLensflareSource1` transparent, -1.0 = 0x0072a468), fade in 0.4 / out 0.6, 1e-9 (0x0072beb8 double), edge fade with 1.2 (0x008baba8), screen position rounded as the 1.5 * 2^52 trick (`lrint`) |
| 14 | `cat_rndLensFlaresRender` 0x0057bec0 | code as the exe (disasm jump table 0x0057caac: -1 six segments; -2 / -3 / -4 segments = radius * 40 * 4 (0x0072bec0, 0x0072a530) clamped 6..32; -3 rim * 0.25, -4 centre * 0.25; 1/256 = 0x0072a6b0; the "one shot" byte +0x12 -> mode 1). **The comments had -3 / -4 swapped** (finding F3) |
| 15 | `cat_rndLensFlareNISSourceAdd/Remove/SetPosition/Enable` 0x0057cac0..0x0057cb40 | as the exe (+0x10 enabled, +0x11 mode = (show == 0), +0x12, +0x18 fade, +0xC brightness 1.0, node at +0x2C) |
| 16 | `cat_lfElementSetCB` 0x0057b970, `cat_lfTextureSetCB` 0x0057ba00, `cat_lfTextureFileSetCB` 0x0057b820 | as the exe (disasm: sscanf argument slots radius/offset/texture/r/g/b; colour 0xff<<24 \| b<<16 \| g<<8 \| r with g and r unmasked, as the exe; 48 elements / 17 cells is the room before the next variable, the bound is a CAT-FIX) |
| 17 | `cat_scriptSetFogCB` 0x0057cb70 | as the exe (disasm 0x0057cbdc..0x0057ccba: speed -> 0x00c32c20, density * 1e7 (0x0072afa0 double) -> 0x00c32c24, colours b<<16 \| g<<8 \| r; texture `race_derelict\dplasmacloud\rl0\lod0\cloud`). The exe's clear of 0x008baac0 at the end is dropped: that dword has no other reference in the decompile |
| 18 | `cat_rndFogLayerOff` 0x0057cd20 / `cat_rndFogLayerFadeTo` 0x0057cd40 | as the exe |
| 19 | `cat_rndFogLayerUpdate` 0x0057ce00 | as the exe |
| 20 | `cat_rndFogLayerRender` 0x0057d2c0 | as the exe (disasm 0x0057d2fb..0x0057d331: s = density * 3.3333e-8, s > 1 -> 1, s == 0 -> return; the scroll speeds -0.0505 / 0.05, -0.015 / 0.005, tiles 0.2 / 0.25, wrap at 100; two quads a frame); the device-flag 0x200 test is GoK's backend, as documented |
| 21 | `trailDrawBillboardedSquareThingz` 0x005a5be0 | as the exe (disasm 0x005a5c44..0x005a5cb3: max speed * speed multiplier, velratio below 0x008c07e0 clamped to it) |
| 22 | `cat_sentinelFieldSegmentAdd` 0x005c5b20 | **wrong cap** (finding F1): the tree held 34 segments, the exe 79 on GLtype |
| 23 | `cat_sentinelFieldSegmentDraw` 0x005c60a0 | as the exe (edge 0.75 / mid 0.25 levels, the fill flash: random stream 0 below 0.5, 0x96ffffff to 0.8, (1 - x) * 5; hit flash over 10 s; vertex glow 4 * level / 0.65) |
| 24 | `cat_sentinelFieldsRenderBehind/Rest` 0x005c5c20 / 0x005c5cb0 | as the exe |
| 25 | `cat_dreadnoughtRenderCharge` 0x005b1110 + `cat_dreadnoughtChargeEffect` 0x00516a40 | as the exe (type 10, +0x1398 / +0x139c, cannon +0xE0; radius 1000 = 0x00893f80; effect `etgSpecialPurposeEffectTable[33]` = 0x00c41cc0 + 33 * 4; 80 segments, jitter 3) |
| 26 | `cat_nomadMoonRenderBeam` 0x005be120 + `cat_nomadMoonBeamEffect` 0x00516bb0 | as the exe (disasm: four arguments, the emitters +0x638 / +0x63c + 0x58, 1 - frac for the second, radius 4300) |
| 27 | `cat_droidRenderArray` 0x005c3cf0 | as the exe (LOD < 2, 0x2C entries, LOD 0 mesh) |
| 28 | `cat_meshLineIntersect` 0x004b5910 + `cat_meshObjectLineIntersect` 0x004b55c0 + `...R` 0x004b5860 | as the exe (p1Outpost 0x5c uses 0x00885d44 = 4, the last level past the end, best t reset to 1e21 per top-level object, object-space test and hit back to world) |
| 29 | the mesh-hit sites of `collCheckBulletTargetColl` 0x0043cd90 and `collCheckMissileShipColl` 0x0043d330 | as the exe (switches 0x00752c48 / 0x00752c4c = 1, levels 0x00752c54 / 0x00752c50 = 3, the box hit moved on by the mesh distance; missiles: position + velocity) |
| 30 | `btgColorVertices` 0x00428e60, the flashes | as the exe (not in SP missions 1 and 2, random stream 0, delay * 0.001, the end clamp "sic", blue * 3 * intensity / 765) |
| 31 | `btgSetColourMultiplier` 0x00429220 | as the exe (two channels at 0x008dfb98) |
| 32 | `rndFade` 0x005753d0 | as the exe (classes 0/9 mothership, 6/0xb fighter, Component by type 0xd / 0xb, renderlistFade override, * 1.3, the asteroid 0x200 case, derelicts) |
| 33 | `univUpdateRenderList` 0x0054be80 limits + `Tweak.c` defaults | as the exe (squared once, MonsterShip / Component = mothership, Micro = fighter; defaults 0x008ac450 = 20000, then 10000) |
| 34 | `meshSetFade` 0x004ae700 | as the exe (8 channels, stored only with `usingShader` 0x00885d2c = 1; the tree starts the channels at 1.0 where the exe's .bss starts at 0 and every channel is written before use) |
| 35 | `cat_colBlendRGBA` 0x00555560 | as the exe (256 = 0x0072a6a8, signed `jle 0xff`) |
| 36 | `cat_shipDissolveComputePlane` 0x00460270 | as the exe |
| 37 | `primRectTranslucent2` 0x00570910 | as the exe; no review entry (finding F2) |
| 38 | `meshBindingFindByName` 0x004b4900 | as the exe (lib_crt_00676c90 is MSVC `_stricmp`, read from the disassembly) |

## 3. The diff (`git diff 84a9d05`)

- A hunk-to-function map (each changed line of `src/` to the function around it, then the delta map by name) found 163
  edited functions: 11 the map calls IDENTICAL, 40 MODIFIED, the rest new `cat_*` / test functions or names the map
  does not carry.
- The 11 IDENTICALs (`rndLightingEnable`, `rndTextureEnable`, `rndAdditiveBlends`, `rndClear`, `rndFlush` and six
  more in `render.c`) are **byte-identical** to the base when compared function by function: the diff moved them.
  `rndAllClearToBlack`, `rndEnvironmentMap*`, `rndHintInc` etc. are moved too. `rndUnFade` changed only to the new
  `meshSetFade(0, 0.0f)` signature.
- One edited MODIFIED function had no review entry: `primRectTranslucent2` (F2). The producer's delta-map numbers
  hold otherwise: 203 new review entries (all `port-R5-render`) and 7 updated; base 3,046 / 1,297 / 77 / 1,801,
  after the port 3,040 / 1,303 / 77 / 2,004.
- `CAT:` comments: every edited HW1 function carries one; the new functions sit under `CAT (M2-R5 render)` section
  comments. No change outside the producer's scope was found (the files are the render scope, the ship renderer
  halves, the collision / ramming mesh tests, the tweak and options rows, the self-test hooks).
- CAT-TODO re-targets as the report lists them (`M2-R6 FOW` in `Ping.c`, `M2-R6 front end and mouse` and `M2-R6
  NIS/camera` in `render.c`, `M2-R6 HUD messages` in `screenshot.c`, `M2-R6 sensors`, `M2-R6 mesh animation`,
  `M2-R6 ships, the repair droids` twice in `UnivUpdate.c` and in `RepairDroid.h`; `SpaceObj.h`'s +0x4C0 comment
  names M2-R6 ships). The `RammingFrigate.c` `CAT-PROVISIONAL`s are gone. The `CAT-TODO(stage 3)` comments at the
  `madAnimationStart` call sites in `Dock.c` / `Salvage.c` stay, as the report says (they go with M2-R6 mesh animation).
- Observation (no finding): the scope's "billboards (`cat_partRenderUnk1System`)" was ported in M2-R3 as
  `cat_partRenderEBoltSystem`; the report did not say so (annotated).

## 4. The DONE WHEN on the plain harness

| claim | run | result |
|---|---|---|
| `/catRenderTest` counts > 0 from the Cataclysm paths, ASan 0 | `r5r-aud-render`, `r5r-aud-render2` (after the fixes) | PASS: 583 frames, 1,148 objects, 581 lens flares (9,296 elements), 1,166 fog layers, 10,906 trails, ASan 0, no leak. The counters are bumped inside `cat_rndRenderObjectList` (ships), `rndMainViewRenderFunction` (listed), `cat_rndLensFlaresRender` and `cat_rndFogLayerRender` (two quads a frame), not in a stub |
| the 6,000-tick mission stream unchanged | `mission_test.sh --label r5r-aud-mission --ticks 6000 --perturb 3000 --copy-tick 4000`, and `r5r-aud2-mission` after the fixes | **MISSION TEST PASS** both times: A and B byte-identical (1,476,370 B, sha256 `878a70337d858c4a`, the producer's `r5r-fin-a`), ASan 0; perturb caught at 3000 (rng10), flipped copy at 4000 / 2000. `hwo_compare` vs the base stream `r5c-aud-mission-a`: IDENTICAL, 6,001 ticks (only the 4-byte build identity in the header differs) |
| render-side randoms stay on their streams | `r5r-aud-render-hwo` (`--mode render -- /oracleDump`) vs `r5r-aud-mission600` (`--mode mission --mission-ticks 600`) | IDENTICAL, 601 ticks: the render self-test (camera on ships, lens flare, fog, LOD timing) leaves every hashed stream as the plain mission run. Note: the render paths that draw stream 0 as the exe does (BTG flashes outside missions 1-2, conversion box phase 3, Sentinel fill, blast lightning, link bolts) do not fire in Mission01 |
| fe unchanged | `r5r-aud-fe`, 40 s | Main_game_screen, 232 file loads (as `r5r-fin-fe`), ASan 0 |
| sp as before | `r5r-aud-sp`, `--timeout 340` | universe running to the timeout, univUpdateCounter 1,248 (producer 1,360; wall-clock dependent), ASan 0, no leak |

## Findings

| id | finding | evidence | fix |
|---|---|---|---|
| F1 (code) | The Sentinel field segment queue held 34 segments; the exe holds 79 on GLtype | `cat_sentinelFieldSegmentAdd` 0x005c5b20: `mov ecx,[0xb3a700]; neg; sbb; and 0x2d; add 0x22` = 0x22 + (RGLtype != 0 ? 0x2d : 0). 0x00b3a700 is RGLtype (the tree's own note in `render.c`: SWtype 0 .. GLtype 3), and the tree is GLtype (`CAT_RND_RGLTYPE` 3); the producer's comment read it as RGL (0x00b3a6d8) | `Sentinel.c`: `CAT_SENTINEL_SEGMENTS_GL` = 0x22 + 0x2d (79), the array sized by it, `[audit]` comment; report annotated |
| F2 (delta map) | `primRectTranslucent2` 0x00570910 was edited and the report lists it as ported and MODIFIED, but it had no review entry (`reviewed` False) | the hunk-to-function map; `delta_map_review.json` had no 0x00570910 | compared with r6 (as the exe); entry added (sample `audit-R5-render`), `tools/delta_map.py merge` (reviewed 2,005), `docs/m2-r1-function-deltas.md` regenerated, report annotated |
| F3 (comment) | The lens flare element comments had the shapes -3 and -4 swapped ("-3 dark centre, -4 light centre") | jump table 0x0057caac: -3 (0x0057c597) scales the rim colour by 0.25, -4 (0x0057c616) the centre colour; the code does that | both comments in `render.c` corrected, marked `[audit: ...]` |

## Precision

| group | checks | wrong |
|---|---|---|
| bodies vs the exe (section 2) | 38 | 1 (F1) |
| builds debug / release from clean, layout32 | 3 | 0 |
| render self-test, mission A/B + base compare, perturb / copy, render-vs-mission stream, fe, sp | 6 | 0 |
| delta-map counts (203 / 7, totals), edited IDENTICALs, review entries, CAT comments, CAT-TODO re-targets, harness mode docs | 6 | 1 (F2) |
| lens flare shape comments | 1 | 1 (F3) |
| total | 54 | 3 |

Precision = 51 / 54 = 0.944.

Fixes: 3 (the Sentinel cap; the review entry with the merge and the regenerated delta report; the two lens flare
comments), plus report annotations (`[audit: ...]`: the Sentinel cap, the review count 2,005, the billboards note,
the audit's sp counter).

Verdict: PASS-WITH-FIXES - 51/54 checks hold (38 render/GL and ship-renderer bodies incl. the object list, lens flares, fog layers, billboards, cat_meshLineIntersect vs r6; render self-test PASS with counts from the Cataclysm paths, ASan 0; two 6,000-tick Mission01 streams identical and IDENTICAL to the base; render-test stream equals the mission stream; fe, sp as before); fixed the Sentinel segment cap (79 on GLtype), one missing delta-map review entry, the swapped lens flare shape comments
