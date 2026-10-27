# M3-R4 ad hoc: the Mission17 black viewport (CAT-FIX nan-positions) and the speech log (HWI-TEST speech-log), 2026-10-27

Landed from the user's terminal session 38027cce between rounds, after the user's Mission15 and Mission17 play.

## The user's words (verbatim)
- "Yeah for some reason I didn't hear dialog 13. Maybe it was because I went into the build menu just at that moment.
  Please investigate possible causes and accomidate. Also on mission 17 I had to stop playing about 3 minutes in when my
  screen went full black. I was still able to see my hud but nothing on the viewport. Moment before the glitch I was
  trying to find my dreadnauts on screen but I couldn't find them. Knowing that they were assigned to group 1, I press
  the "1" key twice to focus on them but thats went the full black screen occured. Please investigate"

## Mission17: what happened
The user's play log (`original/play-home/.homeworld/play-20261027-085310.log`) ends Mission17 with ten
`univCheckShipState(): Ship position contains NaN! Killing it.` lines just before "Quit to main menu!". Mission16 had one.
A ship whose physics state is NaN is not drawn (its coordinate system is NaN before its position is), and its
`collInfo.collPosition` is NaN too: `NewSetFocusPoint` (CameraCommand.c) averages the focus ships' collision centres
into the look-at point, so focusing group 1 on such ships made the camera NaN and the 3D viewport black while the HUD
(2D) kept drawing. The dreadnoughts "could not be found" because they were already invisible.

### The chain, reproduced headlessly
`/catNaNProbe` (HWI-TEST, main.c; UnivUpdate.c `catNaNProbe` / `catNaNProbeShip`) scans every live ship (and bullet) for
a NaN in its physics state after each phase of `univUpdate` and names the first sighting per ship (`nanprobe:` lines);
`nanprobe-fly:` (AIShip.c) logs a fly-to-point whose desired velocity is a zero vector, `nanprobe-selfattack:`
(CommandLayer.c) an attack order whose target list contains the attacker, `nanprobe-kas:` (KASFunc.c) the owners of a
KAS attack list. With it, Mission17's self-test (`m3r4-nan-m17-probe`, unfixed binary) and the user's own save
(`m3r4-nan-save17-before`: "17 - Naval Base Alpha", 4000 ticks, 23 ships NaN: tHeavyInterceptor / tAttackBomber of
player 0, bAcolyte / bMultiGunCorvette / bHiveFrigate / bACV of player 1) both show the same chain:

1. `Init_Mission17` creates every ship as player 1's and puts "all friendly ships inside the volume" into the script's
   selection 0x6da2da9 (`kasfShipsSelectFriendly(FindShipsInside(..))`, Mission17.c:17561): at that moment the Taiidan
   "k" teams' ships are in it (393 ships, all player 1).
2. Each k team's Init (`Init_Mission17_kHeavyCruisers`, `_kFighters`, ..) then does `kasfTeamSwitchPlayerOwner(0)` (the
   ships become the human player's allies) and jumps to its Attack state, which selects from 0x6da2da9 the ships near
   the team (`kasfShipsSelectNearby`, 20000) and orders `kasfAttack` on them: the list still holds the team's own ships
   (`nanprobe-kas: kasfAttack targets n=62 owners=[7,55,..] inTeam=5`, `nanprobe-selfattack: tHeavyInterceptor#3 ..
   targets=62 (own=7 other=55)`), and the Attack state re-runs on a 10..30 s timer.
3. Nothing in the attack path filters an attacker out of its own target list: not `kasfAttack` (0x00492190), not
   `clWrapAttack` (0x00452c00), `clAttack` (0x00448950) or `clAttackThese` (0x00448620), and `AttackClosestOpponent`
   (0x00447740: the nearest target by squared distance from 1e21 down) picks the ship itself at distance 0. The exe's
   decompile is the same code, so the exe's Taiidan teams attack themselves too (the self-targeting is the script's).
