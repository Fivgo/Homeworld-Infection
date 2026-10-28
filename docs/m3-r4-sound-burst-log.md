# M3-R4 ad hoc: the sound log (HWI-TEST sound-burst-log), 2026-10-28

Landed from the user's terminal session 38027cce between rounds, after the full playthrough.

## The user's words (verbatim)
- "also another issue that happened on the tail end now that I think about it was during standard play, there was a
  large audio spike of white noise. Is there any possible way you can track that down in the logs"

## What the logs could say: nothing
The play log of the session (original/play-home/logs/play-20261028-102144.log, 10:21 to 13:18, twelve game loads,
the last one the final mission through Animatic18 and "Quit to main menu") holds no line from the sound system: the
exe's sound code logs nothing and the port's logs only init failures and a stream header with a bad ID (never seen).
Nothing in any play log since 2026-10-20 mentions the mixer, a stream or a patch. Around the tail end the log shows
only the mission's speech lines (speechlog), the build manager's opens and closes, the pauses, and two
`univCheckShipState` kills of Beast Ramming Frigates with a NaN position (#1295 / #1296, owner 1; two more, #628 /
#629, in an earlier mission of the same session): a second NaN path besides the Mission17 one of nan-positions, in
a ramming frigate's special manoeuvre, still open. A NaN ship cannot reach the mixer (its pan and volume are integer
casts, its pitch would silence the channel in `fqPitchShift`, not hiss), so the kills are not the burst's cause by
any path found.

## What was read for a cause (none proven)
The mixer (`src/SDL/smixer.c` isoundmixerprocess 0x0057f850), the streamer (`sstream.c`), the channel code
(`soundlow.c`) and the fq decoder (`fquant.c` / `fqcodec.c`), all following the exe (M2-R5 sound). White noise out
of this mixer means bytes decoded as fq blocks that are not block-aligned audio: the dequantiser spreads random
bits over every band. Where that could start:
- a stream entry whose size is not a multiple of its block (`bitrate >> 3` bytes): the mixer's end tests are exact
  equalities (`currentpos == writepos`, `amountread == size`), so a misaligned entry end is stepped past and the
  stale bytes behind it decode as noise until the half-buffer ends (up to 0.7 s). The exe has the same tests, so
  the data should be aligned; now measured (`stream N end missed`).
- the panic mode (a mixer block seeing no rendered frame for ~128 ms): the user's settings (Cataclysm.cfg
  SoundQuality 0, AutoChannel 1) keep the DCT at full size but let the voice count fall to 8 and the mixer stop
  voices; not a noise source by inspection, now logged (`panic on / off`).
- a stream starved (the stream thread late with the next half block): silence, not noise, now logged.
- the fq decoder's per-band exponents persist from block to block unless a block resets them, so a decode that
  starts at a wrong block (a loop point, a restart, a stream start after another) plays at wrong band scales until
  the next reset; the exe does the same with the same data. The burst report names the source's block position and
  loop points, so a repeat can be checked against this.
- the pitch shifter, the volume / pan fades and the SDL output path are bounded and clamp; a NaN would silence or
  freeze the output, not hiss.

## The sound log (smixer.c, sstream.c, soundlow.c / .h, render.c, main.c)
Lines `soundlog: ... tick=N t=S` in every play log, posted by the mixer thread and the stream thread into a
lock-free ring of 16 slots (`catSoundLogPost`) and printed by the render task once a frame (`catSoundLogFlush`,
before `speechEventUpdate`) with the universe tick; a full ring drops and counts.
- `burst start block=B peak=P zcr=Z% flat=F sources=N voices=V/W inuse=I panic=0|1 dct=norm|half`: three output
  blocks in a row whose left channel peaks at 12000 of 32767 or more with 30 % or more sign changes between
  consecutive samples (white noise: ~50 %, speech and music well under) and a spectral flatness (the geometric over
  the arithmetic mean of the mixed left spectrum) of 0.30 or more (white noise: above 0.5, tonal sound near 0);
  then up to three `burst src` lines, the loudest sources of that block by the energy of their decoded spectra
  scaled by their volume factors: `chan=C ... patch=bankB:N id=0x... block=K/T loop=L-S-E pitch= vol= pan=
  status= prio= fq= rate=` for an SFX channel, `stream=S ... entry=Q event=0x... actor= offset= size= read=
  bitrate= ch= pos= writepos= bs= rb= wb= queued= toplay= vol=` for a speech or music stream; and `burst end
  block=B blocks=N peakmax=P` after it. Forty bursts a run, then "(reports capped)".
