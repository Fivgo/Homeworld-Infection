# The Escape menu's Video screen, laid out again

HWI-FEATURE(video-layout). Code: `src/Game/Options.c` (`opVideoLayoutAtomsAdd`, called by `FEFlow.c
feScreensImageLoad` after the UI SCALE buttons and the SMOOTH MOTION checkbox), the switch `/exeVideoLayout` (main.c);
HWI-TEST `/catVideoLayoutTest` (`src/Game/VideoLayoutTest.c`). Only the in-game screen (in_game_esc_menu.fib "Video");
the front end's Video_Options keeps cataclysm.exe's layout.

## The user's words (verbatim)
"while in a mission, can we shift the position of the UI scale settings to be next to the smooth motion toggle? we also
need to shift the slider for in game brightness to the right so that it fits inside the box. Model detail also need a
similar adjustment. We need to center the texture info button to the center of the texture memory box and ensure that
these changes are compatible with any resolution"

## What cataclysm.exe does
The screen is the file's atoms at their 640x480 positions, centred at any size. The exe draws the file's layout the
same way as this port (docs/features/screen-background.md "The exe's own overlaps"), and the file has:
- every slider's rectangle flush with its box's rectangle, while `ferDrawHorizSlider` draws the rail 11 beyond the
  rectangle on each side (the end caps at x0 - 12 and x1 - 4) and a box's frame is drawn 7 outside its rectangle: IN
  GAME BRIGHTNESS's rail runs 112 .. 284 over the box's frame at 116, MODEL DETAIL's 114 .. 287 over 118, NUMBER OF
  EFFECTS' 371 .. 531 over both sides of 375 .. 527 (its slider is as wide as its box), and that knob at HI reaches 528;
- MODEL DETAIL's "FULL DETAIL" label at y 215 on the box's top frame line (219), its checkbox at the slider's end;
- TEXTURE INFO at the TEXTURE MEMORY box's right end (81 from the box's left side, 4 past its right side).
The mod's UI SCALE sat at the top of the right column and SMOOTH MOTION under NUMBER OF EFFECTS (the earlier round).

## Design (positions in the file's 640x480 coordinates)
Every move sets the atom's loadedX / loadedY (and loadedWidth / Height) and keeps the screen's centring, so the loader's
centring and a relayout (`feAllScreensReposition`, which centres from loaded*) both place it: the layout holds at every
window size and UI scale. The atoms are found by type, name or text and the file's position; a screen that differs is
left as the file has it ("videolayout:" line).
- **Sliders inside their boxes** (`opVlSliderFit`): the slider moves right until its rail starts at its box's left edge
  (box x + 11), and narrows only if the rail would pass the box's right edge; the labels under its ends keep their
  offsets from the slider's ends. IN GAME BRIGHTNESS 123 -> 134 (LO 131, HI ends 292), MODEL DETAIL 125 -> 136 (BEST
  FRAMERATE 134, BEST DETAIL ends 306). Clicks map through the slider's own rectangle (`SetHSliderValue`: max * (x -
  x0) / (x1 - x0)) and the knob is drawn from it, so the ends are still 0 and max.
- **MODEL DETAIL**: FULL DETAIL on a row of its own under BEST FRAMERATE (checkbox 134,255, label 154,257: box left of
  the text, as the screens' radio buttons read; the checkbox's click area spans the label); the box 21 taller (226 ..
  272) to hold the row.
- **TEXTURE MEMORY**: 2 lower (title 283, box 304 .. 327) for that row; TEXTURE INFO centred in the box (163,300: 38 /
  39 from the box's sides, the 32-high button 4 / 5 over the 23-high box, as before) with its text.
- **The right column**: EFFECTS and NUMBER OF EFFECTS 22 up (the EFFECTS title on IN GAME BRIGHTNESS's row 97, its box's
  top on that box's top 118); **UI SCALE with SMOOTH MOTION** one block under NUMBER OF EFFECTS: the title at 271 (11
  under that box, as MODEL QUALITY's title under IN GAME BRIGHTNESS's box), the buttons at 379 / 449 on rows 291 / 310
  (the EFFECTS radio buttons' title-to-row 20 and row pitch 19), SMOOTH MOTION at 379,331 (its glyph ends at 347, ACCEPT
  starts at 357). The block did not fit above ACCEPT with the screen's spacing without the 22.
- **Its own item, which the user did not name: NUMBER OF EFFECTS' slider** is wider than its box once the rail is
  counted, so the fit narrows it, 138 -> 116 (393 .. 509, rail 382 .. 520 = the box), LO at 390 and HI ending at 511.
  Its values are the same 0 .. 192 over a shorter rail. To drop this item, remove the `opVlSliderFit(numSlider, ..)`
  line in `opVideoLayoutApply`.
- `/exeVideoLayout` (this run only): the screen as the file has it, UI SCALE and SMOOTH MOTION where they were before;
  its 4K frame equals the previous build's over the whole panel (an A/B).

## The sliders' values
Unchanged mapping: a click at the slider's left end gives 0, at its middle half the maximum, at its right end the
maximum. A drag past the right end stops one short of the maximum (the mouse is clipped to the rectangle's last
column, x1 - 1, by `mouseClipPointToRect`: 20 / 21 for MODEL DETAIL, 69 / 70 for IN GAME BRIGHTNESS, 190 / 192 for
NUMBER OF EFFECTS). That is the exe's clip and mapping, the same before and after this change (the self-test logs it
for both layouts); a click on the knob's end of the rail reaches the maximum.

## HWI-TEST /catVideoLayoutTest <screen>
Push the screen in a game (`--mode sp -- /catFeScreen Video@60 /catVideoLayoutTest Video`, any `--res`). After 20
frames: every control's and text's drawn extent (a text's font width and its glyph rows, a check box's or radio
button's 16x16 glyph, a slider's rail, a button's art) against every box's frame lines (none may cross one), no two
extents overlapping (a button's own text excepted), the controls inside the panel's frame, TEXTURE INFO centred within a
pixel, the UI SCALE / SMOOTH MOTION block 4 or more above ACCEPT; each slider set to 0, half and max, the frame read
back along the rail and the knob's red centre found: its offset from the value's place the same within a logical pixel
and the knob inside its box's frame; clicks at x0 / middle / x1 and drags past both ends through the region's own
processing (`uicHorizSliderProcess`, the engine's mouse position, no OS input). The values, the options they drive and
the mouse are put back; "videolayouttest: done ... checks=N fails=F result=PASS|FAIL"; the engine quits.
Results: PASS, 1,576 checks, at 1024x768, 1920x1080 (100%), 2560x1440 (150%), 3840x2160 (200%), 1234x777 and
1920x1080 with `/uiScale 150`; with `/exeVideoLayout` FAIL on exactly the six items above (three rails over their
frames, FULL DETAIL on the frame, TEXTURE INFO off-centre, the NUMBER OF EFFECTS knob at max over the frame).

## Seen at 150%, not changed
At fractional UI scales (150%, and 1920x1080 at `/uiScale 150`) every box's frame shows a one-pixel line end below its
bottom corners: the frame's straight tiles and its corner pieces are drawn at whole logical pixels, which fall on half
physical pixels at 1.5x, and the nearest-filtered tile reaches one physical column past the corner's rounded edge. The
same in the front end and in the capture before this change; none at
100% or 200%. A fix would snap the front end's textured quads (FEReg.c `ferDraw`) to whole physical pixels at fractional
scales: a front-end-wide rendering change, left for the user to decide.
