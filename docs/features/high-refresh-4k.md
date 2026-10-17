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