- `panic on block=B voices V->W dct=...` / `panic off ...`: the mixer's panic mode.
- `stream S starved N blocks (ended block=B ...)`: a playing stream found its next half block unwritten for 4
  blocks or more (its start-up gap is shorter); twenty a run.
- `stream S end missed block=B: past writepos by X bytes, read R of entry size Z (block K bytes) ...`: the exact
  end test stepped past an entry's end; once per entry, twenty a run.
- `stream S recover ret=R entry=Q fhandle= offset= size= status= dataleft=`: the stream thread's recovery path (a
  header or block read that failed).
- `test noise injected into stream|channel N block=B (12 blocks)`: HWI-TEST `/catSoundNoiseAt <block>` overwrites
  the first playing source's decoded spectrum with white noise for 12 blocks: the detector's fence.

## Fence (debug builds of the worktree)
Runs in `original/scratch-m2/m2r2-harness/runs/`.

| run | what | result |
|---|---|---|
| m3r4-sbl-sound | `--mode sound` (the sound self-test: music track 0, a Somtaaw and a Beast speech event through the mixer into the dummy device) | 0 soundlog lines: no burst, no starved stream, no missed end; ASan 0 |
| m3r4-sbl-skirmish | skirmish LiveFireExercise2, 1200 ticks, headless | 1 line, `panic on` at the run's end (block 330: no rendered frame in headless mode, the expected trigger); 0 bursts; ASan 0 |
| m3r4-sbl-rc60 | `--scene worker:0:500:800 --real-clock 60` (87 s on the wall clock, 6556 mixer blocks: engines, weapons, chatter, music) | 1 line, the same headless `panic on` at the end; 0 bursts, 0 starved, 0 missed ends; ASan 0 |
| m3r4-sbl-inject1 | `--mode sound -- /catSoundNoiseAt 1` at the first injected amplitude (4000 a bin) | the 12 blocks measured peak 5361-8782, zcr 30-53 %, flat 0.70-0.79: under the 12000 peak gate, no burst reported (a quiet hiss is not "a large spike"); the test amplitude raised to 10000 |
| m3r4-sbl-inject2 | `--mode sound -- /catSoundNoiseAt 1` | `test noise injected into stream 3 from block=39`; the 12 blocks peak 13403-21955, zcr 30-53 %, flat 0.70-0.79; `burst start block=41 peak=21955 zcr=44% flat=0.76 sources=1`; `burst src stream=3 e=5.57e+09 flat=0.76 ... offset=28512528 size=1059200 read=384 bitrate=512 ch=2` (the music stream: Music4Cat.wxd track 0); `burst end block=52 blocks=11 peakmax=21955`; ASan 0 |

White noise measures zcr ~50 % and flatness ~0.77 at this mixer; the gates (30 %, 0.30, peak 12000) sit well under it
and well over speech and music (no line in 6556 blocks of play). Both builds 0 warnings.

## Next time it happens
The play log will hold `soundlog: burst start` with the tick, the mixer block and the source: a patch (bank and
index, its id, the block position within it and against its loop points) or a stream entry (the speech event, the
file offset, the bytes read against the entry's size, the buffer state). That names the mechanism; the fix follows
from it. If the burst comes without a `burst start` line, the noise is not in the mixer's output (the audio device
or the OS), which the line's absence proves.
