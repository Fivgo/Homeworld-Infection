<!-- done: objlist, flares-fog, ship-render, gl-files, final -->
# M2-R5 producer 3: render/GL (object list, lens flares, fog layers, the ship renderers)

Status: **done** (every subsystem in the marker). Branch `m2`, worktree `.claude/worktrees/m2`, base commit `84a9d05`
(nothing committed by this producer: the audit checkpoints). Scope: docs/m2-r4-summary.md "Plan for M2-R5 and M2-R6"
row 3, the stage-3 renderers of "What is left of stage 2" / "Outside stage 2", and the item-6 leftovers marked R5-3.

Done when (row 3): a new `/catRenderTest` renders N frames of Mission01 offscreen through the Cataclysm object-list
renderer and logs its object, lens-flare and fog-layer counts: counts > 0, ASan 0, and the mission stream unchanged
(render-side randoms stay on their streams); fe unchanged; sp as before. **Met**: run `r5r-fin-render` (583 frames,
1,148 objects, 581 lens flares, 1,166 fog layers, `result=PASS`, ASan 0); the 6,000-tick Mission01 streams `r5r-fin-a`
/ `-b` are byte-identical to each other and `hwo_compare` IDENTICAL to the base stream `r5c-aud-mission-a` (6,001
ticks, no difference); fe `r5r-fin-fe` reaches `Main_game_screen` as before; sp `r5r-fin-sp` runs the universe to the
340 s timeout, ASan 0 (sections "Self-test" and "Builds and harness runs").

Subsystems:
- `objlist`: `cat_rndRenderObjectList` and the Cataclysm main view, the render state block, `meshSetFade`'s 8
  channels, the render-time LOD (`lodLevelGet`, `cat_lodLevelGetTimed`, the timing), `rndFade` /
  `rndShipVisibleUsingCoordSys` as the exe, the edge-effect and warped mesh renderers, the material switch, the Missile
  tail layout, `SpaceObj.catVisibleLink`.
- `flares-fog`: lens flares (script callbacks, visibility, renderer, the NIS source), the fog layer (script, fade,
  update, render), the BTG per-channel colour multiplier, the options rows `enableLensFlare` / `enableRepairbots`, the
  self-test `/catRenderTest` and harness mode `render`.
