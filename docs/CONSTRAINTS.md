# Project constraints (user rulings)

These are the user's standing rules for Homeworld-Infection. They override convenience. Date each entry.

1. **No gamescope dependency in the final recreation (2026-10-08).** The user has seen inconsistent frame rates after about an hour of play under gamescope on this machine. gamescope is a *test harness only* (bounded window for screenshots during development). The native builds must create and manage their own window (SDL2 or equivalent, as HomeworldSDL/Gardens of Kadesh do) and run correctly on a plain X11/Xwayland or Wayland desktop and on Windows 11 with nothing but the OS. Long play-tests of the GOG build should use `tools/run_game.sh --no-gamescope --virtual-desktop WxH` rather than the gamescope path.
2. **No fake input during testing (standing rule).** Never inject mouse/keyboard into the game (no xdotool/XTEST/ydotool). Verification is by screenshots, logs, and the user's own play-testing. If a test genuinely needs input, ask the user.
3. **Never commit game binaries or data.** `original/` and `reference/` stay gitignored; the repo records fingerprints (docs/binary-fingerprint.md), never the files.
4. **Stable run first, then additional features (2026-10-08).** The user has extra features in mind for after the 1-to-1 recreation is stable; the plan keeps a clearly marked placeholder for them.
5. **Automated engine runs are silent and headless (2026-10-11).** The user heard Cataclysm's intro and front-end audio on every agent self-test. All harness runs of `hwi-engine` go through `tools/m2_harness.sh`, which forces `SDL_AUDIODRIVER=dummy` and an offscreen video driver. Audible or on-screen runs are only for the user, by explicit choice (`HWI_AUDIBLE=1`).
