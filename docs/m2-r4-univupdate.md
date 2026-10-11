<!-- done: meshstate, glow, deadships, univupdate, identical, final -->
# M2-R4 producer 1: universe update + mesh state (`UnivUpdate.c`)

Status: **done** (subsystems `meshstate`, `glow`, `deadships`, `univupdate`, `identical`, `final`). Branch `m2`,
worktree `.claude/worktrees/m2`, nothing committed. Scope: docs/m2-r3-summary.md "Plan for M2-R4" row 1 and
"What is left of stage 2 part A" item 3.

DONE WHEN, judged on the plain harness (`tools/m2_harness.sh`, section `final`): **met**. sp on Mission01 reaches
"universe running" with univUpdateCounter 1,248 at the 340 s timeout (R3: 1,248), ASan 0, no leak; the etg effect
self-test passes with the same numbers as R3 (618 created, peak 2,515 effects); all 52 IDENTICAL bodies of
`UnivUpdate.c` were compared with the exe, 10 found modified, all recorded as delta-map reviews (plus the 46
MODIFIED ones).

Method: every body was read in the r6 export (`decompiled.c`) and, where Ghidra's field names or parameters are
wrong (the export's structs are HW1/CET layouts), in the exe's disassembly (`objdump -d -M intel` of
`original/game/cataclysm.exe`); constants were read from the exe's data sections. The tree's structs are GoK's LP64
layouts with Cataclysm fields added by name, so exe offsets are mapped to fields, never copied.

Files changed (details and exe addresses in the subsystem tables):

| file | what and why |
|---|---|
| `src/Game/UnivUpdate.c/.h` | the port: mesh state, bindings, nozzle/matrix helpers, deaths and delete lists, bullet/missile/collision deltas, update loop, deferred per-ship update, counters, CAT-TODO headers on unported MODIFIED bodies, HW1 Drone/DefenseFighter/CryoTray cases removed |
| `src/Game/SpaceObj.h` | `catMeshState` (+0x80), `catEffectBlock` (+0x84), the mesh-state types |
| `src/Game/Mesh.c/.h`, `src/Game/MeshAnim.c` | binding parent links, per-object state slots in the renderer, bullet-hit glow, the exe's `madBindingListCallback` |
| `src/Game/Shader.c/.h`, `src/SDL/render.c`, `src/Game/Sensors.c` | per-vertex glow runtime, render object, state passed to the ship renderer |
| `src/Game/Trails.c/.h` | `trailGetNozzleOffset` through the nozzle binding matrix |
| `src/Game/ETG.c`, `src/Game/Gun.c`, `src/Ships/DefenseFighter.c`, `src/Game/SaveGame.c` | the two new fields set/freed/cleared by effect, bullet, missile creators and the loader |
| `src/Game/Collision.c`, `src/Game/Crates.c`, `src/Ships/SalCapCorvette.c` | callers of the changed `univBulletCollidedWithTarget` / `AddTargetToDeleteList` signatures |
| `src/Ships/DefaultShip.c` | `MiningBaseDied` without the list removals (the exe's dead-ship path now handles them) |
| `src/Game/SinglePlayer.c` | quit times are real time |
| `ghidra/delta_map_review.json`, `ghidra/delta_map.json` | 98 review entries for `UnivUpdate.c`, merged |

## Subsystem `meshstate`: the SpaceObj +0x80/+0x84 fields, the per-vertex mesh state, bindings and nozzles

| change | exe evidence | files |
|---|---|---|
| `SpaceObj` gains `catMeshState` (exe +0x80) and `catEffectBlock` (exe +0x84) right after `posinfo` (exe posinfo is +0x58..+0x80: `univInitSpaceObjPosRot` 0x00540460 clears 10 dwords at +0x58 and writes the words at +0x7C/+0x7E). New types `CatVertexState` (0x28 bytes, static assert), `CatMeshObjectState` (12 bytes on ILP32), `CatMeshState` | `cat_univShipMeshStateCreate` 0x00541cd0, `cat_univMeshStateAllocRecursive` 0x00541b80 | `SpaceObj.h` |
| every creator sets both fields: ships (`univCreateShip` 0x00542320, after the colour scheme), asteroids, derelicts, dust/gas clouds, nebulae, missiles (`univAddAsteroid` 0x00540a30, `univAddDerelictByStatInfo` 0x00541000, `univAddDustCloud` 0x00541300, `univAddGasCloud` 0x00541500, `univAddNebula` 0x005416d0, `univAddMissile` 0x00540c70), effects (`etgEffectCreate` 0x00470990, `etgCreateEffects` 0x004704e0, `etgSpawnNewEffect` 0x00470c50: +0x84 = `memAlloc(1)`), bullets (`cat_gunCreateBullet` 0x0048bb00), fired missiles (`missileShoot` 0x0048aac0); the debug effect creators of `ETG.c`/`DefenseFighter.c` set NULL | the `coordsys.m12`/`m22` stores (Ghidra labels for +0x80/+0x84) in each body; `cat_univShipMeshStateCreate` call sites (`call_graph.csv`) | `UnivUpdate.c`, `ETG.c`, `Gun.c`, `DefenseFighter.c` |
| `cat_univShipMeshStateCreate` / `cat_univMeshStateFree` / both recursive helpers: only ships get a state (one slot per polygon object in hierarchy pre-order, a 0x28-byte state per vertex: dirty byte +0x11 = 1, last update +0x14 = -1.0, +0x08 = 1.0 when the vertex info has flag 0x04), and none at all with the low-memory switch 0x00c5236c (set by `utyGameSystemsInit` below 128 MB; `catMeshStateDisabled`, never set here) | 0x00541cd0, 0x00541df0, 0x00541b80, 0x00541c70 | `UnivUpdate.c/.h` |
| the free paths free both: `univFreeShipContents`, `univFreeResourceContents`, `univFreeBulletContents` (which now also tells every ship through CustShipHeader slot 14, `cat_univRemoveBulletReferencesFromShips` 0x0054b1a0), the bullet death in `univUpdateAllPosVelBullets`, `etgEffectDelete` (+0x84 first) | 0x0054b550, 0x0054b030, 0x0054b1d0, 0x00543253, 0x0046a6f0 | `UnivUpdate.c`, `ETG.c` |
| `LoadSpaceObj` clears both fields of every loaded object (the saved pointers are stale) | `LoadSpaceObj` 0x00501660 (`m22 = 0; m12 = 0` after the switch) | `SaveGame.c` |
| bindings: `mhbinding` gains `parentIndex` (+0x14, 0x18 bytes) set by `meshBindingListRecurse`/`Create` (0xFF for top-level objects); `mhlocalbinding` gains `parent` (+0x54, 0x58 bytes), set by the new `cat_univMeshBindingsLinkParents` 0x00541ae0, called by `univMeshBindingsDupe` (which now stores every binding's object, with or without a function), the new `cat_univMeshBindingsDupeNoGuns` 0x005418f0 and `madAnimBindingsDupe` 0x004b6a10. `madBindingListCallback` stores the object of function-less bindings and starts animated ones at the identity (0x004b68f0). ILP32 size asserts for both structs | 0x004b47b0, 0x004b4840, 0x005419d0, 0x005418f0, 0x00541ae0, 0x004b6a10, 0x004b68f0 | `Mesh.h/.c`, `MeshAnim.c`, `UnivUpdate.c` |
| `cat_shipNozzleInfoGet` 0x0053f8c0 (the binding of `engineNozzleObject[LOD][trail]`, 0xFF read as object 0), `cat_meshObjectMatrixConcat` 0x0053f900 / `cat_meshObjectMatrixGet` 0x0053f970 (parent chain times the binding's matrix or the object's local matrix; identity for none) and their only caller `trailGetNozzleOffset` 0x005a4cc0 (the nozzle offset goes through that matrix) | as named | `UnivUpdate.c`, `Trails.c` |

The first sp run after this subsystem (`r4u-ms-sp`) stopped with a SEGV in `cat_meshObjectMatrixConcat`: HW1's
`madBindingListCallback` left the object pointer of function-less animation bindings unset. The exe sets it, so the
fix is the port above; `r4u-ms-sp2` then ran as R3 (below).

## Subsystem `glow`: the per-vertex glow runtime

| change | exe evidence | files |
|---|---|---|
| `cat_shGlowUpdate` 0x0050d560: hit glow decays 0.2/s (vertex info flag 0x01), flag 0x02 turns the lost glow into scorch (x0.2, decays 0.1/s, clamped 0..1); colour = -0.15 x scorch plus the info's colour offset / 256, or, with the trail bits 0x10 << i, plus the team colour weighted by trail i's expansion (`TRAIL_CONTRACTED` stops, `TRAIL_EXPANDING` counts down); flags 0x31 force a recompute. Constants 0.2 (0x0072b0f0, 0x0072a510), 0.1 (0x0072b0ec), -0.15 (0x0072b0e8), 1/256 (0x0072a6b0) | 0x0050d560 | `Shader.c/.h` |
| `cat_shColourAddGlow` 0x0050d7c0 with a state: update, then each channel + 2 x FAST_TO_INT(colour x 256), clamped (the R2 fatal error is gone) | disassembly 0x0050d7da..0x0050d974 (`lea ecx,[ecx+edx*2]`) | `Shader.c` |
| `cat_shGetTeamColour` 0x0050d460: the current object's team detail colour (`teColorSchemes[i].textureColor.detail`, 0x00bb3024 + i x 0x30), 0xFF000000 read as white, scaled so the largest channel is 1; team = colour scheme in single player, owner otherwise | 0x0050d460; `cpColorsUpdate(DAT_00bb3020, DAT_00bb3024)` | `Shader.c` |
| `catShCurrentObject` (exe 0x00c32c00, the render object): set around the ship draw in `render.c` (partial `cat_rndRenderObjectList` 0x005776f0) and around the bullet-hit glow | 0x005776f0 | `Shader.c`, `render.c` |
| the state reaches the mesh renderer: `meshRenderShipHierarchy` takes the ship's `CatMeshState` as a new first argument (0x004b4520), `meshObjectRenderHierarchy`/`meshObjectRenderR` pick the object's slot by its pre-order index (0x004b4370, 0x004b4110) and hand its vertex states to `meshObjectRender`/`meshObjectRenderLit` through `MESH_VERTEX_STATE` (a file static set around the call, instead of the exe's new first argument, so the other callers are untouched); `cat_meshRenderState` is `meshRender` with a LOD's slots (the exe's `meshRender` 0x004b4280 first argument), used for ships without local bindings. The sensors manager passes no state (`cat_smShipMeshDraw` 0x0050a0d0 passes 0) | as named | `Mesh.c/.h`, `render.c`, `Sensors.c` |
| the bullet-hit glow: `cat_meshApplyBulletHit` 0x004b52d0 (per LOD: the vertex nearest the hit point, then every vertex within radius^2 1.0 of it; top-level objects start at slot 0 and their daughters at 1, as in the exe), `cat_meshObjectNearestVertexToHit(R)` 0x004b4f80/0x004b5110, `cat_meshObjectForVerticesInRadius(R)` 0x004b5190/0x004b5240, and the callback `cat_meshBulletHitGlowCB` 0x004b5820 (flag 0x01: update, hit glow += 1.0, set flag 0x02). The exe builds the object matrices on the GL modelview stack and reads them back with `glGetFloatv`; the port multiplies the same matrices on the CPU (render state only). Called from `univBulletCollidedWithTarget` for ships that take damage (0x0054703a) | disassembly 0x004b52d0..0x004b5518 (argument order), 0x004b4f80..0x004b5105, 0x004b5820..0x004b5859 | `Mesh.c/.h`, `UnivUpdate.c` |

HWI deviations, each marked in the code: the bullet-hit glow skips LOD levels without slots and a mesh without
vertices (the exe would read through NULL); the hit point is computed before the effect test (the exe hands the glow
an unset vector when it creates no hit effect); the render object is reset after the glow (the exe leaves it set
until the next render object).

## Subsystem `deadships`: deaths, the delete lists and univDeleteDeadShip

| change | exe evidence | files |
|---|---|---|
| `AddShipToDeleteShipList(ship, deathBy, unused)`: a dying `sMothership` starts its death sequence instead of entering the list (`cat_mothershipStartDeathSequence`, already in `Mothership.c`); the -1 damage type reads the zero byte before `etgDeathModeByGunType` (the tree guards the index, same result). `AddTargetToDeleteList` takes the same unused third argument; its callers in `Crates.c` and `SalCapCorvette.c` pass 1 as the exe does [audit: not quite: `cratesPlayerFoundACrate` 0x00460d60 passes 0 (push 0 at 0x00460ebc), only the inlined `expireCrate` in `cratesUpdate` passes 1 (0x00461f70), and `salCapHarvestTarget` has no exe counterpart; the argument is never read, so behaviour is unchanged. The crate-found caller now passes 0] | 0x00545b80, 0x00545dd0 | `UnivUpdate.c/.h`, `Crates.c`, `SalCapCorvette.c` |
| `univDeleteDeadShip` in the exe's order: `CustShipDied` first, factory notification only for player 0 in single player, repair effect cleared, launch manager by class, the big-ship death explosions with the exe's 10 parameters (collrectoffset x/y/z, `+uplength`, `+rightlength`), inline bullet/missile clean-up (missiles that lose the ship self-destruct within 5 s), `smObjectDied` always, no death sound for type -1, grow-selection entries nulled. Software-renderer branch dropped (GL only, comment). Left as CAT-TODO with owner: salvage depot, external build, droids, `cat_clampUnclampAllFromShip`, the second grow-selection, the speed multiplier (CAT-PROVISIONAL, #5) | 0x00547d00 | `UnivUpdate.c` |
| `univDeleteDeadShips` consumes the list from its head, so ships added while it runs are handled the same frame | 0x00548e00 | `UnivUpdate.c` |
| `univWipeShipOutOfExistence`: removes the ship only from the lists it is in, `catWipeShipFreeDirectly` (0x00a44000, set by the mission-13 KAS hack, stage 6) frees directly; FOW cube and two Cataclysm blocks are CAT-TODO | 0x005477d0 | `UnivUpdate.c` |
| `univDeleteWipeInsideShipOutOfExistence` unlinks the ship and runs the ship-died notifications (etg, sound, pie, mouse, command layer, special ships) before freeing; `univRemoveShipFromOutside`: inline missile loop with the 5 s self-destruct, `smObjectDied` always, the AI told in multiplayer only, attacker fields cleared (module ships #4, FOW stage 3); `univReallyDeleteThisShipRightNow` compared, same as HW1 | 0x00547900, 0x00547990, 0x00548f00 | `UnivUpdate.c` |
| `univRemoveShipReferences`: no `CustShipDied` (moved to `univDeleteDeadShip`), factory only for player 0 in single player, inline bullet/missile clean-up | 0x0054b230 | `UnivUpdate.c` |
| `univRemoveAllShipReferencesFromSpecialShip(s)`: attacker memories and `gettingrocked` cleared whenever they match, no right-of-way callback, slot 11 always called; the docking ship and the exe's inside-ship entry deletion with `lmUpdateShipsInside`, ships in hyperspace too | 0x00547640, 0x005476e0 | `UnivUpdate.c` |
| `univRemoveAllTargetReferencesFromMissiles`: a missile that loses its target self-destructs within 5 s (`min(totallifetime, timelived + 5.0)`, 0x008b5c68) | 0x00547570 | `UnivUpdate.c` |
| `MiningBaseDied` no longer removes the other mining bases from the ship/impactable lists nor wipes them with ETG off (the R3 comment said this waited for `univDeleteDeadShip`; with the exe's dead-ship path they are wiped by their death effect like every other ship) | 0x005b0370 | `DefaultShip.c` |
| `DeleteDeadDerelict`, `DeleteDeadMissile`, `univReallyDeleteThisResourceRightNow` (the tree's `DeleteDeadResource` body) / `univDeleteDeadResources`: `smObjectDied` always, mesh state and +0x84 freed, no HW1 salvage references; asteroid break (attribute 0x200 rule), ramming frigates and KAS selections CAT-TODO | 0x00549430, 0x00549880, 0x005490d0, 0x00549270 | `UnivUpdate.c` |
| `AddDerelictToDeleteDerelictList` guards the -1 death mode like `AddShipToDeleteShipList` | 0x00545d20 (IDENTICAL; the guard is a no-op on valid input) | `UnivUpdate.c` |
| `univRemoveShipFromHotkeyGroup`: the group-defeat speech is Cataclysm's 0x2002076 | 0x00547b70 | `UnivUpdate.c` |
| `univFindBackupMothership` per race: Sect takes `sCarrier` or a CLASS_Mothership ship, Beast `bCarrier` or one, others none; ships in hyperspace searched too | 0x00548ad0 | `UnivUpdate.c` |
| `DisableAllPlayersShips`: valid player index only, neither mothership careens; `cat_mimicUnmimic` CAT-TODO (#4) | 0x00548450 | `UnivUpdate.c` |
| `ApplyDamageToCollidingObjects`: no collision damage except a killer collision (attribute bit 0x200 on exactly one object, `CAT_ATTRIBUTES_KillerCollision`): both take the smaller health and the very-large plasma bomb hit effect plays on the killer | 0x00545540 (REWRITTEN) | `UnivUpdate.c` |
| `univBulletCollidedWithTarget(applyDamage, ...)`: new first argument (the 3 callers in `Collision.c` pass TRUE; the exe's beam draw-only frames pass 0, CAT-TODO #3), crystals immune to projectiles/plasma/missiles, the Nomad moon vs its emitters, armour piercing for both heavy defenders, friendly fire always, float effect parameters, bullet-hit glow; the weapon-vs-armour factor is 1 (CAT-PROVISIONAL #3), big-gun shells and Sentinel fields CAT-TODO (#4) | 0x00546a20 | `UnivUpdate.c/.h`, `Collision.c` |
| `univUpdateAllPosVelBullets` death path: homing update for `WEAPON_ENERGY` bullets (`cat_bulletHomingUpdate` 0x00542cb0 with `cat_shipGetAimPoint` 0x00415af0), big-gun shells lose their sensor ping, `univFreeBulletContents`; the burst explosion is keyed on the bullet's burst flag and weapon word (`CAT_BULLET_WEAPON_TYPE`, CAT-PROVISIONAL: -1 until #3 ports the Gun weapon word, so the homing and burst paths do not run yet) | 0x00543030 | `UnivUpdate.c` |

## Subsystem `univupdate`: the update loop, the deferred per-ship update, win/quit and init

| change | exe evidence | files |
|---|---|---|
| `univUpdate`: frame stepping (`catUnivFrameStep`/`catUnivFrameStepsLeft`, 0x00a6ed74/0x00a6ed78), multiplayer autosave (`catMultiplayerAutoSave*`), real-time quit/win (`universe.realtimeelapsed`), speech 0x2001044, no `tacticsGlobalUpdate`, bullet effects recreated after a load (`cat_univRecreateBulletEffectsIfNeeded`), inline dead-object loops; `cat_blastUpdateAll` (#3), `cat_kasSaveLevelNow` (stage 6) and the stats-screen quit (stage 3) CAT-TODO | 0x0054c3f0 | `UnivUpdate.c/.h` |
| quit times set by `singlePlayerGameUpdate` and `singlePlayerMissionCompleteCB` are real time too | 0x005197d0, 0x0051be40 | `SinglePlayer.c` |
| `CheckPlayerWin`: real-time win/quit, non-multiplayer games end only when nobody (skirmish) or player 0 (single player) is left | 0x005485e0 | `UnivUpdate.c` |
| `univSetupShipForControl`: no cloak-visibility pass, no HW1 self-repair, every live ship queued on the deferred list (exe universe "unk1" grow-selection); `univSetupShipsForControl` and `univUpdateAllPosVelShips` run the deferred updates before the housekeeps; madanim link-in only while an animation plays and not for numbered animations; the protected-speed timer (0x00a6ed6c) | 0x0054a230, 0x0054a340, 0x00544f40 | `UnivUpdate.c` |
| the deferred update `cat_univShipDeferredUpdate` 0x005447b0 (partial: the remove-references broadcast of a ship marked with exe +0x18 bit `CAT_OBJFLAG18_RemoveReferences`, `cat_univRemoveObjReferencesFromShips` 0x0054d350, and `cat_univShipSelfRepair` 0x005433d0 for the Beast or upgrade 0x200000; Beast infection, attack queue, Sentinel timers, dissolve, stun/EMP, gun bursts, upgrade end, drift, stealth and `cat_shipUpdateExternalBuildAndAI` are CAT-TODO with owners #2..#5) | 0x005447b0, 0x005433d0, 0x0054d350 | `UnivUpdate.c` |
| bullet helpers: `cat_univRequestBulletEffectsRecreate` / `cat_univRecreateBulletEffects` / `...IfNeeded` (0x0054d240/0x0054d250/0x0054d330), `cat_univDeleteBulletNow` 0x0054d110, `cat_univSetShipPosition` 0x0054d180, `cat_univPlayerHasFlaggedShip` 0x0054d200 | as named | `UnivUpdate.c/.h` |
| ship counters: `catShipsSalvaged`/`catShipsBeastified` [8][145] and kills/losses by class [8][12] with their getters (`cat_univGetShipCountSalvaged` 0x00540030, `...Beastified` 0x00540080, `cat_univShipSalvagedCountInc` 0x005400d0, `cat_univGetNumberShipsPlayerKilled/Lost` 0x005400f0/0x00540110); cleared by `univupdateInit`/`univupdateReset` (save/load of them, 0x00540130/0x005401d0, is SaveGame's, stage 5) | as named, 0x0054af40, 0x0054acd0 | `UnivUpdate.c/.h` |
| `InitializeEngineTrails`: capital-ship trail style 2 for Beast and Taiidan ships, 3 otherwise | 0x00541f40 | `UnivUpdate.c` |
| `univUpdateAllPosVelResources`: the exe clamps resources with attribute 0x200 into a per-object box (new fields at exe +0x1A8..+0x1B0, M2-R4 #3) — CAT-TODO note | 0x005432d0 | `UnivUpdate.c` |
| HW1 aliases retired in `univCreateShip`/`univAddShip`: the Drone, DefenseFighter and CryoTray cases are gone (the exe has no such per-type setup); CAT-TODO header lists the rest of the exe's 7-argument creator | 0x00542320 | `UnivUpdate.c` |
| CAT-TODO headers (exe function, address, owner) on the MODIFIED bodies not ported here: `ApplyDamageToTarget` (#3/#5), `MakeExplosionRockBumpables`, `pointExplosionInSpace`, `univMissileCollidedWithTarget`, `univFindNearestResource` (#3), `univCheckShipState` (#2/#4/#5), `getEnemiesWithinProximity` (#4/#5), `DeclarePlayerWin` (stage 3/5), `univKillOtherPlayersIfDead`, `univKillPlayer` (#5, stage 3), `univUpdateRenderList` (stage 3) | 0x00546250, 0x00545f00, 0x00546780, 0x00547080, 0x00549c50, 0x0054a650, 0x0054a3f0, 0x005487f0, 0x00548c70, 0x0054ce20, 0x0054be80 | `UnivUpdate.c` |

### Cataclysm-only functions next to `UnivUpdate.c` (51, 15,092 B)

24 are in the tree (5,164 B; 22 new this round, `cat_univRealTimeDelta` and `cat_univRemoveEffectReferencesFromShips`
were already there): the five counters, the six mesh-state/binding functions, `cat_bulletHomingUpdate`,
`cat_univShipSelfRepair`, `cat_univShipDeferredUpdate` (partial), `cat_univRemoveBulletReferencesFromShips`,
`cat_univRealTimeDelta`, `cat_univDeleteBulletNow`, `cat_univSetShipPosition`, `cat_univPlayerHasFlaggedShip`, the
three bullet-effect functions, `cat_univRemoveObjReferencesFromShips`, `cat_univRemoveEffectReferencesFromShips`.

27 are left (9,928 B), with owners:

| functions | bytes | owner |
|---|---:|---|
| `cat_shipStealthInit` 0x0053fce0, `cat_shipStealthDetectionUpdate` 0x0053fd90, `cat_shipStealthTargetState` 0x0053ff30 | 599 | M2-R4 #4 (ship systems) |
| `cat_saveShipCounters` 0x00540130, `cat_loadShipCounters` 0x005401d0 | 334 | stage 5 (SaveGame) |
| `cat_univAddMimicResource` 0x005408b0, `cat_univDeleteMimicResource` 0x00548f40 | 776 | M2-R4 #4 (mimic) |
| `cat_univCreateMothershipModules` 0x005420b0 | 385 | M2-R4 #4 (with `univCreateShip`'s 7 arguments) |
| `cat_univShipFOWActivate` 0x00542240, `cat_univShipFOWDeactivate` 0x005422b0, `cat_univShipFOWUpdate` 0x00544b10 | 937 | stage 3 (FOW) |
| `cat_univBeastInfectionUpdate` 0x00543860, `cat_univConvertShip` 0x00543950 | 1,689 | M2-R4 #3/#4 (Beast infection) |
| `cat_shipUpdateExternalBuildAndAI` 0x00543f10 | 2,088 | M2-R4 #5 (external build) |
| `cat_univLimitProtectedShipSpeed` 0x00544e00 | 311 | M2-R4 #2 |
| `cat_univClampResourceTimer` 0x00547530 | 52 | M2-R4 #3 (clamps) |
| `cat_DeclarePlayerWinReportResults` 0x00548750 | 160 | stage 3/5 |
| `cat_univRemoveTargetFromRammingFrigates` 0x00549230 | 54 | M2-R4 #4 |
| `cat_consoleUpgradeShips` 0x0054d0e0, `cat_consoleExperienceShips` 0x0054d1c0, `cat_moDebugKeyGet` 0x0054d3c0, `cat_moDebugKeyValue` 0x0054d3d0 | 195 | stage 3 (console/debug keys) |
| `cat_kbKeySetCB` 0x0054d460, `cat_kbCommandsCanShareKey` 0x0054d6a0, `cat_kbBindingToString` 0x0054d750, `cat_kbCheckBindingConflict` 0x0054d880, `cat_kbLoadKeyBindings` 0x0054da10 (key bindings, not universe code; they only sit next to it) | 2,348 | stage 3 (front end options) |

## Subsystem `identical`: every IDENTICAL body compared, delta-map reviews

All 52 functions the delta map classed IDENTICAL for `UnivUpdate.c` were compared with the exe (decompiled body,
disassembly where Ghidra's fields are wrong). 10 were found modified and reclassed MODIFIED:

| address | function | difference | port |
|---|---|---|---|
| 0x00540c70 | `univAddMissile` | clears the new +0x80/+0x84; the missile ID also goes into the ID table while a multiplayer game is recorded | fields ported; recorder test stage 5 |
| 0x00541300 | `univAddDustCloud` | clears +0x80/+0x84 | ported |
| 0x00541500 | `univAddGasCloud` | clears +0x80/+0x84 | ported |
| 0x005416d0 | `univAddNebula` | clears +0x80/+0x84 | ported |
| 0x00541f40 | `InitializeEngineTrails` | trail style 2 for Beast and Taiidan ships | ported |
| 0x005432d0 | `univUpdateAllPosVelResources` | attribute-0x200 resources clamped into a per-object box | CAT-TODO M2-R4 #3 |
| 0x00547570 | `univRemoveAllTargetReferencesFromMissiles` | 5 s self-destruct | ported |
| 0x00547b70 | `univRemoveShipFromHotkeyGroup` | speech 0x2002076 | ported |
| 0x00548ad0 | `univFindBackupMothership` | per-race backup | ported |
| 0x0054af40 | `univupdateInit` | second grow-selection (unk1), ship counters, 0x00a6ed68 and the protected-speed timer 0x00a6ed6c [audit: was "no ClampedShipList init; ... frame-step ...": the exe does init ClampedShipList (0x00c52d70, the list univUpdateAllPosVelShips runs updateClampedObject over) and does not touch the frame-step globals 0x00a6ed74/78] | ported except the second grow-selection (#5) |

The other 42 match the exe (inlined helpers noted in each review line). Review entries were written to
`ghidra/delta_map_review.json` (`delta_map.py mark`) for all 52 IDENTICAL and all 46 MODIFIED functions of the file
(98 lines; the 11 earlier reviews among them keep their calibration `sample`), then merged into
`ghidra/delta_map.json` (`merge --done m2r4:univupdate-review`). `UnivUpdate.c` is now 44 IDENTICAL / 53 MODIFIED /
1 REWRITTEN (`ApplyDamageToCollidingObjects`); the whole map 3,106 / 1,237 / 77, 1,461 reviewed. [audit: the 44 IDENTICAL are 52 - 10 + 2: `univWipeDerelictOutOfExistance` 0x00549740 and `univupdateClose` 0x0054bbd0 went MODIFIED -> IDENTICAL (inlining / folded `star3dClose`). The audit added reviews for 3 IDENTICAL functions this port edited outside the file (`meshRender` 0x004b4280, `meshRenderShipHierarchy` 0x004b4520, `LoadSpaceObj` 0x00501660: all MODIFIED) and corrected `univupdateInit`'s note; the map is now 3,103 / 1,240 / 77, 1,463 reviewed] The merge also
applied 21 review entries that were already in the committed review file but not yet merged into the map (Universe.c
7, SalCapCorvette.c 3, DefaultShip.c 2, GenericInterceptor.c 2, one each in Globals.c, LevelLoad.c, MeshAnim.c, MEX.c,
Mothership.c, P1MissileCorvette.c, StandardFrigate.c).

## Subsystem `final`: builds and plain-harness runs

Builds: `tools/build.sh --debug` and `tools/build.sh --release` both rc 0 with 0 compiler warnings under `-Werror`
(the only "WARNING" line in the debug log is meson's sanitizer notice); `tools/m2_layout32.sh` OK (the new ILP32
asserts of `CatVertexState`, `mhbinding`, `mhlocalbinding` hold).

| run | mode | binary | milestone | first stop | univUpdateCounter | ASan / leaks |
|---|---|---|---|---|---:|---|
| r4u-ms-etg | etg | after `meshstate` | etg effect self-test done (618 created) | none (rc 0) | | 0 / none |
| r4u-ms-sp | sp | after `meshstate` | universe running | SEGV in `cat_meshObjectMatrixConcat` (fixed: `madBindingListCallback`, see `meshstate`) | | |
| r4u-ms-sp2 | sp | after the fix | universe running | none: still running at 340 s | 1,248 | 0 / none |
| r4u-ds-sp | sp | after `deadships` | universe running | none: still running at 340 s | 1,248 | 0 / none |
| r4u-id-sp | sp | after `identical` ports | universe running | none: still running at 340 s | 1,248 | 0 / none |
| r4u-uu-sp | sp | | stopped by hand: the binary was rebuilt while it ran, result discarded | | | |
| **r4u-final-sp** | sp | final debug build | universe running | none: still running at 340 s | **1,248** | **0 / none** |
| **r4u-final-etg** | etg | final debug build | etg effect self-test done: 618 created, 3,197 deleted, 1,093 left, peak 2,515 effects / 3,012 particle blocks (as r3-integrate-etg) | none (rc 0) | | 0 / none |
| r4u-final-fe | fe | final debug build | Main_game_screen | none: still running at 40 s | | 0 / none |
| r4u-final-statics | statics | final debug build | ship statics self-test done, 143 of 145 types | none (rc 0) | | 0 / none |

UBSan (non-fatal misaligned-member reports, which UBSan prints once per site): the final sp run has new sites in
`cat_univMeshBindingsLinkParents` (UnivUpdate.c:1324/1329, `struct shipbindings`/`struct mhlocalbinding`), the same
types r3-integrate-sp already reported in `MeshAnim.c` (madanim 27, mhlocalbinding 7, shipbindings 1); no new kind of
report, no ASan error.

## What is left, and who owns it

- The rest of `univCreateShip` 0x00542320 (7 arguments, 0x63C-byte ship blocks, droids, damage points, upgrades,
  dock info, external build, modules, experience, stealth, dissolve, names, FOW): M2-R4 #2..#5 per the CAT-TODO block.
- `ApplyDamageToTarget` 0x00546250 with 7 arguments and its callers (`MakeExplosionRockBumpables`,
  `pointExplosionInSpace`, `univMissileCollidedWithTarget`), the Gun weapon word that replaces
  `CAT_BULLET_WEAPON_TYPE` (CAT-PROVISIONAL; homing and burst code paths stay dormant until then), the weapon-vs-armour
  table, the burst flag, asteroid pieces, the resource box clamp, `univFindNearestResource`: M2-R4 #3.
- `univCheckShipState`, `getEnemiesWithinProximity`, the rest of `cat_univShipDeferredUpdate`, the speed multiplier,
  `cat_univLimitProtectedShipSpeed`: M2-R4 #2/#4/#5 as marked.
- Win/lose/kill paths (`DeclarePlayerWin`, `univKillOtherPlayersIfDead`, `univKillPlayer`), `univUpdateRenderList`,
  FOW, the stats screen and console/key-binding functions: stage 3; the counters' save/load: stage 5; KAS: stage 6.
- 27 Cataclysm-only functions next to `UnivUpdate.c` (9,928 B), table in `univupdate`.
- Open: the exe's +0x18 word is two fields in the tree (`SpaceObjHeader.catHeader18` and `Ship.catShipFlags`,
  bridged by `CAT_OBJ_FLAGS18`); M2-R4 #5's header pass should make it one field.
