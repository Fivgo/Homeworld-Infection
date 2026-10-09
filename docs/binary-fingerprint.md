# Binary fingerprint: GOG Homeworld: Emergence (build 12948, "1.01 hotfix")

Captured 2026-10-08 from `original/setup_homeworld_emergence_1.01_hotfix_(12948).exe` (360,195,200 bytes, InnoSetup 5.5.7 unicode, GOG game ID 1766413869, `innoextract -t` passes). Extracted with `innoextract -m --gog --language english` into `original/game/` (66 files, 387 MB, see `original/game/MANIFEST.sha256`).

## Main executable `cataclysm.exe`

| Field | Value |
|---|---|
| sha256 | `f03d9a3ef26f74c04916b8d17d344cdcf0b5acd040dd2924cd7499af6eef3ed4` |
| size | 5,369,856 bytes |
| PE timestamp | Thu Dec 21 13:18:50 2000 (same day as the retail 1.01 patch readme) |
| linker | 6.0 (MSVC 6) |
| image base | 0x00400000 (CET `dllmain.cpp:11` expects exactly this; CET does no hash/size check) |
| entry point | RVA 0x27bbe0 = VA 0x0067bbe0, inside `.text` |
| entry bytes | `55 8b ec 6a ff 68 08 d7 72 00 68 38 19 68 00 64 a1 00 00 00 00 50 64 89` = standard MSVC 6 CRT `WinMainCRTStartup` SEH prologue |
| subsystem | Windows GUI, subsystem version 4.0 |
| SizeOfImage | 0x894000 |

### Sections

| name | VA | vsize | raw | flags | entropy | notes |
|---|---|---|---|---|---|---|
| .text | 0x401000 | 0x329000 | 0x329000 | code | 6.75 (6.2–6.7 per 256 KB chunk, uniform) | plain, unencrypted x86 |
| .rdata | 0x72a000 | 0x1faf5 | 0x20000 | ro data | 4.49 | strings, vtables, import names |
| .data | 0x74a000 | 0x50a198 | 0x196000 | rw data | 2.08 | 5 MB virtual, 1.6 MB on disk |
| .cms_t | 0xc55000 | 0x14000 | 0x14000 | code | 7.26 | high-entropy 80 KB code blob |
| .cms_d | 0xc69000 | 0x27360 | 0x28000 | rw data | 5.04 | 160 KB |
| .idata | 0xc91000 | 0x1a06 | 0x2000 | | | imports |
| .rsrc | 0xc93000 | 0x7f8 | 0x1000 | | | icon only |

`.cms_t`/`.cms_d` section names are characteristic of SecuROM 4.x disc protection (INFERENCE from section naming; no "SecuROM" string is present). The entry point is the normal CRT, not a protection stub, so in GOG's build the stub is not on the start-up path. A byte scan found 1,309 dword values in `.text` that fall inside the `.cms_*` address range and 111 dwords in `.cms_d` pointing into `.text`; whether those are live cross-references or coincidence is for Ghidra to settle (UNVERIFIED). The game's own CD check strings remain: `Cataclysm CD not detected.`, `Invalid CD-ROM path`, `CDKeys`, `C:\Cataclysm\src\win32\ClientCDKey.cpp`.

### Imports (11 DLLs, 263 functions)

| DLL | count | | DLL | count |
|---|---|---|---|---|
| KERNEL32 | 131 | | WINMM | 8 |
| USER32 | 49 | | ADVAPI32 | 9 |
| WSOCK32 | 26 | | DDRAW | 3 |
| GDI32 | 19 | | comdlg32 | 2 |
| binkw32 | 12 | | SHELL32 | 2 |
| ole32 | 2 | | | |

No DirectSound, OpenGL, or D3D import: rendering goes through the rGL DLLs loaded at run time (`rgl.dll`, `rgld3d.dll`, `rglsw.dll`, `rglfx3.dll`), as in Homeworld 1. Bink video (`binkw32.dll`) is linked directly.

### Statically linked code recognisable from embedded source names

`strings` yields 122 distinct `*.c`/`*.cpp` basenames. Only 14 are game sources (the release build stripped most asserts):

```
game\btg.c game\demo.c game\eval.c game\FeFlow.c game\mex.c game\particle.c
game\soundevent.c game\speechevent.c win32\ClientCDKey.cpp win32\font.c
win32\render.c win32\texreg.c win32\TitanInterface.cpp win32\TitanPacketMsg.cpp
```
(all as `C:\Cataclysm\src\...`, all with same-named files in the Homeworld 1 source). The other 108 are libraries:

- MSVC 6 C runtime sources (`fopen.c`, `printf.c`, `dbgheap.c`, `tidtable.c`, ...): use Ghidra Function ID / known-CRT signatures to mark these and skip them.
- WON/Titan networking SDK (`MMsgRouting*.cpp`, `SMsgDirG2*.cpp`, `SMsgFact*.cpp`, `SMsgFirewall*.cpp`, `TMsgAuth1*.cpp`, `tmessage.cpp`, `AddresseeList.cpp`) plus its crypto (`BFSymmetricKey.cpp`, `EGPrivateKey.cpp`, `EGPublicKey.cpp`, `CryptKeyBase.cpp`): dead online-lobby code (WON servers are gone). Large, identifiable, low priority.
- One Cataclysm-only game file: `ShipNames.c`.

The `.c` list is in `docs/appendix/exe_source_basenames.txt` (split into `exe_hw1_files.txt` / `exe_new_files.txt`).

## Other shipped files

| file | role |
|---|---|
| `cataclysm.big` (76.9 MB), `update.big` (244 KB) | game data archives, HW1 `.big` format; `update.big` is the 1.01 patch overlay |
| `BeastSpeech.vce`, `SectSpeech.vce`, `Music4Cat.wxd` | speech and music streams (HW1 `.vce`/`.wxd` formats) |
| `rgl.dll` 176 KB, `rgld3d.dll` 119 KB, `rglsw.dll` 799 KB, `rglfx3.dll` 94 KB | rGL renderer front end + Direct3D / software / Glide back ends (sha256 in MANIFEST) |
| `glide.dll`, `glide2x.dll`, `glide3x.dll`, `3DfxSpl*.dll`, `nglide_config.exe` | GOG-bundled nGlide (Glide → Direct3D wrapper), dated 2017-05-22 |
| `binkw32.dll` | Bink video |
| `feman/`, `MultiPlayer/`, `Movies/`, `credits/`, `SavedGames/`, `ScreenShots/` | front-end screens, MP maps, Bink movies, user dirs |
| `default_keys.cfg`, `video.txt`, `devstats.dat` | key bindings, video-mode list, dev stats |
| `goggame-1766413869.*`, `__support/` | GOG Galaxy integration (ignore) |

GOG launches the game as `cataclysm.exe /noswddraw` (`goggame-1766413869.info` playTasks). Readme.txt is the retail 1.0 readme (2000-08-01); patchreadme.txt is the 1.01 notes (2000-12-21).

## Consequences for the project

- CET's requirements are met by this exe (name `cataclysm.exe`, base 0x400000); its addresses should apply as-is since CET targets the GOG build.
- Roughly one third of `.text` is CRT + WON SDK; identify and exclude it before counting "game functions named".
- Match against Homeworld 1 by string literals (script variable names such as `cmSelectionTextColor`, error messages, file names) and by CET anchors, not by assert file paths.
- Keep `original/` out of git: this file is the only record of the binary that belongs in the repo.
