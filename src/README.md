# Licence notice: Relic End-User License Agreement (REULA), non-commercial

**Everything under `src/` is derived from Relic Entertainment's Homeworld 1 source code and is bound by the Relic End-User License Agreement (REULA). Its full text is in [`COPYING.REULA`](COPYING.REULA). Use is non-commercial only.**

Homeworld © 1999-2000 Sierra On-Line, Inc. All Rights Reserved. Homeworld is a registered trademark, and Sierra and Sierra Studios are registered trademarks of Sierra On-Line, Inc. Relic is a trademark of Relic Entertainment, Inc. Homeworld is now a registered trademark of Gearbox Enterprises, LLC. This project is not affiliated with, nor endorsed in any way by, Relic, Sierra, Gearbox, 2K or Take-Two.

The notice that Gardens of Kadesh ships with this code (`COPYING`, GoK commit `3af31fb`), verbatim:

> This port of Homeworld is based on the original Homeworld source code released by Relic Entertainment through the Relic Developers Network. That source code is protected by the Relic End-User License Agreement (REULA), a copy of which you can find in the file "COPYING.REULA".
>
> Some code is contributed under the MIT license, which permits inclusion within more restrictively licensed code bases. Any code contributed by the copyright holders listed in "COPYING.MIT" can therefore be (re-)used as MIT.
>
> The project, as a whole, remains bound by the terms of the REULA and is therefore distributed under those terms.

What the REULA says, in short (summary only; the text in `COPYING.REULA` is what counts, and nobody here is a lawyer):

- §1.1: use, reproduction and modification are allowed **solely for non-commercial purposes**, meaning free of any charge and with no direct or indirect financial consideration to anyone. Distribution was allowed only to registered members of the Relic Developers Network (RDN), which went dark in 2007. Like Gardens of Kadesh, this project treats anyone who uses or modifies this code as having agreed to the REULA.
- §2.1: all modifications ("MODIFIED SOFTWARE") are assigned to Relic (today, the IP holder), and moral rights are waived. That includes this project's changes to these files.
- §3.1: a five-year term from download or install. §3.2: Relic may terminate it at its discretion, and it ends on breach.
- §5.2: no circumvention of copy protection.
- §7.5: British Columbia law. §4.1: no warranty. [audit: "no warranty" was listed under §7.5; it is §4.1 "NO WARRANTIES" (COPYING.REULA line 146). §7.5 covers only governing law and venue.]

Gardens of Kadesh additionally licenses some contributions under MIT (its `COPYING.MIT`, at `3af31fb` naming only Thibault Lemaire, 2019-2026, plus all GoK commits after `0e40bdd0`). That grant does not cover Relic's code or the earlier HomeworldSDL community changes, so this tree as a whole is REULA.

Never commit game data (`.big`, executables, extracted assets) here: a GOG copy of the game is needed to run anything (docs/CONSTRAINTS.md rule 3).

---

# hwi-engine source tree

This is the engine source for Homeworld-Infection, the Homeworld: Cataclysm recreation. At M2-R1 it is the **Homeworld 1 engine with no Cataclysm changes**: the Gardens of Kadesh port of Relic's 2003 source release, built on Linux with SDL2 and OpenGL. Where every file came from, and how it differs from Relic's original commit `ad1bb82`, is in [`PROVENANCE.md`](PROVENANCE.md).

| Path | What | Built? |
|---|---|---|
| `Game/` | Engine and gameplay (Relic `src/Game`, as ported by HomeworldSDL / Gardens of Kadesh) | yes |
| `Ships/` | Per-ship behaviour | yes |
| `SDL/` | Platform layer: window, GL renderer, input, audio, timing, paths (GoK's rename of Relic `src/Win32`) | yes (static library `hw_sdl`) |
| `Missions/` | KAS mission scripts, compiled to C by `tools/kas2c` at build time | yes (generated) |
| `ThirdParty/` | CRC32, LZSS, stb_image, Titan stub header | yes |
| `tools/kas2c/` | KAS-to-C mission compiler (flex/bison), built for the build machine | yes (build tool) |
| `Win32/` | Relic's original Win32 platform layer, byte-identical to `ad1bb82` | **no**, reference only |

Build: `tools/build.sh --debug` or `tools/build.sh --release` from the repository root (output `build/<type>/hwi-engine`). Details, prerequisites and measured numbers: `docs/m2-r1-bootstrap.md`.
