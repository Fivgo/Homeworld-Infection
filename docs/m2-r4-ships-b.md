<!-- done: systems, derelicts, strike, capital, retire, final -->
# M2-R4 producer 4: ship code B, the Cataclysm-only ships

Status: DONE, unaudited (resumed 2026-10-11 from the unaudited WIP commit 2f1a847; the retire and final subsystems are this run's). Branch `m2`, worktree `.claude/worktrees/m2`, nothing committed by this run. Scope:
docs/m2-r3-summary.md "Plan for M2-R4" row 4 and "What is left of stage 2 part A" item 4 (the provisional ship-state
queries).

Subsystems (the done marker above lists the finished ones):
1. `systems`: the shared Cataclysm ship systems the headers call (disable / EMP shock / drift / helpless / SU
   exempt, dissolve), the Ship fields they need.
2. `derelicts`: ModuleShip (module ships, Dreadnought), Crystal, NomadMoon, NomadFieldEmitter, dBeastMothership,
   CruiseMissile, Slipgate, RepairDroid, MultiBeamFrigate, Swarmer, HiveFrigate.
3. `strike`: Acolyte, ACV (EMP), Mimic, MCV, Leech; the provisional queries moved home.
4. `capital`: RammingFrigate, Sentinel, Worker, Processor, RKO, the sMothership big gun and the Beast-mothership
   bonuses.
5. `retire`: the 22 dormant HW1 types (ids 200-221), `DefenseFighter.c`, the dormant RepairCorvette header and the
   HW1-only ship files.
6. `final`: delta-map review entries, builds, plain-harness runs.

Method: the r6 decompile (`ghidra/exports/cataclysm_gog_r6/decompiled.c`) read function by function
(`tools/m2_fdec.py`, a range dumper in `/tmp/m2r4sb/`), disassembly where Ghidra's types are wrong
(`tools/m2_fdis.py`), the exe's data by virtual address (`tools/m2_exedata.py`), header contents from
`original/scratch-m2/m2r2-shipstatic-tools/headers.txt`. Exe `Ship` offsets map to this tree's fields through
docs/cet-knowledge.md's Ship table; the specifics start at exe Ship+0x638.

## 1. `systems`: disable, helplessness, drift, dissolve

| file | change | exe evidence |
|---|---|---|
| new `src/Ships/ShipDisable.c` / `.h` | `cat_shipSetDisableReason` (moved here from `ShipSelect.c`, where M2-R3 had put it; `ShipSelect.h` includes `ShipDisable.h` so its callers are unchanged); `cat_shipStartDrift`; `cat_shipSetHelpless` / `cat_shipClearHelpless` (the exe's inlined start/end blocks are two static helpers: SOF_Crazy and SOF_Disabled set with the old bits and autostabilize saved, disable reason 9, `ApplyCareenRotationDirectly`; on the end the bits come back, a moving ship stabilises, the EMP stun ends); `cat_shipUpdateDrift` (drift ends on its timer or outside the universe; helplessness ends on its timer or at once for a ship hyperspacing in multiplayer; an EMP-stunned ship's velocity x0.9 per update, 0x008c4710); `cat_shipSetSUExempt` (SPECIAL_2_CatExempt20; the support-unit calls `cat_suShipRemoved` / `cat_suShipAdded` are CAT-TODO M2-R4 #5, ConsMgr) | `cat_shipSetDisableReason` 0x005b3fe0, `cat_shipStartDrift` 0x005b3b40, `cat_shipSetHelpless` 0x005b3bd0, `cat_shipClearHelpless` 0x005b3c80, `cat_shipUpdateDrift` 0x005b3d60, `cat_shipSetSUExempt` 0x005b4020 |
| new `src/Game/Dissolve.c` / `.h` | the whole dissolve block: `cat_shipDissolveInit`, `IsActive`, `Start` (the ship parts clamped to it are unclamped and destroyed first, `ApplyDamageToTarget(part, 1e21, 0, 666, owner, NULL, 2)`; default time 10 s, 0x00878730), `SetDirection`, `Fraction`, `Update` (the cut plane sweeps the collision box: 12 edges from table 0x00878738, 5 sparks per update (0x0087879c) with weights 0.5..5.0 flying to the point at distance / 5), `Stop`; the two effects are `etgSpecialPurposeEffectTable[EGT_SUCK_SHIP]` (sparks; the exe's 0x00c41d48 = table base 0x00c41cc0 + 34 * 4) and `[EGT_RESOURCE_SHIP]` (the stream, 0x00c41d4c, 4 parameters: distance, half the avoid sphere, time left, crystal flag); `cat_vecShipLocalToWorld`, `cat_vecWorldToShipLocal` (the exe links the second with the Worker code; dissolve is its first caller here). The exe divides the cut centre as integers (`(int)(1 / (long long)n)`, 0 for every n > 1, 0x00460a6f); kept | 0x004601d0, 0x00460220 .. 0x00460c70, `cat_vecWorldToShipLocal` 0x005c85b0 |
| `src/Ships/Mothership.c` / `.h` | `cat_effectDeleteNow` (delete an effect at once) | 0x005bbbb0 |
| `src/Game/Timer.c` / `.h` | `cat_timerElapsed`, `cat_timerRemaining`, `cat_timerRestartWithRemainder` (the restarted timer always runs on game time) | 0x00528470, 0x00528510, 0x00528540 |
| `src/Game/SpaceObj.h` | Ship fields at the exe's offsets: `catEmpStunned` +0x528, `catEmpShockCount` +0x52C, `catHelplessTimer` +0x534, `catHelplessSavedDisabled/Crazy/Autostabilize` +0x544/+0x548/+0x54C, `catDriftVelocity` +0x554, `catDissolveEffectsOn` +0x574, `catDissolvePoint` +0x578, `catDissolveTimer` +0x584, `catDissolveEffect` +0x594 | the bodies above |
| `src/Game/UnivUpdate.c` | `cat_univShipDeferredUpdate` now calls `cat_shipDissolveUpdate` (after the Sentinel / protected-speed block, before the Beast stun timers) and `cat_shipUpdateDrift` (after the upgrade block); `univCreateShip` ends with `cat_shipDissolveInit` and, in multiplayer, makes a derelict-race ship that is no crystal and targetable a helpless KAS derelict (catShipFlags bit 4, `cat_shipSetHelpless(ship, 1e21)`; the exe also takes this path while a game is recorded, 0x00a7519c && 0x00a75190: CAT-TODO stage 3/5 recorder, as `univAddMissile`) | `cat_univShipDeferredUpdate` 0x005447b0 (0x005448b8 dissolve, drift after the upgrade block), `univCreateShip` 0x00542320 (0x00542c1b .. 0x00542c87) |
| `src/Game/Blast.c` | `cat_blastAffectShip` starts the drift (`cat_shipStartDrift(ship, 1, 1)`) where M2-R4 combat left a CAT-TODO | `cat_blastAffectShip` 0x00515d30 |

Exe offsets used for the exe's `Ship` in this port (docs/cet-knowledge.md's table is right from +0x23C on; before that
the exe's `SpaceObj` has the two +0x80/+0x84 words and four more dwords, so): position +0x58, velocity +0x64, force
+0x70, coordsys +0x98, rotspeed +0xBC, collision `rectpos` +0xEC, health +0x170, `nonstatvars.thruststrength`
+0x17C, `clampInfo` +0x1BC, specialFlags +0x1C8, specialFlags2 +0x1CC; the specifics start at +0x638.

`cat_empShockShip` 0x005b3900 (same block) is ported with the ACV in `strike`, because it calls the Leech and
ramming-frigate code.

Run `r4sb-sys-sp` (sp, plain harness, debug ASan, 340 s): universe running, `univUpdateCounter` 1,248, ASan 0, no
unsuppressed leak, 33 ship statics, 3,717 file loads (as `r4cb-audit-fixed2-sp`).

## 2. `derelicts`: module ships, derelict objects, swarm ships

Every header below is the exe's (slot contents from headers.txt), its specifics pinned to the exe's
`sizeofShipSpecifics` on ILP32 (`tools/m2_layout32.sh` now syntax-checks these files too), and `shipHeaders`
(`Universe.c`) points the types at it. "nullsub" slots use the new `nullsubStaticInit` / `nullsubShip` /
`nullsubAttackPassive` / `kasfSpecialOff*` (DefaultShip.c: the exe's shared nullsub 0x0049ae30 and kasfSpecialOff
0x00497db0).

| file (header, exe address) | types | what the bodies do | exe functions |
|---|---|---|---|
| new `Ships/ModuleShip.c` / `.h` (ModuleShipHeader 0x008c3720, 0x3C) | sDreadnoughtCannon, sCarrierSupport, the 13 sMothership modules, bMothershipDockingBay, bMothershipEnginesLower, bSectMoship, bMothershipSupport | per-type orphan delay (no key; ModuleShipStaticInit sets 1.0 .. 5.25 s); Init (Beast modules start stunned 1 s + 2.5 s; sMothershipCommand starts animation 0); a module that loses its host (outside an NIS) drifts off after its delay + up to 2 s and explodes when the push ends (howDidIDie 0x309, owner 99, bounty, totalships-1); `cat_moduleShipEject` (push along the host's up and right vectors from table 0x008c3608 by type 0x008c36b0, force 50000..70000, spin about its up vector, disable reason 8, SOF_CatDocking); the module iterator over the host's clamped component-class ships; the docking bay's death takes the ships being built at an sMothership (SPECIAL_2_CatExternalBuildTimer) with it | 0x005af4c0 .. 0x005afee0 |
| `Game/UnivUpdate.c` | | new `cat_univCreateMothershipModules`: the modules of an sMothership / bMothership (cat_univMothershipModuleTypesGet / Beast), clamped as ship parts at the host's position and velocity; in multiplayer an sMothership also gets seven unit-capped modules. Called from `univAddShip` (see deviations). `univRemoveShipFromOutside` hides the host's modules too. `univCreateShip` allocates `shipsInsideMe` also for catTypeFlags 0x8000 (sHiveFrigate, bHiveFrigate, dRKO) | `cat_univCreateMothershipModules` 0x005420b0, `univRemoveShipFromOutside` 0x00547990 (0x00547b02), `univCreateShip` 0x005428d7 |
| `Ships/Carrier.c` | | `cat_carrierDoorAnimUpdate` (for the dock follow-up), `cat_carrierCountSupportModules` (for unitCapCanCreateShip / cmUpdateShipAvailable, M2-R4 #5); the door now uses the shared `cat_madAnimationStartNumber` (MeshAnim.c, the number lookup R4 combat wrote inside Carrier.c) | 0x005af190, 0x005af460 |
| `Game/LaunchMgr.c` | | `cat_lmFillInModuleSlots` (the launch screen's seven module slots from the sMothership's clamped list, weakest support module, packed forward), called from `lmLaunchBegin` | 0x0049e9f0 |
| new `Ships/Crystal.c` / `.h` (CrystalHeader 0x008c3860, 0x0C) | dCrystal0, dCrystal1 | `resourceValue` key per type; a crystal is disabled for good (reason 12), not selectable, friendly; dies with a type-3 blast unless used up | 0x005affd0 .. 0x005b0090 |
| `Game/Salvage.c` | | `cat_salvageDepotProcessCrystal`: clamp, dissolve into the depot's DPT_Crystal_Effect point over twCrystalProcessingTime (speech 0x2001054), pay the dissolved part of the crystal's resource units each update (x87 `__ftol` of value x (1 - fraction)), then free its point, unclamp and delete it (speech 0x2001055); the depot's death stops the dissolve | 0x004f98b0, `cat_salvageDepotDied` 0x004fcc30 |
| new `Ships/CruiseMissile.c` / `.h` (0x008c37b0) | bCruiseMissile | homes on the target's position and velocity beyond 5000, then kamikaze (specialFlags 0x6) straight in; Beast weapon tweaks | 0x005afef0, 0x005aff20 |
| new `Ships/dBeastMothership.c` / `.h` (0x008c3920, 0x18) | dBeastMothership | every slot calls the Beast weapon when the ship has one; starts stunned; slots 8/9 are Mothership's | 0x005b00d0 .. 0x005b0240 |
| new `Ships/NomadMoon.c` / `.h` (0x008c56e8, 0x1C; NomadFieldEmitterHeader 0x008c5778) | dNomadMoon, dNomadFieldEmitter | the moon makes two emitters 3925 above/below it; while big-gun shells fly (or the KAS fires it) it charges 6 s (5 s in an NIS), sends a type-5 blast (sound 0x815) and cools down 11 s; during an NIS the emitters follow it | 0x005bdbb0 .. 0x005be070 |
| new `Ships/Swarmer.c` / `.h` (0x008c6ab0, 0x20) | sSwarmer, bSwarmer | side-step attack (Sect / Beast parameter blocks); the swarm state machine of `cat_swarmerGetState` (docked when outside the ship list) | 0x005c7930 .. 0x005c7aa0 |
| new `Ships/HiveFrigate.c` / `.h` (0x008c4b40, 0xFD8) | sHiveFrigate, bHiveFrigate | six swarmers made inside the hive (one replaced every 15 s); special targets (up to 1000, its own swarmers excluded); the 8-state swarm machine: approach to 7000, random launch delays (`cat_randomExpDelay` x 2), launch, attack, recall, dock; docked swarmers heal 1 HP/s; an aggressive idle hive targets enemies within 7000 every 16th update | 0x005b54b0 .. 0x005b67a0 |
| new `Ships/MultiBeamFrigate.c` / `.h` (0x008c55a8, 0x28) | sMultiBeamFrigate, bMultiBeamFrigate | closes to 3/4 of the beam range (first gun's range x MultiBeamRangeRatio), steadies and strafes right or left (3:1) while its gimbal guns pick the target or any enemy in range | 0x005bd730 .. 0x005bdb10 |
| new `Ships/Slipgate.c` / `.h` (0x008c68c0, 0x30) | dSlipgateSmall, Medium, Large | gate effects per state (closed / opening 2.25 s / open / closing 0.625 s; sizes and colours per gate), paired opening and closing, auto-close 4 s after opening, the waiting list of ships that came through (their escorts and parade ships dock too) | 0x005c6fa0 .. 0x005c7760 |
| new `Ships/RepairDroid.c` / `.h` (0x008c6170, 8) | sRepairDroid, bRepairDroid | StaticInit only (side-step keys) | 0x005c29b0 |
| `Ships/DefaultShip.c` / `.h` | | `nullsub*` / `kasfSpecialOff*` slot bodies; `DefaultShipAttackPassive` exported | 0x0049ae30, 0x00497db0 |
| `Game/Cloak.c` / `.h` | | `cat_randomExpDelay` (1 - e^(r x 100/2^30 x -0.005), the random wait the exe inlines in the cloak, Acolyte, hive and mimic code) shared | |
| `Game/MeshAnim.c` / `.h` | | `cat_madAnimationStartNumber` (the exe's animation-number lookup; its third argument is stage 3) | |
| `Game/SpaceObj.h` | | `Ship.catCreator` (exe Ship+0x4AC, univCreateShip's 6th argument) | |
| `Game/Dock.h` | | prototypes of `ThisShipCanDockWith` and `ClampToWithin` (Dock.c) | |

Deviations, each with a comment in the code:
- **Modules made in `univAddShip`.** The exe's 7-argument `univCreateShip` adds the ship and makes its modules; this
  tree's `univCreateShip` only creates, so the modules are made when a mothership is added (a ship only created, as by
  the fight statistics, gets none). `cat_rmResearchModuleActivated` and the game option 0x8 of 0x008860ec are
  CAT-TODO(M2-R4 #5).
- **`cat_moduleShipEject` for an unlisted type** (sDreadnoughtCannon, sMothershipCommand, the four Beast modules): the
  exe takes the low byte of the module's address as the table index (out of bounds); the port uses direction 0
  (CAT-FIX).
- **Dock purposes** (hive and slipgate `clDock` / `ThisShipCanDockWith`): CAT-PROVISIONAL HW1 stand-ins until the M2-R4
  #3 follow-up; the hive's purpose 2 (swarmers out of the hive) has none, so the swarmers stay inside.
- **Repair-droid proxies** (`droidCreateArray` / `droidUpdateArray` / `cat_droidRenderArray` / `cat_droidFreeArray`):
  render-side objects that crawl over the hull and make welding effects with the wall-clock stream 0; moved to stage 3
  (the UnivUpdate.c CAT-TODOs now name it).
- `SlipgateRemoveShipReferences` compares the partner with the gate itself (0x005c7674), kept.
- The hive's FOW visibility test (`cat_fowCanPlayerSeeShip`) is CAT-TODO(stage 3).

Runs: `r4sb-d1-statics-init` (statics + `/catShipStaticsStaticInit`): rc 0, ASan 0, no leak, 143 of 145 types,
**92 StaticInits ran** (R3: 56). `r4sb-der-sp` (sp, 340 s): universe running, `univUpdateCounter` 1,248, ASan 0, no
unsuppressed leak, 3,718 file loads. `tools/m2_layout32.sh`: OK, 34 ship files.

## 3. `strike`: Acolyte, ACV, Mimic, MCV, Leech; the provisional queries moved home

| file (header, exe address) | types | what the bodies do | exe functions |
|---|---|---|---|
| new `Ships/Acolyte.c` / `.h`, new `Ships/ShipLink.h` (AcolyteHeader 0x008c3260, 0x5C) | sAcolyte, bAcolyte, dAcolyte | interceptor specifics + the link block (`CatLinkInfo`, specifics +0x18, the block every linkable type keeps for the link orders: partner, role, mode, unlink directions, effect); wings open/close (animations 0/1, a random delay before each); the linked missile weapon (`linkTarget`, fired only with the 0x4000000 upgrade and the wings open); a derelict Acolyte fires only with its wings open; move/halt/attack slots. `cat_shipIsAcolyte`, `cat_shipIsAcolyteOrDerelict`, `cat_acolyteCanFireLinkedWeapon` moved here from `ShipSelect.c` | 0x005ad280 .. 0x005adb70 |
| new `Ships/Leech.c` / `.h` (LeechHeader 0x008c4c60, 0x4C) | sLeech | the action the player chose (drain resource units / hull, speech per choice); auto-targeting within 7000 (`cat_leechFilterTargets`); approach, latch to a `DPT_Leech` point (animation 0 opens the body), clamp, drain once per second (RUs into its hold, or hull with the drain effect), leave at 20% / when full, return to dock to unload; shaken off by an EMP (`released`); per-player stealth detection flags (+0x1C); the "no targets" message timers; save/load of the target list. `cat_leechIsLeeching` moved here from `ShipSelect.c`; `Collision.c`'s `cat_leechIsAttached` now reads the real last target | 0x005b6890 .. 0x005b80f0 |
| new `Ships/ACV.c` / `.h` (ACVHeader 0x008c3328, 0xD4) | sACV, bACV | the side-step attack (Sect / Beast parameter blocks, `EMP_Range`, `EMP_Max_Combine_Distance` keys); the EMP: with upgrade 0x200 a ready ACV pulses (special activate) or flies at a special target and pulses within 2500 (4000 for an RKO); the ACVs of a formation within the combine distance pulse together from the one nearest their centre, and every ship the pulse can affect within `EMP_Range` of one of them is shocked with the group's size as the strength (`cat_empCanAffectShip`: not own, not a depot's, not held by own salvagers, not dissolving; EMP-immune unless KAS-paralyzed); then 2 s pulsing, 75 s recharging (`cat_acvEMPChargeFraction` for the UI); the linked attack order for the formation; save/load of the hit list. `cat_acvLinkState` (the byte `cat_selMakeShipsNotLinkedACVs` reads) moved here from `ShipSelect.c` | 0x005adba0 .. 0x005aeed0 |
| `Ships/ShipDisable.c` | | `cat_empShockShip`: a ship that is not turning Beast and not hyperspacing in multiplayer becomes helpless for min(strength, 10) x `TimeShockedByEMP` (restarted with the remainder if already helpless); the first shock releases a latched Leech, a ramming frigate and salvagers; an EMP-immune ship that KAS paralyzed (SPECIAL_2_CatParalyzed, 0x005b3a7f `and 0x4000`) only counts the shocks and spins (the Naggarok EMP fraction is a KAS variable: CAT-TODO stage 6, the exe's fallback kept) | 0x005b3900 |
| new `Ships/Mimic.c` / `.h` (MimicHeader 0x008c4df0, 0x13C) | sMimic (and the shared block of sMCV, p1StandardCorvette) | `MimicSpec`: the disguise (shown type, race, LOD, bindings, colour, radius, posing player, or an asteroid type with its radii and value), the waiting next disguise, fade, mode (12 modes: on, fade in/out, switch, the link modes, three random-delay modes), the per-player detected flags (+0xAC), the fake asteroid, the ram damage scale 2.0. Become a ship type (what each type may pose as; an MCV posing as a fighter needs upgrade 0x10 and unlinks), become an asteroid (1/2/3: camera radius 130/500/800, the collision box of the asteroid type x 1.1), unmimic, the COMMAND_CAT_MIMIC entry `cat_mimicCommand` (upgrade 0x20), the special target; housekeep: fades of 0.05 per update with sounds 0x817/0x818 and speech, the camera refocuses when the size changes, an idle disguised asteroid leaves a real asteroid in the resource list; the radius / camera radius / LOD / scale / apparent player / class / "appears allied, as a resource, as a derelict" queries the renderer, selection, AI and FOW use; save/load. `cat_shipIsMimicking`, `cat_mimicGetApparentShipType`, `cat_mimicGetSelectRadius`, `cat_mimicAppearsAlliedTo/AsResource/AsDerelict` moved here from `ShipSelect.c`, `cat_shipGetRamDamageScale` from `Collision.c` | 0x005b8330 .. 0x005b9c60 |
| new `Ships/MCV.c` / `.h` (MCVHeader 0x008c4d10, 0x13C) | sMCV | the mimic block with the side step in its gap; straight-on attack (too-close 1000, then kamikaze flags 0x6); the passive attack (a disguised MCV in a sphere formation goes kamikaze, a disguised one never fires); the other slots forward to the Mimic bodies | 0x005b80f0 .. 0x005b8310 |
| `Ships/P1StandardCorvette.c` / `.h` | p1StandardCorvette | its spec starts with `MimicSpec` (union with the side step at +0x44); the seven `p1scMimic*` stand-ins are gone (the Mimic functions are called); slot 6 is `MCVAttackPassive`, slot 27 `MCVHaltCommand` (the copies `P1StandardCorvetteAttackPassive` / `HaltCommand` are gone); a disguised corvette goes kamikaze once facing its target (heading . trajectory > 0.5, 0x005be6e0) | 0x005be660, 0x005be6e0, 0x005be7a0 |
| `Game/UnivUpdate.c` / `.h` | | new `cat_univAddMimicResource` (an asteroid on the resource list only, with a resource number), `cat_univDeleteMimicResource` (the DeleteDeadResource steps, then off the list), `cat_univRemoveTargetFromRammingFrigates` (also called from `DeleteDeadResource`, where M2-R4 combat left a CAT-TODO); `DisableAllPlayersShips` unmimics a mimic; `Asteroid.catMimicShip` (exe +0x1A4) | 0x005408b0, 0x00548f40, 0x00549230, 0x00548450 |
| `Game/ShipSelect.c` | | `cat_shipDetectedByPlayer` / `cat_shipStealthDetectedSet` read and write the mimic (+0xAC) and leech (+0x1C) flags; a mimic that becomes undetected stays detected unless its disguise fools that player | `sub_53F9F0` 0x0053f9f0, 0x0053fad0 |
| `Game/CommandLayer.c` | | `clScuttle` unmimics a mimic (also the tutorial's spared sMothership) | 0x0044a150 |
| `Game/Shader.c` | | a mimic's team colour is the one it poses as (`cat_mimicGetApparentPlayerIndex`) | `cat_shGetTeamColour` 0x0050d460 |
| `Game/ObjTypes.c` / `.h`, `Game/Damage.c` / `.h`, `Ships/P1Mothership.h` | | `cat_ShipTypeToRace` (the race ranges table); `DMG_StrikeLightLimit` exported; `P1MothershipAttackPassive` exported (the ACV's slot 6) | 0x004d7170 |
| `Game/SpaceObj.h` | | `CAT_UPGRADE_MimicSmall` 0x10, `Mimic` 0x20, `MimicRam` 0x80 | |

Deviations and what is left, each with a comment in the code:
- The mimic menu (the disguise list of the special-order menu and its save chunks, `cat_mimicInitDisguiseTables`
  0x005b9c60 .. `cat_saveGameMiscChunks` 0x005ba170) is UI: stage 3.
- `cat_clUnlink` (the unlink order) from the MCV's link modes 5/6/7: CAT-TODO(M2-R4 command follow-up, the link
  orders); only the link code enters those modes.
- `cat_mimicBecomeTarget` with an asteroid type other than 1-3 returns (CAT-FIX: the exe indexes the asteroid table
  with garbage; no caller passes such a value).
- `cat_univDeleteMimicResource` frees the asteroid (CAT-FIX: the exe leaks its 0x1B4-byte block).
- Mimic slot 29 (`MimicSlot74Command`, no known caller) stays NULL.
- ACV: fog of war (`cat_fowCanPlayerSeeShip`) stage 3; Leech: the return dock purpose (M2-R4 #3 follow-up),
  `RemoveShipsFromDoingStuff` reasons (command follow-up), the "no target" message (stage 3); the third argument of
  `madAnimationStart` (stage 3).

Runs: `r4sb-strike-statics` (statics + `/catShipStaticsStaticInit`): rc 0, ASan 0, no leak, 143 of 145 types,
**106 StaticInits ran** (37 types have none: the exe's NULL slot 1, sWorker/bWorker/sProcessor/bProcessor until
`capital`). `r4sb-strike-sp` (sp, 340 s): universe running, `univUpdateCounter` 1,248, ASan 0, no unsuppressed leak,
3,720 file loads. `tools/m2_layout32.sh`: OK, 43 ship files.

## 4. `capital`: RammingFrigate, Sentinel, Dreadnought, RKO, Worker, Processor, the big gun

| file (header, exe address) | types | what the bodies do | exe functions |
|---|---|---|---|
| new `Ships/RammingFrigate.c` / `.h` (RammingFrigateHeader 0x008c5e38, 0xA4) | sRammingFrigate, bRammingFrigate | Sect / Beast statics (RamDamage, cutting-beam damage, push / wait times); the ram: turn to the target, approach, ram at 1.2x (2x upgraded, 0x100) speed, latch on a snap point of the target (`cat_rammingFrigateLatchTarget`), push it away for RamPushTime with the thruster effects (the cutting beam with upgrade 0x2000000 damages it, a Beast ram infects with 0x400000 immunity rules), cool down; `cat_rammingFrigateCollide` (two rams bounce, a ram hitting its target latches, an infecting ram infects), release / force release, the trail and speed queries; the target selection (`ramSetTarget`), save / load. `cat_shipIsLiveWorker`, `cat_shipIsSalvager` users. Now also called from `collCheckShipShipColl` / `collCheckBigShipSmallShipColl` (`Collision.c`, both CAT-TODOs of M2-R4 combat), `cat_blastAffectShip` (`Blast.c`) and `cat_univRemoveTargetFromRammingFrigates` | 0x005bf760 .. 0x005c21e0 |
| new `Ships/Sentinel.c` / `.h` (SentinelHeader 0x008c6830, 0x350) | sSentinel, bSentinel | the charge (4000 / 6000 / 10000 by upgrade level, refilled 4 / 8 / 16 per update, halved in single player), depletion, the force-field formation update (links to the partners of a wall or sphere formation, in position, field level 0 .. 1), bullet and missile hits on the field (effect, charge taken), the formation-command veto (slot 34), the queries `cat_shipIsSentinelFieldActive` / `InField` / `InSphere` (moved here from `ShipSelect.c`); `Collision.c` reads its link list (`cat_sentinelFieldLinks`), the bullet test calls `cat_sentinelFieldBulletHit`, and `collCheckMissileShipColl` now has the exe's missile-vs-field test; `cat_univShipDeferredUpdate` ends a DFG disruption | 0x005c5af0 .. 0x005c6fa0 |
| `Ships/DFGFrigate.c` / `.h` | bDFGFrigate, tDFGFrigate | `DFGFrigateHouseKeep` (slot 10): on 3 of 4 updates every enemy force-field formation whose lead ship is in the field is disrupted for 2 s; `univDFGFieldEffect` returns TRUE after it detonated a big-gun shell (`cat_bigGunBulletExplode`, then the shell, its effect and ping are deleted; `Collision.c` drops the bullet then); `cat_dfgFieldContainsPoint` moved here from a stand-in in `Physics.c` | 0x005b0cf0, 0x005b0940, 0x005b0df0 |
| new `Ships/Dreadnought.c` / `.h` (DreadnoughtHeader 0x008c4130, 0xD68) | sDreadnought | the missile destroyer's regeneration and volley timing; upgrade 0x400 grows the sDreadnoughtCannon module (`cat_dreadnoughtCreateCannon`, clamped, its build progress on a timer), the special activate fires the cannon 1 s later as a type-4 blast every 70 s (rammers let go first); death, references, save fix-ups | 0x005b0e50 .. 0x005b13e0, 0x005b3870 |
| new `Ships/RKO.c` / `.h` (RKOHeader 0x008c6458, 0xD0) | dRKO (the Naggarok) | the 18-state eating machine: every 2 s pick the ship approaching fastest (frigate .. carrier class, not being built, inside the universe; a Taiidan ship or a carrier preferred), plan the approach point, turn (forced after 10 s), wait distance x 5e-5 s, fly to it (ships within 10000 slowed and set drifting), check the target has not moved 400, then swallow it: disable reason 11, halted, dissolved into the "weapon" dock point over its RKOEatMeTime (16 s), clamped, its missiles counted as RKOGetsMissiles shares; then flee 40000 ahead for 64 s (4 s after a Taiidan meal) to random points; brake when moving off its heading; the big-gun warning (danger sphere 68000 around the shot, a random point outside it, 70 s); a move order aborts and kills the meal; `cat_shipReleaseAttachedRammers`, `cat_vecWithinDistance(Sq)`, the missile count queries. `cat_shipIsRKO` moved here from `Collision.c`; `gunShoot` no longer fires at the RKO's meal | 0x005c3df0 .. 0x005c59a0, `gunShoot` 0x0048b5b0 |
| new `Ships/Worker.c` / `.h` (WorkerHeader 0x008c6b50, 0x48) | sWorker, bWorker | statics (jackRetractTime 6, the Grinder and gun0 points, the repair keys repairDistance / repairCone / repairBeamWidth; the first gun's default orientation from its normal in the mesh's first object); the special target is repair, salvage (Salvage.c) or none by `specialMode`; the repair: the nearest damaged allied ship no Worker repairs (a class with `twRepairRate` 0 is not repaired), fly to a standoff of 5 x its radius (800 .. 1500) around its box (and modules), face it within the cone, extend the jack (animation 0), beam (width from 2 tan 10 to 2 tan 26 x distance, at most its diameter) and give its most damaged part `twRepairRate` per second (x10 in the tutorial), chatter when done; with no damaged ship, the least busy one at full health; the jack folds back (animation 1) when its time is up; the harvest callback `cat_workerHarvestsResource` (the code at 0x005c9130): fly within 2000, turn, jack, then beam (EGT_WORKER_RESOURCE_SOLID for asteroids, EGT_VORTEX_EFFECT for nebulae and dust) and `CollectResources` each update, attack a mimic posing as the asteroid, steady when the resource drifts out of 3000 or the facing; `cat_shipIsLiveWorker` moved here from `CommandLayer.c` | 0x005c7ac0 .. 0x005c9bf0 |
| new `Ships/Processor.c` / `.h` (ProcessorHeader 0x008c5b60, 0x1C8) | sProcessor, bProcessor | the salvage depot (Salvage.c) plus up to four auto-repair beams on its DPT_AutoRepair points: with upgrade 0x40000 each beam picks the own damaged ship with the lowest health fraction within `twProcessorAutoRepairDistance` and the point's cone, gives its most damaged part `twRepairRate` per second, and lets go when it is healed, hidden or out of the cone (next search after `twProcessorAutoRepairUpdateDelay`, every `UpdateRate`); no beams while disabled, salvaged, hyperspacing or EMP-stunned | 0x005be8f0 .. 0x005bf3a0 |
| `Ships/Mothership.c` / `.h` | sMothership, bMothership | the big gun replaces the idle stand-in of `SectMothershipHouseKeep`: aim at the target (0.985 dot, 20 s aim time, heading 0.999 / 0.2), charge 11 s with the charge effect (EGT_BIG_GUN_CHARGING, spinning faster), the module's firing animation and "big gun has fired" message, shoot the shell (`gunShoot` returns it; its range is the distance to the target, +-10% in multiplayer beyond 20000), warn the RKO, steady; the reload wait state; `cat_bigGunProcessFireCommand` (a scattered aim point 0.1 x distance off in multiplayer; single-player scatter 0x00a77af8 is never set), `cat_bigGunChargeFraction`, `cat_bigGunMaxRange` (60000 / 90000), `cat_bigGunDamageFalloff` (full to 35000 / 30000, then linear, at least 0.01), `cat_bigGunBulletExplode(At)` (RKO notified, blast with that falloff, sound 0x1010); the Beast mothership's system bonuses (speed +0.2, damage 0.15 instead of 1.0, research x2, build x2) and `cat_beastMothershipSystemActive`. `cat_mothershipBigGunActive` moved here from `ShipSelect.c` | 0x005bb7e0 .. 0x005bd6e0 |
| `Game/UnivUpdate.c` | | a shell that dies or hits explodes (`cat_bigGunBulletExplodeAt`, both CAT-TODOs of M2-R4 univupdate/combat); `cat_univShipSelfRepair` gives a Sect mothership's damaged modules their repair first (by module priority, 7 / count x damageModifier x rate) and skips a Beast mothership with an active system; a Swarmer's kill counts for its hive; `univRemoveAllShipReferencesFromSpecialShip` clears the depot, big-gun/repair and infector pointers; `univWipeShipOutOfExistence` frees the two Cataclysm binding blocks; `univSetupShipForControl` clears the idle-Worker count; `univCreateShip` calls `cat_shipStealthInit` | 0x00543505, 0x005431d6, 0x00546a48, `ApplyDamageToTarget` 0x00546250, 0x00547640, 0x005477d0, 0x0054a26b, 0x00542c15 |
| `Game/ShipSelect.c` / `.h` | | `cat_shipGetSpeedMultiplier` adds the Beast speed bonus and `cat_rammingFrigateSpeedMultiplier`, `cat_shipGetDamageMultiplier` the Beast damage bonus (both CAT-TODOs of M2-R4 combat); new `cat_shipStealthInit` (mimic and cloak types start detected, the leech not) | 0x00514940, 0x005148f0, 0x0053fce0 |
| `Game/ResCollect.c` / `.h` | | Workers harvest with `cat_workerHarvestsResource` (saved as index 2); `CollectResources` exported | 0x004f12c0, 0x004f2090, 0x004f20e0 |
| `Game/Salvage.c` | | the Worker / Processor blocks are typed (`WorkerSpec`, `WorkerStatics`, `ProcessorSpec`); the size guards and CAT-PROVISIONALs for the default header are gone | |
| `Game/Tweak.c` / `.h` | | `twRepairRate[CLASS_*]` (12 entries, 0x00ba2bc0) and `twProcessorAutoRepair{Distance,ConeAngle,UpdateRate,UpdateDelay,BeamWidth}` (0x008ac5f0 .. 0x008ac600, the exe's defaults) | the exe's tweak table |
| `Game/SpaceObj.h` | | Ship `catRKOMissilesTaken` (+0x518), `catRepairIdleWorkers` (+0x600), `catRepairWorkers` (+0x604) | |
| `Game/Gun.c`, `Game/Physics.c`, `Game/Blast.c` | | comments and the stand-ins above | |

Deviations and what is left:
- The big gun's fire command from the UI (`cat_bigGunFireCommand` 0x005bc0c0: pie point mode, sensors manager) is
  stage 3; the point order that calls `cat_bigGunProcessFireCommand` (COMMAND_CAT_POINT, the CommandToDo's point
  block) is an M2-R4 command follow-up, so nothing aims the big gun yet. `cat_bigGunProcessFireCommand` takes the ship
  and the point instead of the CommandToDo.
- CAT-FIX: the exe tracks the big-gun shell before testing it for NULL; the port tests first.
- `cat_mothershipModuleSlotSet` 0x005bd6a0 (`cat_clWrapSetModuleSlot`) is a command-layer wrapper: M2-R4 command
  follow-up; the beast-system multiplayer path likewise.
- The stealth detection update (`cat_shipStealthDetectionUpdate` 0x0053fd90) needs the FOW radius and the research
  tech sets: CAT-TODO(M2-R4 #5); until then a stealth ship keeps its initial detected flags.
- Ramming latch and cutting beam use the mesh hit location (`cat_meshLineIntersect` 0x004b5910): CAT-PROVISIONAL,
  stage 3. RKO KAS variables (G_NaggarokEMP, G_Traitor): CAT-TODO stage 6. FOW visibility tests: stage 3.
- The renderer halves stay stage 3: Sentinel field drawing, Dreadnought charge glow, Processor/Worker beams are made
  here but drawn by the effect code.

Runs: `r4sb-cap1-statics` / `r4sb-cap-statics` (statics + `/catShipStaticsStaticInit`): rc 0, ASan 0, no leak, 143 of
145 types, **108 StaticInits ran** (35 types have none: the exe's NULL slot 1, including sProcessor/bProcessor).
`r4sb-cap1-sp`, `r4sb-cap-sp` (sp, 340 s): universe running, `univUpdateCounter` 1,248, ASan 0, no unsuppressed leak,
3,723 / 3,725 file loads. `tools/m2_layout32.sh`: OK, 45 ship files.

## 5. `retire`: the 22 dormant HW1 types, DefenseFighter.c, the dormant RepairCorvette header

The WIP commit 2f1a847 had already deleted the ids (ShipDefs.h: the 22 HW1 types once at 200..221 are gone, the
alias block's comment says so), the 14 HW1-only ship files (DDDFrigate, DefenseFighter, Drone, LightCorvette,
P2AdvanceSwarmer, P2FuelPod, P2Mothership, P2MultiBeamFrigate, P2Swarmer, P3StandardShip, Probe, ResearchShip,
ResourceController, SensorArray: 28 files, `Ships/meson.build`, `Ships/Ships.h`) and the `RepairCorvetteHeader`
(RepairCorvette.c keeps only the shared repair-effect helpers the Cataclysm repairers call: `startRepairEffect`
0x005c22c0, `stopRepairEffect` 0x005c23b0, `ModifyRepairEffect` 0x005c2410, `refuelRepairShips` 0x005c24e0; its
comment says why), but the tree did not build: 43 compile errors and 2 link errors in HW1 code keyed on the dropped
types. This run removed that code (`ninja -k 0` until clean), each site with a CAT comment; where the exe has a
replacement test the port has it too:

| file | what went | exe evidence |
|---|---|---|
| `Game/Blobs.c` | the Drone skip of the sub-blob mass sum; the Probe `BTF_ProbeDroid` flag and the `probeActive` macro; the enemy-probe detection (`BCE_ProbeDetected` chatter, its two locals) | `bobSubBlobListCreate` 0x00426510 and `bobBlobItemize` 0x004271d0 test no ship type (grav-well generator only) |
| `Game/Collision.c` | the DefenseFighter collision-sphere exemption (HW1 also exempted the DFG frigate; combat had kept this half), the `DefenseFighterReportBullet` block, the `SpecialEffectFlag 0x2` `DefenseFighterBulletRemoval` on bullet deletion, the `CAT_BULLET_HW1DefenseLaser` case | `collCheckBulletTargetColl` 0x0043cd90 (combat's audit: every target tested against its sphere first) |
| `Game/Physics.c`, `Game/SpaceObj.h` | the dormant `CAT_BULLET_HW1DefenseLaser` bullet type (100) and its no-move case | |
| `SDL/render.c` | `onRenderEffect`'s defense-laser half (`defenseFighterAdjustLaser`, the laser-length `etgEffectUpdate`) and the default-line laser branch of `rndMainViewRenderFunction` | the two unresolved symbols at link |
| `Game/ConsMgr.c` | `cmClassCapClassRemap`'s ResourceController case | |
| `Game/Formation.c` | the two ResearchShip parade cases of `processMilitaryParadeToDo` (move closer; keep heading) and their `dontmovecloser` / `dontadjustheading` labels | 0x00485280 tests no ship type |
| `Game/KASFunc.c` | `kasfSwarmMoveTo`: every ship of the team goes into the swarmer selection, the pod selection stays empty (HW1 split on P2AdvanceSwarmer); `SelectionSwitchPlayerOwner`: both `DDDFrigateSwitchSides` calls | 0x00492610 (whole body: no type test, all ships added to one selection); 0x00493f80 |
| `Game/LevelLoad.c` | three ResearchShip `rmActivateFreeLab` blocks and two `addMonkeyResearchShip` blocks (the parade / column placement is unconditional now) | `scriptSetShipsCB` 0x004a13f0 (tests 0x12, 0x35, 0x3e, 0x4c, 0x62 only) |
| `Game/ObjTypes.c` | the `HW1LEGACY_FIRST_SHIP` / `LAST_SHIP` static assert (the ids are gone) | |
| `Game/PiePlate.c`, `Game/Tactical.c` | the Drone skips | `toAllShipsDraw` 0x00528aa0 |
| `Game/ShipView.c` | the Probe dispatch-speed stat | |
| `Game/SinglePlayer.c` | `singlePlayerNisStoppedCB`: the custom formation after NIS 1 takes the player's **sRecon** (Sect) or **bRecon** (Beast) ships instead of HW1's LightInterceptors; `SetWarpFleetCB`'s ResearchShip lab; `spGoNowHyperspaceCB`'s `DDDFrigateDockAllDronesInstantly`; `CalculateRollCall` counts every ship inside (no DDDFrigate exemption); `TellShipsToDockForHyperspace`'s two type calls | 0x0051b430 (0x0051b4a0 .. 0x0051b4f0: race 0 -> type 0, race 1 -> type 0x21); 0x005173a0; 0x005176e0 (0x00517707) |
| `Game/SoundEvent.c`, `Game/SoundEventStop.c` | the CryoTray exemption of the capital-ship death speech; the `>= Drone` skip inside the `#if 0` block of `soundEventUpdateNew`; the DefenseFighter instant burst stop (always `SOUND_FADE_STOPTIME`) [audit: the exe's body also clears `soundevent.gun` (Ship+0x48C) after the stop, which HW1 and the port did not; added] | `soundEventShipDied` 0x0051f960; `soundEventBurstStop` 0x00521710 (whole body: `soundstop(handle, 0.5)`) |
| `Game/SpeechEvent.c` | the resource-ship damage warning tests `sWorker || bWorker` where HW1 tested ResourceController | `speechEventUnderAttack` 0x00523190 (0xe, 0x31; the exe merges the branch with the carrier / heavy-cruiser one: stage 3) |
| `Game/Stats.c` | both fighting-ship callbacks: the frigate exceptions are **both** race variants (bDFGFrigate, tDFGFrigate, kAdvanceSupportFrigate, tAdvanceSupportFrigate), the corvette ones both SalCaps, every fighter fights (the aliases had covered one variant each) | 0x00524b30 / 0x00524bd0: types 0x2d, 0x3c, 0x4a, 0x4e; 0x48, 0x59 |
| `Game/Tactics.c` | the RepairCorvette "don't dodge while repairing" order type; the Probe speed | `tacticsGetShipsMaxVelocity` 0x00529450 (whole body: no type test) |
| `SDL/mouse.c` | the RepairCorvette dock clause of the double-click, the unreachable ResearchShip / SensorArray / DDDFrigate bodies the WIP had left after removed `case` labels, the RepairCorvette `MCF_Dockable` case (every strike craft is `MCF_SmallDockable`) | stage 3 UI, no exe name |
| `SDL/utility.c`, `SDL/utility_mac.c` | the teaser NIS no longer loads Drone statics for a DDDFrigate | |

[audit: the table above lists only the resumed run's 22 files. The WIP commit 2f1a847 had already removed the
dropped-type code of about 20 more files, which this report did not name: the stage-4 AI files `AIAttackMan.c`
(`aiaCleanUpSwarmers` / `aiaProcessSwarm` / the P2 attack manager), `AIDefenseMan.c`, `AIEvents.c`, `AIFleetMan.c`
(`aifP2FleetCommand`), `AIHandler.c`, `AIMoves.c`, `AIOrders.c`, `AIResourceMan.c`, `AIShip.c`, `AITeam.c`,
`AIUtilities.c` (where the AI asked for a LightInterceptor, LightCorvette, Probe, RepairCorvette, ResearchShip or
ResourceController the request is dropped or another type stands in, each marked `CAT-TODO(stage 4 AI)`);
`Dock.c` (-2,139 lines: the ResearchShip, RepairCorvette, ResourceController, P2Mothership, P2FuelPod, DDDFrigate and
Drone dock code and their `dockInitializeCustomFunctions` cases; the exe has none of these dock types, M2-R4 combat's
35-type table); `CommandLayer.c` (`selectionHasSwarmers`, the Probe / DefenseFighter / DDDFrigate / TargetDrone /
LightDefender tests); the `ConsMgr.c` key, disabled and available tables; the `InfoOverlay.c` overlay list;
`Bounties.c`, `Formation.c` (`need_TargetDrone_Solution`, the DDDF drone sphere), `MadLinkIn.c`, `Sensors.c`,
`ResearchGUI.c`, `SDL/mainrgn.c`. The in-function edits among them carry CAT comments; the whole-function and
table-row deletions (about 100 hunks) carry none, which the porting convention asks for.]

Not retired here, with a reason: `Ships/CloakGenerator.c` tests the `Mothership` / `Carrier` aliases (the file is
not in this scope; M2-R3 ships-a's). The `R1` / `R2` race aliases (RaceDefs.h) are not used by the new code
(`RACE_Sect` / `RACE_Beast`).

After this subsystem no whole-word use of a dropped type name is left in `src/Game`, `src/Ships`, `src/SDL`,
`src/Missions` outside comments and string tables (checked with a grep over the 29 identifiers, the `str*` string
ids excluded).

Builds: `tools/build.sh --debug` rc 0, 0 warnings; `tools/build.sh --release` rc 0, 0 warnings;
`tools/m2_layout32.sh` OK (45 ship files, every `sizeofShipSpecifics` assert holds).

Run `r4sb-retire-statics` (statics + `/catShipStaticsStaticInit`, plain harness, debug ASan): rc 0, ASan 0, no
unsuppressed leak, 143 of 145 types loaded (2 not found: the disabled kCloakGenerator / tCloakGenerator .shp), 27
other .shp (170 of 173 .shp), **108 StaticInits ran** (`staticinit=next`), 35 types with the exe's NULL slot 1
(`staticinit=none`); unchanged from `r4sb-cap-statics`.

## 6. `final`: delta-map review entries, builds, runs, what is left

### Delta-map review entries (ghidra/delta_map.json, `done_subsystems` += `m2r4:ships-b`)

Class corrected from IDENTICAL to MODIFIED (the HW1 body has a test on a dropped type; the exe has none):
- `kasfSwarmMoveTo` 0x00492610 (whole body read; reviewed): every team ship goes into one selection.
- `soundEventBurstStop` 0x00521710 (whole body read; reviewed): always `soundstop(handle, 0.5)`.
- `soundEventShipDied` 0x0051f960, `speechEventUnderAttack` 0x00523190, `bobSubBlobListCreate` 0x00426510 (partial
  reads, the type tests only: not marked reviewed; the change text says what was read).

`review_note` added (partial reads, the dropped-type branch only; the rest of each delta belongs to its owner):
`bobBlobItemize` 0x004271d0, `processMilitaryParadeToDo` 0x00485280, `SelectionSwitchPlayerOwner` 0x00493f80,
`scriptSetShipsCB` 0x004a13f0, `singlePlayerNisStoppedCB` 0x0051b430, `toAllShipsDraw` 0x00528aa0,
`tacticsGetShipsMaxVelocity` 0x00529450, `CalculateRollCall` 0x005176e0, `spGoNowHyperspaceCB` 0x005173a0.
Dead-code notes on the exe's bodies of the dropped types: the 10 `DefenseFighter*` functions (0x005b0420 ..
0x005b08b0) and `RepairCorvetteStaticInit` / `Init` / `Died` / `SpecialTarget` (0x005c21e0, 0x005c2240, 0x005c2280,
0x005c2990). Counts after: IDENTICAL 3,086, MODIFIED 1,257, reviewed 1,532. No edited IDENTICAL function was found
in the Ships files this producer changed (checked: the IDENTICAL entries of Carrier.c, DFGFrigate.c, DefaultShip.c,
RepairCorvette.c against the diff's hunks; `DFGFrigateAttack` appears only as a hunk context of the new
`DFGFrigateHouseKeep`).

[audit: the Game files were not checked the same way. The retire step (WIP 2f1a847) edited 20 more functions the map
still called IDENTICAL, all by removing an HW1 test on a dropped type, with no entry: `aiaPriorityShipsConstraints`,
`aidSetupResourceDefenseTeams`, `aifAssignNewShips`, `aifFleetCommand`, `aihFastDefenseDistressHandler`,
`aihSlowDefenseDistressHandler`, `aimProcessTempGuard`, `airAddNewShip`, `airShipDied`, `aishipPrecalcInfo`,
`aiuFindFighterVulnerableEnemyShips`, `aiuFindUnarmedEnemyShips`, `aiuMakeShipsOnlyDangerousToMothership`,
`aiuRateBlobs`, `aiuRateShip`, `aiuUnitCapCanBuildShip`, `BreakupIntoLeaderWingman`,
`LaunchAllInternalShipsOfPlayerThatMustBeLaunched`, `formGroups`, `isCapitalShipStaticOrBig`. The audit reclassified
them MODIFIED (a type id that does not exist in Cataclysm cannot be tested by the exe; the r6 bodies' own type tests
are in each `review_note`, partial reads except `aiaPriorityShipsConstraints` and `isCapitalShipStaticOrBig`, whose
whole bodies test both race variants and, for the frigate constraint, the Processors: stage 4 / command follow-up).
Counts after the audit: IDENTICAL 3,066, MODIFIED 1,277, REWRITTEN 77, reviewed 1,532.]

### Builds and layout
`tools/build.sh --debug` rc 0, 0 warnings; `tools/build.sh --release` rc 0, 0 warnings (-Werror);
`tools/m2_layout32.sh` OK (45 ship files, every ILP32 `sizeofShipSpecifics` assert holds).

### Plain-harness runs (tools/m2_harness.sh, silent; debug ASan binary copied to /tmp/m2r4sb/bin6)
| run | mode | result |
|---|---|---|
| `r4sb-retire-statics` | statics + `/catShipStaticsStaticInit` | rc 0, ASan 0, no unsuppressed leak; 143 of 145 types loaded (the two disabled cloak-generator .shp not found), 170 of 173 .shp; 108 StaticInits ran, 35 types with the exe's NULL slot 1 |
| `r4sb-retire-sp` | sp, 340 s | milestone "universe running", `univUpdateCounter` 1,248 at the timeout (rc 124 = timeout, as every sp run since R3), ASan 0, no unsuppressed leak, 3,723 file loads, 33 ship statics (Mission01); first stop: none (still running at the timeout) |

DONE WHEN: every ship type's `shipHeaders` row is the exe's header (a script compared `Universe.c` with
headers.txt: 143 of 145 rows equal; kCloakGenerator / tCloakGenerator keep `DefaultShipHeader` where the exe has NULL,
the accepted default, because `InitStatShipInfo` dereferences the header and the exe never loads those two .shp);
no HW1-only ship type or file is left; statics 143 / 145 (170 / 173 .shp) unchanged; sp as far as before, ASan 0.
[audit: slot by slot, the 22 new headers match headers.txt and the exe's raw dwords (Acolyte, Worker and Sentinel
re-read from 0x008c3260 / 0x008c6b50 / 0x008c6830) with the typed nullsub / kasfSpecialOff stand-ins, except two
slots left NULL on purpose: Mimic slot 29 (`MimicSlot74Command`, section 3) and `WorkerHeader` slot 26
(`kasfSpecialOff` in the exe; the slot has no caller, Worker.c's header comment says so but this report did not).]

### Counts
Whole producer (d407410 .. working tree): 169 files (48 added, 28 deleted, 93 modified), +14,036 / -8,147 lines;
`src/Ships` 91 files (+12,217 / -4,360), `src/Game` + `src/SDL` 71 files (+1,621 / -3,774); 48 ship .c files now
(22 new Cataclysm ship files + Dissolve.c + ShipDisable.c + ShipLink.h); 615 added lines carry a CAT comment.
This run (the resume): 22 source files (retire) + the report + the delta map.

### What is left and its owner
- M2-R4 #3 (dock follow-up): the dock purposes of the hive, slipgate and leech (`clDock` / `ThisShipCanDockWith`
  stand-ins), the hive's purpose 2 (swarmers out of the hive), the carrier door.
- M2-R4 #5 (ConsMgr / research / support units): `cat_suShipRemoved` / `cat_suShipAdded`, `cat_carrierCountSupportModules`
  users, `cat_rmResearchModuleActivated`, the stealth detection update (FOW radius, tech sets), the 7 module slots'
  unit caps.
- M2-R4 command follow-up: the link orders (`cat_clUnlink`), COMMAND_CAT_POINT for the big gun, `cat_clWrapSetModuleSlot`,
  `RemoveShipsFromDoingStuff` reasons, the tactics speed multipliers.
- Stage 3 (renderer / UI / sound): the mimic menu and its save chunks, the big-gun fire command UI, the repair-droid
  proxies, the FOW visibility tests, `madAnimationStart`'s third argument, the Sentinel field / Dreadnought glow /
  beam drawing, the research and sensors GUI flags of the modules in mouse.c, the merged speech branch of
  `speechEventUnderAttack`.
- Stage 6 (KAS / single player): the KAS variables of the RKO and Naggarok EMP, the NIS-stop formation's KAS flow.
- Audit (this producer's ask): sections 1-4 were written by the cut-off run and committed unaudited in 2f1a847;
  section 5 by this run. The 41 CAT-TODO / CAT-PROVISIONAL comments in the new ship files name their owner.
