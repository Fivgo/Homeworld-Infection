# M3-R4 intro-movies: the Sierra and Barking Dog logos, and the movies' aspect

The user (2026-10-29): "renable the starting cutscenes for sierra and barking dog and maintain their original aspect
ratio instead of stretching them out". Feature page: `docs/features/intro-movies.md`.

## What the exe does

From the r6 export:
- `utyGameSystemsInit` 0x005abe70, after `soundEventInit`, when `enableAVI`:
  1. `binkInit(-1)`.
  2. `binkPlay(.., "Movies\sierra.bik", NULL, 0, 0, 1, 0x3c)`, then `Sleep(2000)`.
  3. `binkPlay(.., "Movies\bdogintro.bik", NULL, 0, 0, 1, 0x3d)`.
  4. `binkCleanup` and `mainCleanupAfterVideo`.
- `binkPlay` 0x00551840 with a track: `soundEventUpdate` / `speechEventUpdate`, a 150 ms wait, the same again; `soundEventPlayMusic(track)` before its message loop, `soundEventStopMusic(0)` after it.
- With a NULL display callback, the frame is shown by `binkDisplayWin32` 0x00551350: a 640x480 GDI blit, centred in a window. In fullscreen the display is switched to 640x480 (`hwSetRes`), which a widescreen monitor stretches.
- `WindowProc` 0x00561f10 calls `binkStop` on Escape's key-up.
- Music4Cat.wxh lists 62 streams, so 0x3c and 0x3d are its last two.

## What the port did

- `aviIntroPlay` (GoK) ran `sierra.bik` and HW1's `relicintro.bik`, which Cataclysm does not have. It played no music. It was gated by GoK's own options row `PlayIntros`, default 0. The play home and the harness template both seeded `PlayIntros 0`.
- GoK's `aviDisplayFrame` assumed HW1's 1.85:1 picture for every movie. All 20 movies were decoded and their non-black area measured:
  - `sierra.bik` and `bdogintro.bik` use the whole 640x480 frame.
  - All 18 animatics use rows 48..431 (640x384, 5:3; Animatic12 rows 49..429).

  So at 16:9, GoK's draw cut 25% of a logo's height and 72 rows of an animatic's 2304 at 4K.
- GoK's texture handling had three faults:
  - One texture name was kept for the whole run, so a movie after a resolution switch (a new GL context) bound a name from the old one.
  - The texture's filter was set before binding it, which changed whatever texture was bound before.
  - `GL_REPLACE` and `GL_TEXTURE_2D` were left set behind the renderer's state cache (`rndTextureEnvironment`).
- `aviStart` allocated an `AVFrame` per movie that was never freed.
- The movie loop read one event a frame and acted only on a key, so a closed window or a resize during a movie was dropped.

## The change (src/SDL/avi.c, uiscale.c, main.c, utility.c, Animatic.c)

- `aviPlayIntros` defaults to 1, so `PlayIntros 0` still turns the intros off. They are also off with `/disableAVI`, in a headless run (`animHeadlessRun`), and in a build without movies.
- `aviIntroPlay` plays the exe's order: `sierra.bik` with track 60 and a 2 s hold on its last frame, then `bdogintro.bik` with track 61. Neither runs the animatic decode callback or draws subtitles (the exe passes neither for the logos).
- `aviPictureRect` fits the picture whole and centred: the whole frame for the logos, the 5:3 band for the animatics.
- The draw:
  - A texture of the frame's own size is made per movie and deleted at its end. It is bound before its filter is set, and `GL_CLAMP_TO_EDGE` is applied.
  - The draw is wrapped in `glPushAttrib` / `glPopAttrib` with its own viewport.
  - Scissor, depth, blending, lighting, culling, fog and alpha are off for the draw.
- `aviEvents` drains the queue each frame:
  - A key press (not a repeat) ends the movie.
  - `SDL_QUIT` ends it and is pushed back for the main loop. `aviQuitAsked` skips the rest of the intros.
  - Window events go to `HandleEvent`. `uiScaleResizeApply` defers the relayout while `aviIsPlaying`, and `uiScaleResizeFlush` runs it after the movie.
- `aviStop` frees the frame. The log reports the frame count from the Bink stream's duration.
- New HWI-TEST switches: `/catMovieShot`, `/catMovieTest`, `/catMovieEventAt`.

## Fence (debug build, ASan; 2026-10-29)

| run | result |
|---|---|
| m3r4-intro-4k (`--mode fe --res 3840x2160`, PlayIntros 1, shots 100/200/239) | sierra 240 of 240 in 16000 ms, held 2003 ms, bdogintro 807 of 807 in 26901 ms, intros 46.2 s; drawn 480,0 2880x2160; the 5 shots vs the source frames scaled into the rect: mean abs diff 0.02 to 0.05 (upside-down would be 5 to 74), bars max 0; ASan 0 |
| m3r4-intro-54 (1280x1024) | drawn 0,32 1280x960; shots diff 0.06 / 0.09, bars 0; frames 240/240 and 807/807; LSan clean |
| m3r4-movie-anim (`/catMovieTest 0:200`, 3840x2160) | Animatic01 drawn 120,-270 3600x2700 (picture 3600x2160); frame 150 diff 0.10, bars 0; 200 frames (the cap); rc 0 |
| m3r4-intro-quit (`/catMovieEventAt 50:quit`) | sierra ended at 51 frames (the window closed), bdogintro skipped, the game quit, rc 0 |
| m3r4-intro-off (`/disableAVI`) | `intro: off (/disableAVI)` |
| m3r4-intro-x11 (`--hidden-x11 --res 2560x1600`, resize to 1920x1080 at frame 60) | the NVIDIA driver under Xwayland: drawn 213,0 2135x1601 in 2561x1601, then 240,0 1440x1080 after the resize ("the relayout waits for the movie's end", flushed after); shots diff 0.02 to 0.06, bars 0 |

The regression results are in the STATE bullet.

## Not verified

- The logos' music by ear. The harness's audio is the dummy driver; the log shows the calls.
- An animatic's subtitles. Under the dummy driver the speech events expire before they play, so no subtitle card is drawn. The draw now restores the GL state the subtitles start from.
- The user's own start on screen.