- `ship-render`: the renderer halves of the ship ports (Sentinel field segments, Dreadnought charge, Nomad beam, blast
  lightning and dots, the repair-droid proxies' renderer, the Naggarok EMP flicker), `cat_meshLineIntersect` with the
  ramming latch / cutting beam and the collision mesh-hit tests, the render limits as distances, the bullet-hit glow's
  render state, the static gate and the damage points compared.
- `gl-files`: every unreviewed function of `render.c`, `Mesh.c`, `LOD.c`, `Shader.c`, `Light.c`, `Clipper.c`,
  `prim2d.c`, `prim3d.c`, `Color.c`, `glcompat.c`, `glcaps.c`, `rinit.c/.cpp`, `Star3d.c`, `BTG.c`, `NavLights.c`,
  `Trails.c`, `Ping.c`, `screenshot.c`, `debugwnd.c`, `Transformer.c` compared with r6 and the modified ones ported or
  given an owner.
- `final`: builds, layout check, harness runs, delta-map merge, this report.

## File by file (with the exe addresses)

### `render.c` / `render.h`

- The render state block (0x00c32c00..0x00c32c9b: `catShCurrentObject`, the fade, dissolve, mimic, Beast-stun,
  timed-effect, EMP-flicker and tint flags) set per object by `cat_rndSetupObjectFlags` with
  `cat_rndSetupDissolveFlag` 0x005759f0, `cat_rndSetupFadeFlag` 0x00575a20, `cat_rndShipEMPdAndMoving` 0x00575ae0,
  `cat_kasNaggarokEMPHitShip` 0x00575b30, `cat_rndSetupTimedEffectPhase` 0x00575b70, `cat_rndSetupMimicFlag`
  0x00575c60, `cat_rndSetupObjectEffectParams` 0x00575f40; `cat_rndObjectEffectsSetup` 0x00575c90 (also used by the
  bullet-hit glow).
- `cat_rndRenderObjectList` 0x005776f0: the multi-pass object list with the deferred depth-sorted pass
  (`cat_rndSortObjectsByDepth` 0x00577660), `cat_meshHierarchyCount` 0x00575ea0 / `cat_lodHierarchyCount` 0x00575ee0,
  `cat_rndDepthTestEnable` 0x0057ae40, the lightning bolts (`cat_rndLightningBoltDraw` 0x00576010,
  `cat_rndLinkLightningDraw` 0x00576330), the conversion box, the module-to-host rule, the repair-droid renderer call
  (option 0x00c52374), the Sentinel field queue (0x00b378a0).
- `rndMainViewRenderFunction` 0x005764e0 (the Cataclysm main view: object list, fog layer, blasts, lens flares,
  fields), `rndFade` 0x005753d0, `rndShipVisibleUsingCoordSys` 0x005750c0, `rndUnFade`.
- Lens flares: `cat_scriptSetLensFlareCB` 0x0057b8c0 with `cat_lfTextureFileSetCB` 0x0057b820, `cat_lfElementSetCB`
  0x0057b970, `cat_lfTextureSetCB` 0x0057ba00, `cat_rndLensFlareTextureUnload` 0x0057b800, the source list 0x00b37880
  (`cat_rndLensFlareSourceAdd/Remove`), `cat_rndLensFlareSourceVisibility` 0x0057bad0, `cat_rndLensFlaresRender`
  0x0057bec0, the NIS source `cat_rndLensFlareNISSourceAdd/Remove/SetPosition/Enable` 0x0057cac0..0x0057cb40.
- Fog layer: `cat_scriptSetFogCB` 0x0057cb70, `cat_rndFogLayerTextureLoad/Unload` 0x0057b7d0 / 0x0057b7b0,
  `cat_rndFogLayerOff` 0x0057cd20, `cat_rndFogLayerFadeTo` 0x0057cd40, `cat_rndFogLayerUpdate` 0x0057ce00,
  `cat_rndFogLayerRender` 0x0057d2c0 (BTG colour channel 1).
- gl-files pass: `rndCameraInside` 0x00574f80 centres a ship's sphere on its collision position (+0xE0);
  `rndRenderAHomeworld` 0x005757f0 restores the depth test as it was; `rndInit` 0x005747d0 initialises the lens flare
  list and the field queue; `rndRenderTask` 0x0057a910's front-end / mouse parts are a `CAT-TODO(M2-R6 front end and
  mouse)`.
- The render self-test (`catRenderSelfTestSet/Requested/Tick`, `CatRndCounts`): see "Self-test".

### `Mesh.c` / `Mesh.h`

- `meshSetFade(channel, fade)` with 8 channels (0 distance, 1 material, 2 crystal, 3 Beast stun, 4 mimic warp, 5 cloak,
  6 salvage beam, 7 docking) and the product into `meshFadeAlpha` / `bFade`.
- Materials: `meshCurrentMaterialDefault` 0x004af790 (both faces for dissolve / build clip, the render-state ambient
  tints, enableCPV smooth shading), `cat_meshCurrentMaterialAlt` 0x004afcb0, `cat_meshMaterialSwitch`;
  `meshAlmostCurrentMaterial` 0x004b0020 clears fade channel 1.
- Per-vertex colour and glow: `meshObjectRender` 0x004b0170, `meshObjectRenderLit` 0x004b0970, `meshSpecObjectRender`
  0x004b1d40 (inlined `meshSpecColour` with the vertex state / info entry), `meshSpecColour` 0x004b00c0 (two new
  arguments), `meshMorphedObjectRender` 0x004b34f0.
- The Beast-stun edge effect (`cat_meshObjectRenderEdgeEffect` 0x004b4a60, `...R` 0x004b4dc0, `...Hierarchy`
  0x004b4d60, `cat_meshRenderEdgeEffect` 0x004b4e90, `cat_meshRenderShipHierarchyEdgeEffect` 0x004b4e00) and the
  mimic warp (`cat_meshWarpVertex` 0x004b5aa0, `cat_meshWarpNormal` 0x004b5b70, `cat_meshObjectRenderWarped`
  0x004b5c90, `cat_meshObjectRenderWarpedHierarchy` 0x004b6330, `cat_meshRenderShipHierarchyWarped` 0x004b6400).
- `cat_meshLineIntersect` 0x004b5910 with `cat_meshObjectLineIntersect` 0x004b55c0 / `...R` 0x004b5860 (object-space
  triangle tests, the best hit per top-level object, p1Outpost at LOD 4).
- `meshBindingFindByName` 0x004b4900 compares names case-insensitively (`_stricmp`).

### `LOD.c` / `LOD.h`

`lodLevelGet` 0x004a4d70 (the screen-size bands, the detail cap, mimics), `cat_lodCurrentLevelGet` 0x004a4af0,
`cat_lodLevelGetTimed` 0x004a5000, `cat_lodTimingStart/Stop` 0x004a4cf0 / 0x004a4d20, `cat_lodSetTimeThreshold`
0x004a50e0; the per-level timing fields and the distance-LOD switch cleared by `lodTableReadScript` 0x004a47e0.

### `BTG.c` / `BTG.h`

`btgSetColourMultiplier(channel, t)` 0x00429220 (channels at 0x008dfb98, product into `btgFade`); the background
flashes of `btgColorVertices` 0x00428e60 (three slots at 0x008dfa60 set up by `btgReset` 0x00428270, random stream 0,
not in single-player missions 1 and 2), the colours made every frame by `btgRender` 0x00429280, HW1's fastBlends flag
back; the texture list lookup case-insensitive (`btgLoad` 0x004286e0).

### `Shader.c` / `Shader.h`, `Clouds.c`, `Nebulae.c`

`shSpecularColour` 0x0050e280 takes the vertex state and info entry and adds the glow (`cat_shColourAddGlow`
0x0050d7c0); the callers `ellipsoid_render` 0x00436520, `nebDrawChunk2` 0x004c8b00, `nebDrawTendril` 0x004c9430 pass
NULL, NULL as the exe.

### `Trails.c`

`cat_trailShouldDraw` 0x005a7240 (clamped Leech, dissolving, SOF_DontDrawTrails / SOF_Disabled, being built, the
ramming frigate's inactive trail; moving or pushing) in `trailDraw` 0x005a7310; the team colour (`cat_shGetTeamColour`
0x0050d460) for the glow sprite, the capital glow (`trailDrawCapitalGlow` 0x005a5570: also the speed multiplier, no
scaleCap translate, the Kushan glow) and the segments (`trailSurpriseColorAdjust` 0x005a6c30 without a colour argument,
`trailLineSequence` 0x005a6e30); `trailDrawBillboardedSquareThingz` 0x005a5be0 keeps the smallest glow below
VELRATIO_THRESH.

### `NavLights.c`, `ObjTypes.c` / `ObjTypes.h`

`RenderNAVLights` 0x004c78e0: NAVLIGHT_Default drawn while lit, the new NAVLIGHT_FadeOnOff (name table 0x00889d88)
always drawn with a pulsing fade, other types not drawn, nothing past minLOD.

### `prim2d.c`, `Ping.c`, `screenshot.c`

`primRectTranslucent2` 0x00570910 draws with normal blending and puts the additive mode back; `pingListDraw`
0x004e8850's FOW test and `ssSaveScreenshot` 0x0057ef80's message are `CAT-TODO`s for M2-R6.

### `Collision.c`

The mesh hit location for a ship with `bUseRayTriangleIntersectionForHitLocation` (+0x408): bullets
(`collCheckBulletTargetColl` 0x0043cd90 at 0x0043d219), beams (`collCheckBeamCollWithTargetsInBlob` 0x0043c570 at
0x0043c81e) and missiles (`collCheckMissileShipColl` 0x0043d330 at 0x0043d677), switches 0x00752c48 / 0x00752c4c and
LOD 3 (0x00752c54 / 0x00752c50).

### `RammingFrigate.c`

`cat_rammingFrigateLineHitsMesh` 0x005c0b20 (origin -/+ 500 * dir, LOD 0) and the cutting beam of
`cat_rammingFrigatePushEffectCreate` 0x005c1900 (length to the mesh hit, the upgrade look 2 / 1) replace the two
`CAT-PROVISIONAL`s.

### `UnivUpdate.c`, `Tweak.c` / `Tweak.h`

`univUpdateRenderList` 0x0054be80 squares the class limits (RENDER_LIMIT_* / RENDER_FADE_* are distances: defaults
0x008ac450.., `scriptSetReal32CB`), MonsterShip / Component as the mothership, Micro as the fighter;
`univBulletCollidedWithTarget` 0x00546a20 sets the glow's render state through `cat_rndObjectEffectsSetup`;
`twnLipsBaseOffset` 0x008ac5e8; the droid TODOs re-targeted.

### Ship renderers

- `Sentinel.c` / `.h`: the field segments (`cat_sentinelFieldSegmentAdd` 0x005c5b20, `cat_sentinelFieldsRenderBehind`
  0x005c5c20, `...RenderRest` 0x005c5cb0, `cat_sentinelFieldQueueSegment` 0x005c5ec0, `cat_sentinelFieldSegmentDraw`
  0x005c60a0, `cat_sentinelFieldQueue` 0x005c67b0). [audit: the segment queue held 34 segments; 0x005c5b20 caps it at
  0x22 + (RGLtype 0x00b3a700 != 0 ? 0x2d : 0), i.e. 79 on this tree's GLtype (34 only on the software renderer); fixed
  in `Sentinel.c` (`CAT_SENTINEL_SEGMENTS_GL`)]
- `Blast.c` / `.h`: `cat_blastDrawLightning` 0x005163b0, `cat_blastRenderAll` 0x00516750,
  `cat_dreadnoughtChargeEffect` 0x00516a40, `cat_nomadMoonBeamEffect` 0x00516bb0.
- `Dreadnought.c` / `.h`: `cat_dreadnoughtRenderCharge` 0x005b1110; `NomadMoon.c` / `.h`: `cat_nomadMoonRenderBeam`
  0x005be120; `RepairDroid.c` / `.h`: `cat_droidRenderArray` 0x005c3cf0.
- `Color.c` / `.h`: `cat_colBlendRGBA` 0x00555560; `Dissolve.c` / `.h`: `cat_shipDissolveComputePlane` 0x00460270.
- `HS.c`: `hsStaticGateRender` 0x00490240 compared (equal; `cat_hsGateRectangle` 0x0048f8b0 is hsRectangle with the
  colour fixed). `Damage.c`: `dmgGetLights` / `dmgClearLights` compared with `cat_dmgShipLoadDamagePoints` 0x00462950 /
  `cat_dmgShipFreeDamagePoints` 0x00462a50 (the free also clears the pointer).

### Others

- `SpaceObj.h`: the Missile tail (+0x214 reserved, colour scheme +0x250, asserts), `SpaceObj.catVisibleLink` (+0x88),
  the repair-droid field comment.
- `Particle.h`: the lens flare source fields named by their use; `cat_partRenderEBoltSystem` 0x004de880.
- `LevelLoad.c`: the mission script's `LensFlare` / `Fog` keys (table 0x00883b80); `levelPreInit` 0x004a3230 drops the
  last level's fog layer and lens flare.
- `Sensors.c`: `smZoomUpdate` 0x0050b8d0 / `smSensorsCloseForGood` 0x0050ba90 use BTG channel 0.
- `utility.c` / `.h`: options rows `enableLensFlare` (0x00c52380, row 0x008c14cc) and `enableRepairbots` (0x00c52374,
  row 0x008c14d8); `/catRenderTest`. `main.c`, `OracleDump.c`, `CameraCommand.h`: the self-test hook.
- `MeshAnim.c`: `madAnimationStart`'s third argument re-targeted (see "Leftovers").
- `tools/m2_harness.sh`: mode `render`, `--render-frames N` (default 600).
- [audit: the scope's "billboards (`cat_partRenderUnk1System`)" item was already in the tree: M2-R3 ported it as
  `cat_partRenderEBoltSystem` 0x004de880 (`Particle.c`); this port only calls it (the edge effect)]

## Self-test

`/catRenderTest <frames>` (harness mode `render`): the headless Mission01 run of the mission mode drawn through the
Cataclysm object-list renderer. At tick 16 the lens flare option goes on, the sensors manager is closed, the camera is
focused on the first 20 ships (`ccCameraTimeoutOverride`), a fog line is set and the NIS light source is shown ahead
of the camera (moved every tick); at the end `rendertest: counts` lines and `rendertest: done ... result=PASS|FAIL`
(PASS: objects, lens flares and fog layers > 0). Final run `r5r-fin-render`: 583 frames, 1,148 objects listed and
drawn, 10,906 trail draws, 581 lens flares (9,296 elements), 1,166 fog layers, PASS, ASan 0, no leak.

## Builds and harness runs

- `tools/build.sh --debug` and `--release`: OK, 0 warnings under -Werror. `tools/m2_layout32.sh`: OK.
- Harness runs (all silent, offscreen, through `tools/m2_harness.sh`):

| label | mode | milestone | first stop | notes |
|---|---|---|---|---|
| `r5r-fin-render` | render 600 | render self-test done | none (rc 0) | PASS, counts above, ASan 0 |
| `r5r-fin-a` / `-b` | mission 6,000 | oracle mission run done | none (rc 0) | byte-identical, 1,476,370 B, ASan 0; `hwo_compare` vs `r5c-aud-mission-a`: IDENTICAL, 6,001 ticks |
| `r5r-fin-perturb` | mission + perturb 1500 | oracle mission run done | none | divergence at tick 1500 (rng10), flipped copy at 2000: MISSION TEST PASS |
| `r5r-d-mission` | mission 6,000 | oracle mission run done | none (rc 0) | after ship-render: IDENTICAL to `r5c-aud-mission-a`, ASan 0 |
| `r5r-fin-fe` | fe, 40 s | Main_game_screen | none: running at the timeout | as before, ASan 0 |
| `r5r-fin-sp` | sp, timeout 340 | universe running (univUpdateCounter > 0) | none: running at the timeout | ASan 0, no leak, univUpdateCounter 1,360 at 340 s (`r5ld-final-sp` 1,248) [audit: the audit's `r5r-aud-sp` 1,248; the counter follows the wall clock] |

## Delta-map entries

`ghidra/delta_map_review.json`: 203 new review entries and 7 updated (sample `port-R5-render`, notes tagged
`[review: M2-R5 render]`), merged with `tools/delta_map.py merge --done m2r5:render`; `docs/m2-r1-function-deltas.md`
regenerated. Counts now IDENTICAL 3,040 / MODIFIED 1,303 / REWRITTEN 77, reviewed 2,004 (was 3,046 / 1,297 / 77, 1,801).
[audit: `primRectTranslucent2` 0x00570910, edited and listed below as ported, had no review entry; added (sample
`audit-R5-render`), merged: reviewed 2,005]

- Every function of the GL files the map had unread was compared. Ported and now MODIFIED: `btgColorVertices`,
  `btgRender`, `btgReset`, `btgLoad`, `lodTableReadScript`, `meshAlmostCurrentMaterial`, `meshBindingFindByName`,
  `meshSpecColour`, `meshSpecObjectRender`, `shSpecularColour`, `RenderNAVLights`, `trailDraw`, `trailDrawCapitalGlow`,
  `trailDrawBillboardedSquareThingz`, `trailSurpriseColorAdjust`, `trailLineSequence`, `primRectTranslucent2`,
  `rndCameraInside`, `rndRenderAHomeworld`, `rndInit`, `rndFade`, `meshCurrentMaterialDefault` and the objlist bodies;
  `pingListDraw`, `rndRenderTask`, `ssSaveScreenshot` recorded with their M2-R6 parts.
- Functions with no counterpart in GoK (glcompat, glcaps' RGL table, the Transformer, the RGL / Glide device list,
  debugwnd, the shameless plug, the perspective hint) recorded as such.
- Edited functions outside the GL files: `collCheckBeamCollWithTargetsInBlob`, `collCheckBulletTargetColl`,
  `collCheckMissileShipColl`, `univBulletCollidedWithTarget`, `univUpdateRenderList`, `univDeleteDeadShip`,
  `univCreateShip`, `levelPreInit`, `smZoomUpdate`, `smSensorsCloseForGood`, `btgSetColourMultiplier`,
  `ellipsoid_render`, `nebDrawChunk2`, `nebDrawTendril`, `StrToNAVLightType`, `hsStaticGateRender`,
  `madAnimationStart`.

## CAT-TODOs closed and re-targeted

- Closed: the two `CAT-PROVISIONAL (stage 3, the mesh hit location)` in `RammingFrigate.c`; the collision
  `CAT-TODO(stage 3)` for the mesh hit location; `univUpdateRenderList`'s `CAT-TODO(stage 3)`; the stage-3 renderers of
  the ship ports (Sentinel, Dreadnought, Nomad, blast, the droid renderer); `hsStaticGateRender`; the damage points.
- Re-targeted: `CAT-TODO(M2-R6 ships, the repair droids)` (UnivUpdate.c twice, RepairDroid.h, SpaceObj.h);
  `CAT-TODO(M2-R6 mesh animation, MeshAnim.c / MadLinkIn.c)` for `madAnimationStart`'s third argument (MeshAnim.c);
  `CAT-TODO(M2-R6 FOW)` in `pingListDraw`; `CAT-TODO(M2-R6 front end and mouse)` in `rndRenderTask`;
  `CAT-TODO(M2-R6 HUD messages)` in screenshot.c; `CAT-TODO(M2-R6 sensors)` for the HW1 BTG dimming (Sensors.c);
  `CAT-TODO(M2-R6 NIS/camera)` for the motion cues (`cat_mcRender`, render.c).

## Leftovers and owners

- **Repair-droid runtime** (droidCreateArray 0x005c3070, droidUpdateArray 0x005c32f0, cat_droidFreeArray 0x005c32b0
  and helpers): M2-R6 ships. They draw on random stream 0, which the oracle hashes, so they belong with a ship-runtime
  round that re-baselines the stream; the renderer half is in.
- **Mesh animation** (madAnimationStart 0x004b6b90's mode word, madAnimationUpdate / Stop / BindingsDupe, the exe's
  madanim layout, the per-type animation sounds 0x00c32660): M2-R6 mesh animation. It changes when gun bindings swap
  back (simulation side). The call-site `CAT-TODO(stage 3)` comments ("the exe passes a third argument") go with it.
- **Motion cues** (`cat_mcRender`): M2-R6 NIS/camera. **Ping FOW test**: M2-R6 FOW. **Render task front-end / mouse
  parts**, **screenshot message**: M2-R6 front end / HUD.
- **The beam's sixth argument** (`collCheckBeamCollWithTargetsInBlob`'s Sentinel-field triangle out-parameter):
  combat follow-up (the tree's signature has five arguments).
- **ETG real32 parameters**: no open marker remains in the tree; the new effect call
  (`cat_rammingFrigatePushEffectCreate`) passes its length as `Real32ToUdword` like the earlier ports.
- Not ported on purpose (GoK's backend): glcompat, the RGL / Glide device enumeration, the Transformer, the WGL
  set-up, the perspective hint, `rndMaterialfv`'s glMaterialfv cache, the device-flag (0x00a75ef0) shading switches,
  GoK's vertex-array BTG star quads and the 0.325 pixel-centre offset.
