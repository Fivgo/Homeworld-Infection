# Feature target: 3840x2160 at up to 144 fps (user, 2026-10-17)

The user wants the recreation to run at 4K and up to 144 fps on a modern system by the end, as the demonstration of
"what it is like on a modern machine". It is a TARGET for M3's exit test and the features phase, not a separate port.

What it needs (each already touched by the plan; this ties them together):
1. **Render rate decoupled from the 16 Hz simulation:** already the engine's design (the render task runs every frame).
   Cataclysm added a frame-rate limiter in the render task (CET NOPs 0x0057B78B / 0x0057B7A1, `DisableFrameRateLimit`);
   the recreation must make it a setting, default off, so the renderer can run at the display's refresh.
2. **Motion interpolation** (docs/features/motion-interpolation.md): without it, 144 fps only shows 16 Hz stepping
   nine times per tick; with it, high refresh is the point. Keep the toggle.
3. **4K window and viewport:** the SDL/GL window must accept 3840x2160 (and the user's 150% desktop scale), with the
   GL viewport, picking, HUD placement and front-end art handled at that size. The front end draws fixed 640x480 art
   centred (docs/m0-first-run.md); UI scaling becomes a follow-on feature, listed below, not a blocker for the fps target.
4. **Frame pacing, measured:** M3's hour-long soak runs at 3840x2160 with vsync at 144 Hz on the RTX 5090; the harness
   records frame times; exit = 99th-percentile frame time under 1/144 s with no drift over the hour (the user's gamescope
   complaint was exactly this drift). Budget check: the 2000-era renderer is immediate-mode GL; expect CPU-side draw
   submission, not the GPU, to be the limit; profile before optimising.
5. **No gamescope, no wrapper** in the measurement (docs/CONSTRAINTS.md rule 1).

Follow-on features (not required for the target): UI/front-end scaling above 640x480; higher-resolution text.

## The soak's instrument (M3-R2, docs/m3-r2-instrumentation.md)

- **`/frameLog <file>`** (HWI-TEST, run-only; the harness's `--frame-log` writes `<run>/frame.log`): a header (`# hwi-framelog
  v1 build=.. commit=.. size=WxH logical=WxH refresh=R (source) heap=67108864 driver=..`, then `# settings: <the settings
  line>`), then once a second `s=N t=.. frames=.. ft_p50 / ft_p95 / ft_p99 / ft_max` (the frame PERIOD, ms: from one end of
  the render task, after the flip, to the next: every task of the frame and the wait for the swap included), `over` (frames
  longer than one refresh interval) with `over1 / over2 / over3p` (1, 2, 3+ refreshes late), `rnd_p50 / rnd_max` (the
  render task alone, its top to after the flip, no glFinish), `ticks` and `tick_p50 / tick_max` (each universe task call,
  ms), `heap_used` / `heap_growth` / `growth_pools` (the memory module's allocated bytes in the main heap and its growth
  heaps, small-block heaps counted by block; the growth heaps made), `dupe` (live "dupe trcolorinfo list" blocks, M3-R1's
  O4; -1 in the release build, which keeps no block names), `verts_p50 / verts_max` (the /catLodStats count); and an
  `h s=N bin:count,..` line, the second's frame periods in 0.1 ms bins. The refresh R is the `--real-clock` rate in a
  harness run, else the display mode's refresh (SDL), else 144 (the header says which). Nothing is read back, no
  glFinish, no random stream: the streams do not move with it.
- **`python3 -I tools/frame_log_report.py <frame.log> [--window 10] [--tolerance 0.10] [--oneline]`**: the whole run's
  frame-period p50 / p95 / p99 / max from the histograms, the frames over budget, the universe ticks and their cost, the
  vertices; **the drift test** (the soak's exit rule "no drift over the hour"): the last `--window` minutes' p50 and p99
  within `--tolerance` of the first's (a run shorter than two windows compares its first and last thirds) = PASS; **the
  heap's growth a minute** (least squares over the seconds) and the growth heaps; **the seconds over budget** (a second
  whose ft_p99 exceeds the refresh interval).
- The attended hour: `tools/play.sh -- /frameLog <path>` (and the Menu key or `-- /overlay` to watch it live); the
  log's `hwi-settings:` lines say what the session ran with.
