<!-- done: vclock, recorder, loss, combat, reviews, rebaseline -->
<!-- subsystems: reviews, recorder (pkts tail + bigotry), loss, combat, vclock, rebaseline -->
# M2-R10 tests-leftovers: the tests and the oracle's leftovers (producer 3)

Base commit `3ea7b12` (producer 2's audited tree); the work is `git diff 3ea7b12` in the m2 worktree. Six subsystems:
**reviews** (`scriptSetTweakableGlobals` and the sample of the 148 unflagged rows), **recorder** (the `.pkts` header
tail and the skirmish test's bigotry option), **loss** (the deliberate-loss test of the failed-jump return), **combat**
(the mission self-test's combat), **vclock** (the mission runs on the virtual audio clock), **rebaseline** (the 17
mission streams, once). `<R>` is `original/scratch-m2/m2r2-harness/runs`, `<S>` is `original/scratch-m2/m2r10-tests`.

Status: DONE (M2-R10 producer 3; nothing committed by the producer, the auditor checkpoints).

## 1. vclock: Mission06's `rng_all` and the virtual audio clock (item c)

**The cause was not the mixer.** The mission self-test runs with sound and speech off (`CatMissionTestSet`:
`enableSFX` / `enableSpeech` FALSE), so `soundEventInit` returns before `soundinit` and no mixer or stream thread runs at
all: decision 94's suspicion (the mixer's `RANDOM_SOUND` on the wall clock) could not hold. An experiment binary (scratch
only, `<S>/exp`: the tree plus a per-tick dump of all 27 streams' `n` words and a backtrace of chosen draws, never in the
tree) ran Mission06 four times: two runs agreed, two split at **tick 2081 in stream 5, `RANDOM_NEBULAE`, on the main
thread** (R9's pair split at 2081 too). The draws are `nebRenderTendril`'s first geometry build of a tendril
(`realRand`, `nebCreateCylinder`: 4 draws a tendril) inside `rndMainViewRenderFunction`; at 2080 the camera jumps to the
recon at (822, -66966) and nebula 4's tendrils come into view. With the camera bit-identical in both runs (logged every
frame), one run built 10 tendrils and the other none: the clip test in `nebRenderNebula` reads
`tendril->midpoint`, which only `nebTendrilBounds` sets, from the first `nebRenderTendril`. Before that the field is
whatever the heap block held (ASLR'd pointers in this process). **The exe is the same**: `nebInitTendril` 0x004c7d50
sets flags, counter, a / b, prev / next, direction, the LODs and the colour but not the bounding sphere, and
`nebRenderNebula` 0x004cadb0's inlined clip test reads `(tendril->midpoint)` for every active tendril. Stream 5 is
render-only after the level load (`chunkRandomVelocity`'s other caller `nebUpdateChunk` has none), so only `rng_all`
saw it.

**Fix** (`src/Game/Nebulae.c`, `nebInitTendril`, **CAT-FIX**): `radius` and `midpoint` start at zero (what a fresh pool
block holds). **Mission06 four times** (`r10p3-v-m6-{a,b,c,d}`, `--mission 6 --mission-test-ticks 90000 --timeout
3600`): 7/7 PASS, done 31011, ASan 0, all four **byte-identical `d0ac2eeb9a668f8b`**; against producer 2's audited
`r10a2-m6-b` and `-a`: **`rng_all` only, from tick 22235** (the fields of every other kind IDENTICAL).

**The virtual audio clock** (`src/SDL/main.c`, `CatMissionTestSet`, HWI-TEST): every `/catMissionTest` run (missiontest
Mission01 .. 17 and the warp start) sets `soundVirtualClock`, as the tutorial leg does (decision 94's default). With sound
and speech off it changes nothing in these streams today (no `soundinit`); it keeps the mixer and the stream on the game
thread if a mission run ever turns sound on. A normal run is unchanged (the switch is only set by the self-tests).

## 2. recorder: the `.pkts` header tail (item e) and the skirmish test's bigotry (item d)

### 2.1 The header tail and the command padding

**What the exe writes**: `captainServerTask` 0x00451580 builds the sync packet in three branches: no command
(`memAllocFunction(0x1c, 0)` then the count at +0x18), one command (a copy of the command packet), several (a new block,
the type 0x5555 and the count at +0x18, the bodies copied after +0x1C); in none is +0x1A .. +0x1B written, and
`recPackRecordPacket` appends the block as it is. **GOG's bytes** there: 10-17 `oracle01.pkts` 4,800 packets, 2,265
zero, the rest heap words (`0x059c` 1,100, `0x059d` 580, `0x050d` 193, `0x412e` 114, ...); 10-18: 604 of 1,763 zero
(`0x059d` 312, `0x059c` 163, `0x09a8` 161, ...). Ours (`hwirec01` / `hwirec02`): 982 / 1,094 of 2,400 zero.

**Fix** (`src/Game/CommandNetwork.c`, `captainServerTask`, **CAT-FIX**): the two bytes after `numberOfCommands` are zeroed
before `netcheckFillInChecksum`, in all three branches at once. A second difference showed in the first pair of
recordings with alliances: a misc command's last two bytes (`NetMiscCommand` is 12 bytes with one ship, the exe's `10 + 2n`;
its tail padding was heap bytes, `40 86` / `80 f6` in frame 1504). The move / hyperspace / formation senders already
zero their blocks (`clPktCommandAlloc`, M2-R9); **the other 16 HW1-layout senders now do the same** (`memset` after
the `memAlloc`: attack, targets, dock, launch, misc, god, RU transfer, autolaunch, alliance, research, drop, create,
deterministic build, build, collect, protect; **CAT-FIX**, noted in `clPktCommandAlloc`'s description). Nothing reads
these bytes (the sync checksum is the four checksum words).

**Result**: four record runs of the same game (`r10p3-recb2-{a,b}` and `r10p3-recbv-{a,b}`, LFE2, bigotry 2, 5,000 ticks,
`--record ... --record-stop 4800`) wrote **byte-identical `.pkts` (89,094 B, sha256 `135fc5d57c48c8be`)**, header tail
set in 0 packets (`pkts_probe.py`). The saves still differ in their stale pointer slots (decision 95). **The playbacks are
unchanged**: `r10p3-play01-a` (`hwirec01`) and `r10p3-play02-a` (`hwirec02`) are IDENTICAL in every field to producer 1's
`r10p1g-play01-a` / `r10p1g-play02-a` (producer 2's audit ran no playback; `hwirec02`'s sync errors are producer 1's flight
changes, as its report states).

### 2.2 The bigotry option

`OP_CPU_Attacks` is a radio button on update.big's `Skirmish_Basic` (and the options screens), three buttons with pData
**0, 1, 2** (read from the .fib's atoms); its callback `opCPUAttacks` 0x004db440 stores pData in
`tpGameCreated.aiplayerBigotry`. The front end's default is 0. **New switch `/catSkirmishBigotry <0..2>`** (`src/SDL/main.c`,
`src/Game/LevelLoad.c` `catSkirmishSelfTestSetBigotry`, HWI-TEST): the skirmish test calls `opCPUAttacks` with a button
atom whose pData is N (not its first call) after the map and race picks and before StartNewGame, and logs
`skirmishtest: bigotry=N (OP_CPU_Attacks radio button N)`. **Harness flag `--skirmish-bigotry N`** (skirmish mode,
documented in the header). Without it every default skirmish stream is unchanged: `r10p3-skirmish-lfe2` (LFE2, 91 ships)
IDENTICAL to producer 2's audited `r10a2-skirmish-lfe2`, `r10p3-skirmish-armed8` IDENTICAL to `r10p1g-skirmish-armed8`.

**What a 2-player test carries**: `cat_aisSetupAlliances` 0x004310a0 (at the skirmish AI's first play): bigotry 1 pairs the
computers by how many there are (3 computers: 2 alliances; 4-5: 4; 6-7: 6; one or two: none), so a 2-player test sends
none; **bigotry 2 sends `ALLIANCE_FORM_NEW_ALLIANCE` for every index pair from the first computer to 7, whether the
player exists or not: 21 commands in a 2-player game** (1..7). That is exactly GOG's two skirmish sessions (21 alliance
commands, body 8, at frame 88), so **no extra computer players are needed**. Ours come at frame 160 (the test's first
skirmish-AI play on the virtual clock; GOG's real-time session played first at 88).

**The recording** (`r10p3-recbv-a`, `--skirmish-bigotry 2 --record hwibig02 --record-stop 4800 -- /catVirtualAudio`):
skirmish PASS, `record stopped 'hwibig02' tick=4800 packets=2400 ships=63 randSyncErr=0 univSyncErr=0 blobSyncErr=0
pktSyncErr=0`, ASan 0; `recording_compare.py` against GOG's 10-17 `oracle01`: **SAME FORMAT**, `alliance ref 21 {8: 21}
cand 21 {8: 21}`. Its replay (`r10p3-playbig-{a,b}`, `--mode replay --recording hwibig02 --recording-src
<R>/r10p3-recbv-a/settings/SavedGames/RecordedGames --recording-as hwibig02p3 --timeout 900`): `replay done ticks=4801
packets=2400 ships=63 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0`, ASan 0, **a and b byte-identical** (`cmp`),
and against the record run

```
python3 -I tools/oracle/hwo_compare.py <R>/r10p3-recbv-a/run.hwo <R>/r10p3-playbig-a/run.hwo --from-tick 1 --to-tick 4800 \
  --fields rng10,ships_n,ships_misc,ships_pos,ships_vel,ships_health,ships_rot,missiles,bullets,derelicts,resources,univcheck,players,dt,total,real \
  --json <R>/r10p3-playbig-a/compare-rec.json
IDENTICAL: 4800 ticks and 2400 packets compared (ticks 1-4800), no difference
```

(`fpcw` IDENTICAL too; `rng0` from 2, `rng6` from 321 and `rng_all`: the computer player does not run in a playback,
decision 95.) **The record run's own stream**: the skirmish test keeps sound and speech on (as the front end does), so
`RANDOM_SOUND_BOTH_THREADS` (stream 12: `SpeechEvent.c`'s variation draws, paced by the speech on SDL's audio thread)
splits two record runs with alliances in `rng_all` only (`r10p3-recb2-a` / `-b` from 2810; the experiment binary named the
stream: 12 at 2811 / 2845 in three runs); with `-- /catVirtualAudio` the two record runs are **byte-identical
(`5c40dc371212d2c4`)**. The default skirmish test is left on SDL's clock (its streams unchanged); pass `--
/catVirtualAudio` for a record run that must pair in every field (documented with the flag).

## 3. loss: the deliberate-loss test of the failed jump (item b)

**What sends a jump down the failed return in the exe.** `singlePlayerGameInfo.hyperspaceFails` is written 1 in one
place only, `kasfDisablePlayerHyperspace` 0x004935e0 (the decompile's every other reference reads it or clears it:
`singlePlayerGameUpdate` 0x005197d0, `UpdateHyperspacingShip`, `singlePlayerGameCleanup`, the hyperspace clip in
`cat_rndRenderObjectList`); no mission script calls it (the generator's 2,085 bodies: none; the export's note "0 mission
call sites"); the only caller is the KAS dispatcher by name `cat_kasCallFunctionByName` 0x0045b9a0 (hash 0x009c2c05),
whose only caller is the developer console's `cat_consoleKasCommand` 0x0045f270. So in a retail game a jump fails only
from the console. With the flag set the jump runs `UpdateHyperspacingShip`'s fizzle branch (slice in, collapse, no
clip), and when every ship has finished its effect `singlePlayerGameUpdate` falls from `HYPERSPACE_SHIPSLEAVING` into
case 10 (`HYPERSPACE_CATFAILEDRETURN`): `NO_HYPERSPACE`, `catSpEtherCleanup(FALSE, FALSE)`, `spUnlockout`, the ships that
must launch, the autolaunch restored, `"Game_HyperspaceFailed"`, `hyperspaceFails` cleared. The letterbox jump of the
`catSpMissionNextLevel` missions never looks at the flag (it puts the fleet into the ether at once), so only a roll-call
jump can fail. Case 10 does not give `playerCanHyperspace` back (`spHyperspaceButtonPushed` cleared it), so after a failed
jump the player stays in the level until the script allows a jump again.

**The test** (`src/Game/MissionSelfTest.c`, HWI-TEST: `catMtFailJumpArm`, `catMtFailJumpWatch`, `catMtFailJumpCount`;
switch `/catMissionTestFailJump` in `src/SDL/main.c`; harness `--fail-jump` in missiontest mode with `--mission N`,
documented in the header): at the test's first hyperspace command it calls `cat_kasCallFunctionByName("DisablePlayerHyperspace",
...)`, the call the console makes after parsing the typed line (no input), presses the hyperspace button as usual and
clicks the roll call's enabled button; at the return it judges: state `NO_HYPERSPACE` with `hyperspaceFails` clear, the
same level (no level load), the player's mothership outside and not hyperspacing, none of the player's ships in the ether,
none lost (counted outside and docked: Go Now docks the small ships). The run ends 32 ticks after the return; PASS also
needs every objective of the table reached as in the normal run, the level / KAS / arrival, no mission loss and no next
level. Lines: `missiontest: failed jump armed / begun / return ... ok|FAIL`, `missiontest: failed jump: playerCanHyperspace=..
after the return ...`, `missiontest: failed jump judged ... PASS|FAIL`, then the usual `end` / `done` lines.

**The mission: Mission03.** Trials on six roll-call missions (`r10p3-fjt-m{3,4,10,11,12,14}`): the return was reached
and judged ok in 3, 4, 10, 11 and 14 (in 12 ten ships of the level stayed in the ether: the reason the judgement counts
only the player's). The mission scripts then decide: Mission11's Objectives FSM sends 0x0d2247a4 eleven ticks after the
return (team 0x0589433d counted empty once its ships are docked) and FleetIntel's Fail state loses the mission at 8288;
Mission04 is lost 4,139 ticks later; **Mission03 runs on to the 90,000-tick limit without a loss**, has a roll call (Go
Now), a 32-ship fleet (23 docked by Go Now) and costs about 85 s. The gate runs: `r10p3-fj3-{a,b}` (`--mode missiontest
--mission 3 --fail-jump --mission-test-ticks 90000 --timeout 3600`):

```
failed jump armed tick=17472 (DisablePlayerHyperspace through cat_kasCallFunctionByName, the console's KAS call; ret=0 hyperspaceFails=1 playerShips=32 docked=1)
failed jump begun tick=17472 (hyperspaceState=2 hyperspaceFails=1)
failed jump return tick=17635 (case HYPERSPACE_CATFAILEDRETURN: hyperspaceState=0 hyperspaceFails=0 level=same mothership=outside playerShips=32 (armed 32; docked 23) hyperspacing=0 ether=0 (the player's 0)) ok
failed jump: playerCanHyperspace=0 after the return; the run ends at tick=17667
failed jump judged armed=17472 begun=17472 return=17635 PASS
end mission=3 tick=17667 level=1 kas=1 arrived=1 inOrder=1 jump=17472 rollcall=GoNowHyperspace next=-1 failed=0 completed=-1 gameend=-1
done mission=3 ticks=17667 objectives=5/5 result=PASS
```

milestone "mission self-test done", ASan 0, no leak, **a and b byte-identical `4c6b1299c1b6b9ba`**; repeated on the final
build (`r10p3g-fj3-{a,b}`): the same lines, the same stream. Against the normal Mission03 run of the base build
(`r10p3b-m03`, section 6) the stream is IDENTICAL up to the return; from there `ships_pos` / `ships_vel` / `univcheck`
17634, `ships_rot` 17636, `rng6` / `rng_all` 17648 (the fleet stays in the level and flies on).

## 4. combat: why the attack order makes no kills (item a): deferred, measured

An experiment binary (scratch only, `<S>/exp`: the tree plus an `HWI_EXP_FIGHT` hook in `catMtDestroyTeams`, never in the
tree) gave Mission01's destroy objectives to the player's armed ships first (every armed ship of player 0 outside the
ether but the mothership; the command layer's `clWrapAttack` on the objective's team), logging the targets' positions,
health and flags, the attackers' orders, speeds and last shot, and dumping chosen ships with `/oracleShipDump` ('S' / 'T'
lines). Runs `r10p3-xf-m1` .. `r10p3-xf5-m1` (missiontest, Mission01 from the NIS-first start). At DestroyBombers'
creation (tick 4784):

- **The targets are fine**: the 14 `tAttackBomber`s of team 0x04fcebd3 are targetable (flags `0x00102003`: rotatable,
  impactable, selectable, targetable; attributes 0), 350 hull each (4,450 in all with the team's three others), and they
  are busy: they raid the player's five `kIonCannonFrigate`s (their command's targets are ships 0..4) near (25000, 63000).
- **The player's fighters are far away**: 37 armed ships (18 `kHeavyInterceptor`, 9 `kIonCannonFrigate`, 4
  `kMultiGunCorvette`, 2 `kStandardDestroyer`, 3 `sRecon`, 1 `kCarrier`); interceptor 48 sits at (-6096, 42146), **37,600
  units from its bomber**; interceptors 48 / 49 were 30,000-54,000 units from their targets throughout. A
  `kHeavyInterceptor` flies 775 u/s, a raiding bomber about 577 u/s, so the chase closes at 200 u/s at best.
- **R9's experiment re-issued the order every 64 ticks**, and each order makes every ship pick its target again among 14:
  interceptor 48 went 14 -> 7 -> 10 -> 11 -> 8 -> 15 -> 120 in 1,600 ticks and never came closer than 29,582 units; the
  only fire came from the frigates the bombers were attacking. **210 of 4,450 hull points in 1,600 ticks, no kill**
  (R9: none either).
- **11 interceptors never take the order**: #97 .. #107 (near (72672, -30180)) keep order 1 (a move) and never fire; the
  attack given at 4784 is gone at 4900 (`T 4900 97 ... cmd=-`): the script moves them.
- **One attack order, held** (`HWI_EXP_ONCE`, 3,200 ticks): the first bomber dies after about 2,000 ticks, two by 7088;
  **593 of 4,450 hull points off by 7856, 12 bombers left**, and the player has lost 3 armed ships. At that rate the 14
  bombers take of the order of 15,000-20,000 ticks, while the normal run's next objective (ExploreRegion) appears at
  8096 and R9 measured that a 1,664-tick delay of DestroyBombers already leaves ExploreRegion unfinished.

**Verdict: not cheap; deferred.** The attack order works (the ships take it, chase and fire, and the bombers die), but the
player's fleet is placed for the human player's later moves, not for the script's objective at that moment: a faithful
fight would need a scripted player that launches, positions and holds interceptors near the raided frigates and keeps the
mission's later timing, which is a play-test question. **Owner: M3 test** (the campaign play-test's combat: an attended
player shows how long the fight takes and what the script tolerates). Decision 77's `ShipsDamage` stays. No stream moved
(nothing in the tree).

## 5. reviews: `scriptSetTweakableGlobals` and the sample of the unflagged rows (item f)

**The count.** At the start of this step the map had **235** MODIFIED / REWRITTEN rows without a `reviewed` flag, **143**
outside `TitanInterface.cpp` (R9's 148 less the 5 producers 1 and 2 reviewed): ETG.c 45, Particle.c 41, ObjTypes.c 20,
MultiplayerLANGame.c 8, StatScript.c 6, MultiplayerGame.c 5, GameChat.c 3, texreg.c 3, Clouds.c / CommandNetwork.c /
File.c 2 each, Captaincy.c / Collision.c / Select.c / TitanNet.c / ddraw.cpp / DefenseFighter.c 1 each.

**The sample: 23 rows over 12 files** (each compared with `tools/m2_fdec.py` and, where the shape mattered,
`tools/m2_fdis.py`; each a review entry `[review: M2-R10 tests-leftovers]`, sample `sample-R10-tests-leftovers`):

| row | file | verdict |
|---|---|---|
| `scriptSetTweakableGlobals` 0x00527820 | StatScript.c | the tree's body: the `canMpHyperspace` clear over the 145 `HW_Cost` entries (0x00ba1fc0 + 0x10, stride 0x14), `scriptSet` inlined for the first five scripts (file name into `globalScriptFileName` 0x00c52da0), then the other 19 in the tree's order, no Dock.script, `crateInit`, `paradeSetTweakables` |
| `parseLine` 0x00526120 | StatScript.c | **ported**: a line holding `"http:"` keeps its `//` (the three english.script URLs had loaded as `http:`) |
| `scriptSetNAVLightStatics` 0x00526dc0 | StatScript.c | the tree's (the NavLight table 0x00897860 = `{"NavLight", 0x00526ce0}`, n * 0x48 + 4 cleared) |
| `StrToShipType` 0x004d6e30, `ShipRaceToStr` 0x004d6b90, `StrToEffectNum` 0x004d6df0, `StrToGunSoundType` 0x004d6ff0 | ObjTypes.c | the tree's: each table (145, 7, 43, 20 rows) string for string and in order the exe's |
| `cloudZapLoop` 0x00438c30 | Clouds.c | **ported, x87 shape**: the radius and its square on the FPU, (dz*dz + dy*dy) + dx*dx unrounded, the damage `fild` / `fidiv` * 0.7 |
| `cloudUpdateSystem` 0x00439130 | Clouds.c | **tagged `CAT-TODO(M3 oracle physics)`**: the burst threshold rounds to nearest (the tree truncates), the 0.9 radius product order, the x87 distances; lightning drawing and `RANDOM_CLOUDS` only |
| `selCentrePointComputeGeneral` 0x00508be0 | Select.c | **ported**: the missile scale `catMissileScale` +0x254 (the old provisional 1 replaced) and the sum on the FPU (0x00508db5) |
| `trMeshSortAdd` 0x00586190 | texreg.c | **IDENTICAL** (HW1's body with `trColorInfosPointer` inlined): the map's class corrected |
| `fileExists` 0x00479c60 | File.c | GoK's portable file layer, the same order: nothing to port |
| `etgFdiv` 0x0046fdd0, `etgSetColorA` 0x00471290, `etgEffectDelete` 0x0046a6f0, `etgStartup` 0x0046a260 | ETG.c | the tree's (the argument-array convention, the clears and counts, the effect delete's Cataclysm frees) |
| `partSetAlphaMode` 0x004e4320, `partModifyTexture` 0x004e29b0 | Particle.c | the tree's |
| `partMakeSPos` 0x004e00e0 | Particle.c | the same 8 draws of stream 25 in the same order; the x87 grouping of the offset differs in the last bits: render only, not ported |
| `collCheckShipDerelictColl` 0x0043bed0 | Collision.c | **ported**: the two clamp skips (a ship clamped to the derelict, a derelict clamped to the ship) and the squared distance against colldist squared on the FPU, `fsqrt` of the double only for the collision; the two-way kamikaze damage was already the tree's `kamikazeCheck` |
| `DefenseFighterStaticInit` 0x005b0420 | DefenseFighter.c | no Cataclysm ship type is a defense fighter, the tree has no DefenseFighter.c: nothing to port |
| `lgStartGame` 0x004c5900 | MultiplayerLANGame.c | stage 5 LAN (after M3): the start-position shuffle the LevelLoad.c tag owns |
| `ReceivedPacketCB` 0x0044e370 | CommandNetwork.c | stage 5 LAN chat (after M3) |

Of the 23: **4 ported** (`parseLine`, `cloudZapLoop`, `selCentrePointComputeGeneral`, `collCheckShipDerelictColl`),
**1 tagged** for M3 oracle physics (`cloudUpdateSystem`), **1 found IDENTICAL** (`trMeshSortAdd`), 2 stage 5 (review
only, no tag added: decision 9's 23 sites stay as they are), 2 not applicable or render-only, 13 the tree's already.
Merged with `python3 -I tools/delta_map.py merge --done m2r10:tests-leftovers` ("records changed 23") and `report`:
**IDENTICAL 2,762 -> 2,763, MODIFIED 1,575 -> 1,574, REWRITTEN 83, reviewed 3,531 -> 3,554**; 120 unflagged rows remain
outside `TitanInterface.cpp` (212 in all). [audit: M2-R10 tests-leftovers: the CAT-FIX edits of this step touched 15 bodies the
map calls IDENTICAL with no review entry (`nebInitTendril`, `captainServerTask` and 13 command senders); the audit added their
entries (sample `audit-R10-tests-leftovers`, class IDENTICAL kept) and a note on `clSendSpecial`'s, merged: reviewed 3,554 -> 3,569]

**The ports' effect on the streams**: `parseLine` touches no stream (strings); `cloudZapLoop` changes ship damage in a
charged dust cloud; `selCentrePointComputeGeneral` feeds the AI's team centres and moves; `collCheckShipDerelictColl`
changes ship-derelict bumps. Section 6 states every stream they moved.

## 6. rebaseline: the 17 mission streams, once (item h)

Final debug build of this tree (copied to `<S>/final/hwi-engine-debug` before the runs), plain harness, `--mode missiontest
[--mission N] --mission-test-ticks 90000 --timeout 3600`, 8 runs at a time: every mission twice (`r10p3g-mNN-{a,b}`),
Mission06 four times (`r10p3g-m06-{a,b,c,d}`). To attribute every move, each mission also ran once on **the base build**
(`3ea7b12`, untouched, `<S>/base/hwi-engine-debug`: `r10p3b-mNN`). The comparisons are `hwo_compare.py` per field (each
field's first divergent tick). Producer 2's audit ran Mission01 (missiontest), 04, 06, 12, 14, 15, 16, 17; the others are
compared with R9's streams as the task names them (their moves against R9 are producer 1's and producer 2's, which the
base-build column separates from this step's).

| mission | objectives (in order) | done tick | stream (a) | pair | against the base build `3ea7b12` | against producer 2's audited / R9's |
|---|---|---|---|---|---|---|
| 01 | 6/6 DestroyBombers, ExploreRegion, DestroyResourcers, DestroyProxAttackers, DestroyDestroyer, Hyperspace | 11424 | `15c7ac5528a7f81c` | byte-identical | **IDENTICAL** | `r10a2-missiontest-a`: IDENTICAL |
| 02 | 6/6 FindDestroyer, Build, RepairDestroyer, SalvagePod, DestroyPirates, Hyperspace | 25936 | `d575eb871d4ef80b` | byte-identical | **IDENTICAL** | `r9i-m2-a`: producer 1's move (ships_rot 2, ships_vel 5, rng_all 11035, ships_pos 11065, ...) |
| 03 | 5/5 BuildHangarModule, Find, Mimic, Lure, Hyperspace | 17825 | `71d125484aff745f` | byte-identical | **IDENTICAL** | `r9i-m3-a`: **rng_all only, from 6708** (producer 2's: below) |
| 04 | 5/5 Replace, Escort, Research, Destroy, Hyperspace | 20818 | `de634bc1458a3afb` | byte-identical | **IDENTICAL** | `r10a2-m4-a`: IDENTICAL |
| 05 | 6/6 InterceptBentusi, SaveBentusi, ProtectCaalShto, DestroyBeast, BuildEngineering, Hyperspace | 16384 | `bc69953f551ca239` | byte-identical | **IDENTICAL** | `r9i-m5-a`: producer 1's move (ships_rot 2518, ships_vel 3394, ... 5034) |
| 06 | 7/7 Explore, Scout, Escort, Mines, HvCruiser, Leech, Hyperspace | 31011 | `d0ac2eeb9a668f8b` | **byte-identical over 4 runs** (a = b = c = d) | **rng_all only, from 22235** (the nebula fix, section 1) | `r10a2-m6-b`: rng_all only, from 22235 |
| 07 | 4/4 Mayday, Protect, Destroy, Hyperspace | 16592 | `8d485c877406e2d1` | byte-identical | **IDENTICAL** | `r9i-m7-a`: IDENTICAL |
| 08 | 6/6 Resource, Protect, CaptureResourcers, CaptureFrigate, NoSurvivors, Hyperspace | 29361 | `2e383f6c8cd804c3` | byte-identical | **IDENTICAL** | `r9i-m8-a`: producer 1's move (ships_rot 8809, ships_vel 8810, ... 12558) |
| 09 | 6/6 MoveTransports, DestroySensor, Wait, Engage, Protect, Hyperspace | 20617 | `814390fe68591cdb` | byte-identical | **IDENTICAL** | `r9i-m9-a`: producer 1's move (ships_rot 3242, rng10 / rng_all 3262, ...) |
| 10 | 5/5 SALVAGE_BIGGUN, RESEARCH_BIGGUN, BUILD_BIGGUN, FIRE_BIG_GUN, HYPERSPACE | 21939 | `a3a9f7b392f079ec` | byte-identical | **IDENTICAL** | `r9i-m10-a`: producer 1's move (ships_rot 1, ships_pos / vel 358, rng_all 6695, ...) |
| 11 | 5/5 DESTROY_ATTACKERS, REPAIR_FACTORUM, DESTROY_BEAST, PROTECT_FACTORUM, Hyperspace | 8275 | `cde13f4612c0bc96` | byte-identical | **IDENTICAL** | `r9i-m11-a`: producer 1's move (ships_rot 2, ships_vel 3, ...) |
| 12 | 3/3 DefendMothership, AttackOutpost, Hyperspace | 10995 | `44d0d3b3d5aeb236` | byte-identical | **IDENTICAL** | `r10a2-m12-a`: IDENTICAL |
| 13 | 3/3 DESTROY_ENEMY, PROTECT_FAALCORUM, Hyperspace | 19283 | `a1f63b94807d860b` | byte-identical | **IDENTICAL** | `r9i-m13-a`: producer 1's move (ships_rot 2083, rng10 / rng_all ... 2105, ...) |
| 14 | 4/4 PROTECT_MOTHERSHIP, GET_SAMPLE, CAPTURE_TAIIDAN, Hyperspace | 22338 | `5bafe922231f0438` | byte-identical | **IDENTICAL** | `r10a2-m14-a`: IDENTICAL |
| 15 | 3/3 DestroyGateNode, Block, Hyperspace | 15424 | `73d53a0e3f7201c4` | byte-identical | **IDENTICAL** | `r10a2-m15-a`: IDENTICAL |
| 16 | 6/6 LOCATE_CS, CRIPPLE_CS, DOCKWITH_CS, HARVEST, DESTROY_BEAST, HYPERSPACE | 21523 | `6f7d63514bb4effb` | byte-identical | **IDENTICAL** | `r10a2-m16-a`: IDENTICAL |
| 17 | 4/4 + the game's end (`game end tick=15584`, credits) | 15584 | `fa1b4c657f887752` | byte-identical | **IDENTICAL** | `r10a2-m17-a`: IDENTICAL |

All 17 PASS with every objective in the table's order (`inOrder=1`), every pair byte-identical, ASan 0 in all 36 runs and
in the 17 base runs (also all PASS). **This step moved one mission stream: Mission06's `rng_all` from tick 22235** (the
nebula CAT-FIX); the virtual audio clock moves none (no mixer runs with sound and speech off), and the review ports
(`cloudZapLoop`, `selCentrePointComputeGeneral`, `collCheckShipDerelictColl`, `parseLine`) move no mission stream. The
task expected every mission's `rng_all` to move once; it does not, because decision 94's suspected cause (the mixer) does
not run in these tests: the cause was the nebula.

**An unstated move of producer 2's**: Mission03's `rng_all` differs from R9's and producer 1's from tick 6708 in the base
build already. The logging experiment binary built from producer 1's commit `36289a8` and from `3ea7b12` (both scratch,
`<S>/expp1`, `<S>/expbase`) names it: stream 14 (`RANDOM_TRAILS_1`) at 6708, one draw in `trailDraw` ->
`trailDrawBillboardedSquareThingz` (render) in `36289a8`'s build and none in `3ea7b12`'s: producer 2's texture-registry
change (`meshFree` / `trTextureUnregister`) reaches the trail drawing; render only, `rng_all` only. Producer 2's audit did
not run Mission03.

## 7. The other streams and gates (final build, plain harness)

| run(s) | mode | result | stream | against |
|---|---|---|---|---|
| `r10p3g-mission-{a,b}`, `-perturb` (`tools/oracle/mission_test.sh --bin <S>/final/hwi-engine-debug --label r10p3g-mission --ticks 6000 --perturb 3000 --copy-tick 4000`) | headless Mission01 | a = b, ASan 0; the perturbed run first differs at 3000 (`rng10`) | `65456c876ff727cc` (1,476,370 B) | **IDENTICAL** to producer 2's audited `r10a2-mission-a` (`902884367d98025d`) and to R9's `r9i-mission-a` (`553845b65e87f3b5`): **the headless Mission01 stream did not move** |
| `r10p3g-mission-rel-{a,b}` (`--no-perturb-run`, release) | headless Mission01 | a = b | `4a699037f527783e` | IDENTICAL to `r10a2-mission-rel-a` |
| `r10p3g-skirmish-lfe2`, `r10p3g-skirmish-armed8` | skirmish (default bigotry) | PASS (91 / 54 ships), ASan 0 | `ca7a75badc8f2842`, `9492e24f4971db3f` | IDENTICAL to `r10a2-skirmish-lfe2` / `r10a1-skirmish-armed8` |
| `r10p3g-warp10-{a,b}` | warpstart 10 (now on the virtual audio clock: no effect) | PASS, a = b | `e5b4c7a105101506` | IDENTICAL to `r10a2-warp10` |
| `r10p3g-tutorial-{a,b}` | missiontest `--tutorial` | 12/12, a = b | `77f00ad417ceffe6` | IDENTICAL to `r10a2-tutorial`: **the Tutorial1 leg is still a pair, unchanged** |
| `r10p3g-tload-{a,b}` | loadgame `--training` "LESSON 11", from `r10p3g-tutorial-a`'s save | PASS, a = b | `97e26aec1a841481` | IDENTICAL to `r10p1g-tload-a` |
| `r10p3g-loadgame-a` | loadgame "01 - Hiigara", 3,600 | PASS | `30a208dcdefbd8b6` | IDENTICAL to `r10a2-loadgame` |
| `r10p3g-r17`, `r10p3g-r18` | replay 10-17 / 10-18 | ASan 0 | `5f1acd4e3daf051b`, `819c1a0abc28644e` | IDENTICAL to `r10a2-r17` / `r10a1-r18-a`: **the rung is where producer 1 left it (2823 / 2871)** |
| `r10p3g-play01-a`, `r10p3g-play02-a` | replay `hwirec01` / `hwirec02` | ASan 0 | `85f11abf8338134f`, `9a197e5f70dcb011` | IDENTICAL to `r10p1g-play01-a` / `-play02-a` |
| `r10p3g-recb-{a,b}` (`--skirmish-bigotry 2 --record hwibig03 --record-stop 4800 -- /catVirtualAudio`) | skirmish record | PASS, a = b `7c9db01f2344911b`; **`.pkts` a = b `135fc5d57c48c8be`** (= the earlier build's), tail set in 0, 21 alliance commands | | the stream IDENTICAL to `r10p3-recbv-a` |
| `r10p3g-playbig-{a,b}` | replay `hwibig03` | `randSyncErr=0 ... pktSyncErr=0`, a = b `d864aa920d1db871` | | IDENTICAL to its record run on decision 95's list (ticks 1-4800, 2,400 packets) |
| `r10p3g-ai-{a,b}`, `r10p3g-ai-soak` | ai 400 / 4,000 | PASS, a = b, ASan 0, no fatal | `20f2257aac535352`, `15c3f19947070a97` | IDENTICAL to `r10a2-ai` / `r10a1-ai-soak` |
| `r10p3g-build-a` | build | PASS | `cc7f42e943762505` | IDENTICAL to `r10a2-build` |
| `r10p3g-motion-noi`, `r10p3g-nis-noi` | motion / nis `-- /noInterp` | PASS | dumps `183681235a3fdc7b` / `13ca70eea998e5de` | = R9's and producer 2's |
| `r10p3g-strings2` | strings (with the new URL check) | PASS 1705 / 1705, `urls=3 whole=3` | | |
| `r10p3g-fe` | fe 120 s | Main_game_screen, rc 124, **ASan 0**, no leak | | |
| `r10p3g-sp` | sp `--timeout 400` | universe running, **ASan 0**, no leak; levels Mission01_NIS01, 01, 02, 03, 04_NIS02, 04 (`univUpdateCounter` 912 at the timeout), 40 screenshots, Animatic01 .. 04 skipped (headless); 0 Select.c / FOW.c reports; UBSan only the Particle / ETG misaligned kinds | | |

**Builds**: `tools/build.sh --debug` and `--release` after every subsystem and at the end: rc 0, **0 warnings** under
`-Werror`; `tools/m2_layout32.sh`: OK. (The strings test's URL check came after the final binaries were copied; it
changes only the strings mode, rerun as `r10p3g-strings2` on the tree's build.)

**Harness milestone / first stop**: every gate run reached its "done" milestone with "first stop: none" (fe and sp run to
their timeouts with "none: still running").

## Files changed

- `src/Game/Nebulae.c`: `nebInitTendril` zeroes the bounding sphere (CAT-FIX).
- `src/SDL/main.c`: `CatMissionTestSet` sets the virtual audio clock; new switches `/catMissionTestFailJump`
  (`CatMissionTestFailJumpSet`) and `/catSkirmishBigotry` (`CatSkirmishBigotrySet`) with their table rows.
- `src/Game/CommandNetwork.c`: `captainServerTask` zeroes the sync header's tail (CAT-FIX); the 16 HW1-layout command
  senders zero their blocks (CAT-FIX, noted at `clPktCommandAlloc`).
- `src/Game/LevelLoad.c` / `.h`: `catSkirmishSelfTestSetBigotry` and the radio button press in `catSkirmishSelfTestStart`.
- `src/Game/Options.h`: `opCPUAttacks` declared.
- `src/Game/MissionSelfTest.c` / `.h`: the failed-jump test (`catMtFailJumpArm`, `catMtFailJumpWatch`,
  `catMtFailJumpCount`, `catMissionSelfTestFailJumpSet`, the end condition and judgement).
- `src/Game/StatScript.c`: `parseLine`'s `http:` rule (CAT).
- `src/Game/StringSupport.c`: the strings test checks that every loaded `http:` string holds `://` (HWI-TEST).
- `src/Game/Clouds.c`: `cloudZapLoop`'s x87 shape (CAT, `catCloudDistSqD`); `cloudUpdateSystem` tagged
  `CAT-TODO(M3 oracle physics)`.
- `src/Game/Select.c`: `selCentrePointComputeGeneral`'s missile scale and sum (CAT).
- `src/Game/Collision.c`: `collCheckShipDerelictColl`'s clamp skips and distance shape (CAT).
- `tools/m2_harness.sh`: `--skirmish-bigotry N`, `--fail-jump` (header documented; the bigotry flag's note on
  `-- /catVirtualAudio`).
- `ghidra/delta_map_review.json` (23 rows), `ghidra/delta_map.json` (merged), `docs/m2-r1-function-deltas.md`
  (regenerated), this report.

## CAT-TODOs

Closed: none named the test owner (`/bin/grep -rn -E 'CAT-(TODO|PROVISIONAL)\([^)]*(M2-R9|M2-R10|test)' src/` is empty
before and after). Added: 1, `CAT-TODO(M3 oracle physics)` at `cloudUpdateSystem` (Clouds.c). The CAT-PROVISIONAL in
`selCentrePointComputeGeneral` is replaced by the port. The stage-5 tags are untouched (23).

## For STATE.md

- **Mission06's rng_all flake was the nebula tendrils' uninitialised bounding sphere** (`nebInitTendril` 0x004c7d50 never
  sets it, `nebRenderNebula` 0x004cadb0 reads it): CAT-FIX zero (decision 94 answered by a fix, not by the sound streams).
- **The mission self-test runs on the virtual audio clock** (`CatMissionTestSet`), a no-op while sound and speech are
  off; decision 77's silence stays.
- **The failed-jump test** `/catMissionTestFailJump` (harness `--fail-jump`, Mission03 in the gates): the jump is made to
  fail through the console's KAS call (`cat_kasCallFunctionByName("DisablePlayerHyperspace")`, the exe's only route);
  judged at the return; the run ends 32 ticks later (no second jump: case 10 does not give `playerCanHyperspace` back).
  Names: `catMtFailJumpArm`, `catMtFailJumpWatch`, `catMtFailJumpCount`.
- **The skirmish test's bigotry** `/catSkirmishBigotry N` (harness `--skirmish-bigotry N`): `opCPUAttacks` pressed with
  pData N; 2 gives GOG's 21 alliance commands in a 2-player game; the default stays 0. A record run that must pair in
  every field adds `-- /catVirtualAudio` (the skirmish test keeps sound on).
- **The `.pkts` bytes the exe leaves to the heap are zeroed**: the sync header's +0x1A tail and every command's struct
  padding (CAT-FIX); two record runs' `.pkts` are byte-identical.
- **The combat**: deferred to M3 test with the measurements of section 4; `ShipsDamage` stays (decision 77).
- **Unstated move found**: producer 2's tree moves Mission03's `rng_all` at 6708 against producer 1's (one
  `RANDOM_TRAILS_1` draw in `trailDraw`, render only).
- Names: `catCloudDistSqD`, `catSkirmishSelfTestSetBigotry`, `CatSkirmishBigotrySet`, `CatMissionTestFailJumpSet`.

## What is left (owners)

1. **The mission self-test's combat** (M3 test): section 4's measurements; a scripted player that positions
   interceptors near the raid and keeps the script's timing, or the attended play-test's figures.
2. **`cloudUpdateSystem`'s shape** (M3 oracle physics, the `CAT-TODO` in Clouds.c): the rounding of the burst threshold,
   the radius product order, the x87 distances.
3. **120 unflagged MODIFIED / REWRITTEN rows outside `TitanInterface.cpp`** (a later audit's sample; ETG.c and
   Particle.c rows are mostly the argument-array convention, every one of the 8 sampled here was the tree's already).
4. **`partMakeSPos`'s x87 offset** (render only; whoever compares particle positions).
5. **Decision 94** (the user's): it named the mixer; the cause was the nebula, fixed as a CAT-FIX (zero, as a fresh
   block): the user may prefer to keep the exe's undefined read for the release build (no stream outside `rng_all` sees it).
6. **The ai soak's rng_all under load** (decision 84): not investigated here; the same experiment binary
   (`HWI_EXP_RNGLOG`) would name the stream in two runs.
7. **Producer 2's unstated Mission03 `rng_all` move** (render, trails, from 6708): for the R10 integrate's record.
