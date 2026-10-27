# M3-R4 ad hoc: every crystal's tooltip read "(0)" (HWI-FIX crystal-tooltip), 2026-10-27

Landed from the user's terminal session 38027cce between rounds, after the user's Mission11 play.

## The user's words (verbatim)
- "I've been noticing that the crystals that I have found throughout the missions, most recently mission 11, that when
  leaving my cursor over the crystal, the resource value given is always 0, but I do know that I have been getting
  resources from them. Please look into what is happening and ensure that all crystals list their appropriate values"

## What a crystal is
- `dCrystal0` / `dCrystal1` are RACE_DERELICT ship types (`race_derelict/dcrystal0.shp` `resourceValue 6000`,
  `dcrystal1.shp` `resourceValue 10000`); a level places them as ships (Mission11's `resourcesphere_1.missphere` lines 19,
  20, 114: two dCrystal0, one dCrystal1; Mission08's `resourcesphere_4.missphere` one of each; Mission17's
  `crystals` five dCrystal0; Mission06 and 16 include the types too).
- `CrystalInit` (src/Ships/Crystal.c, exe 0x005b0040) copies the statics' value into the ship's `CrystalSpec.resourceValue`
  (Ship+0x638); a salvage depot dissolves the crystal over `twCrystalProcessingTime` and pays the owner the dissolved
  fraction each tick (`cat_salvageDepotProcessCrystal`, Salvage.c:1885, exe 0x004f98b0), so the field counts down to 0 as
  the RUs arrive. That is why the user's resource count rose while every tooltip said 0.

## The tooltip
- `cat_moTooltipDraw` (src/Game/InfoOverlay.c, exe 0x004a79d0) classifies the object under the cursor by
  `mouseCursorOverShipType` (Select.c `selShipUnderMouse`: a crystal counts as the player's own there, placed for player
  1 and flagged friendly by the mission's script, SPECIAL_FriendlyStatus / `kasfShipsSetNonRetaliation`; the test shows
  `owner=1 over=126`, the own-ship branch) and
  prints a crystal as `"%s (%d)"` with `ShipTypeToNiceStr` and its resource units.
- The port printed `cat_mimicGetApparentObject(ship)` for the `%d`: the mimic's apparent asteroid value (exe 0x005b9940:
  Ship+0x72c, only when the ship's mimic flag `catFlags18 & 8` is set, 0 for any other ship). A crystal is never a mimic, so
  every crystal read "(0)". The decompile hides the fault: Ghidra drops a varargs `sprintf`'s extra arguments, and the mimic
  branch two lines up really does print 0x005b9940's result, so the two branches looked alike.
- The exe's crystal branch (cataclysm.exe 0x004a7df0-0x004a7e2b, `objdump -d -M intel`):
  ```
  4a7df1  call 0x5affa0            ; cat_shipIsCrystal
  4a7dfd  mov eax, [0xa75800]      ; mouseCursorObjPtr
  4a7e02  mov ecx, [eax+0x638]     ; CrystalSpec.resourceValue  <- the %d
  4a7e08  mov edx, [eax+0x2b0]     ; shiptype
  4a7e10  call 0x4d6c30            ; ShipTypeToNiceStr          <- the %s
  4a7e1d  push 0x885970            ; "%s (%d)"
  4a7e23  call 0x670b30            ; sprintf
  ```
  The mimic-as-resource branch (0x004a7d2x) calls 0x005b9940 for its `%d`; the crystal branch never does.

## The fix (HWI-FIX (M3-R4 crystal-tooltip))
- InfoOverlay.c: the crystal branch prints `((CrystalSpec *)ship->ShipSpecifics)->resourceValue` (Crystal.h included). The
  exe's one-function draw is split: `cat_moTooltipText(obj, overType, text, &outline, &textColour, &box)` builds the text
  and colours (the old chain verbatim, `mouseCursorOverShipType` passed in), `cat_moTooltipDraw` keeps the gating, the delay
  and the drawing. Render only: no stream input changes.
- A crystal being dissolved shows its remaining units (the field counts down), as the exe does.

## HWI-TEST (M3-R4 crystal-tooltip): `/catTooltipAt <tick>:<ShipType>`
With `/catMissionTest`: at that universe tick the first unhidden ship of the type is put under the cursor exactly as the
mouse code does (`selShipUnderMouse` with the cursor state reset, then restored) and `cat_moTooltipText` is logged:
`tooltiptest: tick=N ship=<type>#<id> owner=P over=T text="..."`, a crystal's line adding `crystal units=U statics=S`.
MissionSelfTest.c declares the three cursor globals itself (mouse.h's and InfoOverlay.h's `objectives` collide with the
file's own extern).

## Fence (debug builds of the worktree, ASan 0 in every run, both builds 0 warnings)
Runs in `original/scratch-m2/m2r2-harness/runs/`; "unfixed" is the test binary built before the one-line fix.

| run | what | result |
|---|---|---|
| m3r4-ct-m11-before | unfixed, Mission11 600 ticks, `/catTooltipAt 300:dCrystal0` | `tooltiptest: tick=300 ship=dCrystal0#44 owner=1 over=126 text="CRYSTAL SMALL (0)" crystal units=6000 statics=6000` (the user's symptom) |
| m3r4-ct-m11-after | fixed, the same | `text="CRYSTAL SMALL (6000)"` |
| m3r4-ct-m11 | fixed, Mission11 to its end, `/catTooltipAt 300:dCrystal1` | `ship=dCrystal1#45 ... text="CRYSTAL LARGE (10000)" crystal units=10000 statics=10000`; done 8275 ticks 5/5 PASS; stream IDENTICAL to m3r3i-e-m11-a (8244 ticks) |
| m3r4-ct-m8 | fixed, Mission08 to its end, `/catTooltipAt 300:dCrystal0` | `ship=dCrystal0#43 ... text="CRYSTAL SMALL (6000)"`; done 29361 ticks 6/6 PASS; stream IDENTICAL to m3r3i-e-m8-a (29330 ticks); file list identical |
| m3r4-ct-mission | fixed, mode mission 6000 ticks, no switches | stream IDENTICAL to m3r3i-e-mission-a (6001 ticks); file list identical |
| m3r4-ct-m11-pre | the 0c0d634-era binary (pre infect-convert), Mission11 to its end, no switches | the control for the file list below: stream IDENTICAL to m3r4-ct-m11 |

- Mission11's file list has 26 more `.LiF` lines than M3-R3's baseline (6 `dJunk1Strut` / `dJunk2Shipwreck`, 20
  `RACE_BEAST\Missile`): the mine / missile recolour's registration across the Mission11 -> Mission12 change (0c0d634), not
  this change's: the control run with the pre-item binary has the same 26 against the baseline and 0 against m3r4-ct-m11.

## Tags
HWI-FIX 76 -> 77, HWI-TEST 511 -> 519, CAT-FIX 201 (unchanged: the exe is right here).

## Pre-existing, noted, not touched
- UBSan's misaligned ushort load at src/Game/BTG.c:389 in every level load (non-fatal).
- The 26 extra Mission11 texture lines above (0c0d634's double registration), harmless.

## For the user
Hovering a crystal now reads "CRYSTAL SMALL (6000)" or "CRYSTAL LARGE (10000)", and while a depot dissolves one the number
counts down to 0 as the resource units arrive, as the original does. It is in the next release build (play.sh rebuilds a
stale binary on launch; the game was running while this landed, so the release was not rebuilt here).