4. `GenericInterceptorAttack` STATE_REAPPROACH (0x005b2620 case 5, the port's GenericInterceptor.c:679) flies to the
   target's position with flags 2 (PointInDirectionFlying) and no minimum velocity: `aishipFlyToPointAvoidingObjsFunc`
   (0x00414610, the exe lines 11949-11957 and the port's AIShip.c:1803-1856) gets a zero desired velocity and, since the
   zero-velocity branch needs StopIfVelocityTooLow or DontFlyToObscuredPoints, calls `vecCopyAndNormalize` on it: 1/0 =
   inf, 0 x inf = NaN, in the exe (0x00550820: no zero test) as in the port.
5. **The divergence.** `aitrackHeadingFunc` (0x00418de0) starts with the dot product of the ship's heading and the
   desired heading and compares it with -1.0 for the "exactly opposite, rotate about X" case:
   ```
   00418eb9  fcom   dword ptr [0x72a468]   ; dot vs -1.0
   00418ebf  fnstsw ax
   00418ec1  test   ah, 0x41               ; C0 | C3
   00418ec4  je     00418ee4               ; neither set: dot > -1.0, go on
   00418ec6  .. physApplyRotToObj(ship, rotstrength[ROT_ABOUTXCW], ROT_ABOUTXCW); return 0
   ```
   An x87 compare with a NaN is UNORDERED and sets C0, C2 and C3, so the exe takes the "exactly opposite" branch: a
   finite rotation about X, no torque from the NaN, return. The ship spins in place and the NaN dies there. The port's
   IEEE compare `dotProdCheckHeading <= -1.0` is false for a NaN, so the port went on: `matMultiplyVecByMat` of the NaN
   heading, NaN torque, NaN rotation speed and coordinate system (`nanprobe: tick=330 .. mask=0x38`), NaN velocity and
   finally position, where the GoK addition `univCheckShipState` killed the ship. Every ship that then targeted or was
   attacked by a NaN ship (the Beast ones, the user's dreadnoughts) caught the NaN the same way.

### The fix (CAT-FIX (M3-R4 nan-positions))
- `AITrack.c`: the three "exactly opposite" compares take the exe's unordered branch too: `aitrackHeadingFunc`
  (0x00418eb9, `test ah,0x41`) and both of `aitrackHeadingAndUp` (0x00418ad0 / 0x00418afd, the same test):
  `(dot <= -1.0) || isnan(dot)`. That is the x87 shape of the compare, not a guard: with it a self-attacking Taiidan
  interceptor behaves as in the exe (spins about X, keeps firing from where it is) and no NaN enters any ship.
- `CameraCommand.c NewSetFocusPoint`: HWI-FIX safety net, not in the exe: a focus ship whose collision centre is NaN is
  left out of the look-at box and of the zoom, and with none left the focus is refused (FOCUS_NO_SHIPS). The exe never
  has such ships; the port had, and will again if another NaN source appears, so the viewport must not go black.
- `UnivUpdate.c univCheckShipState`'s kill message names the ship (type, number, owner).
- The probes stay as HWI-TEST under `/catNaNProbe` (silent otherwise, portable: the SIGFPE trap and backtraces used
  during the investigation were removed before landing).

### Residual, by design
- The Taiidan teams' self-targeting is the script's own (`kasfShipsSelectFriendly` before `kasfTeamSwitchPlayerOwner`)
  and the exe's engine does nothing about it; the port keeps it. Two `nanprobe-fly:` sightings per Mission17 self-test
  (ticks 328 and 353, tHeavyInterceptor#27 / #30) and in the user's save are those self-attacks reaching the normalise:
  harmless now.
- `pickMultipleAttackTargets` (CommandLayer.c:5560) divides by a zero distance for the same self-target; its NaN only
  takes part in target scoring compares (the exe's unordered semantics there are not ported: a degenerate case with no
  visible effect found).

## Mission15 line 13: the speech log (HWI-TEST (M3-R4 speech-log))
No path was found by which opening the build manager alone silences a queued or playing mission line: the manager's
open (`cmConstructionBegin`) stops the sound effect channels (`soundstopallSFX`, streams untouched), locks the main
region and hides the sensors; speech plays on the three streams and its queue is advanced from the renderer every
frame (render.c:5284) whether or not the manager is open. What does discard a line: `smSensorsSkip` (Sensors.c:2644:
`speechEventActorStop(ACTOR_ALL_ACTORS)` clears every queued mission line and fades the playing one), reached from the
Skip_Sensors key (the user's keys.cfg: Backspace, also polled by `cmBuildTaskFunction` every tick as the exe does), from
Escape while a fleet-intel sensors view is up (KeyBindings.c:1917), from the sensors screen's Skip button; and the
script's `kasfSubtitleReset`. If the build manager was left with Escape during the fleet-intel line, that is the likely
cause; the log could not say. So the log now can: every speech event that is not a unit's chatter / status / command
line writes a `speechlog:` line (SpeechEvent.c `catSpeechLog`: queued with its stream, dropped with the reason - speech
off, NIS running, actor disabled, another player's, queue full, would time out, already playing - play, ended, expired,
cleared / faded by an actor stop), each with the tick, the time and the sensors / fleet-intel / build-manager / NIS
state; the script's request (`kasfSpeechEvent`), the subtitle reset, the build manager's open and close, and each
skip's trigger (`smSensorsSkip` with its circumstances, the key in the fleet-intel view, Backspace in the build task)
log with the same prefix. The next play log answers the question for line 13 and any other line.

## Fence (debug builds of the worktree, ASan 0 in every run, both builds 0 warnings)
Runs in `original/scratch-m2/m2r2-harness/runs/`; "unfixed" is the probe binary built before the AITrack.c change.

| run | what | result |
|---|---|---|
| m3r4-nan-m17-probe | unfixed + probe, Mission17 self-test to its end, `/catNaNProbe` | 4 `nanprobe:` ships (first at tick 330, mask 0x38 = coordsys / rotspeed / torque in the clProcess phase, position later), 4 NaN kills (M3-R3's baseline m3r3i-e-m17-a has the same 4 kills); done 15584 ticks 4/4 PASS |
| m3r4-nan-save17-before | unfixed + probe, the user's save "17 - Naval Base Alpha" (loadgame, tick 144, 410 ships), 4000 ticks | 23 `nanprobe:` ships (tHeavyInterceptor / tAttackBomber of player 0; bAcolyte / bMultiGunCorvette / bHiveFrigate / bACV of player 1) |
| m3r4-nan-save17-after | fixed, the same save, 4000 ticks, `/catNaNProbe` | 0 `nanprobe:` ships, 0 `nanprobe-bullet:`, 0 NaN kills; the 2 `nanprobe-fly:` self-attack sightings remain (harmless) |
| m3r4-nan-m17 | fixed, Mission17 self-test to its end, `/catNaNProbe` | 0 NaN ships / bullets / kills; done 15584 ticks 4/4 PASS; stream differs from m3r3i-e-m17-a as it must (the baseline's 4 ships died of NaN) |
| m3r4-nan-m16 | fixed, Mission16 to its end, `/catNaNProbe` | 0 NaN ships / bullets / kills; done 21523 ticks 6/6 PASS; stream IDENTICAL to m3r3i-e-m16-a (21492 ticks compared) |
| m3r4-nan-m8 | fixed, Mission08 to its end, no switches | done 29361 ticks 6/6 PASS; stream IDENTICAL to m3r3i-e-m8-a (29330 ticks) |
| m3r4-nan-m15 | fixed, Mission15 to its end, no switches | done 15424 ticks 3/3 PASS; stream IDENTICAL to m3r3i-e-m15-a (15393 ticks); 40 `speechlog:` lines (the self-test runs with speech off: every script request is logged "dropped .. speech off or sound paused") |
| m3r4-nan-mission | fixed, mode mission 6000 ticks | stream IDENTICAL to m3r3i-e-mission-a (6001 ticks) |

"IDENTICAL" is `tools/oracle/hwo_compare.py`'s verdict (the run.hwo sha256 differs only by the build identity in the
header). The speech log cannot change a stream (logging only); Mission15 / 08 / 16 and the mission mode say so.

## Files
- `src/Game/AITrack.c` (the fix), `src/Game/CameraCommand.c` (the focus safety net), `src/Game/UnivUpdate.c` /
  `UnivUpdate.h` (the probe, the kill message), `src/SDL/main.c` (`/catNaNProbe`), `src/Game/AIShip.c`,
  `src/Game/CommandLayer.c`, `src/Game/KASFunc.c` (the probes' other sightings; the speech log's script lines),
  `src/Game/SpeechEvent.c`, `src/Game/Sensors.c`, `src/Game/KeyBindings.c`, `src/Game/ConsMgr.c` (the speech log).
