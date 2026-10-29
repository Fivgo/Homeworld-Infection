# Intro logos and movie aspect (HWI-FEATURE(intro-movies), M3-R4)

The user (2026-10-29): "renable the starting cutscenes for sierra and barking dog and maintain their original aspect
ratio instead of stretching them out".

## What plays

At every start, before the front end, as cataclysm.exe plays them (utyGameSystemsInit 0x005abe70, whenever `enableAVI`):

| movie | length | music | after it |
|---|---|---|---|
| `Movies/sierra.bik` (640x480, 15 fps, 240 frames) | 16.0 s | track 0x3c (60) | its last frame stays 2 s (the exe's `Sleep(2000)`) |
| `Movies/bdogintro.bik` (640x480, 30 fps, 807 frames) | 26.9 s | track 0x3d (61) | the front end |

Music4Cat's header lists 62 tracks: 60 and 61 are the logos'. The music starts with the first frame and stops at the
movie's end (binkPlay 0x00551840). A key skips the movie playing (the exe: Escape's key-up, WindowProc 0x00561f10's
`binkStop`; GoK and this engine: any key). Closing the window ends the intros and the game quits.

The exe plays them before its renderer starts (Bink blits through GDI). Here they need GL, so they play after `rndInit`,
where GoK left its placeholder (`aviIntroPlay`). GoK's placeholder played `sierra.bik` and HW1's `relicintro.bik` with no
music, and only with its own options row `PlayIntros` set (default 0).

## The shape

Each movie's picture keeps its aspect: the part of the frame that holds the picture is fitted whole into the window
and centred, and the rest of the window is black. Nothing is stretched or cropped.

| movies | the picture (every frame measured) | at 3840x2160 | at 1280x1024 |
|---|---|---|---|
| the two logos | the whole 640x480 frame (4:3) | 2880x2160, bars left and right | 1280x960, bars above and below |
| `Animatic01..18.bik` | rows 48..431, 640x384 (5:3), black above and below | 3600x2160 (frame 3600x2700) | 1280x768 |

cataclysm.exe blits the 640x480 frame 1:1 in the middle of a window, or switches a fullscreen display to 640x480, which
a widescreen monitor stretches. GoK's player assumed HW1's 1.85:1 picture for every movie, so at 16:9 it cut a quarter of
a logo's height and about 6% off the top and bottom of every animatic.

## Settings

| how | what |
|---|---|
| Cataclysm.cfg `PlayIntros 0` | no intro logos (default 1) |
| switch `/disableAVI` | no intro logos, this run only (the exe's switch) |

The harness's home template keeps `PlayIntros 0`, and a headless run (the virtual clock, `/debugScreenshots`) plays no
movie at all, as for the animatics. The Windows build has no movie support (`-Dmovies=false`): it logs
`intro: off (no movie support in this build)`.

## The log

```
movie: start file=Movies/sierra.bik 640x480 15.00 fps frames=240 music=60 (an intro logo)
movie: drawn at 480,0 size 2880x2160 in the window's 3840x2160 (logical 1920x1080), picture the whole frame (4:3)
movie: end file=sierra frames=240 of 240 in 16000 ms (the end)
movie: held the last frame of sierra 2003 ms of 2000 (the end)
...
intro: done in 46218 ms
```

`drawn at` is logged again whenever the window's size changes the placement. The end's reason is `the end`, `a key`,
`the window closed` or `the test's frame cap`. With intros off: `intro: off (PlayIntros 0 in Cataclysm.cfg)`,
`(/disableAVI)`, `(a headless run)` or `(no movie support in this build)`.

## Tests (HWI-TEST)

| switch | what |
|---|---|
| `/catMovieShot <frame>[,<frame>...]` | those frames of every movie are read back into `<settings>/movie-<name>-<frame>.ppm`, one `movieshot:` line each |
| `/catMovieTest <level>[:<frames>]` | once the front end is up, that level's animatic plays (0 = Animatic01, at most `<frames>` frames), then quit |
| `/catMovieEventAt <frame>:quit\|<W>x<H>` | once, after that frame of a movie: the window's close request (an SDL_QUIT in the engine's own queue) or a new window size (`SDL_SetWindowSize`). No keyboard or mouse event |

The harness's `--cfg "PlayIntros    1"` turns the intros on for a run (`--mode fe`).
