# Cataclysm Engine Tweaks (CET): everything it knows about the GOG Homeworld: Emergence executable

Scope: a full read of `reference/CataclysmEngineTweaks` (commit `19a08c3`, "Fix mothership self-damage prevention hack."; shallow clone, no tags) and `reference/hw_rgl` (commit `41ff3c3`, "Handle Cataclysm attrib stack calls"). All paths below are relative to `reference/` unless they start with `docs/` or `ghidra/`. `CET/` = `CataclysmEngineTweaks/`, `RGL/` = `hw_rgl/`, `HW1/` = `HomeworldSDL/src/`.

Companion machine-readable file: `ghidra/cet_symbols.json` (184 entries: 50 patch sites, 56 functions, 78 data incl. 25 field addresses named `universe_*`/`*_hyperspaceState`/`tpGameCreated_*`. 23 of them are derived (base + struct offset); `universe_phystimeelapsed` 0x00B62ABC and `universe_univUpdateCounter` 0x00B62BAC are also literals in CET). Everything in it is regenerated from the tables in section 3.

Status: written before the real game binary was in hand. Since then the GOG offline installer (build 12948, "1.01 hotfix") has been extracted to `original/game/` (`cataclysm.exe` sha256 `f03d9a3e…`, see `docs/binary-fingerprint.md`). The verification pass at the end of this doc byte-checked the section 10.2 predictions and the "expected original" columns against that file (read-only reads plus the existing `ghidra/exports/cataclysm_gog/` export; nothing was executed). Notes tagged **[exe 12948]** come from that check. Everything else is only as good as the CET author's own testing. "UNVERIFIED" marks inferences; anything without that label is read directly from the cited source.

Key takeaways:
- CET pins the exe **only** by image base 0x00400000. There is no hash or version check, so every address must be re-verified on the real `cataclysm.exe` (byte predictions in section 10.2). **[exe 12948]** Done for GOG build 12948: 35 of the 36 predicted addresses match. The one miss, 0x0072A818, is a `double` that CET writes as a `float`, which is a CET bug (section 9, item 12).
- About 50 patch sites, 56 named functions and about 50 named globals, plus partial layouts of `Universe`, `Player`, `SpaceObj`, `Ship`, `ShipStaticInfo`, `Bullet`, `AIPlayer`, `SinglePlayerGameInfo` and the Cataclysm-only skirmish-AI team table. Asserted offsets are the trustworthy part.
- Cataclysm-vs-HW1 deltas CET found: QPC-driven `universe.realtimeelapsed`; support units (`Player.supportUnitsMax/Used`, `ShipStaticInfo.supportUnits/providesSU`); external construction (`externalBuild`, `Ship.buildTimer`); per-ship N-LIPS; Beast infection (`beastSystemType`); new particle types; a bigger `SpaceObj` header (+12 B); `GetShipStaticInfo(type)` without race; `lodLevelGet` returning int; extra args on `univBulletCollidedWithTarget`, `rmCanBuildShip`, `clCommandMessage`.
- **CET defaults to a 64 Hz universe** (`UpdateRateShift=2`). Vanilla is 16 Hz, and many 16 Hz frame masks are left unconverted (section 9). For a 1-to-1 recreation, treat CET's bug fixes as opt-in and its timing changes as non-canonical. The `HW1/` reference port is **also 64 Hz by default** (`UNIVERSE_UPDATE_RATE_SHIFT = 2`, `HW1/Game/Universe.c:75`), and only `/vanilla` restores 16 Hz (section 9, item 16).
- `hw_rgl` documents the `rgl.dll` <-> `rgld3d.dll` ABI (`init_driver`, `gl_driver_funcs`) and the exe <-> `rgl.dll` contract (stdcall GL 1.1 exports plus `wglGetProcAddress` extensions). None of it is needed for a native SDL/OpenGL port.

Method notes:
- Struct offsets in section 4 come from compiling CET's own headers with `clang++ -target i686-pc-windows-msvc -fms-extensions -Xclang -fdump-record-layouts` (case-insensitive include shim, stub `windows.h`). Define `HW_Release` as CET's vcxproj does (`CataclysmEngineTweaks.vcxproj:63,87`). Without it, `SO_CLOOGE_SCALE` becomes 1 (`spaceobj.h:29-41`), `ShipStaticInfo.scaleFactor` is compiled in (`:736-738`), and `ShipStaticInfo` grows to 0x4D0 with its tail shifted +4. Every `ASSERT_OFFSET`/`static_assert` in CET's headers passed under that layout, so the offsets match what CET's MSVC build sees. Where CET did not assert an offset, it is only a guess.
- Keystone encodings in section 2 come from assembling CET's exact strings with `keystone-engine` (pip, scratch venv) at the target VA.

---

## 1. Target executable

| Item | Value | Source |
|---|---|---|
| Product | GOG "Homeworld: Emergence" (= Homeworld: Cataclysm, renamed). The CD release is untested by CET. | `CET/README.md:3,7` |
| Exe name | `cataclysm.exe`. The loader hard-codes it and requires it in the CWD. | `CET/loader/Main.cpp:6,19-23,37` |
| DLL name | `CataclysmEngineTweaks.dll`, loaded from the CWD | `CET/loader/Main.cpp:7,13` |
| Version / hash / size / timestamp checks | **None.** No PE timestamp, checksum, CRC, file-size or byte-signature check exists anywhere in CET. The only check is the image base. | grep of all `.cpp/.h`; `CET/dllmain.cpp:9-18` |
| Image base | `0x00400000`. `GetModuleHandle(nullptr)` must equal it, or DllMain returns FALSE. | `CET/dllmain.cpp:11-17` |
| Rebasing | **None.** Every address is an absolute VA (`DEFINE_ADDRESS`/`DEFINE_FUNCTION` casts, `CET/Cataclysm.h:14-19`). CET assumes the exe has no ASLR. **[exe 12948]** Confirmed: `DllCharacteristics` = 0 (no `DYNAMIC_BASE`), and the base-relocation directory is empty. | `CET/Cataclysm.h` |
| Bug in the base check | `LogAndThrow("... image base {:x} ...")` is called with no argument, so `fmt::runtime` throws `format_error`. It is still caught and DllMain still fails, but with the wrong message, and it fires before `InitLogger`, so no log file is written. | `CET/dllmain.cpp:16,31,38`, `CET/Main.h:13-21` |
| Version identity written into the exe | `networkVersion` (0x008B8850) is set to `"v1.02"` and `minorBuildVersion` (0x008B8860) to `"0"`. **[exe 12948]** The originals are `"Emergence 1"` and `"01"`. CET players therefore only match CET players in MP. | `CET/GameHooks.cpp:9-10,135-136` |
| Update bigfile | CET makes the exe open `Update102.big` (CET's own archive) in place of the stock update bigfile. | `CET/GameHooks.cpp:12,108-116,129` |
| Shipped artifacts | `cataclysm_loader.exe`, `CataclysmEngineTweaks.dll`, `CataclysmEngineTweaks.ini`, `rgl.dll`, `rgld3d.dll` (hw_rgl D3D9), `update102.big` | `CET/Compress-Release.ps1:4-11` |
| Build | VS2022 v143, **Win32 only**, C++20, `HW_Release;_CRT_SECURE_NO_WARNINGS;NOMINMAX`, static CRT, vcpkg triplet `x86-windows-static`, deps keystone/spdlog/fmt/inih/minhook, links `dsound.lib` | `CET/CataclysmEngineTweaks.vcxproj:1-100`, `CET/vcpkg.json` |
| hw_rgl as submodule | `game/rgl` -> `https://github.com/ncblakely/hw_rgl` (empty in this clone; cloned separately to `reference/hw_rgl`) | `CET/.gitmodules` |

Implied facts about the exe layout (predicted before the exe arrived; items checked against it are tagged **[exe 12948]**):
- CET writes several float literals in the 0x0072Axxx range (`TimerResolutionMax` 0x0072A614, `ETG_UpdateRoundOff` 0x0072A818, `etgEffectorCI_UniverseUpdateRate` 0x0072A82C) through plain pointers with **no `VirtualProtect`** (`CET/PatchUniverse.cpp:49-53,448`). The pages holding those constants must therefore be writable. **[exe 12948]** They are in `.rdata` (VA 0x0072A000-0x00749AF5), and this exe's `.rdata` has `IMAGE_SCN_MEM_WRITE` set (characteristics 0xC0000040, as does `.data`), so the writes work. The earlier guess "`.data`, not `.rdata`" was wrong. 0x0072A818 is a `double`, not a `float` (section 9, item 12), and 0x0072A614 is a pooled constant with 8 users (item 13).
- Patched code spans roughly 0x00420880 to 0x005BC900, and two `mem*` helpers sit at 0x006725D0/0x00673080. Data spans roughly 0x00879000 to 0x00C42000 (`.data`/`.bss`).
- Function addresses mostly cluster in alphabetical order of HW1 module names, with exceptions (`ais*` 0x431xxx, `clCommandMessage` 0x4AC820, the SensorHack site 0x50A43B): Alliance 0x420880 < CommandWrap 0x452xxx < ETG 0x46xxxx < FEFlow 0x475990 < Gun 0x48Cxxx < LOD 0x4A4D70 < MultiplayerGame 0x4BDxxx < NIS 0x4CCxxx < ObjTypes/Options 0x4D6/0x4D7 < Particle 0x4DDxxx < Randy 0x4EF860 < ResCollect/ResearchAPI 0x4F1/0x4F2 < SaveGame 0x4FDAF0 < ... < UnivUpdate 0x54xxxx < main.c 0x55F-0x563 < rinit 0x57Dxxx < soundlow 0x5808E0 < texreg 0x589xxx < Trails 0x5A4-0x5A7 < utility 0x5ABxxx). Several section 3 attributions lean on this clustering, and each one is marked UNVERIFIED.

---

## 2. Hooking mechanism

### 2.1 Mechanisms

| # | Mechanism | Implementation | Notes |
|---|---|---|---|
| M1 | **MinHook entry detour** | `CreateAndEnableHook(target, detour, &orig)` calls `MH_CreateHook` then `MH_EnableHook`. Any failure throws, which aborts all later patches. | `CET/Main.h:23-36`. MinHook comes from vcpkg and is not vendored. Its standard behaviour is to overwrite the first >=5 bytes with `E9 rel32` and build a trampoline (`orig`). When CET passes `&_discard`, the original is never called. |
| M2 | **MinHook mid-function detour** | Same call, but the target is an instruction address from `namespace Instructions`. The detour is a `__declspec(naked)` function that ends in `jmp` to a hard-coded resume address. Some detours run `pushad`/`popad` around C++ code. | `CheckPlayerWin_GameTypeCheck` is the only one that resumes through the trampoline (`CET/GameHooks.cpp:73-92`). Naked functions that contain C++ `if` code (e.g. `GameHooks.cpp:56-71`) rely on MSVC codegen not touching unsaved registers. That is fragile but works in CET's build. |
| M3 | **Keystone instruction overwrite** | `Assembler::Write(asm, addr)` does `ks_asm(KS_ARCH_X86, KS_MODE_32)` at `addr` (so relative branches resolve for that VA), then `VirtualProtect(RWX)`, `memcpy`, and restores the protection. | `CET/Assembler.cpp:6-48`. **No length check, no check of the original bytes, no `FlushInstructionCache`** (and the Keystone buffer is never `ks_free`d, a small leak per write). The new instruction must have exactly the old length, or the next instruction is corrupted. |
| M4 | **Direct data poke** | Writes through typed pointers in `namespace Globals` | No `VirtualProtect`, so the target pages must be writable (see section 1). |
| M5 | **Callback-slot replacement** | Overwrites `setVarCB` pointers inside the exe's `ShipStaticScriptTable` (`scriptStructEntry`, 16 B each, callback at +4) and the `SongNumber` data-pointer slot | `CET/PatchUniverse.cpp:449-453`, `CET/PatchSaveGame.cpp:11-12` |
| M6 | **System API detour** | MinHook on `&QueryPerformanceCounter` as resolved in CET's own import. The detour is process-wide, so it also affects rgl/dsound/etc. | `CET/PatchUniverse.cpp:291-325,349` |

Order of application (`CET/Main.cpp:33-49`): `ApplyStartupPatches` (M4) -> `InstallGameHooks` -> `ApplyETGPatches` -> `ApplyLODPatches` -> `ApplyRGLPatches` -> `ApplySaveGamePatches` -> `ApplyUniversePatches` -> `ApplySoundPatches` -> `ApplyAIPatches`. A single `Assembler` (Keystone engine) instance is shared by all of them.

### 2.2 Every hook / patch site

Notation: "Keystone bytes" are what CET writes, shown at the default `UpdateRateShift=2` (64 Hz, period 0.015625f = `0x3C800000`). "Expected original" was **inferred** from CET's comments, the instruction CET writes, and HW1 source. CET never checks it. **[exe 12948]** Every site below has since been read from the real exe (see ## Verification). Re-assembling each CET string with Keystone at its VA gives exactly the length of the original instruction there, so no CET write spills into the next instruction.

#### Always installed (`CET/GameHooks.cpp:118-169`, `CET/Main.cpp:19-31`, `CET/PatchSaveGame.cpp`, `CET/PatchRGL.cpp:90-103`)

| Address | Mech | Condition | Expected original | CET writes / detour | Effect |
|---|---|---|---|---|---|
| 0x0054C3F0 `univUpdate` | M1 (trampoline) | always | fn entry | `univUpdate` hook `GameHooks.cpp:16-43` | Autosave countdown on `phystimeelapsed`. At <=0 it calls `SaveGame("SavedGames\\{SinglePlayer\|MultiPlayer}\\Autosave")` then `clCommandMessage("Game Autosaved", -1)` |
| 0x004D7E30 `opOptionsAccept` | M1 (trampoline) | always | fn entry | `GameHooks.cpp:45-54` | After the original runs, `rndAspectRatio = MAIN_WindowWidth / MAIN_WindowHeight` |
| 0x00527820 `scriptSetTweakableGlobals` | M1 (trampoline) | always | fn entry | `GameHooks.cpp:95-106` | If `EnableNewAI`, also `scriptSet(NULL,"aiskirmish.script",AISkirmishTweaks)` |
| 0x0054860C `CheckPlayerWin_GameTypeCheck` | M2 (trampoline) | always | **[exe 12948]** `mov eax,[multiPlayerGame]; test eax,eax; je 0x005486E5` (`A1 94 3C A4 00 85 C0 0F 84 CC 00 00 00`). This is a `multiPlayerGame` test, not the `singlePlayerGame` test first guessed; 0x00548619 is its fall-through | `GameHooks.cpp:73-92` | Skirmish has both `multiPlayerGame` and `singlePlayerGame` = FALSE, so it `jmp 0x00548619` (MP win-condition branch) |
| 0x00589AEA `LoadMission_AdjustTextureMemoryLimit` | M2 (discard) | always | code that shrinks the texture pool on SP mission 17. **[exe 12948]** `je 0x00589B1F`; `cmp [0x00C0A5C0],17` (`singlePlayerGameInfo.currentMission`); `jne 0x00589B1F`; then pool = max(0x100000, x*0.3f) into `[esp+0x18]` | `GameHooks.cpp:56-71` | If `UnlimitedVideoMemory`: `[esp+0x18] = INT32_MAX`. **Always** `jmp 0x00589B1F`, so the mission-17 shrink is skipped even with the option off |
| 0x005ABB21 / 0x005ABB3E / 0x005ABB5C | M3 | always | `push offset "<update bigfile name>"` (5 B, `68 imm32`). **[exe 12948]** `push 0x008C0F38`, which points at `"Update.big"` | `push &"Update102.big"` (5 B) | Exe opens `Update102.big` (`GameHooks.cpp:108-116`) |
| 0x004DD43E | M3 | always | 0x2A bytes that set point filtering. **[exe 12948]** Two `glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN/MAG_FILTER, GL_NEAREST)` calls through `[0x00B3A444]` | `jmp 0x004DD468` = `EB 28` (2 B) | Ship textures no longer randomly drop to point filtering (`GameHooks.cpp:141-144`) |
| 0x00562401 | M3 | always | `je rel32` to the F12 debug-key handler. **[exe 12948]** `0F 84 D9 00 00 00` = `je 0x005624E0` (6 B) | `je 0x00562943` = `0F 84 3C 05 00 00` (6 B) | F12 key-up reaches the normal handler, so repeat presses work (`GameHooks.cpp:146-149`) |
| 0x008B8850 / 0x008B8860 | M4 (`strcpy`) | always | version strings. **[exe 12948]** `"Emergence 1"` / `"01"` | `"v1.02"` / `"0"` | MP version identity |
| 0x0087922C `dbgInt3Enabled` | M4 | `!IsDebuggerPresent()` | TRUE | FALSE | Assertion message boxes show instead of `int3` (`GameHooks.cpp:151-156`) |
| 0x00892670 `SongNumberLoadGameOffset` | M5 | always | pointer to the exe's `SongNumber`. **[exe 12948]** The slot holds 0x00883960; it is entry 2 of a `{ptr,size}[3]` table at 0x00892660 (section 3d) | `&songNumberDummy` | The load path that zeroes the "pre-game" values no longer clobbers `SongNumber` (`PatchSaveGame.cpp:8-12`) |
| 0x008B8638 `fullScreen`, 0x008B8628 `showBorder` | M4 | `BorderlessWindow` | TRUE / TRUE | FALSE / FALSE | `/window /noBorder` (`Main.cpp:21-25`) |
| 0x008B8610 `enableAVI` | M4 | `DisableIntroMovies` | TRUE | FALSE | `/disableAVI` (`Main.cpp:27-30`) |
| 0x00559AC0 `glCapNT` | M1 (discard) | always | fn | `glcaps.cpp:21-48` | Returns TRUE when `FixWindowsNTBug` (default), else the original NT<=4 test |
| 0x00559BE0 `glCapStartup` | M1 (trampoline) | always | fn | `PatchRGL.cpp:63-74` | After the original, `glCapCompiledVertexArray = FALSE` if `NoCompiledVertexArrays` |
| 0x00562084 | M3 | `!*fullScreen` **at DLL-attach time** | 10 B renderer re-init on WM_ACTIVATE. **[exe 12948]** It is `mov dword [0x00A74D50], 2`, a flag write and not a call (meaning UNVERIFIED) | `jmp 0x0056208E` = `EB 08` | Faster alt-tab in windowed mode (`PatchRGL.cpp:90-94`). Evaluated before the exe parses its command line, so `/window` alone does not trigger it. `BorderlessWindow=true` does |
| 0x0050A43B | M3 | `_DEBUG` build and `[Debug] SensorHack` | 2-byte instruction. **[exe 12948]** `7E 0A` = `jle 0x0050A447` | `90 90` | Debug-only, purpose undocumented (`GameHooks.cpp:160-168`) |
| 0x00589115 | (none) | commented out | **[exe 12948]** `6A 28` = `push 0x28` | `push 0` | Dead experiment (`GameHooks.cpp:158`) |

#### Renderer-selection patches (`[Display] NewRendererSelection=true`, `CET/PatchRGL.cpp:80-88`)

| Address | Mech | Detour | Effect |
|---|---|---|---|
| 0x0057DE00 `rinSortModes` | M1 (discard) | `CET/game/Game/rinit.cpp:70` | Keeps every 32-bpp mode in enumeration order. Drops the original's sort/filter (HW1 `SDL/rinit.c:170`) |
| 0x0057E5C0 `rinEnumDisplayModes_cb` | M1 (discard) | `rinit.cpp:114` | DDraw7 callback: accepts only 32 bpp, >=640x480, any aspect |
| 0x0057E710 `rinEnumPrimaryDisplayModes_cb` | M1 (discard) | `rinit.cpp:149` | Old-DDraw callback: 16/32 bpp, >=640x480 |
| 0x00563563 `InitWindow_DeviceCrcCheck` | M2 (discard) -> resume 0x00563645 | `PatchRGL.cpp:29-61` | If the saved `opDeviceCRC != rinDeviceCRC()`: store the new CRC, set `MAIN_WindowWidth/Height` = desktop client rect (`GetClientRect(GetDesktopWindow())`, `PatchRGL.cpp:18-27`), depth 32, call `mainRescaleMainWindow()`, `strcpy_s(glToSelect,512,"opengl32.dll")`. The stock game falls back to the **software** renderer (HW1 `SDL/main.c:1979-1996`) |

#### Frame limiter (`[Engine] DisableFrameRateLimit=true`, `CET/PatchRGL.cpp:96-100`)

| Address | Mech | Writes | Effect |
|---|---|---|---|
| 0x0057B78B | M3 | 8x `90` | Removes the render-task frame limiter (instruction 1). **[exe 12948]** Each site is `6A 01 FF 15 18 A2 72 00` = `push 1; call [0x0072A218]`, i.e. `Sleep(1)` (IAT slot = KERNEL32!Sleep, `ghidra/exports/cataclysm_gog/imports.txt`). Ghidra places it in FUN_0057B760 (proposed `rndRenderTask`). HW1 `SDL/render.c rndRenderTask:3774` has no such limiter |
| 0x0057B7A1 | M3 | 8x `90` | Same, instruction 2 (the second `Sleep(1)`) |

#### High-detail mode (`[Display] HighDetailMode=true`, `CET/PatchETG.cpp:84-95`, `CET/PatchLOD.cpp:27-33`)

| Address | Mech | Detour | Effect |
|---|---|---|---|
| 0x004A4D70 `lodLevelGet` | M1 (orig saved, unused) | `PatchLOD.cpp:8-18` | Always LOD 0. Writes `obj->currentLOD=0` (+0x34), `cameraDistanceVector` (+0x44) and `cameraDistanceSquared` (+0x40), returns 0 |
| 0x00470BD0 `etgFrequencyExceeded` | M1 (discard) | `PatchETG.cpp:68-72` | Always FALSE: no cap on simultaneous effects |
| 0x0046E110 `etgNParticleBlocksSet` | M1 (discard) | `PatchETG.cpp:49-66` | Parses `nParticleBlocks` and shifts it left by `UpdateRateShift`. **Only installed when HighDetailMode is on**, even though it is an update-rate fix (see section 9) |

#### Audio (`CET/PatchSound.cpp:10-28`)

| Address | Mech | Condition | Detour / bytes | Effect |
|---|---|---|---|---|
| 0x005808E0 `soundStartDSound` | M1 (discard) | `!ForceWaveOut` (default) | `CET/game/Win32/soundlow.cpp:16-109` | Same as Relic's code but treats every driver as `DSCAPS_CERTIFIED` (`soundlow.cpp:65`), so DirectSound is actually used |
| 0x00558F20 `fqEqualize` | M3 | `DisableEqualizer` | `C3` (`ret`) | Equalizer becomes a no-op (returns garbage EAX; callers presumably ignore it, UNVERIFIED). **[exe 12948]** The entry is `push ecx; push ebx; mov ebx,[esp+0xC]` (cdecl stack args), so the bare `ret` is stack-safe |

#### AI (`CET/PatchAI.cpp:16-35`)

| Address | Mech | Condition | Effect |
|---|---|---|---|
| 0x00431750 `aisFleetUpdate` | M1 (discard) | `[AI] EnableNewAI=true` (default false) | Replaced by CET's reimplementation `CET/game/Game/AISkirmish.cpp:257-720`, tunable via `assets/aiskirmish.script`. Debug builds otherwise wrap it with `DumpAITeams()` |

#### Universe update-rate patches (installed only when `UpdateRateShift > 0`; `CET/PatchUniverse.cpp:426-458`)

Keystone overwrites (period P = 1/(16<<shift); 64 Hz: P=`0x3C800000`, P/2=`0x3C000000`, P/4=`0x3B800000`, timer res 15*64=960=`0x3C0`):

| Address | Writes (64 Hz bytes) | Len | Expected original (**[exe 12948]**: every row confirmed) | Purpose |
|---|---|---|---|---|
| 0x00524841, 0x005256B7, 0x0053E8A5, 0x0053E8DB, 0x0053E9B5 | `push P` = `68 00 00 80 3C` | 5 | `68 00 00 80 3D` (0.0625f) | dt argument of `univUpdate()` calls (`PatchUniverse.cpp:403-411`) |
| 0x005AC174 | `push P` | 5 | `68 00 00 80 3D` | `taskStart(universeUpdateTask, period)` (`:374-382`) |
| 0x005ABF68 | `push 0x3C0` = `68 C0 03 00 00` | 5 | `68 F0 00 00 00` (240) | task-timer resolution (`:380-381`) |
| 0x004CC8F2 | `push P` | 5 | `68 00 00 80 3D` | `taskStart(nisUpdateTask, period)` (`:384-388`) |
| 0x004CC8BE | `mov dword [0x00A4CAB4], P` = `C7 05 B4 CA A4 00 00 00 80 3C` | 10 | same with `...80 3D` | NIS `timeElapsed = period` (`:390-395`) |
| 0x004CC801 | `push P` | 5 | `68 00 00 80 3D` | `nisUpdate(..., period)` |
| 0x0052AF12 | `push P` | 5 | `68 00 00 80 3D` | baby-callback task period (`:397-401`) |
| 0x0054ACDB | `mov dword [0x00B62ABC], P` = `C7 05 BC 2A B6 00 00 00 80 3C` | 10 | same with `...80 3D` | `universe.phystimeelapsed = period` in `univUpdateReset` (`:413-417`) |
| 0x0048CBD0, 0x0048CC57 | `mov dword [esp+0x1C], P` = `C7 44 24 1C 00 00 80 3C` | 8 | same with `...80 3D` | `*fireTime = UNIVERSE_UPDATE_PERIOD` (inlined `gunFirePower`) (`:419-424`) |
| 0x0046E042 / 0x0046E04F / 0x0046E05B | `fcom` / `fsub` / `fld dword [&s_ETGUpdateRoundOffDividedBy2]` = `D8 15` / `D8 25` / `D9 05` + abs32 | 6 each | same opcodes with abs32 = 0x0072A810 (float 0.015625, shared with 3 `fmul` sites at 0x0055A9E2/0x0055AF0A/0x0055B2B6) | `etgAtDefine` time rounding (`:352-362`) |
| 0x004CC634 / 0x004CC701 / 0x004CC72C / 0x004CC7B8 | `fadd` / `fcomp` / `fmul` / `fadd dword [&s_UniverseUpdatePeriod]` = `D8 05` / `D8 1D` / `D8 0D` / `D8 05` + abs32 | 6 each | same opcodes with abs32 = 0x0072AAE8 (float 0.0625, 11 code refs) | NIS fade timing (`:364-372`) |

Data writes and slot swaps (`PatchUniverse.cpp:448-453` plus the `univUpdateReset` hook `:43-59`):

| Address | Written value | When |
|---|---|---|
| 0x0072A614 `TimerResolutionMax` (float) | `15*rate`. **[exe 12948]** A pooled constant with 8 `fmul` users (section 9, item 13) | at attach |
| 0x008B2B9C / 0x008B2C0C / 0x008B2C7C / 0x008B2CEC / 0x008B2D5C | `&scriptSetReal32CBSkipIfNLipsDisabled` | at attach |
| 0x0089410C `HYPERSPACE_SLICE_RATE` | `1/(rate*2.5)` (64 Hz: 0.00625) | every `univUpdateReset` |
| 0x0088F580 `RegenerateRURate` | `16*rate-1` (64 Hz: 1023) | every `univUpdateReset` |
| 0x0072A82C `etgEffectorCI_UniverseUpdateRate` | `-rate` | every `univUpdateReset` |
| 0x0072A818 `ETG_UpdateRoundOff` | `P/2`, written as a float. **[exe 12948]** The target is a **double** 0.03125, so only its low dword changes and the write has no effect (section 9, item 12) | every `univUpdateReset` |

MinHook detours (`PatchUniverse.cpp:328-350`):

| Address | Mech | Detour | Effect |
|---|---|---|---|
| 0x0054ACD0 `univUpdateReset` | M1 (trampoline) | `:43-59` | Rewrites the four data values above, `ResetAutosaveTimer()`, then the original |
| 0x0053AC70 `scriptSetFramesCB` | M1 (discard) | `:61-65` | `value * rate` (HW1 multiplies by `UNIVERSE_UPDATE_RATE`, `HW1/Game/Universe.c:1408-1412`; used for `buildTime`, `Universe.c:555`) |
| 0x004BDE80 `mgResourceInjectionInterval`, 0x004BE090 `mgResourceLumpSumInterval` | M1 (discard) | `CET/game/Game/MultiplayerGame.cpp:22-91` | Front-end minutes <-> frames conversion at the new rate |
| 0x005BC900 `beastMothershipSelfDamage` | M1 (trampoline) | `:134-141` | Original runs only on frames where `univUpdateCounter % factor == 0` |
| 0x00546A20 `univBulletCollidedWithTarget` | M1 (trampoline) | `:221-253` | (a) Ignore a bullet hit when owner and target are both a mothership/module (list `:193-219`) of the same player. (b) `BULLET_Beam` collisions only on frames where `counter % factor == 0` |
| 0x005197D0 `singlePlayerGameUpdate` | M1 (trampoline) | `:256-274` | Pass-through. Debug builds log tick variables |
| 0x005A7A70 `trailSegmentsRead` | M1 (trampoline) | `:107-117` | Engine-trail segment count `<<= shift` |
| 0x005A4D90 `mistrailUpdate` | M1 (trampoline) | `:98-105` | Missile trails update only on frames where `counter % factor == 0` |
| 0x004F17C9 | M2 (discard) | `:67-96` | `[ecx+0x25C]` = `ShipStaticInfo.harvestRate`. If `UNIVERSE_WOODPECKER(harvestRate,0)`, `jmp 0x004F17D5`, else `jmp 0x004F1A46`. **[exe 12948]** The original is `test [ecx+0x25C],eax` (eax = `univUpdateCounter`) then `jne 0x004F1A46` |
| 0x0054CD73 | M2 (discard) -> 0x0054CD8F | `:143-159` | `WOODPECKER(255,86)` then `univCheckRegrowResources()`. **[exe 12948]** The original reads mask/frame from globals 0x0088F558/0x0088F55C (255/86) |
| 0x0054C83D | M2 (discard) -> 0x0054C859 | `:161-177` | `WOODPECKER(15,13)` then `rmUpdateResearch()`. **[exe 12948]** The original reads mask/frame from the tweakables 0x008AC5C4/0x008AC5C8 (15/13); CET hard-codes them |
| 0x00543F48 | M2 (discard) -> 0x00543F51 | `:179-191` | `EAX(buildTime) /= rate` (unsigned `div`) for the AI external-build timer. **[exe 12948]** The original 9 B are `99 83 E2 0F 03 C2 C1 F8 04` = signed `/16`. CET's detour clobbers ECX, which is dead at 0x00543F51 |
| kernel32 `QueryPerformanceCounter` | M6 | `:303-325` | Returns QPC minus (first value minus 1 s of frequency), so the counter starts near 1 s instead of at system uptime |

`UNIVERSE_WOODPECKER(r,f)` = `(counter % factor == 0) && (((counter >> shift) & r) == f)` (`CET/PatchUniverse.cpp:39`). It is the same macro as in `HW1/Game/Universe.h:53` (Gardens of Kadesh), and it makes a 16 Hz mask/frame pair fire on the equivalent 64 Hz frames.

---

## 3. Every absolute address CET references

Generated from `ghidra/cet_symbols.json`, so the two always agree. "Source" is the CET line where the address literal appears (or, for derived entries, the struct-offset assertion). Kinds: `patch_site` means an instruction CET overwrites or detours, or a jump/resume target. `data` entries named `universe_*`, `*_hyperspaceState` and `tpGameCreated_*` are **derived** (global base + CET struct offset), not literals in CET. The exceptions are `universe_phystimeelapsed` (literal at `PatchUniverse.cpp:416`) and `universe_univUpdateCounter` (`Cataclysm.h:189`). "Declared, unused" entries are defined in `CET/Cataclysm.h` but referenced by no `.cpp`. They are still useful anchors, but CET never exercised them.

Calling convention: everything is `__cdecl` (the `DEFINE_FUNCTION` typedefs carry no convention, `CET/Cataclysm.h:17-19`), except the two DirectDraw enum callbacks, which are `WINAPI` (`__stdcall`) in CET's replacements (`CET/game/Game/rinit.cpp:114,149`) even though their `Cataclysm.h` typedefs omit it. The `rinEnumPrimaryDisplayModes_cb` typedef also takes `LPDDSURFACEDESC2` (`CET/Cataclysm.h:157`), while the replacement takes `LPDDSURFACEDESC` (`rinit.cpp:149`, `PatchRGL.cpp:11`). The replacement is what runs, so 3b lists `LPDDSURFACEDESC`.

### 3a. Patch sites / instruction addresses

| Address | CET name | Type / signature | Source | HW1 equivalent | Notes |
|---|---|---|---|---|---|
| `0x0042090D` | `allianceFormWith_UniverseUpdatePeriod` | UNIVERSE_UPDATE_RATE use in alliance-timeout computation | CET/Cataclysm.h:112 | HW1/Game/Alliance.c:allianceFormWith | Defined but NOT patched by CET (alliance proposal timeout stays in 16Hz frames -> expires 4x faster at 64Hz). HW1 Alliance.c:57 `univUpdateCounter + UNIVERSE_UPDATE_RATE * 90`. [exe 12948] the instruction AT this VA is `81 C1 A0 05 00 00` = add ecx,1440 (=16*90; imm32 at 0x0042090F). It is preceded at 0x00420907 by `8B 0D AC 2B B6 00` = mov ecx,[0x00B62BAC] (univUpdateCounter) and followed at 0x00420913 by `89 88 F8 D9 B6 00` = mov [eax+0x00B6D9F8],ecx, i.e. players[sigsPlayerIndex].AllianceProposalsTimeout with a Player stride of 0x6530 (ghidra/exports/cataclysm_gog/decompiled.c:18570): confirmed. A CET-style fix would rewrite the imm32 to 1440<<shift (same 6-byte length). |
| `0x0046E042` | `etgDefineAt_UpdateRate1` | fcom dword ptr [abs32] (ETG_UpdateRoundOff/2 operand) | CET/Cataclysm.h:87 | HW1/Game/ETG.c:etgAtDefine | CET writes 6B fcom m32 pointing at its own static (period/4). [exe 12948] original operand = 0x0072A810 (.rdata, float 0.015625), shared with 3 `fmul dword` sites at 0x0055A9E2/0x0055AF0A/0x0055B2B6, which is why CET redirects the operand instead of rewriting the constant. HW1 ETG.c:5255-5262. |
| `0x0046E04F` | `etgDefineAt_UpdateRate2` | fsub dword ptr [abs32] (ETG_UpdateRoundOff/2 operand) | CET/Cataclysm.h:88 | HW1/Game/ETG.c:etgAtDefine | CET writes 6B fsub m32 pointing at its own static (period/4). [exe 12948] original operand = 0x0072A810 (.rdata, float 0.015625), shared with 3 `fmul dword` sites at 0x0055A9E2/0x0055AF0A/0x0055B2B6, which is why CET redirects the operand instead of rewriting the constant. HW1 ETG.c:5255-5262. |
| `0x0046E05B` | `etgDefineAt_UpdateRate3` | fld dword ptr [abs32] (ETG_UpdateRoundOff/2 operand) | CET/Cataclysm.h:89 | HW1/Game/ETG.c:etgAtDefine | CET writes 6B fld m32 pointing at its own static (period/4). [exe 12948] original operand = 0x0072A810 (.rdata, float 0.015625), shared with 3 `fmul dword` sites at 0x0055A9E2/0x0055AF0A/0x0055B2B6, which is why CET redirects the operand instead of rewriting the constant. HW1 ETG.c:5255-5262. |
| `0x0048CBD0` | `gunShipFirePower_UniverseUpdatePeriod1` | mov dword ptr [esp+0x1C], imm32 (*fireTime = UNIVERSE_UPDATE_PERIOD) | CET/Cataclysm.h:84 | HW1/Game/Gun.c:gunShipFirePower | CET writes 8B C7 44 24 1C <imm32> (Keystone drops the redundant SS prefix). HW1 gunFirePower Gun.c:1657, inlined into gunShipFirePower (Gun.c:1686). [exe 12948] original `C7 44 24 1C 00 00 80 3D` (imm 0.0625f) confirmed; same length as CET's write. |
| `0x0048CC57` | `gunShipFirePower_UniverseUpdatePeriod2` | mov dword ptr [esp+0x1C], imm32 | CET/Cataclysm.h:85 | HW1/Game/Gun.c:gunShipFirePower | Second inlined copy (interceptor path, Gun.c:1700ff) - UNVERIFIED. [exe 12948] original `C7 44 24 1C 00 00 80 3D` (imm 0.0625f) confirmed; same length as CET's write. |
| `0x004CC634` | `nisUpdateTask_UniverseUpdatePeriod1` | fadd dword ptr [abs32] (UNIVERSE_UPDATE_PERIOD operand) | CET/Cataclysm.h:91 | HW1/Game/NIS.c:nisUpdateTask | CET writes 6B fadd m32 -> &s_UniverseUpdatePeriod. Plausible HW1 line NIS.c:438 by op sequence - UNVERIFIED. [exe 12948] original operand = 0x0072AAE8 (.rdata, float 0.0625), a pooled constant with 11 code refs. |
| `0x004CC701` | `nisUpdateTask_UniverseUpdatePeriod2` | fcomp dword ptr [abs32] (UNIVERSE_UPDATE_PERIOD operand) | CET/Cataclysm.h:92 | HW1/Game/NIS.c:nisUpdateTask | CET writes 6B fcomp m32 -> &s_UniverseUpdatePeriod. Plausible HW1 line NIS.c:457 by op sequence - UNVERIFIED. [exe 12948] original operand = 0x0072AAE8 (.rdata, float 0.0625), a pooled constant with 11 code refs. |
| `0x004CC72C` | `nisUpdateTask_UniverseUpdatePeriod3` | fmul dword ptr [abs32] (UNIVERSE_UPDATE_PERIOD operand) | CET/Cataclysm.h:93 | HW1/Game/NIS.c:nisUpdateTask | CET writes 6B fmul m32 -> &s_UniverseUpdatePeriod. Plausible HW1 line NIS.c:464 by op sequence - UNVERIFIED. [exe 12948] original operand = 0x0072AAE8 (.rdata, float 0.0625), a pooled constant with 11 code refs. |
| `0x004CC7B8` | `nisUpdateTask_UniverseUpdatePeriod4` | fadd dword ptr [abs32] (UNIVERSE_UPDATE_PERIOD operand) | CET/Cataclysm.h:94 | HW1/Game/NIS.c:nisUpdateTask | CET writes 6B fadd m32 -> &s_UniverseUpdatePeriod. Plausible HW1 line NIS.c:483 by op sequence - UNVERIFIED. [exe 12948] original operand = 0x0072AAE8 (.rdata, float 0.0625), a pooled constant with 11 code refs. |
| `0x004CC801` | `nisUpdateTask_UNIVERSE_UPDATE_PERIOD_2` | push imm32 (dt arg of nisUpdate) | CET/Cataclysm.h:60 | HW1/Game/NIS.c:nisUpdateTask | CET writes `push <period imm32>` (5B: 68 xx xx xx xx; 64Hz=68 00 00 80 3C). [exe 12948] original `68 00 00 80 3D` (push 0.0625f) confirmed. HW1 NIS.c:494/525/546 nisUpdate(..., UNIVERSE_UPDATE_PERIOD). |
| `0x004CC8BE` | `nisUpdateTask_UNIVERSE_UPDATE_PERIOD_1` | mov dword ptr [0x00A4CAB4], imm32 (timeElapsed = UNIVERSE_UPDATE_PERIOD) | CET/Cataclysm.h:58 | HW1/Game/NIS.c:nisUpdateTask | CET writes 10B C7 05 B4 CA A4 00 <imm32>. HW1 NIS.c:516 `timeElapsed = UNIVERSE_UPDATE_PERIOD;` (timeElapsed is a static at 0x00A4CAB4 in Cataclysm). [exe 12948] original `C7 05 B4 CA A4 00 00 00 80 3D` (imm 0.0625f) confirmed; same length as CET's write. |
| `0x004CC8F2` | `nisStartup_UNIVERSE_UPDATE_PERIOD` | push imm32 (task period for nisUpdateTask) | CET/Cataclysm.h:55 | HW1/Game/NIS.c:nisStartup | CET writes `push <period imm32>` (5B: 68 xx xx xx xx; 64Hz=68 00 00 80 3C). [exe 12948] original `68 00 00 80 3D` (push 0.0625f) confirmed. HW1 NIS.c:580. |
| `0x004DD43E` | `partRenderBillSystem_SetPointFiltering` | buggy texture-filter change | CET/Cataclysm.h:103 | HW1/Game/Particle.c:partRenderBillSystem | CET writes `jmp 0x004DD468` (2B EB 28), skipping 0x2A bytes that switched non-particle textures to point filtering. HW1 Particle.c:703. [exe 12948] the 0x2A skipped bytes are 2x (`push 0x2600; push 0x2801` / `0x2800`; `push 0x0DE1; call [0x00B3A444]`) = glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER / MAG_FILTER, GL_NEAREST) through the GL function-pointer slot 0x00B3A444. |
| `0x004DD468` | `partRenderBillSystem_AfterSetPointFiltering` | jump target | CET/Cataclysm.h:104 | HW1/Game/Particle.c:partRenderBillSystem |  |
| `0x004F17C9` | `CollectResources_HarvestRateCheck` | MinHook mid-function hook; ECX = ShipStaticInfo* | CET/Cataclysm.h:69 | HW1/Game/ResCollect.c:CollectResources | Detour CollectResourcesHarvestRateCheck reads [ecx+0x25C]=harvestRate, applies UNIVERSE_WOODPECKER(harvestRate,0), jumps to 0x004F17D5 (collect) or 0x004F1A46 (skip). HW1 ResCollect.c:596. [exe 12948] original `85 81 5C 02 00 00 0F 85 71 02 00 00` = test [ecx+0x25C],eax (eax = univUpdateCounter, loaded at 0x004F17C2); jne 0x004F1A46. Confirms harvestRate @+0x25C, ECX = ShipStaticInfo*, and both jump targets. |
| `0x004F17D5` | `collectResources` | jump target: add resources to worker | CET/Cataclysm.h:81 | HW1/Game/ResCollect.c:CollectResources | Taken branch of the harvest-rate check. |
| `0x004F1A46` | `skipCollectResources` | jump target: skip adding resources | CET/Cataclysm.h:82 | HW1/Game/ResCollect.c:CollectResources | Not-taken branch of the harvest-rate check. |
| `0x0050A43B` | `SensorHack_site` | 2-byte instruction (jle short, 7E 0A) | CET/GameHooks.cpp:166 | - | Debug builds only, [Debug] SensorHack: `nop; nop`. Purpose undocumented (name suggests sensors/fog-of-war visibility). UNVERIFIED. [exe 12948] original `7E 0A` = jle 0x0050A447. Ghidra container FUN_0050A310. |
| `0x00524841` | `univUpdate1` | push imm32 (real32 phystimeelapsed arg of univUpdate) | CET/Cataclysm.h:41 | - | CET writes `push <period imm32>` (5B: 68 xx xx xx xx; 64Hz=68 00 00 80 3C). [exe 12948] original `68 00 00 80 3D` (push 0.0625f) confirmed. Caller unidentified: lies between speechEventQueue 0x00522560 and scriptSetReal32CB 0x005259D0; HW1 non-Universe callers of univUpdate(UNIVERSE_UPDATE_PERIOD) are Stats.c:434 and NIS.c:543 - UNVERIFIED. [exe 12948] Ghidra container FUN_00524790 (unnamed); the push feeds `call 0x0054C3F0` (univUpdate). |
| `0x005256B7` | `univUpdate2` | push imm32 (phystimeelapsed) | CET/Cataclysm.h:43 | - | CET writes `push <period imm32>` (5B: 68 xx xx xx xx; 64Hz=68 00 00 80 3C). [exe 12948] original `68 00 00 80 3D` (push 0.0625f) confirmed. Same caller cluster as univUpdate1 - UNVERIFIED. [exe 12948] Ghidra container FUN_00525640 (unnamed). |
| `0x0052AF12` | `BabyTasksStartup_UNIVERSE_UPDATE_PERIOD` | push imm32 (baby-callback task period) | CET/Cataclysm.h:63 | HW1/Game/Task.c:taskCallBackInit | CET writes `push <period imm32>` (5B: 68 xx xx xx xx; 64Hz=68 00 00 80 3C). [exe 12948] original `68 00 00 80 3D` (push 0.0625f) confirmed. HW1 Task.c:539 taskStart(taskCallBackProcess, BABY_CallBackPeriod=1/16 (Task.h:90), 0). |
| `0x0053E8A5` | `univUpdate3` | push imm32 (phystimeelapsed) | CET/Cataclysm.h:45 | HW1/Game/Universe.c:universeUpdateTask | CET writes `push <period imm32>` (5B: 68 xx xx xx xx; 64Hz=68 00 00 80 3C). [exe 12948] original `68 00 00 80 3D` (push 0.0625f) confirmed. Mapping by address: three sites within 0x110 bytes just below GetShipStaticInfo 0x0053EA50; HW1 universeUpdateTask (Universe.c:3784) calls univUpdate(UNIVERSE_UPDATE_PERIOD) at 3971/3984/4026/4035 - UNVERIFIED. |
| `0x0053E8DB` | `univUpdate4` | push imm32 (phystimeelapsed) | CET/Cataclysm.h:47 | HW1/Game/Universe.c:universeUpdateTask | CET writes `push <period imm32>` (5B: 68 xx xx xx xx; 64Hz=68 00 00 80 3C). [exe 12948] original `68 00 00 80 3D` (push 0.0625f) confirmed. Mapping by address: three sites within 0x110 bytes just below GetShipStaticInfo 0x0053EA50; HW1 universeUpdateTask (Universe.c:3784) calls univUpdate(UNIVERSE_UPDATE_PERIOD) at 3971/3984/4026/4035 - UNVERIFIED. |
| `0x0053E9B5` | `univUpdate5` | push imm32 (phystimeelapsed) | CET/Cataclysm.h:49 | HW1/Game/Universe.c:universeUpdateTask | CET writes `push <period imm32>` (5B: 68 xx xx xx xx; 64Hz=68 00 00 80 3C). [exe 12948] original `68 00 00 80 3D` (push 0.0625f) confirmed. Mapping by address: three sites within 0x110 bytes just below GetShipStaticInfo 0x0053EA50; HW1 universeUpdateTask (Universe.c:3784) calls univUpdate(UNIVERSE_UPDATE_PERIOD) at 3971/3984/4026/4035 - UNVERIFIED. |
| `0x00543F48` | `aiUpdateExternalShipBuild_CalculateTimerDuration` | MinHook mid-function hook; EAX = ShipStaticInfo->buildTime (frames) | CET/Cataclysm.h:115 | - | Cataclysm-only (external construction). Detour: EAX = EAX / s_UniverseUpdateRate (unsigned div), jmp 0x00543F51. [exe 12948] original 9 bytes `99 83 E2 0F 03 C2 C1 F8 04` = cdq; and edx,15; add eax,edx; sar eax,4 (SIGNED buildTime/16), preceded by `mov eax,[eax+0x23C]` (ShipStaticInfo.buildTime). The result (seconds) goes as a float to FUN_00528410(&ship->buildTimer @+0x5B4, secs) (ghidra/exports/cataclysm_gog/decompiled.c:210779). CET's detour uses unsigned div and clobbers ECX, which is dead there (next use is `lea ecx,[edi+0x5B4]`). |
| `0x00543F51` | `aiUpdateExternalShipBuild_SetTimerDuration` | resume address | CET/Cataclysm.h:116 | - |  |
| `0x0054860C` | `CheckPlayerWin_GameTypeCheck` | MinHook mid-function hook (trampoline kept) | CET/Cataclysm.h:100 | HW1/Game/UnivUpdate.c:CheckPlayerWin | Skirmish fix: if !multiPlayerGame && !singlePlayerGame jump to MP branch 0x00548619, else run original bytes via trampoline. HW1 UnivUpdate.c:4150 (`if (!singlePlayerGame)` 4158). [exe 12948] original `A1 94 3C A4 00 85 C0 0F 84 CC 00 00 00` = if (!multiPlayerGame) goto 0x005486E5; 0x00548619 is the fall-through (MP) branch. Cataclysm tests multiPlayerGame where HW1 tests !singlePlayerGame, so skirmish (both FALSE) took the non-MP path at 0x005486E5. |
| `0x00548619` | `CheckPlayerWin_IsMultiplayerGame` | jump target: multiplayer win-condition branch | CET/Cataclysm.h:101 | HW1/Game/UnivUpdate.c:CheckPlayerWin |  |
| `0x0054ACDB` | `univUpdateReset_UNIVERSE_UPDATE_PERIOD` | mov dword ptr [0x00B62ABC], imm32 (universe.phystimeelapsed = UNIVERSE_UPDATE_PERIOD) | CET/Cataclysm.h:66 | HW1/Game/UnivUpdate.c:univupdateReset | CET writes 10B C7 05 BC 2A B6 00 <imm32>. univUpdateReset fn = 0x0054ACD0 (+0xB). HW1 UnivUpdate.c:6224. [exe 12948] original `C7 05 BC 2A B6 00 00 00 80 3D` (imm 0.0625f) confirmed; same length as CET's write. |
| `0x0054C83D` | `startRmUpdateResearch` | MinHook mid-function hook in univUpdate | CET/Cataclysm.h:78 | HW1/Game/UnivUpdate.c:univUpdate | Replaced by WOODPECKER(15,13) -> rmUpdateResearch(); resumes at 0x0054C859. HW1 UnivUpdate.c:7462 (REFRESH_RESEARCH_RATE/FRAME 15/13 also in tweak.script). [exe 12948] original: (univUpdateCounter & [0x008AC5C4]=15) == [0x008AC5C8]=13 -> call 0x004F2A20, i.e. mask/frame come from tweakable globals; CET hard-codes 15/13, so any script override is ignored. |
| `0x0054C859` | `endRmUpdateResearch` | resume address after research update | CET/Cataclysm.h:79 | HW1/Game/UnivUpdate.c:univUpdate |  |
| `0x0054CD73` | `startUnivCheckRegrowResources` | MinHook mid-function hook in univUpdate | CET/Cataclysm.h:74 | HW1/Game/UnivUpdate.c:univUpdate | Replaced by WOODPECKER(255,86) -> univCheckRegrowResources(); resumes at 0x0054CD8F. HW1 UnivUpdate.c:7588 (REGROW_RESOURCES_CHECK_RATE/FRAME = 255/86, ResCollect.c:60-61). [exe 12948] original: (univUpdateCounter & [0x0088F558]=255) == [0x0088F55C]=86 -> call 0x0054C270; globals, CET hard-codes 255/86. |
| `0x0054CD8F` | `endUnivCheckRegrowResources` | resume address after regrow check | CET/Cataclysm.h:75 | HW1/Game/UnivUpdate.c:univUpdate |  |
| `0x00562084` | `ActivateMe_ReInitRenderer` | 10-byte global write on WM_ACTIVATE (windowed) | CET/Cataclysm.h:98 | HW1/SDL/main.c:ActivateMe | CET writes `jmp 0x0056208E` (2B EB 08) when not fullscreen, skipping 10 bytes. [exe 12948] the skipped 10 bytes are `C7 05 50 4D A7 00 02 00 00 00` = mov dword [0x00A74D50], 2: a global set to 2 (presumably a deferred renderer re-init request; meaning UNVERIFIED), not a call. Nothing equivalent in the SDL port ActivateMe (main.c:1154). Ghidra finds no containing function. |
| `0x0056208E` | `ActivateMe_AfterReInitRenderer` | jump target | CET/PatchRGL.cpp:93 | HW1/SDL/main.c:ActivateMe | Literal in PatchRGL.cpp. |
| `0x00562401` | `WindowProc_JumpIfF12KeyUp` | Jcc in WM_KEYUP handling (F12 -> debug key handler) | CET/Cataclysm.h:109 | - | CET writes `je 0x00562943` (6B 0F 84 3C 05 00 00). [exe 12948] original `0F 84 D9 00 00 00` = je 0x005624E0 (debug-key path): same 6-byte length. Win32 WindowProc is not in HomeworldSDL (SDL event loop, main.c ~1939). |
| `0x00562943` | `WindowProc_NormalKeyUpEvent` | jump target: normal key-up handling | CET/Cataclysm.h:110 | - |  |
| `0x00563563` | `InitWindow_DeviceCrcCheck` | MinHook mid-function hook (NewRendererSelection only) | CET/Cataclysm.h:96 | HW1/SDL/main.c:InitWindow | Detour: if opDeviceCRC != rinDeviceCRC(): save CRC, set MAIN_Window* = desktop size/32bpp, mainRescaleMainWindow(), glToSelect="opengl32.dll"; then jmp 0x00563645. HW1 main.c:1979-1996 (forces software renderer instead). |
| `0x00563645` | `InitWindow_DeviceCrcCheck_Continue` | resume address after device-CRC block | CET/PatchRGL.cpp:29 | HW1/SDL/main.c:InitWindow | Hard-coded in PatchRGL.cpp, not in Cataclysm.h. |
| `0x0057B78B` | `rndRenderTask_FrameLimiter1` | 8 bytes of framerate-limiter code | CET/PatchRGL.cpp:98 | - | DisableFramerateLimit: 8x NOP (90). Limiter is not present in HW1 render.c rndRenderTask; [exe 12948] original `6A 01 FF 15 18 A2 72 00` = push 1; call [0x0072A218] (KERNEL32!Sleep IAT slot, ghidra/exports/cataclysm_gog/imports.txt), i.e. Sleep(1). Ghidra container FUN_0057B760 (proposed rndRenderTask). |
| `0x0057B7A1` | `rndRenderTask_FrameLimiter2` | 8 bytes of framerate-limiter code | CET/PatchRGL.cpp:99 | - | DisableFramerateLimit: 8x NOP. [exe 12948] original: a second `push 1; call [Sleep]` (same 8 bytes). |
| `0x00589115` | `texreg_unknown_push0_site` | push imm8 (6A 28) | CET/GameHooks.cpp:158 | - | Commented-out experiment `push 0`; not applied. Just below trCramRAMComputeAndScale 0x00589A50. [exe 12948] original `6A 28` = push 0x28; CET's `push 0` (6A 00) would be the same length. Ghidra container FUN_005890C0. |
| `0x00589AEA` | `LoadMission_AdjustTextureMemoryLimit` | MinHook mid-function hook; [esp+0x18] = texture RAM pool limit local | CET/Cataclysm.h:106 | HW1/SDL/texreg.c:trCramRAMScaleCompute | Always installed. Detour sets [esp+0x18]=INT32_MAX if UnlimitedVideoMemory, then ALWAYS jmp 0x00589B1F (mission-17 pool reduction skipped unconditionally). Lies 0x9A into trCramRAMComputeAndScale (0x00589A50); [exe 12948] original: `74 33` je 0x00589B1F; `83 3D C0 A5 C0 00 11` cmp [singlePlayerGameInfo.currentMission],17; `75 2A` jne 0x00589B1F; then pool = max(0x100000, int64 * 0.3f [0x0072B064]) stored at [esp+0x18]. Confirms the mission-17 shrink and the [esp+0x18] local. HW1 mapping still UNVERIFIED. |
| `0x00589B1F` | `LoadMission_AfterTextureMemoryLimitAdjustment` | resume address | CET/Cataclysm.h:107 | - |  |
| `0x005ABB21` | `utyGameSystemsPreInit_UpdateBigFileName1` | push imm32 (char* update bigfile name) | CET/Cataclysm.h:119 | HW1/SDL/utility.c:utyGameSystemsPreInit | CET writes `push &"Update102.big"` (5B 68 <addr in CET DLL>). Original pushes the exe string for the update bigfile (HW1 BigFile.c:74 "Update.big"; [exe 12948] original `68 38 0F 8C 00` = push 0x008C0F38 -> "Update.big"). |
| `0x005ABB3E` | `utyGameSystemsPreInit_UpdateBigFileName2` | push imm32 (char* update bigfile name) | CET/Cataclysm.h:120 | HW1/SDL/utility.c:utyGameSystemsPreInit | CET writes `push &"Update102.big"` (5B 68 <addr in CET DLL>). Original pushes the exe string for the update bigfile (HW1 BigFile.c:74 "Update.big"; [exe 12948] original `68 38 0F 8C 00` = push 0x008C0F38 -> "Update.big"). |
| `0x005ABB5C` | `utyGameSystemsPreInit_UpdateBigFileName3` | push imm32 (char* update bigfile name) | CET/Cataclysm.h:121 | HW1/SDL/utility.c:utyGameSystemsPreInit | CET writes `push &"Update102.big"` (5B 68 <addr in CET DLL>). Original pushes the exe string for the update bigfile (HW1 BigFile.c:74 "Update.big"; [exe 12948] original `68 38 0F 8C 00` = push 0x008C0F38 -> "Update.big"). |
| `0x005ABF68` | `utyGameSystemsInit_TimerResolutionMax` | push imm32 (task-timer frequency) | CET/Cataclysm.h:71 | HW1/SDL/utility.c:utyGameSystemsInit | CET writes `push 15*rate` (64Hz: 68 C0 03 00 00). Original presumably push 0xF0 (240 = UTY_TimerResolutionMax, HW1 utility.h:39; taskStartup at utility.c:3940): [exe 12948] confirmed by bytes `68 F0 00 00 00`. |
| `0x005AC174` | `utyGameSystemsInit_UNIVERSE_UPDATE_PERIOD` | push imm32 (task period for universeUpdateTask) | CET/Cataclysm.h:52 | HW1/SDL/utility.c:utyGameSystemsInit | CET writes `push <period imm32>` (5B: 68 xx xx xx xx; 64Hz=68 00 00 80 3C). [exe 12948] original `68 00 00 80 3D` (push 0.0625f) confirmed. HW1 utility.c:4071 taskStart(universeUpdateTask, UNIVERSE_UPDATE_PERIOD, 0). |

### 3b. Functions

| Address | CET name | Type / signature | Source | HW1 equivalent | Notes |
|---|---|---|---|---|---|
| `0x00420880` | `allianceFormWith` | void allianceFormWith(udword playerindex) | CET/Cataclysm.h:129 | HW1/Game/Alliance.c:allianceFormWith | Declared, unused. |
| `0x00431510` | `aisRequestShip` | void aisRequestShip(Player*, ShipType, sdword buildCost) | CET/Cataclysm.h:128 | - | Cataclysm-only; declared but unused by CET. |
| `0x00431750` | `aisFleetUpdate` | void aisFleetUpdate(void) | CET/Cataclysm.h:127 | - | Cataclysm skirmish-AI build planner. Replaced wholesale by CET AISkirmish.cpp reimplementation when [AI] EnableNewAI=true (no trampoline). |
| `0x00452F30` | `clWrapCreateShip` | void clWrapCreateShip(CommandLayer*, ShipType, ShipRace, uword playerIndex, Ship* creator) | CET/Cataclysm.h:133 | HW1/Game/CommandWrap.c:clWrapCreateShip | Used by CET AI for external/module construction. |
| `0x00452FA0` | `clWrapBuildShip` | void clWrapBuildShip(CommandLayer*, ShipType, ShipRace, uword playerIndex, Ship* creator) | CET/Cataclysm.h:132 | HW1/Game/CommandWrap.c:clWrapBuildShip |  |
| `0x00453390` | `clWrapScuttle` | void clWrapScuttle(CommandLayer*, SelectCommand*) | CET/Cataclysm.h:134 | HW1/Game/CommandWrap.c:clWrapScuttle |  |
| `0x00453820` | `clWrapSetKamikaze` | void clWrapSetKamikaze(CommandLayer*, SelectCommand*) | CET/Cataclysm.h:135 | HW1/Game/CommandWrap.c:clWrapSetKamikaze |  |
| `0x0046A6F0` | `etgEffectDelete` | void etgEffectDelete(Effect*) | CET/Cataclysm.h:136 | HW1/Game/ETG.c:etgEffectDelete | Debug hook only (#if 0). |
| `0x0046E110` | `etgNParticleBlocksSet` | sdword etgNParticleBlocksSet(etgeffectstatic*, ubyte* dest, char* opcodeString, char* params, char* ret) | CET/Cataclysm.h:139 | HW1/Game/ETG.c:etgNParticleBlocksSet | Replaced (only when HighDetailMode): nParticleBlocks <<= UpdateRateShift. |
| `0x0046E7B0` | `etgFunctionCall` | sdword etgFunctionCall(Effect*, etgeffectstatic*, ubyte* opcode) | CET/Cataclysm.h:138 | HW1/Game/ETG.c:etgFunctionCall | Debug hook only (#if 0). |
| `0x00470BD0` | `etgFrequencyExceeded` | bool32 etgFrequencyExceeded(etgeffectstatic*) | CET/Cataclysm.h:137 | HW1/Game/ETG.c:etgFrequencyExceeded | HighDetailMode: replaced by `return FALSE` (no effect cap). |
| `0x00475990` | `feToggleButtonSet` | void feToggleButtonSet(char* name, sdword bPressed) | CET/Cataclysm.h:140 | HW1/Game/FEFlow.c:feToggleButtonSet |  |
| `0x004A4D70` | `lodLevelGet` | int lodLevelGet(void* spaceObj, vector* camera, vector* ship) | CET/Cataclysm.h:145 | HW1/Game/LOD.c:lodLevelGet | HW1 returns lod*; Cataclysm returns int level. HighDetailMode replacement forces currentLOD=0 and sets cameraDistanceVector/Squared (SpaceObj+0x44/+0x40). |
| `0x004AC820` | `clCommandMessage` | void clCommandMessage(const char msg[100], udword flags) | CET/Cataclysm.h:131 | HW1/Game/CommandWrap.c:clCommandMessage | HW1 has 1 arg; Cataclysm adds flags (CET passes -1). Used for "Game Autosaved". |
| `0x004BDE80` | `mgResourceInjectionInterval` | void mgResourceInjectionInterval(char* name, featom* atom) | CET/Cataclysm.h:150 | HW1/Game/MultiplayerGame.c:mgResourceInjectionInterval | Replaced by CET copy (MultiplayerGame.cpp) that converts minutes <-> frames with the new rate. Always hooked when shift>0. |
| `0x004BE090` | `mgResourceLumpSumInterval` | void mgResourceLumpSumInterval(char* name, featom* atom) | CET/Cataclysm.h:151 | HW1/Game/MultiplayerGame.c:mgResourceLumpSumInterval | Replaced likewise. |
| `0x004C2590` | `mgGameTypesOtherButtonPressed` | void mgGameTypesOtherButtonPressed(void) | CET/Cataclysm.h:149 | HW1/Game/MultiplayerGame.c:mgGameTypesOtherButtonPressed |  |
| `0x004D6BC0` | `ShipTypeToStr` | char* ShipTypeToStr(ShipType) | CET/Cataclysm.h:168 | HW1/Game/ObjTypes.c:ShipTypeToStr |  |
| `0x004D7E30` | `opOptionsAccept` | void opOptionsAccept(char* name, featom* atom) | CET/Cataclysm.h:153 | HW1/Game/Options.c:opOptionsAccept | Hooked: after original, rndAspectRatio = W/H. |
| `0x004EF860` | `ranRandomFn` | udword ranRandomFn(sdword ranIndex) | CET/Cataclysm.h:154 | HW1/Game/Randy.c:ranRandomFn | Release signature (HW1 debug has file,line). CET uses ranRandomFn(6). |
| `0x004F2630` | `rmCanBuildShip` | bool32 rmCanBuildShip(Player*, ShipType, int p3) | CET/Cataclysm.h:159 | HW1/Game/ResearchAPI.c:rmCanBuildShip | Cataclysm adds a 3rd int (CET passes 1 or 0); meaning UNVERIFIED. |
| `0x004F2A20` | `rmUpdateResearch` | void rmUpdateResearch(void) | CET/Cataclysm.h:160 | HW1/Game/ResearchAPI.c:rmUpdateResearch |  |
| `0x004FDAF0` | `SaveGame` | bool32 SaveGame(const char* filename) | CET/Cataclysm.h:161 | HW1/Game/SaveGame.c:SaveGame | CET autosave: SaveGame("SavedGames\\{SinglePlayer\|MultiPlayer}\\Autosave"). |
| `0x005118D0` | `selNumShipsInSelection` | int selNumShipsInSelection(MaxSelection*, ShipType) | CET/Cataclysm.h:167 | - | Not in HW1 (closest: Select.c selShipsInSelection). |
| `0x00519700` | `UpdateMidLevelHyperspacingShips` | void UpdateMidLevelHyperspacingShips(void) | CET/Cataclysm.h:181 | HW1/Game/SinglePlayer.c:UpdateMidLevelHyperspacingShips | Declared, unused. |
| `0x005197D0` | `singlePlayerGameUpdate` | void singlePlayerGameUpdate(void) | CET/Cataclysm.h:169 | HW1/Game/SinglePlayer.c:singlePlayerGameUpdate | Hooked (pass-through; debug tick logging only). |
| `0x00522560` | `speechEventQueue` | sdword speechEventQueue(void* obj, sdword event, sdword var, sdword variation, sdword actornum, sdword playernum, sdword linkto, real32 timeout, sword volume) | CET/Cataclysm.h:171 | HW1/Game/SpeechEvent.c:speechEventQueue | Declared, unused. |
| `0x005259D0` | `scriptSetReal32CB` | void scriptSetReal32CB(char* dir, char* field, void* data) | CET/Cataclysm.h:164 | HW1/Game/StatScript.c:scriptSetReal32CB |  |
| `0x00525C20` | `scriptSetUdwordCB` | void scriptSetUdwordCB(char* dir, char* field, void* data) | CET/Cataclysm.h:166 | HW1/Game/StatScript.c:scriptSetUdwordCB |  |
| `0x00526420` | `scriptSet` | void scriptSet(const char* directory, const char* filename, scriptEntry info[]) | CET/Cataclysm.h:162 | HW1/Game/StatScript.c:scriptSet |  |
| `0x00527820` | `scriptSetTweakableGlobals` | void scriptSetTweakableGlobals(void) | CET/Cataclysm.h:165 | HW1/Game/StatScript.c:scriptSetTweakableGlobals | Hooked: also parses aiskirmish.script when EnableNewAI. |
| `0x005388F0` | `uicTextBufferResize` | void uicTextBufferResize(textentryhandle, sdword size) | CET/Cataclysm.h:174 | HW1/Game/UIControls.c:uicTextBufferResize |  |
| `0x00538980` | `uicTextEntrySet` | void uicTextEntrySet(textentryhandle, char* text, sdword cursorPos) | CET/Cataclysm.h:176 | HW1/Game/UIControls.c:uicTextEntrySet |  |
| `0x00538A30` | `uicTextEntryInit` | void uicTextEntryInit(textentryhandle, udword flags) | CET/Cataclysm.h:175 | HW1/Game/UIControls.c:uicTextEntryInit |  |
| `0x0053AC70` | `scriptSetFramesCB` | void scriptSetFramesCB(char* dir, char* field, void* data) | CET/Cataclysm.h:163 | HW1/Game/Universe.c:scriptSetFramesCB | Replaced: value * new rate (HW1 * UNIVERSE_UPDATE_RATE). Used for buildTime. |
| `0x0053EA50` | `GetShipStaticInfo` | ShipStaticInfo* GetShipStaticInfo(ShipType) | CET/Cataclysm.h:142 | HW1/Game/Universe.c:GetShipStaticInfo | Cataclysm drops the ShipRace parameter (HW1: (ShipType, ShipRace)). |
| `0x0053F9F0` | `sub_53F9F0` | - | CET/game/Game/AISkirmish.h:105 | - | IDA-style name in a comment: decides which enemy team (TEAM_MimicEnemy1/2) a mimic group is assigned to. Cataclysm-only. |
| `0x00546A20` | `univBulletCollidedWithTarget` | void univBulletCollidedWithTarget(int unknown, SpaceObjRotImpTarg* target, StaticHeader* targetstaticheader, Bullet* bullet, real32 collideLineDist, sdword collSide) | CET/Cataclysm.h:177 | HW1/Game/UnivUpdate.c:univBulletCollidedWithTarget | Cataclysm adds a leading int. Hooked: skip own-mothership-module friendly fire; beams only collide every factor-th frame. |
| `0x0054ACD0` | `univUpdateReset` | void univUpdateReset(void) | CET/Cataclysm.h:180 | HW1/Game/UnivUpdate.c:univupdateReset | Hooked (when shift>0): rewrites HS slice rate, RU regen rate, ETG constants; resets autosave timer. |
| `0x0054C270` | `univCheckRegrowResources` | void univCheckRegrowResources(void) | CET/Cataclysm.h:178 | HW1/Game/UnivUpdate.c:univCheckRegrowResources |  |
| `0x0054C3F0` | `univUpdate` | bool32 univUpdate(real32 phystimeelapsed) | CET/Cataclysm.h:179 | HW1/Game/UnivUpdate.c:univUpdate | Hooked always: autosave timer decremented by phystimeelapsed. |
| `0x00558F20` | `fqEqualize` | int fqEqualize(float* aBlock, float* aEq) | CET/Cataclysm.h:141 | HW1/SDL/fqeffect.c:fqEqualize | [Audio] DisableEqualizer: first byte overwritten with `ret` (C3) via Keystone. [exe 12948] entry bytes `51 53 8B 5C 24 0C` (push ecx; push ebx; mov ebx,[esp+0xC]): cdecl stack args, so a bare `ret` leaves the stack balanced. |
| `0x00559AC0` | `glCapNT` | bool32 glCapNT(void) | CET/Cataclysm.h:143 | - | Relic Win32 glcaps.c (not in HomeworldSDL; port only has `extern glCapNT` in SDL/rinit.c:43). Replaced: returns TRUE when [OpenGL] FixWindowsNTBug. |
| `0x00559BE0` | `glCapStartup` | void glCapStartup(void) | CET/Cataclysm.h:144 | - | Relic Win32 glcaps.c. Hooked: after original, clears glCapCompiledVertexArray if NoCompiledVertexArrays. |
| `0x0055FC70` | `mainRescaleMainWindow` | void mainRescaleMainWindow(void) | CET/Cataclysm.h:146 | HW1/SDL/main.c:mainRescaleMainWindow |  |
| `0x0057DCF0` | `rinDeviceCRC` | udword rinDeviceCRC(void) | CET/Cataclysm.h:155 | HW1/SDL/rinit.c:rinDeviceCRC |  |
| `0x0057DE00` | `rinSortModes` | void rinSortModes(rdevice*) | CET/Cataclysm.h:158 | HW1/SDL/rinit.c:rinSortModes | Replaced: keeps only 32-bit modes, no 4:3 filtering. |
| `0x0057E5C0` | `rinEnumDisplayModes_cb` | HRESULT WINAPI rinEnumDisplayModes_cb(LPDDSURFACEDESC2, LPVOID ctx) | CET/Cataclysm.h:156 | - | DirectDraw7 EnumDisplayModes callback (Relic Win32 rinit.c; removed from SDL port). Replaced: 32bpp >=640x480 only. Cataclysm.h typedef lacks WINAPI. |
| `0x0057E710` | `rinEnumPrimaryDisplayModes_cb` | HRESULT WINAPI rinEnumPrimaryDisplayModes_cb(LPDDSURFACEDESC, LPVOID ctx) | CET/Cataclysm.h:157 | - | Old-DDraw primary enum callback. Replaced: 16/32bpp >=640x480. |
| `0x005808E0` | `soundStartDSound` | sdword soundStartDSound(HWND) | CET/Cataclysm.h:170 | - | Relic Win32 soundlow.c (SDL port has no DirectSound). Replaced unless ForceWaveOut: skips DSCAPS_CERTIFIED check. |
| `0x00589A50` | `trCramRAMComputeAndScale` | void trCramRAMComputeAndScale(void) | CET/Cataclysm.h:173 | HW1/SDL/texreg.c:trCramRAMScaleCompute | Declared, unused. Likely merges HW1 trCramRAMScaleCompute+trCramIntoRAM - UNVERIFIED. Contains LoadMission_AdjustTextureMemoryLimit (+0x9A). |
| `0x005A4D90` | `mistrailUpdate` | void mistrailUpdate(missiletrail*, vector* position) | CET/Cataclysm.h:152 | HW1/Game/Trails.c:mistrailUpdate | Hooked: original only every factor-th universe frame. |
| `0x005A7A70` | `trailSegmentsRead` | void trailSegmentsRead(char* dir, char* field, void* data) | CET/Cataclysm.h:172 | HW1/Game/Trails.c:trailSegmentsRead | Hooked: segment count << shift before calling original. |
| `0x005BC900` | `beastMothershipSelfDamage` | void beastMothershipSelfDamage(ShipStaticInfo*) | CET/Cataclysm.h:130 | - | Cataclysm-only (Beast mothership infection self-damage). Hooked: only called when univUpdateCounter % factor == 0. |
| `0x006725D0` | `memAlloc` | void* memAlloc(size_t size) | CET/Cataclysm.h:147 | HW1/SDL/rinit.c:rinMemAlloc | CET calls it with 1 arg, only from its rinAddMode replacement (game/Game/rinit.cpp:27). [exe 12948] NOT Relic's memAllocFunction: it is the statically linked MSVC debug-CRT malloc (38-byte body calling the _nh_malloc_dbg-style FUN_00672650(size, [0x00A78F14], 1=_NORMAL_BLOCK, 0, 0); FID-matched __malloc_dbg follows at 0x00672600; only 39 call sites). This matches HW1 rinit.c, whose rinMemAlloc/rinMemFree wrap CRT malloc/free (SDL/rinit.c:45-58). Relic's release memAllocFunction(length, flags) (HW1 Memory.h:309,375) is a different function, probably FUN_004ADEE0 (849 call sites, 2 args; UNVERIFIED). |
| `0x00673080` | `memFree` | void memFree(void* ptr) | CET/Cataclysm.h:148 | HW1/SDL/rinit.c:rinMemFree | [exe 12948] CRT free: 19-byte body = __free_dbg(ptr, 1) (FID-matched __free_dbg at 0x006730A0). Pairs with the CRT malloc above, as in HW1 rinit.c:55-58. Relic's memFree is probably FUN_004AE130 (1066 call sites; UNVERIFIED). |

### 3c. Globals and data (incl. derived field addresses)

| Address | CET name | Type / signature | Source | HW1 equivalent | Notes |
|---|---|---|---|---|---|
| `0x0072A614` | `TimerResolutionMax` | real32 (constant 240.0f) | CET/Cataclysm.h:190 | HW1/SDL/utility.h:UTY_TimerResolutionMax | CET writes 15*rate without VirtualProtect. [exe 12948] value 240.0f confirmed; it sits in .rdata, which this exe flags writable (0xC0000040). POOLED constant: 8 `fmul dword` refs in FUN_0042ED90, FUN_0042F1C0, FUN_0042F270 (x3), FUN_0052EC50 (x2), FUN_0058A000, so CET's write rescales all of them (presumably all seconds->timer-tick conversions as in HW1 TimeoutTimer.c:95 / Captaincy.c:211, e.g. decompiled.c:196735; UNVERIFIED). |
| `0x0072A818` | `ETG_UpdateRoundOff` | real64 (double constant 0.03125; CET declares it float*) | CET/Cataclysm.h:194 | HW1/Game/ETG.h:ETG_UpdateRoundOff | (double)ETG_UpdateRoundOff, the fmod divisor of HW1 ETG.c:5255. CET writes period/2 through a float*. [exe 12948] bytes `00 00 00 00 00 00 A0 3F` = DOUBLE 0.03125; sole code ref is `fld qword [0x0072A818]` (DD 05) at 0x0046E037 in etgAtDefine. CET's 4-byte float store (64Hz: 0x3C000000) lands in the LOW dword only, so the double stays ~0.03125: the write is ineffective (CET bug, see docs/cet-knowledge.md section 9). |
| `0x0072A82C` | `etgEffectorCI_UniverseUpdateRate` | real32 (constant -16.0f) | CET/Cataclysm.h:193 | HW1/Game/ETG.c:etgEffectorCI | CET writes -rate. [exe 12948] value -16.0f confirmed; only 1 code ref (`fmul dword` at 0x0046F0E0), so the write affects only this use. |
| `0x0087922C` | `dbgInt3Enabled` | sdword | CET/Cataclysm.h:246 | - | Relic Debug.c (not in port). CET clears it unless a debugger is attached so assertion dialogs show. [exe 12948] initial value 1 (TRUE) confirmed. |
| `0x00886028` | `tpGameCreated` | CaptainGameInfo (688 bytes in Cataclysm per CET) | CET/Cataclysm.h:199 | HW1/SDL/TitanInterfaceC.c:tpGameCreated | CET reads resourceInjectionInterval @+0xB0 and resourceLumpSumTime @+0xB8 (HW1 layout; not asserted). |
| `0x008860D8` | `tpGameCreated_resourceInjectionInterval` | udword (frames) | CET/game/Game/MultiplayerGame.cpp:31 | HW1/SDL/TitanInterfaceC.h:CaptainGameInfo.resourceInjectionInterval | Derived from HW1 layout used by CET (+0xB0); Cataclysm struct changed (688 B) so UNVERIFIED but exercised by CET release code. |
| `0x008860E0` | `tpGameCreated_resourceLumpSumTime` | udword (frames) | CET/game/Game/MultiplayerGame.cpp:69 | HW1/SDL/TitanInterfaceC.h:CaptainGameInfo.resourceLumpSumTime | Derived (+0xB8); UNVERIFIED. |
| `0x0088F580` | `RegenerateRURate` | sdword | CET/Cataclysm.h:188 | HW1/Game/ResCollect.c:regenerateRUrate | HW1 255 (mask, WOODPECKER(regenerateRUrate,0) UnivUpdate.c:7405). CET sets 16*rate-1. [exe 12948] initial value 255 confirmed. |
| `0x00892670` | `SongNumberLoadGameOffset` | sdword* (ptr field of entry 2 of a {void* ptr; udword size}[3] table at 0x00892660) | CET/Cataclysm.h:233 | HW1/Game/LevelLoad.c:SMMissionTweaks | CET redirects slot to a dummy. [exe 12948] NOT an HW1 SMMissionTweaks scriptEntry (0x008925FC is ASCII text). It is the ptr of the 3rd {ptr,size} pair in a 3-entry table at 0x00892660 = {0x00A6B204,4},{0x00A6B20C,4},{0x00883960,4}, summed and packed by FUN_004FDA00 (decompiled.c:159117-159140) and copied back over each ptr by FUN_004FE0E0 (decompiled.c:159505-159525). 0x00883960 = SongNumber: loaded by FUN_004FDFC0 (decompiled.c:159414, cf. HW1 SaveGame.c:640) and reset to -1 by FUN_004A3A10 (decompiled.c:105708, cf. HW1 LevelLoad.c:236). SongNumber var: LevelLoad.c:49. |
| `0x0089410C` | `HYPERSPACE_SLICE_RATE` | real32 | CET/Cataclysm.h:187 | HW1/Game/SinglePlayer.c:HYPERSPACE_SLICE_RATE | HW1 default 0.075f; Cataclysm scripts use 0.025. CET sets 1/(rate*2.5) in univUpdateReset hook. [exe 12948] initial value 0.075f confirmed. |
| `0x008B2428` | `ShipStaticScriptTable` | scriptStructEntry[] (16-byte entries) | CET/Cataclysm.h:224 | HW1/Game/Universe.c:ShipStaticScriptTable | Declared, unused directly; NLIPS/trail entries patched by slot address. |
| `0x008B2B9C` | `shipNLipsRead` | setVarCback (ShipStaticScriptTable[119].setVarCB) | CET/Cataclysm.h:261 | HW1/Game/Universe.c:ShipStaticScriptTable | (0x008B2B9C-0x008B2428)/16 = 119 r4. HW1 key "N-LIPS" (entry 106) fills scaleCap; Cataclysm fills ShipStaticInfo.nlips @0x30C ([exe 12948] entry 119 = {"N-LIPS", 0x005259D0 scriptSetReal32CB, 0x00B3B72C, dummy base 0x00B3B420} -> +0x30C confirmed; the table has 178 named entries). Replaced by CET callback (writes 0 if DisableNLips). |
| `0x008B2C0C` | `trailScaleCapRead` | setVarCback (ShipStaticScriptTable[126].setVarCB) | CET/Cataclysm.h:262 | HW1/Game/Universe.c:ShipStaticScriptTable | "trailScaleCap"; entries 7 apart per trail as in HW1. [exe 12948] entry name "trailScaleCap", setVarCB 0x005259D0, field offset +0x384 confirmed. |
| `0x008B2C7C` | `trailScaleCapRead2` | setVarCback (ShipStaticScriptTable[133].setVarCB) | CET/Cataclysm.h:263 | HW1/Game/Universe.c:ShipStaticScriptTable | "trailScaleCap2". [exe 12948] entry name "trailScaleCap2", setVarCB 0x005259D0, field offset +0x388 confirmed. |
| `0x008B2CEC` | `trailScaleCapRead3` | setVarCback (ShipStaticScriptTable[140].setVarCB) | CET/Cataclysm.h:264 | HW1/Game/Universe.c:ShipStaticScriptTable | "trailScaleCap3". [exe 12948] entry name "trailScaleCap3", setVarCB 0x005259D0, field offset +0x38C confirmed. |
| `0x008B2D5C` | `trailScaleCapRead4` | setVarCback (ShipStaticScriptTable[147].setVarCB) | CET/Cataclysm.h:265 | HW1/Game/Universe.c:ShipStaticScriptTable | "trailScaleCap4". [exe 12948] entry name "trailScaleCap4", setVarCB 0x005259D0, field offset +0x390 confirmed. |
| `0x008B85E8` | `MAIN_WindowWidth` | sdword | CET/Cataclysm.h:228 | HW1/SDL/main.c:MAIN_WindowWidth |  |
| `0x008B85EC` | `MAIN_WindowHeight` | sdword | CET/Cataclysm.h:229 | HW1/SDL/main.c:MAIN_WindowHeight |  |
| `0x008B85F0` | `MAIN_WindowDepth` | sdword | CET/Cataclysm.h:230 | HW1/SDL/main.c:MAIN_WindowDepth |  |
| `0x008B8610` | `enableAVI` | bool32 | CET/Cataclysm.h:204 | HW1/SDL/main.c:enableAVI | CET DisableIntroMovies -> FALSE (= /disableAVI). [exe 12948] initial value 1 (TRUE) confirmed. |
| `0x008B8628` | `showBorder` | bool32 | CET/Cataclysm.h:202 | HW1/SDL/main.c:showBorder | CET BorderlessWindow -> FALSE (= /noBorder). [exe 12948] initial value 1 (TRUE) confirmed. |
| `0x008B8638` | `fullScreen` | bool32 | CET/Cataclysm.h:203 | HW1/SDL/main.c:fullScreen | CET BorderlessWindow -> FALSE (= /window). [exe 12948] initial value 1 (TRUE) confirmed. |
| `0x008B8644` | `glToSelect` | char[512?] | CET/Cataclysm.h:227 | - | GL DLL name the game will load ("rgl.dll"/"opengl32.dll"); CET strcpy_s(...,512,"opengl32.dll"). Relic Win32 main.c; not in port. [exe 12948] initial contents "rgl.dll". |
| `0x008B8850` | `networkVersion` | char[16] | CET/Cataclysm.h:243 | HW1/SDL/main.c:networkVersion | CET writes "v1.02". 16 bytes before minorBuildVersion matches MAX_NETWORKVERSION_STRING_LEN=16 (SDL/TitanInterfaceC.h:29). [exe 12948] original contents "Emergence 1". |
| `0x008B8860` | `minorBuildVersion` | char[] | CET/Cataclysm.h:244 | HW1/SDL/main.c:minorBuildVersion | CET writes "0". [exe 12948] original contents "01". |
| `0x008DF604` | `aiCurrentAIPlayer` | AIPlayer* (pointer variable) | CET/Cataclysm.h:252 | HW1/Game/AIPlayer.c:aiCurrentAIPlayer |  |
| `0x008DFC38` | `aiCarrier1Ship` | Ship* (pointer variable) | CET/Cataclysm.h:250 | - | Cataclysm AI. |
| `0x008DFC3C` | `aiCarrier2Ship` | Ship* (pointer variable) | CET/Cataclysm.h:251 | - | Cataclysm AI. |
| `0x008DFC64` | `aiBuildingShip` | udword[TOTAL_STD_SHIPS=60] | CET/Cataclysm.h:249 | - | Cataclysm skirmish AI: ships queued per type. |
| `0x008DFD60` | `aisTeams` | AISTeamEntry[34] (0x9D10 each) | CET/Cataclysm.h:253 | - | Cataclysm AI team table; ends at 0x00A2D980 (static_assert AISkirmish.h:134). |
| `0x00A2D980` | `aisTeams_end` | end of AISTeamEntry[34] | CET/game/Game/AISkirmish.h:134 | - | sizeof(AISTeamEntry)*34 == 0x00A2D980-0x008DFD60 = 0x14DC20. |
| `0x00A2D984` | `aiHasExternalConstruction` | bool32 | CET/Cataclysm.h:254 | - | Cataclysm AI. |
| `0x00A2D98C` | `aiHasCarrier1ModuleQueued` | bool32 | CET/Cataclysm.h:255 | - | Cataclysm AI. |
| `0x00A2D994` | `aiHasSupportModuleQueued` | bool32 | CET/Cataclysm.h:256 | - | Cataclysm AI. |
| `0x00A2D9A8` | `aiHasCarrier2ModuleQueued` | bool32 | CET/Cataclysm.h:257 | - | Cataclysm AI. |
| `0x00A2D9B4` | `aiSupportUnitsPending` | sdword | CET/Cataclysm.h:258 | - | Cataclysm AI (support units). |
| `0x00A43C94` | `multiPlayerGame` | bool32 | CET/Cataclysm.h:207 | HW1/Game/Globals.c:multiPlayerGame | Skirmish: both this and singlePlayerGame are FALSE. |
| `0x00A43C9C` | `singlePlayerGame` | bool32 | CET/Cataclysm.h:208 | HW1/Game/Globals.c:singlePlayerGame |  |
| `0x00A4C6C4` | `mgResourceInjectionIntervalEntryBox` | textentryhandle | CET/Cataclysm.h:196 | HW1/Game/MultiplayerGame.c:mgResourceInjectionIntervalEntryBox |  |
| `0x00A4C6CC` | `mgResourceLumpSumIntervalEntryBox` | textentryhandle | CET/Cataclysm.h:197 | HW1/Game/MultiplayerGame.c:mgResourceLumpSumIntervalEntryBox |  |
| `0x00A4CAB4` | `nisUpdateTask_timeElapsed` | real32 (static) | CET/PatchUniverse.cpp:393 | HW1/Game/NIS.c:nisUpdateTask | Literal in PatchUniverse.cpp; HW1 local `timeElapsed`. |
| `0x00A4CC20` | `opDeviceCRC` | udword | CET/Cataclysm.h:222 | HW1/Game/Options.c:opDeviceCRC | Saved renderer-device CRC (registry option). |
| `0x00A6D9A0` | `realtimeremainder` | LARGE_INTEGER | CET/Cataclysm.h:216 | - | Cataclysm QPC-based universe timer remainder (Win32 utility.c). Declared, unused. |
| `0x00A72548` | `glCapCompiledVertexArray` | bool32 | CET/Cataclysm.h:205 | - | Relic Win32 glcaps.c (GL_EXT_compiled_vertex_array). CET NoCompiledVertexArrays -> FALSE. |
| `0x00A74DC8` | `coopDSound` | bool32 | CET/Cataclysm.h:240 | HW1/SDL/main.c:coopDSound | Command-line DSound cooperative mode. |
| `0x00A75F28` | `lpDirectSound` | LPDIRECTSOUND | CET/Cataclysm.h:236 | - | Relic Win32 soundlow.c. |
| `0x00A75F2C` | `lpPrimaryBuffer` | LPDIRECTSOUNDBUFFER | CET/Cataclysm.h:237 | - | Relic Win32 soundlow.c. |
| `0x00A75F3C` | `bDirectSoundCertified` | bool32 | CET/Cataclysm.h:238 | - | Relic Win32 soundlow.c. |
| `0x00AB7520` | `utyTimerDivisor` | LONGLONG | CET/Cataclysm.h:218 | HW1/SDL/utility.c:utyTimerDivisor | 64-bit in Cataclysm (QPC ticks per timer tick); Uint32 ms in SDL port. Declared, unused. |
| `0x00B06C80` | `waveFormatEX` | WAVEFORMATEX | CET/Cataclysm.h:239 | - | Primary-buffer format. |
| `0x00B377A4` | `rndAspectRatio` | real32 | CET/Cataclysm.h:231 | HW1/SDL/render.c:rndAspectRatio |  |
| `0x00B62A00` | `universe` | Universe | CET/Cataclysm.h:210 | HW1/Game/Universe.c:universe | Layout partially reversed (see universe.h asserts). |
| `0x00B62AB8` | `universe_unknown1` | sdword | CET/game/Game/universe.h:159 | - | Derived: universe 0x00B62A00 + 0xB8 (NOT asserted; CET comment says position uncertain). Cataclysm-only field before phystimeelapsed |
| `0x00B62ABC` | `universe_phystimeelapsed` | real32 | CET/PatchUniverse.cpp:416 | HW1/Game/Universe.h:Universe.phystimeelapsed | = universe+0xBC (asserted universe.h:234). |
| `0x00B62AC0` | `universe_totaltimeelapsed` | real32 | CET/game/Game/universe.h:235 | HW1/Game/Universe.h:Universe.totaltimeelapsed | Derived: universe 0x00B62A00 + 0xC0 (asserted offset). |
| `0x00B62AC4` | `universe_realtimeelapsed` | real32 | CET/game/Game/universe.h:236 | - | Derived: universe 0x00B62A00 + 0xC4 (asserted). Cataclysm-new field (no HW1 twin; CET universe.h:162 '@CATA: New timer keyed off QueryPerformanceCounter'). |
| `0x00B62AC8` | `universe_radius` | real32 | CET/game/Game/universe.h:237 | HW1/Game/Universe.h:Universe.radius | Derived: universe 0x00B62A00 + 0xC8 (asserted offset). |
| `0x00B62AE4` | `universe_effectList` | LinkedList | CET/game/Game/universe.h:238 | HW1/Game/Universe.h:Universe.effectList | Derived: universe 0x00B62A00 + 0xE4 (asserted offset). |
| `0x00B62AF0` | `universe_ShipList` | LinkedList | CET/game/Game/universe.h:239 | HW1/Game/Universe.h:Universe.ShipList | Derived: universe 0x00B62A00 + 0xF0 (asserted offset). |
| `0x00B62B98` | `universe_HousekeepShipList` | GrowSelection | CET/game/Game/universe.h:240 | HW1/Game/Universe.h:Universe.HousekeepShipList | Derived: universe 0x00B62A00 + 0x198 (asserted offset). |
| `0x00B62BA0` | `universe_unk1` | GrowSelection | CET/game/Game/universe.h:241 | - | Derived: universe 0x00B62A00 + 0x1A0 (asserted offset). Cataclysm-only |
| `0x00B62BA8` | `universe_star3dinfo` | Star3dInfo* | CET/game/Game/universe.h:242 | HW1/Game/Universe.h:Universe.star3dinfo | Derived: universe 0x00B62A00 + 0x1A8 (asserted offset). |
| `0x00B62BAC` | `universe_univUpdateCounter` | udword | CET/Cataclysm.h:189 | HW1/Game/Universe.h:Universe.univUpdateCounter | = universe+0x1AC (asserted). |
| `0x00B62BBC` | `universe_wintime` | real32 | CET/game/Game/universe.h:244 | HW1/Game/Universe.h:Universe.wintime | Derived: universe 0x00B62A00 + 0x1BC (asserted offset). |
| `0x00B62BC0` | `universe_quittime` | real32 | CET/game/Game/universe.h:245 | HW1/Game/Universe.h:Universe.quittime | Derived: universe 0x00B62A00 + 0x1C0 (asserted offset). |
| `0x00B62BC4` | `universe_lasttimeadded` | udword | CET/game/Game/universe.h:246 | HW1/Game/Universe.h:Universe.lasttimeadded | Derived: universe 0x00B62A00 + 0x1C4 (asserted offset). |
| `0x00B62CEA` | `universe_aiplayerEnemy` | sbyte[8] | CET/game/Game/universe.h:247 | HW1/Game/Universe.h:Universe.aiplayerEnemy | Derived: universe 0x00B62A00 + 0x2EA (asserted offset). |
| `0x00B62CF4` | `universe_aiplayerProcessing` | bool32 | CET/game/Game/universe.h:248 | HW1/Game/Universe.h:Universe.aiplayerProcessing | Derived: universe 0x00B62A00 + 0x2F4 (asserted offset). |
| `0x00B62CF8` | `universe_CapitalShipCaptured` | sdword | CET/game/Game/universe.h:249 | HW1/Game/Universe.h:Universe.CapitalShipCaptured | Derived: universe 0x00B62A00 + 0x2F8 (asserted offset). |
| `0x00B62D00` | `universe_bounties` | sdword | CET/game/Game/universe.h:251 | HW1/Game/Universe.h:Universe.bounties | Derived: universe 0x00B62A00 + 0x300 (asserted offset). |
| `0x00B62D08` | `universe_gameStats` | GameStats | CET/game/Game/universe.h:253 | HW1/Game/Universe.h:Universe.gameStats | Derived: universe 0x00B62A00 + 0x308 (asserted offset). |
| `0x00B674F0` | `universe_numCratesInWorld` | udword | CET/game/Game/universe.h:255 | HW1/Game/Universe.h:Universe.numCratesInWorld | Derived: universe 0x00B62A00 + 0x4AF0 (asserted offset). |
| `0x00B674F8` | `universe_numPlayers` | uword | CET/game/Game/universe.h:256 | HW1/Game/Universe.h:Universe.numPlayers | Derived: universe 0x00B62A00 + 0x4AF8 (asserted offset). |
| `0x00B67500` | `universe_players` | Player[9] (Cataclysm sizeof(Player) = 0x6530 per exe stride; CET's 0x2F8 copy is asserted only up to 0x2A4) | CET/game/Game/universe.h:257 | HW1/Game/Universe.h:Universe.players | Derived: universe 0x00B62A00 + 0x4B00 (asserted offset). [exe 12948] players[] stride is 0x6530 (= the size CET comments out at universe.h:136): allianceFormWith indexes players[i].AllianceProposals as (&0x00B6D9F6)[i*0x3298] (uword) and .AllianceProposalsTimeout as (&0x00B6D9F8)[i*0x194C] (udword) (decompiled.c:18561,18570), and two player loops start at &players[0]+0x27 (0x00B67527) and step `p += 0x6530` (decompiled.c:64101-64121, 189024-189027). So players[1] starts at 0x00B6DA30 and everything CET places after players[0] (world @+0x65B8) is wrong; real world[] would be at universe+0x3DA30 (derived, UNVERIFIED). |
| `0x00BB3260` | `taskFrequency` | real32 | CET/Cataclysm.h:212 | HW1/Game/Task.c:taskFrequency | Declared, unused by CET. |
| `0x00C0A5C0` | `singlePlayerGameInfo` | SinglePlayerGameInfo | CET/Cataclysm.h:214 | HW1/Game/SinglePlayer.c:singlePlayerGameInfo | Declared, unused. hyperspaceState @+0x10 asserted. |
| `0x00C0A5D0` | `singlePlayerGameInfo_hyperspaceState` | HyperspaceState | CET/game/Game/singleplayer.h:137 | HW1/Game/SinglePlayer.h:SinglePlayerGameInfo.hyperspaceState | Derived: 0x00C0A5C0 + 0x10 (asserted). |
| `0x00C41C74` | `etgTotalTimeElapsed` | real32 | CET/Cataclysm.h:220 | HW1/Game/ETG.c:etgTotalTimeElapsed | Declared, unused. |

### 3d. Addresses found by the byte check (not CET literals, so NOT in `cet_symbols.json`)

Read from `original/game/cataclysm.exe` (GOG build 12948, sha256 `f03d9a3e…`) and the existing Ghidra export `ghidra/exports/cataclysm_gog/` (`decompiled.c`, `imports.txt`, `functions.csv`). They are kept out of `cet_symbols.json` so that file stays "every address CET references".

| Address | What | Evidence |
|---|---|---|
| 0x00883960 | `SongNumber` (sdword) | Target of CET's slot 0x00892670. Loaded by FUN_004FDFC0 (`decompiled.c:159414`, cf. `HW1/Game/SaveGame.c:640`), reset to -1 by FUN_004A3A10 (`:105708`, cf. `HW1/Game/LevelLoad.c:236`), range-checked `> 0x10` (`:105728`) |
| 0x00892660 | Pre-game save table `{void* ptr; udword size}[3]` = {0x00A6B204,4}, {0x00A6B20C,4}, {0x00883960,4}. Ends where the string `"Error %d loading game %s"` (0x00892678) begins | Sizes summed and values packed by FUN_004FDA00 (`decompiled.c:159117-159140`); copied back over each `ptr` on load by FUN_004FE0E0 (`:159505-159525`). No HW1 equivalent: HW1 `SavePreGameInfo`/`LoadPreGameInfo` (`SaveGame.c:595-660`) use no such table |
| 0x008C0F38 | `"Update.big"` | Operand of the three pushes at 0x005ABB21/3E/5C |
| 0x0072A810 | float 0.015625 (= `ETG_UpdateRoundOff/2` at 16 Hz) | Operand of 0x0046E042/4F/5B; also `fmul` at 0x0055A9E2/0x0055AF0A/0x0055B2B6 |
| 0x0072AAE8 | float 0.0625 (`UNIVERSE_UPDATE_PERIOD`) | Operand of the 4 NIS sites; 11 code refs in all |
| 0x0046E037 | `fld qword [0x0072A818]`, the fmod divisor load in `etgAtDefine` (HW1 `ETG.c:5255`) | The only code ref to 0x0072A818 |
| 0x0072B064 | float 0.3, the mission-17 texture-pool factor | `fmul` at 0x00589B01 inside the 0x00589AEA block |
| 0x008AC5C4 / 0x008AC5C8 | `REFRESH_RESEARCH_RATE` = 15 / `REFRESH_RESEARCH_FRAME` = 13 (tweakables, `assets/tweak.script:798-799`; HW1 `Tweak.c:404,987`) | Read by the research site 0x0054C83D |
| 0x0088F558 / 0x0088F55C | `REGROW_RESOURCES_CHECK_RATE` = 255 / `_FRAME` = 86 (HW1 `ResCollect.c:60-61,106`) | Read by the regrow site 0x0054CD73 |
| 0x0072A218 | IAT slot `KERNEL32!Sleep` | `imports.txt`; called by both frame-limiter sites |
| 0x0072A228 / 0x0072A0A8 | IAT slots `KERNEL32!QueryPerformanceCounter` (29 code refs) / `QueryPerformanceFrequency` (4 refs) | `imports.txt:144-145`. The exe reaches QPC through this IAT slot, so CET's process-wide M6 detour on kernel32's `QueryPerformanceCounter` (`PatchUniverse.cpp:349`) covers all 29 call sites. Its `realtimeelapsed` writer is not identified (UNVERIFIED) |
| 0x00A43DF4 | `sigsPlayerIndex` (the local player index; name by match with HW1 `Alliance.c:55-57`) | `allianceFormWith` indexes `players[DAT_00a43df4]` exactly where HW1 uses `players[sigsPlayerIndex]` (`decompiled.c:18561-18570`). BSS (reads as 0 on disk) |
| 0x00B6D9F6 / 0x00B6D9F8 | `players[0].AllianceProposals` (uword, Player+0x64F6) / `.AllianceProposalsTimeout` (udword, Player+0x64F8) | Same function, with strides 0x3298 words / 0x194C dwords = **0x6530 bytes** (section 4.1 `Player` row). CET's layout puts them at +0x2EE/+0x2F0 |
| 0x00B3A444 | GL function-pointer slot called with `(GL_TEXTURE_2D, pname, param)`, so `glTexParameteri` (name inferred, UNVERIFIED) | The two calls in the 0x004DD43E block |
| 0x00A74D50 | Global set to 2 by `ActivateMe` (0x00562084); meaning UNVERIFIED | Bytes at 0x00562084 |
| 0x005486E5 | `CheckPlayerWin` non-MP branch (target of the original `je`) | Bytes at 0x0054860C |
| 0x005624E0 | `WindowProc` F12 debug-key path (target of the original `je`) | Bytes at 0x00562401 |
| 0x00528410 | Cataclysm timer start `(Timer*, real32 seconds)` | Called with `&ship->buildTimer` (+0x5B4) right after 0x00543F51 (`decompiled.c:210779`) |
| 0x00525920 / 0x00525B90 / 0x00525BC0 / 0x00525BF0 | `scriptSetRGBCB` / `scriptSetReal32SqrCB` / `scriptSetSbyteCB` / `scriptSetSwordCB` | `ShipStaticScriptTable` callbacks whose keys match HW1 `Universe.c:498ff` entries |
| 0x00525CB0 / 0x00525D20 | `scriptSetBool8` (1-byte store, HW1 `StatScript.c:432-435`) / `scriptSetBool` (bool32 store, HW1 `StatScript.c:437-440`) | Same table: 16 and 5 keys. `externalBuild` uses `scriptSetBool8` and `repairBeamCapable` uses `scriptSetBool`, so in the exe they are bool8 and bool32, the reverse of CET's `bool32`/`bool` declarations |
| 0x0053AB10 / 0x0053AB50 / 0x0053AC30 / 0x0053AC50 | `LODScriptLoad` / `loadMexDataCB` / `setClassCB` / the `beastAttackConvertTo` parser (Cataclysm-new) | Same table. `scriptSetFramesCB` 0x0053AC70 serves exactly one key, `buildTime` |
| 0x004ADEE0 / 0x004AE130 | Probably Relic's `memAllocFunction(length, flags)` / `memFree` (849 / 1066 call sites, 2 / 1 args; release macro `HW1/Game/Memory.h:309,375`). UNVERIFIED | `functions.csv`; FUN_004ADEE0(size,0) allocates the pre-game save chunk (`decompiled.c:159125`) |

---

## 4. Reconstructed structs (`CET/game/Game`, `CET/game/Win32`; `CET/game/rgl` is an empty submodule dir)

How to read this section:
- Only offsets marked **Asserted** were reverse-engineered against the exe by CET (each has an `ASSERT_OFFSET`/`static_assert`). Everything else is the HW1 layout carried forward, padded with `unk*` blobs until the asserted fields line up. Treat unasserted offsets as hypotheses.
- "In HW1?" is a name match against the HomeworldSDL struct of the same name (heuristic). **no (Cataclysm/CET)** marks a field CET added that HW1 lacks, and these are the best leads for Cataclysm-specific features.
- Integer types: `udword/sdword` are 32-bit, `uword/sword` 16-bit, `bool8/bool16/bool32` are typedefs (`CET/game/Game/TYPES.h:11-24`). CET is C++, so a bare `bool` in its headers is the **1-byte** C++ bool. Relic's C `bool` was probably 32-bit, because HomeworldSDL renamed `bool` to `bool32` everywhere (e.g. `HW1/Game/Globals.h` vs `CET/game/Game/Globals.h` diff). **[exe 12948]** The exe's script table settles two of these: `repairBeamCapable` is filled by `scriptSetBool` (a bool32 store), and `externalBuild` (CET `bool32`) by `scriptSetBool8` (a 1-byte store). CET reads `externalBuild` as bool32 (`AISkirmish.cpp:176,360`), which is benign only if the padding bytes are zero (HW1 memsets each `ShipStaticInfo`, `Universe.c:3536`; UNVERIFIED for Cataclysm). Every bare-`bool` field in CET (`ShipStaticInfo.repairBeamCapable` 0x224, `SinglePlayerGameInfo.onCompleteHyperspace` 0x08, `giveComputerFleetControl`..`hyperspaceFails` 0x44-0x47, `shiptrail.wobbly`) therefore only matches the exe by alignment luck or not at all. UNVERIFIED.
- Constants the asserts pin down: `TOTAL_NUM_SHIPS = 145` (Player `shiptotals` 0x30..0x274; `CET/game/Game/ShipDefs.h:150`), `MAX_MULTIPLAYER_PLAYERS = 8`, `NUM_CLASSES = 12` in CET (`ClassDefs.h`; but Cataclysm's `tweak.script:837` also names `CLASS_Composite`, so the exe may have 13, UNVERIFIED), `COMMAND_MAX_SHIPS = 1000` (`MaxSelection` 0xFA8, forced by `sizeof(AISTeamEntry)==0x9D10`), `NUM_AIS_TEAMS = 34`.
- Ship type IDs (Cataclysm): `CET/game/Game/ShipDefs.h:1-150`. Sect s* 0-32, Beast b* 33-59, then Kushan/Taiidan/P1/Traders/derelicts/NIS 60-144; `STD_FIRST_SHIP=sRecon(0)`, `STD_LAST_SHIP=bMissileCorvette(59)`. Races (`RaceDefs.h`): Sect 0, Beast 1, Kushan 2, Taiidan 3, P1 4, Traders 5, Derelict 6. Classes (`ClassDefs.h`): Mothership 0 .. Micro 11. Modules (`ModuleDefs.h`): HANGAR 0, ARMOUR 1, ENGINEERING 2, ADVANCED_ENGINEERING 3, WEAPON 4. `BeastSystemType` (`objtypes.h:273-281`): None, Thrust, Infection, Defensive, Research, Build.

### 4.1 Struct-to-engine-concept map

| CET struct (file) | Engine concept | HW1 counterpart | Cataclysm deltas CET found |
|---|---|---|---|
| `Universe` (`universe.h:150-257`) | The single world state `universe` @0x00B62A00 | `HW1/Game/Universe.h:126-204` `Universe` | `unknown1` before `phystimeelapsed` (0xBC); new `realtimeelapsed` (QPC clock) 0xC4; `AttackMemory` **or** `RetreatList` removed; extra `GrowSelection unk1` 0x1A0; `BYTE unk3[0x1F8]` after `gameStats` (GameStats likely grew); `players` at 0x4B00. **[exe 12948]** With the real 0x6530-byte `Player` (row below), `players[9]` spans 0x4B00-0x3DA30. So CET's `world` (0x65B8) and `sizeof(Universe)` 0x65C4 are wrong. Derived: `world[3]` at +0x3DA30 (0x00BA0430) and `sizeof(Universe)` >= 0x3DA3C (UNVERIFIED; no direct reference to 0x00BA0430 is in `decompiled.c`) |
| `Player` (`universe.h:110-148`) | Per-player state | `HW1/Game/Universe.h:104-124` | `udword unk` 0x08, **`supportUnitsMax` 0x0C, `supportUnitsUsed` 0x10** (support-unit cap). CET: real `sizeof(Player)` is "significantly larger" (commented `0x6530`, `universe.h:135-136`), so `players[i]` for i>0 and everything after is unreliable. **[exe 12948]** Confirmed: the stride is **0x6530**. `allianceFormWith` (0x00420880) indexes `players[i].AllianceProposals` as `(&0x00B6D9F6)[i*0x3298]` (uword) and `.AllianceProposalsTimeout` as `(&0x00B6D9F8)[i*0x194C]` (udword) (`decompiled.c:18561,18570`, the HW1 statements of `Alliance.c:55-57`), and two player loops start at `&players[0]+0x27` and step 0x6530 (`decompiled.c:64101-64121`, `189024-189027`). So `AllianceProposals` sits at Player+0x64F6 and `AllianceProposalsTimeout` at +0x64F8 (CET: 0x2EE/0x2F0). Cataclysm therefore inserts about 0x6250 B between the last asserted field (`classtotals`, ends 0x2A4) and `AllianceProposals`. Where they go (a bigger `researchinfo`?) is UNVERIFIED |
| `SpaceObj` (`spaceobj.h:1193-1219`) | Base object header | `HW1/Game/SpaceObj.h` `SpaceObj` | +8 bytes `unk1` after `flags` (0x18), +4 `unknown2` after `attributesPad` (0x3C), so `staticinfo` 0x20, `cameraDistanceSquared` 0x40, `posinfo` 0x58. **Not** propagated to CET's `SpaceObjRot*`, `Missile`, `Derelict`, `Resource`... (still HW1 layout, wrong) |
| `Ship` (`spaceobj.h:1517-1876`) | Ship instance | `HW1/Game/SpaceObj.h` `Ship` | Same header delta as SpaceObj; `buildProgress` 0x170; `unk2[0x50]` before `shiplink` 0x23C; `buildTimerElapsed` 0x3E8; external `Timer buildTimer` 0x5B4; `beastSystemType` 0x650 / `beastUnk` 0x654 (Beast infection systems); `sizeof` >= 0x660. **Commented-out asserts** (`spaceobj.h:1820-1822`) claim `putOnDoor` 0x1C3, `dockingship` 0x1C4, `specialFlags` 0x1C8, i.e. 8 B later than CET's compiled layout (0x1BB/0x1BC/0x1C0). If they are right, Cataclysm adds 8 B somewhere in 0x174-0x1BB (`nonstatvars`..`aistate`), absorbed by `unk2[0x50]` before the asserted `shiplink` 0x23C. Treat 0x174-0x23C as unresolved (UNVERIFIED). **[exe 12948]** Weak support for the +8 hypothesis: `aiUpdateExternalShipBuild` (0x00543F10) tests `[ship+0x1CC] & 0x80000` (`8B 87 CC 01 00 00 A9 00 00 08 00`). That is `specialFlags2` under +8; in CET's layout 0x1CC falls inside `salvageNumTagged[]`. The same function confirms `staticinfo` +0x20, `buildTimerElapsed` +0x3E8, `playerowner` +0x32C, `buildTimer` +0x5B4, and `Player.resourceUnits` +4 (`decompiled.c:210769-210783`) |
| `StaticHeader` (`spaceobj.h:400-434`) | Static-info header (physics, collision) | same name | Unchanged vs HW1 up to the asserted `staticCollInfo.collspheresize` 0x4C. **[exe 12948]** The exe's `ShipStaticScriptTable` puts `preciseSelection` at 0x88 and `collBoxScale*`/`collBoxOffset*` at 0x8C-0xA0, all **+0xC** from CET's layout (0x7C, 0x80-0x94). So Cataclysm adds 12 B inside `StaticCollInfo` somewhere in 0x50-0x87, the real `StaticHeader` is 0xA4 B, and CET's `ShipStaticInfo.unk[12]` (0x98) is that growth in the wrong place. Every other static-info type embedding `StaticHeader` shifts too (UNVERIFIED) |
| `ShipStaticInfo` (`spaceobj.h:557-916`) | Per-ship-type stats parsed from `.shp` via `ShipStaticScriptTable` | same name | **[exe 12948]** all 178 script-table entries were compared with CET's compiled offsets: 171 match (0x00-0x454, including `nlips` 0x30C, `Overlay_Selection_Scalar` 0x3E8, `supportUnits` 0x410, `harvestRate` 0x25C, `buildTime` 0x23C). The 7 misses are the `StaticHeader` +0xC shift (row above). `unk[12]` before `maxhealth` 0xA4 (really inside `StaticHeader`, see above); `unk2[64]` after `custstatinfo`; `externalBuild` 0x1F0, `forceFieldProtectionDistanceScale`, `upgradeTimeModifier`, `fowRadius` 0x240, `RKOEatMeTime`, **`nlips` 0x30C** (per-ship N-LIPS), `nRepairDroids` 0x320, `hyperspaceBoxScale`, `leech*`, `beastAttack*`, `Overlay_Selection_Scalar` 0x3E8, `bUseRayTriangleIntersectionForHitLocation`, `linkDistance`, **`supportUnits` 0x410 / `providesSU` 0x414**, ram/cutting-beam/EMP stats, `UpgradeByDocking`, `pilotViewOffset{X,Y,Z}`, `RKOGetsMissiles` 0x454. Fields after 0x454 (`tacticalSelectionScale`, gun/dock/nozzle pointers, `engineNozzleOffset`, `trailStatic`, `scaleCap`, `madStatic`, dock fn ptrs) are flagged "locations unknown, do not use" (`spaceobj.h:733`) |
| `Bullet` (`spaceobj.h:1998-2046`) | Bullet/beam instance | same name | Only `bulletType` 0x108 and `owner` 0x11C asserted; CET says the other offsets are "probably wrong" (`:2023`); header **not** updated with SpaceObj's +8/+4 |
| `Effect` (`spaceobj.h:1949-1992`) | ETG effect instance | same name | `sdword unknown[2]` after `flags` (so `staticinfo` 0x20, consistent with SpaceObj); nothing asserted |
| `SinglePlayerGameInfo` (`singleplayer.h:77-137`) | SP campaign state `singlePlayerGameInfo` @0x00C0A5C0 | `HW1/Game/SinglePlayer.h` | `hyperspaceState` 0x10 asserted, otherwise HW1 |
| `AIPlayer` (`AIPlayer.h:94-205`) | Computer player | `HW1/Game/AIPlayer.h` | Only 0x4-0x10 asserted. Commented assert `aifHyperSkimming==0xB9C` (`:205`) disagrees with CET's own layout (0xB94), so Cataclysm adds >=8 bytes before it |
| `AISTeamEntry`, `AISTeamSelection` (`AISkirmish.h:8-134`) | Cataclysm skirmish-AI team buckets `aisTeams[34]` @0x008DFD60 | none (Cataclysm-only) | `selection[10]` of {`MaxSelection` + centre `vector`}, `numselections` 0x9D08, `totalteamsize` 0x9D0C, size 0x9D10; team index enum `AITeamType` (`AISkirmish.h:21-127`) |
| `CommandToDo` (`CommandLayer.h:83-109`) | Queued ship command | same name | CET comment "Size is now 0x330" (CET's HW1 copy is 0x100) |
| `CaptainGameInfo` (`Win32/TitanInterfaceC.h:149-183`) | MP game-setup record `tpGameCreated` @0x00886028 | `HW1/SDL/TitanInterfaceC.h` | "struct has changed. New size is 688" (CET copy 616). CET still uses the HW1 offsets of `resourceInjectionInterval` (+0xB0) and `resourceLumpSumTime` (+0xB8) |
| `Timer` (`Win32/timer.h:3-9`) | Cataclysm build timer {start, duration, unk, unk2} (16 B) | **different** from HW1 `Game/Timer.h` named timer | New |
| `shiptrail` (`Win32/trails.h:76-105`) | Engine trail instance | `HW1/Game/Trails.h` | Extra `sdword unknown` 0x38 before `grainCounter` 0x3C |
| `texreg` (`Win32/texreg.h:154-181`) | Texture-registry entry | `HW1/SDL/texreg.h` | Same as HW1 (asserts for 0x4/0x6/0xC) |
| `SFXLUT` (`soundstructs.h:46-57`) | Sound-event lookup table header | `HW1/Game/SoundStructs.h` | Same as HW1. Sample `assets/soundfx/shipcmnevents.lut` begins `"PKG0"`, numvariations=10, numevents=10, numobjects=0x92 (146) |
| `particleAttribs` (`particle.h:208-304`) | Particle-system attribute block, moved from `particle.c` to the header | `HW1/Game/Particle.c` (file-local) | `effectVelocity` (was a static), `sdword unknown[3]` after line fields; size **456** asserted. Two new `particleType`s `PART_UNK1` (0x24 B) / `PART_UNK2` (0x1C B) (`particle.h:31-40`) |
| `rmode`, `rdevice` (`Win32/rinit.h`) | Renderer/display-mode lists | `HW1/SDL/rinit.h` (SDL port dropped `devcaps`) | Relic Win32 layout |
| `scriptEntry` (12 B), `scriptStructEntry` (16 B) (`statscript.h:19-34`) | Script-binding tables | `HW1/Game/StatScript.h` | Same |
| `GrowSelection`, `MaxSelection`, `SelectCommand` (`shipselect.h`) | Selections | `HW1/Game/ShipSelect.h` | Same |
| `etgeffectstatic`, `etgfunctioncall` (`etg.h:373-523`) | ETG effect definition / p-code | `HW1/Game/ETG.h` | Used only by debug hooks; unverified |
| `uictextentry`, `featom` (`UIcontrols.h:205-224`, `FeFlow.h`) | FE text box / FE atom | `HW1/Game/UIControls.h`, `FEFlow.h` | Not asserted; CET dereferences `textBuffer`, `message`, `pData`, `status` in `MultiplayerGame.cpp`, which implicitly validates those offsets |

Also reconstructed but used for nothing beyond compiling (HW1 copies, unverified): `AIAttackMan.h AIFeatures.h AIResourceMan.h AIUtilities.h AIVar.h Attributes.h blobs.h b-spline.h camera.h CameraCommand.h clouds.h CommandNetwork.h consMgr.h crc32.h Debug.h dock.h FormationDefs.h formation.h gamestats.h Globals.h KAS.h KASFunc.h KEY.H linkedlist.h LOD.h matrix.h meshanim.h mesh.h nebulae.h pieplate.h region.h rescollect.h researchapi.h select.h star3d.h task.h tweak.h vector.h Volume.h`, `Win32/color.h font.h gldefines.h gldll.h glext.h glinc.h prim2d.h Queue.h rglu.h`. `Win32/gldll.h` is the game-side declaration of the GL function-pointer table loaded from `rgl.dll`/`opengl32.dll` (see section 6).

### 4.2 Field-level layouts (CET's view, 32-bit MSVC)

Where a row cites a sub-field assert line (for example `attackvars.pad`, `soundevent.hyperspaceHandle`, `staticCollInfo.collspheresize`, `trailStyle[3]`, `cameraDistanceVector.z`), only that sub-field is asserted. The base offset follows from CET's sub-struct layout. Offsets were re-derived for this verification with `clang++ -target i686-pc-windows-msvc` on CET's headers; every CET assert passes. **[exe 12948]** Cross-checks against the exe are in 4.1 (`StaticHeader`, `ShipStaticInfo`, `Ship` rows).

#### `Universe` - CET size 0x65C4 (26052 B); HW1 counterpart: `Game/Universe.h` `Universe`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `struct CommandLayer mainCommandLayer` |  | yes |
| 0xC | `CameraCommand mainCameraCommand` |  | yes |
| 0x9C | `struct LinkedList RenderList` |  | yes |
| 0xA8 | `struct LinkedList MinorRenderList` |  | yes |
| 0xB4 | `BOOL dontUpdateRenderList` |  | yes |
| 0xB8 | `sdword unknown1` |  | **no (unknown pad)** |
| 0xBC | `real32 phystimeelapsed` | yes (CET/game/Game/universe.h:234) | yes |
| 0xC0 | `real32 totaltimeelapsed` | yes (CET/game/Game/universe.h:235) | yes |
| 0xC4 | `real32 realtimeelapsed` | yes (CET/game/Game/universe.h:236) | **no (Cataclysm/CET)** |
| 0xC8 | `real32 radius` | yes (CET/game/Game/universe.h:237) | yes |
| 0xCC | `struct LinkedList SpaceObjList` |  | yes |
| 0xD8 | `struct LinkedList MinorSpaceObjList` |  | yes |
| 0xE4 | `struct LinkedList effectList` | yes (CET/game/Game/universe.h:238) | yes |
| 0xF0 | `struct LinkedList ShipList` | yes (CET/game/Game/universe.h:239) | yes |
| 0xFC | `struct LinkedList BulletList` |  | yes |
| 0x108 | `struct LinkedList ResourceList` |  | yes |
| 0x114 | `struct LinkedList DerelictList` |  | yes |
| 0x120 | `struct LinkedList ImpactableList` |  | yes |
| 0x12C | `struct LinkedList MissileList` |  | yes |
| 0x138 | `struct LinkedList MineFormationList` |  | yes |
| 0x144 | `struct LinkedList DeleteResourceList` |  | yes |
| 0x150 | `struct LinkedList DeleteDerelictList` |  | yes |
| 0x15C | `struct LinkedList DeleteShipList` |  | yes |
| 0x168 | `struct LinkedList DeleteMissileList` |  | yes |
| 0x174 | `struct LinkedList collBlobList` |  | yes |
| 0x180 | `struct LinkedList RetreatList` |  | yes |
| 0x18C | `struct LinkedList ResourceVolumeList` |  | yes |
| 0x198 | `GrowSelection HousekeepShipList` | yes (CET/game/Game/universe.h:240) | yes |
| 0x1A0 | `GrowSelection unk1` | yes (CET/game/Game/universe.h:241) | **no (unknown pad)** |
| 0x1A8 | `Star3dInfo * star3dinfo` | yes (CET/game/Game/universe.h:242) | yes |
| 0x1AC | `udword univUpdateCounter` | yes (CET/game/Game/universe.h:243) | yes |
| 0x1B0 | `color backgroundColor` |  | yes |
| 0x1B4 | `uword resourceNumber` |  | yes |
| 0x1B6 | `uword derelictNumber` |  | yes |
| 0x1B8 | `uword shipNumber` |  | yes |
| 0x1BA | `uword missileNumber` |  | yes |
| 0x1BC | `real32 wintime` | yes (CET/game/Game/universe.h:244) | yes |
| 0x1C0 | `real32 quittime` | yes (CET/game/Game/universe.h:245) | yes |
| 0x1C4 | `udword lasttimeadded` | yes (CET/game/Game/universe.h:246) | yes |
| 0x1C8 | `uword[145] shipMaxUnits` |  | yes |
| 0x2EA | `sbyte[8] aiplayerEnemy` | yes (CET/game/Game/universe.h:247) | yes |
| 0x2F4 | `BOOL aiplayerProcessing` | yes (CET/game/Game/universe.h:248) | yes |
| 0x2F8 | `sdword CapitalShipCaptured` | yes (CET/game/Game/universe.h:249) | yes |
| 0x2FC | `sdword PlayerWhoWon` | yes (CET/game/Game/universe.h:250) | yes |
| 0x300 | `sdword bounties` | yes (CET/game/Game/universe.h:251) | yes |
| 0x304 | `sdword bountySize` | yes (CET/game/Game/universe.h:252) | yes |
| 0x308 | `struct GameStats gameStats` | yes (CET/game/Game/universe.h:253; `gameStats.universeRUWorth` == 0x328 at :254) | yes |
| 0x48F0 | `BYTE[504] unk3` |  | **no (unknown pad)** |
| 0x4AE8 | `BOOL DerelictTech` |  | yes |
| 0x4AEC | `real32 crateTimer` |  | yes |
| 0x4AF0 | `udword numCratesInWorld` | yes (CET/game/Game/universe.h:255) | yes |
| 0x4AF4 | `bool32 collUpdateAllBlobs` |  | yes |
| 0x4AF8 | `uword numPlayers` | yes (CET/game/Game/universe.h:256) | yes |
| 0x4AFA | `uword curPlayerIndex` |  | yes |
| 0x4AFC | `Player * curPlayerPtr` |  | yes |
| 0x4B00 | `Player[9] players` | yes (CET/game/Game/universe.h:257) | yes |
| 0x65B8 | `Derelict *[3] world` |  | yes |

**[exe 12948]** `players` really spans 0x4B00-0x3DA30 (9 x 0x6530), so the `world` row and the 0x65C4 size above are CET-only (4.1 `Universe` row).

#### `Player` - CET size 0x2F8 (760 B); HW1 counterpart: `Game/Universe.h` `Player`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `Ship * PlayerMothership` | yes (CET/game/Game/universe.h:138) | yes |
| 0x4 | `sdword resourceUnits` | yes (CET/game/Game/universe.h:139) | yes |
| 0x8 | `udword unk` |  | **no (unknown pad)** |
| 0xC | `sdword supportUnitsMax` | yes (CET/game/Game/universe.h:140) | **no (Cataclysm/CET)** |
| 0x10 | `sdword supportUnitsUsed` | yes (CET/game/Game/universe.h:141) | **no (Cataclysm/CET)** |
| 0x14 | `ShipRace race` | yes (CET/game/Game/universe.h:142) | yes |
| 0x18 | `PlayerState playerState` |  | yes |
| 0x1C | `struct AIPlayer * aiPlayer` | yes (CET/game/Game/universe.h:143) | yes |
| 0x20 | `udword autoLaunch` |  | yes |
| 0x24 | `uword playerIndex` | yes (CET/game/Game/universe.h:144) | yes |
| 0x26 | `ubyte sensorLevel` |  | yes |
| 0x27 | `ubyte bounty` |  | yes |
| 0x28 | `real32 timeMoShipAttacked` | yes (CET/game/Game/universe.h:145) | yes |
| 0x2C | `sdword totalships` | yes (CET/game/Game/universe.h:146) | yes |
| 0x30 | `sdword[145] shiptotals` | yes (CET/game/Game/universe.h:147) | yes |
| 0x274 | `sdword[12] classtotals` | yes (CET/game/Game/universe.h:148) | yes |
| 0x2A4 | `PlayerResearchInfo researchinfo` |  | yes |
| 0x2EC | `uword Allies` |  | yes |
| 0x2EE | `uword AllianceProposals` |  | yes |
| 0x2F0 | `udword AllianceProposalsTimeout` |  | yes |
| 0x2F4 | `sdword initialShipCost` |  | yes |

**[exe 12948]** Real size 0x6530 (stride evidence in 4.1). All of CET's asserted rows lie in 0x0-0x2A4. The rows from 0x2A4 on (`researchinfo`..`initialShipCost`) are unasserted HW1 positions, and the exe contradicts at least two of them: `AllianceProposals`/`AllianceProposalsTimeout` are really at +0x64F6/+0x64F8.

#### `SpaceObj` - CET size 0x80 (128 B); HW1 counterpart: `Game/SpaceObj.h` `SpaceObj`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `struct Node objlink` |  | yes |
| 0x10 | `ObjType objtype` |  | yes |
| 0x14 | `udword flags` |  | yes |
| 0x18 | `ubyte[8] unk1` |  | **no (unknown pad)** |
| 0x20 | `ShipStaticInfo * staticinfo` |  | yes |
| 0x24 | `struct Node renderlink` |  | yes |
| 0x34 | `ubyte currentLOD` | yes (CET/game/Game/spaceobj.h:1214) | yes |
| 0x35 | `ubyte renderedLODs` |  | yes |
| 0x36 | `uword attributes` |  | yes |
| 0x38 | `sword attributesParam` |  | yes |
| 0x3A | `ubyte[2] attributesPad` |  | yes |
| 0x3C | `udword unknown2` |  | **no (unknown pad)** |
| 0x40 | `real32 cameraDistanceSquared` | yes (CET/game/Game/spaceobj.h:1215) | yes |
| 0x44 | `vector cameraDistanceVector` | yes (CET/game/Game/spaceobj.h:1218) | yes |
| 0x50 | `real32 collOptimizeDist` |  | yes |
| 0x54 | `struct blob * collMyBlob` |  | yes |
| 0x58 | `PosInfo posinfo` | yes (CET/game/Game/spaceobj.h:1219) | yes |

#### `Ship` - CET size 0x660 (1632 B); HW1 counterpart: `Game/SpaceObj.h` `Ship`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `struct Node objlink` |  | yes |
| 0x10 | `ObjType objtype` | yes (CET/game/Game/spaceobj.h:1808) | yes |
| 0x14 | `udword flags` | yes (CET/game/Game/spaceobj.h:1809) | yes |
| 0x18 | `udword unk_18` |  | **no (unknown pad)** |
| 0x1C | `udword unk_1C` |  | **no (unknown pad)** |
| 0x20 | `ShipStaticInfo * staticinfo` | yes (CET/game/Game/spaceobj.h:1810) | yes |
| 0x24 | `struct Node renderlink` | yes (CET/game/Game/spaceobj.h:1811) | yes |
| 0x34 | `ubyte currentLOD` | yes (CET/game/Game/spaceobj.h:1812) | yes |
| 0x35 | `ubyte renderedLODs` | yes (CET/game/Game/spaceobj.h:1813) | yes |
| 0x36 | `uword attributes` |  | yes |
| 0x38 | `sword attributesParam` |  | yes |
| 0x3A | `ubyte[2] attributesPad` |  | yes |
| 0x3C | `udword unknown2` |  | **no (unknown pad)** |
| 0x40 | `real32 cameraDistanceSquared` | yes (CET/game/Game/spaceobj.h:1814) | yes |
| 0x44 | `vector cameraDistanceVector` | yes (CET/game/Game/spaceobj.h:1817) | yes |
| 0x50 | `real32 collOptimizeDist` |  | yes |
| 0x54 | `struct blob * collMyBlob` | yes (CET/game/Game/spaceobj.h:1818) | yes |
| 0x58 | `PosInfo posinfo` |  | yes |
| 0x80 | `RotInfo rotinfo` |  | yes |
| 0xBC | `CollInfo collInfo` |  | yes |
| 0x148 | `struct Node impactablelink` |  | yes |
| 0x158 | `real32 health` |  | yes |
| 0x15C | `real32 flashtimer` |  | yes |
| 0x160 | `real32 lasttimecollided` |  | yes |
| 0x164 | `udword unk_164` |  | **no (unknown pad)** |
| 0x168 | `udword unk_168` |  | **no (unknown pad)** |
| 0x16C | `udword unk_16C` |  | **no (unknown pad)** |
| 0x170 | `real32 buildProgress` | yes (CET/game/Game/spaceobj.h:1819) | **no (Cataclysm/CET)** |
| 0x174 | `NonStats nonstatvars` |  | yes |
| 0x1B0 | `ClampInfo * clampInfo` |  | yes |
| 0x1B4 | `SalvageInfo * salvageInfo` |  | yes |
| 0x1B8 | `ubyte aistate` |  | yes |
| 0x1B9 | `ubyte[2] pad1` |  | yes |
| 0x1BB | `sbyte putOnDoor` | **no; commented-out assert says 0x1C3** (CET/game/Game/spaceobj.h:1820) | yes |
| 0x1BC | `struct Ship * dockingship` | **no; commented-out assert says 0x1C4** (CET/game/Game/spaceobj.h:1821) | yes |
| 0x1C0 | `udword specialFlags` | **no; commented-out assert says 0x1C8** (CET/game/Game/spaceobj.h:1822) | yes |
| 0x1C4 | `udword specialFlags2` |  | yes |
| 0x1C8 | `sdword[8] salvageNumTagged` |  | yes |
| 0x1E8 | `NAVLightInfo * navLightInfo` |  | yes |
| 0x1EC | `ubyte[80] unk2` |  | **no (unknown pad)** |
| 0x23C | `struct Node shiplink` | yes (CET/game/Game/spaceobj.h:1823) | yes |
| 0x24C | `vector moveTo` |  | yes |
| 0x258 | `vector moveFrom` |  | yes |
| 0x264 | `vector formationOffset` |  | yes |
| 0x270 | `vector formationHeading` |  | yes |
| 0x27C | `vector engineOffset` |  | yes |
| 0x288 | `vector enginePosition` | yes (CET/game/Game/spaceobj.h:1824) | yes |
| 0x294 | `struct Ship * rowGetOutOfWay` |  | yes |
| 0x298 | `vector rowOriginalPoint` |  | yes |
| 0x2A4 | `vector rowOutOfWayPoint` |  | yes |
| 0x2B0 | `ShipType shiptype` | yes (CET/game/Game/spaceobj.h:1825) | yes |
| 0x2B4 | `ShipRace shiprace` | yes (CET/game/Game/spaceobj.h:1826) | yes |
| 0x2B8 | `TacticsType tacticstype` | yes (CET/game/Game/spaceobj.h:1827) | yes |
| 0x2BC | `bool32 isDodging` | yes (CET/game/Game/spaceobj.h:1828) | yes |
| 0x2C0 | `real32 DodgeTime` | yes (CET/game/Game/spaceobj.h:1829) | yes |
| 0x2C4 | `uword DodgeDir` |  | yes |
| 0x2C6 | `uword pad2` |  | yes |
| 0x2C8 | `bool32 tacticsFormationVar1` | yes (CET/game/Game/spaceobj.h:1830) | yes |
| 0x2CC | `udword DodgeFlag` |  | yes |
| 0x2D0 | `sdword tactics_ordertype` | yes (CET/game/Game/spaceobj.h:1831) | yes |
| 0x2D4 | `udword tacticsTalk` | yes (CET/game/Game/spaceobj.h:1832) | yes |
| 0x2D8 | `sdword tacticsNeedToFlightMan` | yes (CET/game/Game/spaceobj.h:1833) | yes |
| 0x2DC | `sdword dockwaitforNorm` | yes (CET/game/Game/spaceobj.h:1834) | yes |
| 0x2E0 | `real32 timeCreated` | yes (CET/game/Game/spaceobj.h:1835) | yes |
| 0x2E4 | `sdword kamikazeState` |  | yes |
| 0x2E8 | `vector kamikazeVector` |  | yes |
| 0x2F4 | `real32 speedBurstTime` |  | yes |
| 0x2F8 | `real32 singlePlayerSpeedLimiter` |  | yes |
| 0x2FC | `real32 deathtime` |  | yes |
| 0x300 | `udword madAnimationFlags` | yes (CET/game/Game/spaceobj.h:1836) | yes |
| 0x304 | `udword madGunStatus` | yes (CET/game/Game/spaceobj.h:1837) | yes |
| 0x308 | `udword madWingStatus` | yes (CET/game/Game/spaceobj.h:1838) | yes |
| 0x30C | `udword madDoorStatus` | yes (CET/game/Game/spaceobj.h:1839) | yes |
| 0x310 | `udword madSpecialStatus` | yes (CET/game/Game/spaceobj.h:1840) | yes |
| 0x314 | `udword cuedAnimationIndex` | yes (CET/game/Game/spaceobj.h:1841) | yes |
| 0x318 | `udword cuedAnimationType` | yes (CET/game/Game/spaceobj.h:1842) | yes |
| 0x31C | `udword nextAnim` |  | yes |
| 0x320 | `udword shaneyPooSoundEventAnimationTypeFlag` |  | **no (Cataclysm/CET)** |
| 0x324 | `sdword forceCancelDock` |  | yes |
| 0x328 | `real32 gravwellTimeEffect` |  | yes |
| 0x32C | `struct Player * playerowner` | yes (CET/game/Game/spaceobj.h:1843) | yes |
| 0x330 | `sdword resources` |  | yes |
| 0x334 | `real32 lastTimeRepaired` |  | yes |
| 0x338 | `sdword unk_338` |  | **no (unknown pad)** |
| 0x33C | `real32 magnitudeSquared` |  | yes |
| 0x340 | `struct Ship * gettingrocked` | yes (CET/game/Game/spaceobj.h:1844) | yes |
| 0x344 | `struct Ship * recentAttacker` |  | yes |
| 0x348 | `struct Ship * firingAtUs` |  | yes |
| 0x34C | `bool8 shipidle` | yes (CET/game/Game/spaceobj.h:1845) | yes |
| 0x34D | `bool8 autostabilizeship` | yes (CET/game/Game/spaceobj.h:1846) | yes |
| 0x34E | `ubyte autostabilizestate` |  | yes |
| 0x34F | `ubyte autostabilizewait` |  | yes |
| 0x350 | `ubyte aistateattack` |  | yes |
| 0x351 | `ubyte flightmanState1` |  | yes |
| 0x352 | `ubyte flightmanState2` |  | yes |
| 0x353 | `ubyte rcstate1` |  | yes |
| 0x354 | `ubyte rcstate2` |  | yes |
| 0x355 | `ubyte aistatecommand` |  | yes |
| 0x356 | `sbyte dontrotateever` |  | yes |
| 0x357 | `sbyte dontapplyforceever` |  | yes |
| 0x358 | `bool8 shipisattacking` |  | yes |
| 0x359 | `ubyte colorScheme` |  | yes |
| 0x35A | `ubyte recentlyAttacked` |  | yes |
| 0x35B | `ubyte recentlyFiredUpon` |  | yes |
| 0x35C | `ubyte chatterBits` |  | yes |
| 0x35D | `ubyte rowState` |  | yes |
| 0x360 | `real32 aidescend` |  | yes |
| 0x364 | `udword flightman` |  | yes |
| 0x368 | `void * flightmanInfo` |  | yes |
| 0x36C | `struct CommandToDo * formationcommand` |  | yes |
| 0x370 | `struct CommandToDo * command` | yes (CET/game/Game/spaceobj.h:1847) | yes |
| 0x374 | `SpecificAttackVars attackvars` | yes (CET/game/Game/spaceobj.h:1855) | yes |
| 0x38C | `sdword unk_38c` |  | **no (unknown pad)** |
| 0x390 | `udword unk_390` |  | **no (unknown pad)** |
| 0x394 | `udword unk_394` |  | **no (unknown pad)** |
| 0x398 | `udword unk_398` |  | **no (unknown pad)** |
| 0x39C | `sdword howDidIDie` | yes (CET/game/Game/spaceobj.h:1856) | yes |
| 0x3A0 | `sdword whoKilledMe` |  | yes |
| 0x3A4 | `udword nDamageLights` | yes (CET/game/Game/spaceobj.h:1857) | yes |
| 0x3A8 | `DamageLightStatic * damageLights` | yes (CET/game/Game/spaceobj.h:1858) | yes |
| 0x3AC | `struct Effect *[3][3] showingDamage` | yes (CET/game/Game/spaceobj.h:1859) | yes |
| 0x3D0 | `udword unk_3D0` |  | **no (unknown pad)** |
| 0x3D4 | `udword unk_3D4` |  | **no (unknown pad)** |
| 0x3D8 | `udword unk_3D8` |  | **no (unknown pad)** |
| 0x3DC | `real32 damageModifier` | yes (CET/game/Game/spaceobj.h:1860) | yes |
| 0x3E0 | `udword unk_3E0` |  | **no (unknown pad)** |
| 0x3E4 | `udword unk_3E4` |  | **no (unknown pad)** |
| 0x3E8 | `real32 buildTimerElapsed` | yes (CET/game/Game/spaceobj.h:1861) | **no (Cataclysm/CET)** |
| 0x3EC | `udword unk_3EC` |  | **no (unknown pad)** |
| 0x3F0 | `udword unk_3F0` |  | **no (unknown pad)** |
| 0x3F4 | `udword unk_3F4` |  | **no (unknown pad)** |
| 0x3F8 | `udword unk_3F8` |  | **no (unknown pad)** |
| 0x3FC | `udword unk_3FC` |  | **no (unknown pad)** |
| 0x400 | `sdword[4] tractorPosinfo` | yes (CET/game/Game/spaceobj.h:1862) | yes |
| 0x410 | `udword unk_410` |  | **no (unknown pad)** |
| 0x41C | `sdword needNewLeader` | yes (CET/game/Game/spaceobj.h:1863) | yes |
| 0x420 | `struct Ship * newDockWithTransfer` |  | yes |
| 0x424 | `sdword newDockIndexTransfer` |  | yes |
| 0x428 | `real32 shipDeCloakTime` | yes (CET/game/Game/spaceobj.h:1864) | yes |
| 0x42C | `shiptrail *[4] trail` | yes (CET/game/Game/spaceobj.h:1865) | yes |
| 0x43C | `struct ShipID shipID` |  | yes |
| 0x43E | `uword hotKeyGroup` | yes (CET/game/Game/spaceobj.h:1873) | yes |
| 0x440 | `sdword retaliateSpeechVar` |  | yes |
| 0x444 | `real32 retaliateSpeechTime` |  | yes |
| 0x448 | `SOUNDEVENT soundevent` | yes (CET/game/Game/spaceobj.h:1866) | yes |
| 0x490 | `GunInfo * gunInfo` |  | yes |
| 0x494 | `SpecificDockVars dockvars` | yes (CET/game/Game/spaceobj.h:1867) | yes |
| 0x4B4 | `ShipsInsideMe * shipsInsideMe` | yes (CET/game/Game/spaceobj.h:1868) | yes |
| 0x4B8 | `shipbindings * bindings` | yes (CET/game/Game/spaceobj.h:1869) | yes |
| 0x4BC | `madanim * madBindings` | yes (CET/game/Game/spaceobj.h:1870) | yes |
| 0x4C0 | `struct RepairDroid * pRepairDroids` | yes (CET/game/Game/spaceobj.h:1871) | yes |
| 0x4C4 | `struct ShipSinglePlayerGameInfo * shipSinglePlayerGameInfo` | yes (CET/game/Game/spaceobj.h:1872) | yes |
| 0x5B4 | `struct Timer buildTimer` | yes (CET/game/Game/spaceobj.h:1874) | **no (Cataclysm/CET)** |
| 0x650 | `BeastSystemType beastSystemType` | yes (CET/game/Game/spaceobj.h:1875) | **no (Cataclysm/CET)** |
| 0x654 | `udword beastUnk` | yes (CET/game/Game/spaceobj.h:1876) | **no (Cataclysm/CET)** |
| 0x658 | `udword sizeofShipSpecifics` |  | yes |
| 0x65C | `udword[1] ShipSpecifics` |  | yes |
| 0x414-0x418, 0x4C8-0x5B0, 0x5C4-0x64C | `udword unk_*` runs (elided) | | **no (unknown)** |

#### `StaticHeader` - CET size 0x98 (152 B); HW1 counterpart: `Game/SpaceObj.h` `StaticHeader`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `lodinfo * LOD` | yes (CET/game/Game/spaceobj.h:422) | yes |
| 0x4 | `void * pMexData` | yes (CET/game/Game/spaceobj.h:423) | yes |
| 0x8 | `real32 mass` | yes (CET/game/Game/spaceobj.h:424) | yes |
| 0xC | `real32 momentOfInertiaX` | yes (CET/game/Game/spaceobj.h:425) | yes |
| 0x10 | `real32 momentOfInertiaY` | yes (CET/game/Game/spaceobj.h:426) | yes |
| 0x14 | `real32 momentOfInertiaZ` | yes (CET/game/Game/spaceobj.h:427) | yes |
| 0x18 | `real32 oneOverMass` |  | yes |
| 0x1C | `real32 oneOverMomentOfInertiaX` |  | yes |
| 0x20 | `real32 oneOverMomentOfInertiaY` |  | yes |
| 0x24 | `real32 oneOverMomentOfInertiaZ` |  | yes |
| 0x28 | `real32 maxvelocity` | yes (CET/game/Game/spaceobj.h:428) | yes |
| 0x2C | `real32 maxrot` | yes (CET/game/Game/spaceobj.h:429) | yes |
| 0x30 | `ubyte infoFlags` |  | yes |
| 0x31 | `bool8 immobile` | yes (CET/game/Game/spaceobj.h:430) | yes |
| 0x32 | `sbyte rightOfWay` | yes (CET/game/Game/spaceobj.h:431) | yes |
| 0x34 | `StaticCollInfo staticCollInfo` | yes (CET/game/Game/spaceobj.h:434) | yes |

#### `ShipStaticInfo` - CET size 0x4CC (1228 B); HW1 counterpart: `Game/SpaceObj.h` `ShipStaticInfo`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `StaticHeader staticheader` |  | yes |
| 0x98 | `BYTE[12] unk` |  | **no (unknown pad)** |
| 0xA4 | `real32 maxhealth` | yes (CET/game/Game/spaceobj.h:760) | yes |
| 0xA8 | `real32 oneOverMaxHealth` |  | yes |
| 0xAC | `real32 minCollDamage` | yes (CET/game/Game/spaceobj.h:761) | yes |
| 0xB0 | `real32 maxCollDamage` | yes (CET/game/Game/spaceobj.h:762) | yes |
| 0xB4 | `real32[6] thruststrengthstat` | yes (CET/game/Game/spaceobj.h:768) | yes |
| 0xCC | `real32[6] rotstrengthstat` | yes (CET/game/Game/spaceobj.h:774) | yes |
| 0xE4 | `real32[3] turnspeedstat` | yes (CET/game/Game/spaceobj.h:777) | yes |
| 0xF0 | `bool8[8] teamColor` |  | yes |
| 0xF8 | `SalvageStaticInfo * salvageStaticInfo` |  | yes |
| 0xFC | `real32 sinbank` |  | yes |
| 0x100 | `real32 pitchturn` |  | yes |
| 0x104 | `real32 pitchdescend` |  | yes |
| 0x108 | `NAVLightStaticInfo * navlightStaticInfo` |  | yes |
| 0x10C | `CustShipHeader custshipheader` |  | yes |
| 0x154 | `void * custstatinfo` |  | yes |
| 0x158 | `BYTE[64] unk2` |  | **no (unknown pad)** |
| 0x198 | `real32[3] bulletRangeSquared` |  | yes |
| 0x1A4 | `real32[3] bulletRange` | yes (CET/game/Game/spaceobj.h:780) | yes |
| 0x1B0 | `real32[3] minBulletRange` | yes (CET/game/Game/spaceobj.h:783) | yes |
| 0x1BC | `real32 blastRadiusShockWave` | yes (CET/game/Game/spaceobj.h:784) | yes |
| 0x1C0 | `real32 blastRadiusDamage` | yes (CET/game/Game/spaceobj.h:785) | yes |
| 0x1C4 | `real32 passiveRetaliateZone` | yes (CET/game/Game/spaceobj.h:786) | yes |
| 0x1C8 | `real32[6] collSideModifiers` | yes (CET/game/Game/spaceobj.h:792) | yes |
| 0x1E0 | `ShipType shiptype` |  | yes |
| 0x1E4 | `ShipRace shiprace` |  | yes |
| 0x1E8 | `ShipClass shipclass` | yes (CET/game/Game/spaceobj.h:793) | yes |
| 0x1EC | `udword unk3` |  | **no (unknown pad)** |
| 0x1F0 | `bool32 externalBuild` | yes (CET/game/Game/spaceobj.h:794) | **no (Cataclysm/CET)** |
| 0x1F4 | `real32 forceFieldProtectionDistanceScale` | yes (CET/game/Game/spaceobj.h:795) | **no (Cataclysm/CET)** |
| 0x1F8 | `real32 upgradeTimeModifier` | yes (CET/game/Game/spaceobj.h:796) | **no (Cataclysm/CET)** |
| 0x1FC | `bool8 canBuildShips` |  | yes |
| 0x1FD | `bool8 canReceiveShips` | yes (CET/game/Game/spaceobj.h:797) | yes |
| 0x1FE | `bool8 canReceiveResources` | yes (CET/game/Game/spaceobj.h:798) | yes |
| 0x1FF | `bool8 canReceiveShipsPermanently` | yes (CET/game/Game/spaceobj.h:799) | yes |
| 0x200 | `bool8 canReceiveShipsForRetire` | yes (CET/game/Game/spaceobj.h:800) | yes |
| 0x201 | `bool8 canReceiveSomething` |  | yes |
| 0x202 | `bool8 canBuildBigShips` | yes (CET/game/Game/spaceobj.h:801) | yes |
| 0x203 | `bool8 canTargetMultipleTargets` | yes (CET/game/Game/spaceobj.h:802) | yes |
| 0x204 | `bool8 rotateToRetaliate` | yes (CET/game/Game/spaceobj.h:803) | yes |
| 0x205 | `bool8[4] canReceiveTheseShips` | yes (CET/game/Game/spaceobj.h:807) | yes |
| 0x20C | `bool32 passiveAttackPenaltyExempt` | yes (CET/game/Game/spaceobj.h:808) | yes |
| 0x210 | `bool8 specialActivateIsContinuous` | yes (CET/game/Game/spaceobj.h:809) | yes |
| 0x211 | `bool8 canSpecialBandBoxFriendlies` | yes (CET/game/Game/spaceobj.h:810) | yes |
| 0x212 | `bool8 unk4` |  | **no (unknown pad)** |
| 0x213 | `bool8 unk5` |  | **no (unknown pad)** |
| 0x214 | `bool8 canSingleClickSpecialActivate` | yes (CET/game/Game/spaceobj.h:811) | yes |
| 0x216 | `sword maxDockableFighters` | yes (CET/game/Game/spaceobj.h:812) | yes |
| 0x218 | `sword maxDockableCorvettes` | yes (CET/game/Game/spaceobj.h:813) | yes |
| 0x21C | `sdword canHandleNumShipsDocking` | yes (CET/game/Game/spaceobj.h:814) | yes |
| 0x220 | `sdword canHandleNumShipsDepositingRU` | yes (CET/game/Game/spaceobj.h:815) | yes |
| 0x224 | `_Bool repairBeamCapable` | yes (CET/game/Game/spaceobj.h:816) | yes |
| 0x228 | `real32 healthPerSecond` | yes (CET/game/Game/spaceobj.h:817) | yes |
| 0x22C | `real32 CapitalDistanceRepairStart2` | yes (CET/game/Game/spaceobj.h:818) | yes |
| 0x230 | `real32 CapitalDistanceRepairStart` | yes (CET/game/Game/spaceobj.h:819) | yes |
| 0x234 | `real32 AngleDotProdThreshold` | yes (CET/game/Game/spaceobj.h:820) | yes |
| 0x238 | `sdword buildCost` | yes (CET/game/Game/spaceobj.h:821) | yes |
| 0x23C | `sdword buildTime` | yes (CET/game/Game/spaceobj.h:822) | yes |
| 0x240 | `real32 fowRadius` | yes (CET/game/Game/spaceobj.h:823) | **no (Cataclysm/CET)** |
| 0x244 | `sdword svManeuverability` | yes (CET/game/Game/spaceobj.h:824) | yes |
| 0x248 | `sdword svCoverage` | yes (CET/game/Game/spaceobj.h:825) | yes |
| 0x24C | `sdword svFirePower` | yes (CET/game/Game/spaceobj.h:826) | yes |
| 0x250 | `real32 RKOEatMeTime` | yes (CET/game/Game/spaceobj.h:827) | **no (Cataclysm/CET)** |
| 0x254 | `sdword resourcesAtOneTime` | yes (CET/game/Game/spaceobj.h:828) | yes |
| 0x258 | `sdword maxresources` | yes (CET/game/Game/spaceobj.h:829) | yes |
| 0x25C | `sdword harvestRate` | yes (CET/game/Game/spaceobj.h:830) | yes |
| 0x260 | `sdword harvestAmount` | yes (CET/game/Game/spaceobj.h:831) | yes |
| 0x264 | `sdword groupSize` | yes (CET/game/Game/spaceobj.h:832) | yes |
| 0x268 | `real32 maxfuel` |  | yes |
| 0x26C | `real32 lowfuelpoint` |  | yes |
| 0x270 | `real32 fuelburnrate` |  | yes |
| 0x274 | `real32 attackfuelburnrate` |  | yes |
| 0x278 | `real32 repairTime` | yes (CET/game/Game/spaceobj.h:833) | yes |
| 0x27C | `real32 repairDamage` | yes (CET/game/Game/spaceobj.h:834) | yes |
| 0x280 | `real32 repairCombatTime` | yes (CET/game/Game/spaceobj.h:835) | yes |
| 0x284 | `real32 repairCombatDamage` | yes (CET/game/Game/spaceobj.h:836) | yes |
| 0x288 | `real32 pumpFuelRate` |  | yes |
| 0x28C | `real32 repairOtherShipRate` | yes (CET/game/Game/spaceobj.h:837) | yes |
| 0x290 | `real32 repairResourceCollectorRate` | yes (CET/game/Game/spaceobj.h:838) | yes |
| 0x294 | `real32 clearanceDistance` | yes (CET/game/Game/spaceobj.h:839) | yes |
| 0x298 | `udword clearanceDirection` | yes (CET/game/Game/spaceobj.h:840) | yes |
| 0x29C | `real32 dockShipRange` | yes (CET/game/Game/spaceobj.h:841) | yes |
| 0x2A0 | `real32 formationPaddingModifier` | yes (CET/game/Game/spaceobj.h:842) | yes |
| 0x2A4 | `bool32 cannotForceAttackIfOwnShip` | yes (CET/game/Game/spaceobj.h:843) | yes |
| 0x2A8 | `bool32 shipIsCapital` | yes (CET/game/Game/spaceobj.h:844) | yes |
| 0x2AC | `bool32 cantMoveAndAttack` | yes (CET/game/Game/spaceobj.h:845) | yes |
| 0x2B0 | `BYTE[92] unk6` |  | **no (unknown pad)** |
| 0x30C | `real32 nlips` | yes (CET/game/Game/spaceobj.h:846) | **no (Cataclysm/CET)** |
| 0x310 | `BYTE[8] unk7` |  | **no (unknown pad)** |
| 0x318 | `real32 madGunOpenDamagedHealthThreshold` | yes (CET/game/Game/spaceobj.h:847) | yes |
| 0x31C | `real32 madWingOpenDamagedHealthThreshold` | yes (CET/game/Game/spaceobj.h:848) | yes |
| 0x320 | `udword nRepairDroids` | yes (CET/game/Game/spaceobj.h:849) | yes |
| 0x324 | `udword[4] trailStyle` | yes (CET/game/Game/spaceobj.h:853) | yes |
| 0x334 | `real32[4] trailWidth` | yes (CET/game/Game/spaceobj.h:857) | yes |
| 0x344 | `real32[4] trailHeight` | yes (CET/game/Game/spaceobj.h:861) | yes |
| 0x354 | `real32[4] trailAngle` | yes (CET/game/Game/spaceobj.h:865) | yes |
| 0x364 | `real32[4] trailRibbonAdjust` | yes (CET/game/Game/spaceobj.h:869) | yes |
| 0x374 | `real32[4] trailLength` | yes (CET/game/Game/spaceobj.h:873) | yes |
| 0x384 | `real32[4] trailScaleCap` | yes (CET/game/Game/spaceobj.h:877) | yes |
| 0x394 | `real32[4] trailSpriteRadius` | yes (CET/game/Game/spaceobj.h:881) | yes |
| 0x3A4 | `real32[4] trailSpriteOffset` | yes (CET/game/Game/spaceobj.h:885) | yes |
| 0x3B4 | `real32 minimumZoomDistance` | yes (CET/game/Game/spaceobj.h:886) | yes |
| 0x3B8 | `real32 renderlistFade` | yes (CET/game/Game/spaceobj.h:887) | yes |
| 0x3BC | `real32 renderlistLimitSqr` | yes (CET/game/Game/spaceobj.h:888) | yes |
| 0x3C0 | `real32 dockLightNear` | yes (CET/game/Game/spaceobj.h:889) | yes |
| 0x3C4 | `real32 dockLightFar` | yes (CET/game/Game/spaceobj.h:890) | yes |
| 0x3C8 | `color dockLightColor` | yes (CET/game/Game/spaceobj.h:891) | yes |
| 0x3CC | `color hyperspaceColor` | yes (CET/game/Game/spaceobj.h:892) | yes |
| 0x3D0 | `real32 hyperspaceBoxScale` | yes (CET/game/Game/spaceobj.h:893) | **no (Cataclysm/CET)** |
| 0x3D4 | `real32 leechRUsPerSecond` | yes (CET/game/Game/spaceobj.h:894) | **no (Cataclysm/CET)** |
| 0x3D8 | `real32 leechHPsPerSecond` | yes (CET/game/Game/spaceobj.h:895) | **no (Cataclysm/CET)** |
| 0x3DC | `real32 beastAttackCascadeCost` | yes (CET/game/Game/spaceobj.h:896) | **no (Cataclysm/CET)** |
| 0x3E0 | `sdword beastAttacksRequired` | yes (CET/game/Game/spaceobj.h:897) | **no (Cataclysm/CET)** |
| 0x3E4 | `ShipType beastAttackConvertTo` | yes (CET/game/Game/spaceobj.h:898) | **no (Cataclysm/CET)** |
| 0x3E8 | `real32 Overlay_Selection_Scalar` | yes (CET/game/Game/spaceobj.h:899) | **no (Cataclysm/CET)** |
| 0x3EC | `BYTE[28] unk8` |  | **no (unknown pad)** |
| 0x408 | `bool32 bUseRayTriangleIntersectionForHitLocation` | yes (CET/game/Game/spaceobj.h:900) | **no (Cataclysm/CET)** |
| 0x40C | `real32 linkDistance` | yes (CET/game/Game/spaceobj.h:901) | **no (Cataclysm/CET)** |
| 0x410 | `udword supportUnits` | yes (CET/game/Game/spaceobj.h:902) | **no (Cataclysm/CET)** |
| 0x414 | `udword providesSU` | yes (CET/game/Game/spaceobj.h:903) | **no (Cataclysm/CET)** |
| 0x418 | `real32 RamDamage` | yes (CET/game/Game/spaceobj.h:904) | **no (Cataclysm/CET)** |
| 0x41C | `real32 cuttingBeamDamagePerSecond` | yes (CET/game/Game/spaceobj.h:905) | **no (Cataclysm/CET)** |
| 0x420 | `real32 cuttingBeamUpgradedDamagePerSecond` | yes (CET/game/Game/spaceobj.h:906) | **no (Cataclysm/CET)** |
| 0x424 | `real32 RamPushTime` | yes (CET/game/Game/spaceobj.h:907) | **no (Cataclysm/CET)** |
| 0x428 | `real32 RamWaitAfterPushTime` | yes (CET/game/Game/spaceobj.h:908) | **no (Cataclysm/CET)** |
| 0x42C | `udword unk9` |  | **no (unknown pad)** |
| 0x430 | `real32 TimeShockedByEMP` | yes (CET/game/Game/spaceobj.h:909) | **no (Cataclysm/CET)** |
| 0x434 | `real32 EMP_Range` | yes (CET/game/Game/spaceobj.h:910) | **no (Cataclysm/CET)** |
| 0x438 | `real32 EMP_Max_Combine_Distance` | yes (CET/game/Game/spaceobj.h:911) | **no (Cataclysm/CET)** |
| 0x43C | `bool32 UpgradeByDocking` | yes (CET/game/Game/spaceobj.h:912) | **no (Cataclysm/CET)** |
| 0x440 | `real32 pilotViewOffsetX` | yes (CET/game/Game/spaceobj.h:913) | **no (Cataclysm/CET)** |
| 0x444 | `real32 pilotViewOffsetY` | yes (CET/game/Game/spaceobj.h:914) | **no (Cataclysm/CET)** |
| 0x448 | `real32 pilotViewOffsetZ` | yes (CET/game/Game/spaceobj.h:915) | **no (Cataclysm/CET)** |
| 0x44C | `udword unk10` |  | **no (unknown pad)** |
| 0x450 | `udword unk11` |  | **no (unknown pad)** |
| 0x454 | `udword RKOGetsMissiles` | yes (CET/game/Game/spaceobj.h:916) | **no (Cataclysm/CET)** |
| 0x458 | `real32 tacticalSelectionScale` |  | yes |
| 0x45C | `GunStaticInfo * gunStaticInfo` |  | yes |
| 0x460 | `DockStaticInfo * dockStaticInfo` |  | yes |
| 0x464 | `DockOverideInfo * dockOverideInfo` |  | yes |
| 0x468 | `ResNozzleStatic * resNozzleStatic` |  | yes |
| 0x46C | `RepairNozzleStatic * repairNozzleStatic` |  | yes |
| 0x470 | `TractorBeamStatic * tractorEmitterStatic` |  | yes |
| 0x474 | `vector[4] engineNozzleOffset` |  | yes |
| 0x4A4 | `trailstatic *[4] trailStatic` |  | yes |
| 0x4B4 | `udword multipleEngines` |  | yes |
| 0x4B8 | `real32 scaleCap` |  | yes |
| 0x4BC | `sdword hierarchySize` |  | yes |
| 0x4C0 | `madstatic * madStatic` |  | yes |
| 0x4C4 | `_Bool (*)(struct CommandToDo *, struct Ship *, struct Ship *) ShipXDocksAtShipY` |  | yes |
| 0x4C8 | `_Bool (*)(struct Ship *, struct Ship *) LaunchShipXFromShipY` |  | yes |

#### `Bullet` - CET size 0x150 (336 B); HW1 counterpart: `Game/SpaceObj.h` `Bullet`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `struct Node objlink` |  | yes |
| 0x10 | `ObjType objtype` |  | yes |
| 0x14 | `udword flags` |  | yes |
| 0x18 | `StaticInfo * staticinfo` |  | yes |
| 0x1C | `struct Node renderlink` |  | yes |
| 0x2C | `ubyte currentLOD` |  | yes |
| 0x2D | `ubyte renderedLODs` |  | yes |
| 0x2E | `uword attributes` |  | yes |
| 0x30 | `sword attributesParam` |  | yes |
| 0x32 | `ubyte[2] attributesPad` |  | yes |
| 0x34 | `real32 cameraDistanceSquared` |  | yes |
| 0x38 | `vector cameraDistanceVector` |  | yes |
| 0x44 | `real32 collOptimizeDist` |  | yes |
| 0x48 | `struct blob * collMyBlob` |  | yes |
| 0x4C | `PosInfo posinfo` |  | yes |
| 0x74 | `RotInfo rotinfo` |  | yes |
| 0xB0 | `struct Node bulletlink` |  | yes |
| 0xC0 | `vector bulletheading` |  | yes |
| 0xCC | `vector lengthvec` |  | yes |
| 0xD8 | `real32 lengthmag` |  | yes |
| 0xDC | `real32 traveldist` |  | yes |
| 0xE0 | `real32 beamtraveldist` |  | yes |
| 0xE4 | `sdword[9] unknown` |  | **no (unknown pad)** |
| 0x108 | `BulletType bulletType` | yes (CET/game/Game/spaceobj.h:2045) | yes |
| 0x10C | `GunSoundType soundType` |  | yes |
| 0x110 | `real32 timelived` |  | yes |
| 0x114 | `real32 totallifetime` |  | yes |
| 0x118 | `sdword unknown2` |  | **no (unknown pad)** |
| 0x11C | `Ship * owner` | yes (CET/game/Game/spaceobj.h:2046) | yes |
| 0x120 | `Gun * gunowner` |  | yes |
| 0x124 | `SpaceObjRotImpTarg * target` |  | yes |
| 0x128 | `color bulletColor` |  | yes |
| 0x12C | `real32 bulletmass` |  | yes |
| 0x130 | `etglod * hitEffect` |  | yes |
| 0x134 | `Effect * effect` |  | yes |
| 0x138 | `real32 damage` |  | yes |
| 0x13C | `real32 damageFull` |  | yes |
| 0x140 | `udword SpecialEffectFlag` |  | yes |
| 0x144 | `real32 DFGFieldEntryTime` |  | yes |
| 0x148 | `real32 BulletSpeed` |  | yes |
| 0x14C | `struct Player * playerowner` |  | yes |

#### `Effect` - CET size 0x11C (284 B); HW1 counterpart: `Game/SpaceObj.h` `Effect`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `struct Node objlink` |  | yes |
| 0x10 | `ObjType objtype` |  | yes |
| 0x14 | `udword flags` |  | yes |
| 0x18 | `sdword[2] unknown` |  | **no (unknown pad)** |
| 0x20 | `StaticInfo * staticinfo` |  | yes |
| 0x24 | `struct Node renderlink` |  | yes |
| 0x34 | `ubyte currentLOD` |  | yes |
| 0x35 | `ubyte renderedLODs` |  | yes |
| 0x36 | `uword attributes` |  | yes |
| 0x38 | `sword attributesParam` |  | yes |
| 0x3A | `ubyte[2] attributesPad` |  | yes |
| 0x3C | `real32 cameraDistanceSquared` |  | yes |
| 0x40 | `vector cameraDistanceVector` |  | yes |
| 0x4C | `real32 collOptimizeDist` |  | yes |
| 0x50 | `struct blob * collMyBlob` |  | yes |
| 0x54 | `PosInfo posinfo` |  | yes |
| 0x7C | `RotInfo rotinfo` |  | yes |
| 0xB8 | `struct Node effectLink` |  | yes |
| 0xC8 | `udword effectFlags` |  | yes |
| 0xCC | `Ship * owner` |  | yes |
| 0xD0 | `real32 magnitudeSquared` |  | yes |
| 0xD4 | `sdword nSpawnedAttachedToOwner` |  | yes |
| 0xD8 | `real32 timeElapsed` |  | yes |
| 0xDC | `vector LoF` |  | yes |
| 0xE8 | `vector velocity` |  | yes |
| 0xF4 | `real32 theta` |  | yes |
| 0xF8 | `real32 drag` |  | yes |
| 0xFC | `real32 spin` |  | yes |
| 0x100 | `real32 deltaSpin` |  | yes |
| 0x104 | `ubyte * variable` |  | yes |
| 0x108 | `ubyte * variableRate` |  | yes |
| 0x10C | `ubyte * effectID` |  | yes |
| 0x110 | `sdword nParticleBlocks` |  | yes |
| 0x114 | `sdword iParticleBlock` |  | yes |
| 0x118 | `ubyte *[1] particleBlock` |  | yes |

#### `SinglePlayerGameInfo` - CET size 0x48 (72 B); HW1 counterpart: `Game/SinglePlayer.h` `SinglePlayerGameInfo`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `udword currentMission` |  | yes |
| 0x4 | `sdword onCompletePlayNIS` |  | yes |
| 0x8 | `_Bool onCompleteHyperspace` |  | yes |
| 0xC | `sdword onLoadPlayNIS` |  | yes |
| 0x10 | `HyperspaceState hyperspaceState` | yes (CET/game/Game/singleplayer.h:137) | yes |
| 0x14 | `udword oldAutoLaunch` |  | yes |
| 0x18 | `real32 oldClipPlane` |  | yes |
| 0x1C | `sdword rollcallShipsDocked` |  | yes |
| 0x20 | `sdword rollcallShipsRemaining` |  | yes |
| 0x24 | `HyperspaceSubState hyperspaceSubState` |  | yes |
| 0x28 | `real32 hyperspaceTimeStamp` |  | yes |
| 0x2C | `struct LinkedList ShipsInHyperspace` |  | yes |
| 0x38 | `sdword resourceUnitsCollected` |  | yes |
| 0x3C | `GrowSelection ShipsToHyperspace` |  | yes |
| 0x44 | `_Bool giveComputerFleetControl` |  | yes |
| 0x45 | `_Bool asteroid0sCanMove` |  | yes |
| 0x46 | `_Bool playerCanHyperspace` |  | yes |
| 0x47 | `_Bool hyperspaceFails` |  | yes |

#### `AIPlayer` - CET size 0xD68 (3432 B); HW1 counterpart: `Game/AIPlayer.h` `AIPlayer`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `Player * player` |  | yes |
| 0x4 | `AIPlayerLevel aiplayerDifficultyLevel` | yes (CET/game/Game/AIPlayer.h:200) | yes |
| 0x8 | `udword ResourceFeatures` | yes (CET/game/Game/AIPlayer.h:201) | yes |
| 0xC | `udword AttackFeatures` | yes (CET/game/Game/AIPlayer.h:202) | yes |
| 0x10 | `udword DefenseFeatures` | yes (CET/game/Game/AIPlayer.h:203) | yes |
| 0x14 | `udword aiplayerUpdateRate` |  | yes |
| 0x18 | `udword aiplayerFrameCount` |  | yes |
| 0x1C | `udword enemyPlayerCount` |  | yes |
| 0x20 | `Player * primaryEnemyPlayer` |  | yes |
| 0x24 | `GrowSelection newships` |  | yes |
| 0x2C | `GrowSelection[145] enemyShipsIAmAwareOf` |  | yes |
| 0x4B4 | `GrowSelection[145] primaryEnemyShipsIAmAwareOf` |  | yes |
| 0x93C | `sdword NumRUDockPoints` |  | yes |
| 0x940 | `sdword NumRCollectorsBeingBuilt` |  | yes |
| 0x944 | `sdword NumRControllersBeingBuilt` |  | yes |
| 0x948 | `sdword NumASFBeingBuilt` |  | yes |
| 0x94C | `sdword NumResearchShipsBeingBuilt` |  | yes |
| 0x950 | `ShipPtr ScriptCreator` |  | yes |
| 0x954 | `ShipPtr AICreator` |  | yes |
| 0x958 | `sdword NumRUsSpentOnResourceman` |  | yes |
| 0x95C | `sdword NumRUsSpentOnAttman` |  | yes |
| 0x960 | `sdword NumRUsSpentOnDefman` |  | yes |
| 0x964 | `sdword NumRUsSpentOnScriptman` |  | yes |
| 0x968 | `struct RequestShips ResourceManRequestShips` |  | yes |
| 0x988 | `struct LinkedList AttackManRequestShipsQ` |  | yes |
| 0x994 | `struct LinkedList DefenseManRequestShipsQ` |  | yes |
| 0x9A0 | `struct LinkedList ScriptManRequestShipsQ` |  | yes |
| 0x9AC | `struct LinkedList AttackManTeamsWaitingForShipsQ` |  | yes |
| 0x9B8 | `struct LinkedList DefenseManTeamsWaitingForShipsQ` |  | yes |
| 0x9C4 | `struct LinkedList ScriptManTeamsWaitingForShipsQ` |  | yes |
| 0x9D0 | `struct ShipTypesBeingBuilt AttackManShipsBeingBuilt` |  | yes |
| 0xA61 | `struct ShipTypesBeingBuilt DefenseManShipsBeingBuilt` |  | yes |
| 0xAF2 | `struct ShipTypesBeingBuilt ScriptManShipsBeingBuilt` |  | yes |
| 0xB84 | `udword TechnologyDeficit` |  | yes |
| 0xB88 | `ubyte ResourcersToBuild` |  | yes |
| 0xB89 | `ubyte ResearchersToBuild` |  | yes |
| 0xB8A | `ubyte ResearchDelay` |  | yes |
| 0xB8B | `_Bool firstTurn` |  | yes |
| 0xB8C | `_Bool recalculateAllies` |  | yes |
| 0xB90 | `sdword aifHyperSavings` |  | yes |

#### `AISTeamEntry` - CET size 0x9D10 (40208 B); HW1 counterpart: none (Cataclysm-only)

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `AISTeamSelection[10] selection` |  | - |
| 0x9D08 | `udword numselections` | yes (CET/game/Game/AISkirmish.h:129) | - |
| 0x9D0C | `sdword totalteamsize` | yes (CET/game/Game/AISkirmish.h:130) | - |

#### `AISTeamSelection` - CET size 0xFB4 (4020 B); HW1 counterpart: none (Cataclysm-only)

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `MaxSelection sel` |  | - |
| 0xFA8 | `vector center` |  | - |

#### `texreg` - CET size 0x2C (44 B); HW1 counterpart: `SDL/texreg.h` `texreg`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `sword diskWidth` |  | yes |
| 0x2 | `sword diskHeight` |  | yes |
| 0x4 | `sword scaledWidth` | yes (CET/game/Win32/texreg.h:179) | yes |
| 0x6 | `sword scaledHeight` | yes (CET/game/Win32/texreg.h:180) | yes |
| 0x8 | `sdword nUsageCount` |  | yes |
| 0xC | `ubyte flags` | yes (CET/game/Win32/texreg.h:181) | yes |
| 0xD | `sbyte nPalettes` |  | yes |
| 0xE | `sbyte currentPalette` |  | yes |
| 0xF | `ubyte nSharedTo` |  | yes |
| 0x10 | `crc32 paletteCRC` |  | yes |
| 0x14 | `ubyte * palettes` |  | yes |
| 0x18 | `void * meshReference` |  | yes |
| 0x1C | `char * fileName` |  | yes |
| 0x20 | `udword handle` |  | yes |
| 0x24 | `uword baseScalar` |  | yes |
| 0x26 | `uword stripeScalar` |  | yes |
| 0x28 | `udword sharedFrom` |  | yes |

#### `shiptrail` - CET size 0xB0 (176 B); HW1 counterpart: `Game/Trails.h` `shiptrail`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `trailstatic * staticInfo` |  | yes |
| 0x4 | `void * vship` |  | yes |
| 0x8 | `real32 width` |  | yes |
| 0xC | `real32 height` |  | yes |
| 0x10 | `real32 angle` |  | yes |
| 0x14 | `real32 ribbonadjust` |  | yes |
| 0x18 | `bool8 style` | yes (CET/game/Win32/trails.h:104) | yes |
| 0x19 | `bool8 second` |  | yes |
| 0x1A | `ubyte trailNum` |  | yes |
| 0x1B | `ubyte state` |  | yes |
| 0x1C | `ubyte counter` |  | yes |
| 0x1D | `_Bool wobbly` |  | yes |
| 0x20 | `real32 lastvelsquared` |  | yes |
| 0x24 | `real32 prevvelsquared` |  | yes |
| 0x28 | `sdword ranCounter` |  | yes |
| 0x2C | `real32 ran` |  | yes |
| 0x30 | `real32 exponent` |  | yes |
| 0x34 | `real32 scalecap` |  | yes |
| 0x38 | `sdword unknown` |  | **no (unknown pad)** |
| 0x3C | `sdword grainCounter` | yes (CET/game/Win32/trails.h:105) | yes |
| 0x40 | `sdword iHead` |  | yes |
| 0x44 | `sdword iTail` |  | yes |
| 0x48 | `sdword nLength` |  | yes |
| 0x4C | `trailsegment[1] segments` |  | yes |

#### `SFXLUT` - CET size 0x10 (16 B); HW1 counterpart: `Game/SoundStructs.h` `SFXLUT`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `sdword ID` |  | yes |
| 0x4 | `udword checksum` |  | yes |
| 0x8 | `sword numvariations` | yes (CET/game/Game/soundstructs.h:56) | yes |
| 0xA | `sword numevents` | yes (CET/game/Game/soundstructs.h:57) | yes |
| 0xC | `sword numobjects` |  | yes |
| 0xE | `sword[] lookup` |  | yes |

#### `Timer` - CET size 0x10 (16 B); HW1 counterpart: `Game/Timer.h` `Timer`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `real32 start` |  | **no (Cataclysm/CET)** |
| 0x4 | `real32 duration` |  | yes |
| 0x8 | `udword unk` |  | **no (unknown pad)** |
| 0xC | `udword unk2` |  | **no (unknown pad)** |

#### `rdevice` - CET size 0x94 (148 B); HW1 counterpart: `SDL/rinit.h` `rdevice`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `int type` |  | yes |
| 0x4 | `char[64] data` |  | yes |
| 0x44 | `char[64] name` |  | yes |
| 0x84 | `unsigned int devcaps` |  | **no (Cataclysm/CET)** |
| 0x88 | `unsigned int devcaps2` |  | **no (Cataclysm/CET)** |
| 0x8C | `struct rmode * modes` |  | yes |
| 0x90 | `struct rdevice * next` |  | yes |

#### `rmode` - CET size 0x10 (16 B); HW1 counterpart: `SDL/rinit.h` `rmode`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `int width` |  | yes |
| 0x4 | `int height` |  | yes |
| 0x8 | `int depth` |  | yes |
| 0xC | `struct rmode * next` |  | yes |

#### `scriptEntry` - CET size 0xC (12 B); HW1 counterpart: `Game/StatScript.h` `scriptEntry`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `const char * name` |  | yes |
| 0x4 | `setVarCback setVarCB` |  | yes |
| 0x8 | `void * dataPtr` |  | yes |

#### `scriptStructEntry` - CET size 0x10 (16 B); HW1 counterpart: `Game/StatScript.h` `scriptStructEntry`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `char * name` |  | yes |
| 0x4 | `setVarCback setVarCB` |  | yes |
| 0x8 | `udword offset1` |  | yes |
| 0xC | `udword offset2` |  | yes |

#### `CaptainGameInfo` - CET size 0x268 (616 B); HW1 counterpart: `SDL/TitanInterfaceC.h` `CaptainGameInfo`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `wchar_t[32] Name` |  | yes |
| 0x40 | `wchar_t[20] Password` |  | yes |
| 0x68 | `char[32] MapName` |  | yes |
| 0x88 | `char[32] DisplayMapName` |  | yes |
| 0xA8 | `char userBehindFirewall` |  | yes |
| 0xA9 | `unsigned char[3] pad` |  | yes |
| 0xAC | `unsigned char numComputers` |  | yes |
| 0xAD | `unsigned char startingFleet` |  | yes |
| 0xAE | `unsigned char bountySize` |  | yes |
| 0xAF | `unsigned char startingResources` |  | yes |
| 0xB0 | `unsigned long resourceInjectionInterval` |  | yes |
| 0xB4 | `unsigned long resourceInjectionsAmount` |  | yes |
| 0xB8 | `unsigned long resourceLumpSumTime` |  | yes |
| 0xBC | `unsigned long resourceLumpSumAmount` |  | yes |
| 0xC0 | `unsigned char aiplayerDifficultyLevel` |  | yes |
| 0xC1 | `unsigned char aiplayerBigotry` |  | yes |
| 0xC2 | `unsigned short pad2` |  | yes |
| 0xC4 | `unsigned short flag` |  | yes |
| 0xC6 | `unsigned short numPlayers` |  | yes |
| 0xC8 | `PlayerInfo[8] playerInfo` |  | yes |

#### `GrowSelection` - CET size 0x8 (8 B); HW1 counterpart: `Game/ShipSelect.h` `GrowSelection`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `sdword maxNumShips` |  | yes |
| 0x4 | `SelectCommand * selection` |  | yes |

#### `MaxSelection` - CET size 0xFA8 (4008 B); HW1 counterpart: `Game/ShipSelect.h` `MaxSelection`

| Off | Field (CET decl) | Asserted (CET file:line) | Name in HW1 struct? (heuristic) |
|---|---|---|---|
| 0x0 | `sdword numShips` |  | yes |
| 0x4 | `real32 timeLastStatus` |  | yes |
| 0x8 | `ShipPtr[1000] ShipPtr` |  | yes |
---

## 5. Per-file: original bug or limitation, CET's fix, and the HW1 code it corresponds to

| CET file | Original engine bug / limitation | CET fix | HW1 counterpart (`HW1/`) |
|---|---|---|---|
| `GameHooks.cpp` `univUpdate` hook (:16-43) | No autosave | Countdown of `AutoSaveInterval*60` s of game time (sum of `phystimeelapsed`). Calls `SaveGame()` and shows a `clCommandMessage` | `Game/UnivUpdate.c:7292 univUpdate`; HW1 only has a debug autosave (`gardens-of-kadesh/src/Game/UnivUpdate.c:7279-7292`) |
| `GameHooks.cpp` `opOptionsAccept` (:45-54) | Changing resolution in Options leaves `rndAspectRatio` stale | Recompute W/H after accept | `Game/Options.c:1307 opOptionsAccept`; `SDL/render.c:181 rndAspectRatio` |
| `GameHooks.cpp` `CheckPlayerWin_GameTypeCheck` (:73-92) | Skirmish (neither SP nor MP flag) never reached a win condition | Route skirmish into the MP branch | `Game/UnivUpdate.c:4150 CheckPlayerWin` (`if (!singlePlayerGame)` at 4158). **[exe 12948]** Cataclysm instead tests `multiPlayerGame` (`je 0x005486E5` when FALSE), so skirmish fell into the SP branch; CET forces the fall-through at 0x00548619 |
| `GameHooks.cpp` `LoadMission_AdjustTextureMemoryLimit` (:56-71) | SP mission 17 shrinks the ship texture pool, so ships load at low detail; the pool is also capped to "video memory" | Skip the M17 shrink; optionally set the pool to INT32_MAX | `SDL/texreg.c:2392 trCramRAMScaleCompute` / `2464 trCramIntoRAM` (Cataclysm's `trCramRAMComputeAndScale` 0x00589A50 contains the site: Ghidra container, `cet_apply_report.csv`). **[exe 12948]** The block tests `singlePlayerGameInfo.currentMission == 17` and sets pool = max(1 MB, x*0.3f) |
| `GameHooks.cpp` `RedirectUpdateBigFile` (:108-116) | (distribution) | CET ships its data overrides in `Update102.big` | `SDL/utility.c:3530 utyGameSystemsPreInit`, `Game/BigFile.c:74` |
| `GameHooks.cpp` partRenderBillSystem jmp (:141-144) | Billboard particle rendering leaves global texture filter at point, so ship textures turn pixelated at random | Skip the offending block | `Game/Particle.c:703 partRenderBillSystem` |
| `GameHooks.cpp` F12 (:146-149) | F12 key-up was eaten by a debug-key handler, so the second press (military parade) failed | Branch to the normal key-up path | Relic Win32 `WindowProc` (SDL port: event loop in `SDL/main.c` ~1690-1939) |
| `GameHooks.cpp` `dbgInt3Enabled` (:151-156) | Asserts executed `int3` (crash) instead of showing the message | Disable unless a debugger is attached | Relic `Game/Debug.c` (removed in port) |
| `PatchUniverse.cpp` (whole file) | Fixed 16 Hz universe tick (`UNIVERSE_UPDATE_RATE 16`, `CET/game/Game/universe.h:57-58`) makes movement choppy | Re-time every period/rate constant CET found (section 2) and gate 16 Hz-mask logic with `UNIVERSE_WOODPECKER` | `Game/Universe.h:44-53` already has this generalized (`UNIVERSE_UPDATE_RATE_SHIFT`, set to 2 in both GoK and the `HW1/` mirror, `Game/Universe.c:75`; `/vanilla` sets 0, `SDL/main.c:848,2166-2168`); `Game/UnivUpdate.c:7405/7462/7588`, `Game/ResCollect.c:596`, `Game/NIS.c:416-580`, `Game/Task.c:539`, `Game/Gun.c:1657`, `Game/ETG.c:5255,6580`, `SDL/utility.c:3937-3940,4071` |
| `PatchUniverse.cpp` QPC hook (:291-325) | Cataclysm's new `universe.realtimeelapsed` is a `real32` fed from raw QueryPerformanceCounter, so with high PC uptime the float stops advancing (SP fade-in/out never completes). Worse at 64 Hz | Rebase QPC to ~1 s at first call | none in HW1 (Cataclysm-new timer; Win32 `utility.c` timer with `realtimeremainder` 0x00A6D9A0, `utyTimerDivisor` 0x00AB7520 as LONGLONG) |
| `PatchUniverse.cpp` `univBulletCollidedWithTarget` (:221-253) | Somtaaw command ship / Beast mothership (and modules) shoot themselves through buggy firing arcs | Drop collisions where both sides are the same player's mothership/module | `Game/UnivUpdate.c:3118 univBulletCollidedWithTarget` (Cataclysm adds a leading `int`) |
| `PatchUniverse.cpp` `scriptSetReal32CBSkipIfNLipsDisabled` (:119-132) | N-LIPS (non-linear inverse perspective scaling) enlarges distant ships | `DisableNLips`: force per-ship `nlips` and all 4 `trailScaleCap` to 0 | `SDL/render.c:2674-2717` (HW1 uses `shipStaticInfo->scaleCap`, script key `"N-LIPS"`, `Game/Universe.c` ShipStaticScriptTable entry 106) |
| `PatchETG.cpp` | (a) Effects that spawn sprites per tick overflow their particle-block array at >16 Hz. (b) ETG drops effects when too many play (`etgFrequencyExceeded`) | (a) `nParticleBlocks <<= shift` (CET calls it a hack). (b) never exceeded | `Game/ETG.c:5295 etgNParticleBlocksSet`, `7609 etgFrequencyExceeded` |
| `PatchLOD.cpp` | LOD scaling picks low-detail meshes on modern resolutions | Force LOD 0 | `Game/LOD.c:268 lodLevelGet` (HW1 returns `lod*`, Cataclysm `int`) |
| `PatchRGL.cpp` + `game/Game/rinit.cpp` | Video options only list 4:3 / 16-bit-capable modes; a device-CRC change (new GPU/driver) silently falls back to **software** rendering at low res; OpenGL unusable on XP+ due to an NT<=4 check; alt-tab in windowed mode re-initializes the renderer; compiled-vertex-arrays corrupt on some AMD; built-in frame limiter | Replace mode enumeration/sort; on CRC change pick OpenGL at desktop resolution; `glCapNT` returns TRUE; skip re-init; optional CVA disable; optional limiter NOPs | `SDL/rinit.c:170 rinSortModes`, `:71 rinDeviceCRC`; `SDL/main.c:1948-1996 InitWindow`, `:1154 ActivateMe`; Relic Win32 `glcaps.c` (`glCapNT`, `glCapStartup`) not in port |
| `PatchSaveGame.cpp` | Loading an SP save zeroes `SongNumber` right after reading it, so the wrong music track plays | Redirect the zeroing slot to a dummy | `Game/SaveGame.c:640` (loads `SongNumber`), `Game/LevelLoad.c:133` (`SMMissionTweaks` `makeEntry(SongNumber)`), `:236` (`SongNumber = -1` reset). **[exe 12948]** Slot 0x00892670 is the `ptr` of entry 2 in a 3-entry `{ptr,size}` pre-game table at 0x00892660. On load, FUN_004FE0E0 copies the saved values back over all three pointers, overwriting the `SongNumber` that FUN_004FDFC0 had just loaded (section 3d) |
| `PatchSound.cpp` + `game/Win32/soundlow.cpp` | `soundStartDSound` rejects non-"certified" DirectSound drivers (all modern ones), so the engine falls back to waveOut (higher latency); built-in equalizer can distort | Accept all drivers; optional equalizer `ret` | Relic Win32 `soundlow.c` (SDL port replaced with SDL audio; `HW1/SDL/soundlow.c`); `SDL/fqeffect.c:130 fqEqualize` |
| `PatchAI.cpp` + `game/Game/AISkirmish.cpp` | Cataclysm skirmish AI build logic is hard-coded | Optional data-driven reimplementation (`aiskirmish.script`) | none (Cataclysm-only `aisFleetUpdate`; HW1 AI is `Game/AIPlayer.c`/`AIResource.c`/etc.) |
| `MultiplayerGame.cpp` | MP setup stores resource injection/lump-sum intervals as frames (`minutes*60*16`) | Convert with the new rate | `Game/MultiplayerGame.c:5041 mgResourceInjectionInterval`, `:5119 mgResourceLumpSumInterval` |
| `GameState.cpp` | (CET state) | `ResetAutosaveTimer`: `interval*60` or disabled | n/a |

Reimplemented-AI facts worth keeping for the port (from `CET/game/Game/AISkirmish.cpp`, "tweakable version of Cataclysm's skirmish AI", `:1-6`). It is a reconstruction, and how faithful it is is UNVERIFIED:
- Per-difficulty target counts (`:33-44`): mimic {0,0,20}, scout {5,8,12}, cruise missile {0,0,10}, fighter {25,35,60}, DFG {0,1,2}, leech {0,10,20}, worker normal {5,8,12} / high-RU {2,4,6}, high-RU threshold 50000, fighter-build threshold 15. The shipped `assets/aiskirmish.script` overrides the advanced mimic count to **14**.
- Build ladder (`:381-427`): Beast carrier (2000, max 2 carriers) > heavy cruiser (3000, if RU>2000) > processor (1000); Sect carrier > dreadnought > destroyer (1500) > processor. Sect command-ship modules are built when missing (`:290-325`), and BigGun costs 5000. Support modules are capped at 12 for the mothership and 6 per carrier.
- Medium-ship pick: `ranRandomFn(6) % 7` (Beast) / `% 6` (Sect) tables (`:624-679`). The advanced AI sets fighter group 0 to kamikaze, lower levels scuttle it (`:705-719`).
- Probable bugs carried in CET's copy: `GetUnassignedShipCount` counts `sWorker` whatever type it is asked for (`:155`); MCV count is not checked (`:523`, comment "Original game bug?").

---

## 6. rGL DLL ABI as hw_rgl implements it

### 6.1 How the game reaches rGL
- The exe never links OpenGL statically. It loads a GL DLL **by name** (global `glToSelect` @0x008B8644, e.g. `"rgl.dll"` or `"opengl32.dll"`; CET writes `"opengl32.dll"`, `CET/PatchRGL.cpp:53`) and resolves `__stdcall` GL 1.1 entry points plus `wgl*` into function pointers (`glDLLGetProcs(char* dllName)`; prototypes in `CET/game/Win32/gldll.h:32-276`, e.g. `rwglGetProcAddress`).
- rGL-specific extensions are fetched through `wglGetProcAddress(name)`. hw_rgl's `wglGetProcAddress` forwards to `rglGetProcAddress` (`RGL/wgl.c:9-13`). `wglCreateContext`/`DeleteContext`/`MakeCurrent` are stubs returning 1 (`RGL/wgl.c:6-8`).

### 6.2 `rgl.dll` exports (`RGL/draw.def`, `RGL/rgl.vcxproj:120,142`)
- 105 `gl*` + 4 `wgl*` stdcall exports, renamed from decorated names (`glAlphaFunc=_glAlphaFunc@8` ...). `glColorTableEXT` aliases `glColorTable`. The full list is in `RGL/draw.def:3-112`. Notable non-core entries: `glColorTable(EXT)`, `glEvalCoord*/glMap*/glEvalMesh*` (bezier), `glInterleavedArrays`, `glDrawArrays/Elements`, `glArrayElement`, `glGetTexImage`, `glClipPlane`.
- Extension table returned by `rglGetProcAddress` (`RGL/kgl.c:6457-6508`, 32 entries): `glColorTableEXT glLitColorTableEXT rglFeature rglSpecExp rglLightingAdjust rglSaveCursorUnder rglRestoreCursorUnder rglIsFast rglCreateWindow rglDeleteWindow rglIsClipped rglNumPolys rglCulledPolys rglBackground rglSetAllocs rglSuperClear rglEnable rglDisable rglListSpec rglList rglNormal rglTriangle rglTexturedTriangle rglSmoothTriangle rglSmoothTexturedTriangle rglMeshRender rauxInitPosition rglSelectDevice rglD3DSetDevice rglD3DGetDevice rglGetFramebuffer rglDrawPitchedPixels`.
- Additional `__declspec(dllexport)` cdecl functions in `kgl.c`/`rglext.c` (exported by name through the `DLL` macro, `RGL/kgl_macros.h:9-10`): `gl_get_context_ext glGetFrameCountEXT glLitColorTable glSuperClear rauxInitDisplayMode rauxInitPosition rglAnotherPoly rglBackground rglCreate0/1/2 rglCreateWindow rglCulledPolys rglD3DGetDevice rglD3DSetDevice rglDeleteWindow rglDisable rglDrawLightVectors rglDrawPitchedPixels rglEnable rglFeature rglGetFramebuffer rglGetFullscreen rglGetMaxTexobj rglGetSkip rglGetSlow rglGetTexobj rglGetTexobjs rglGetTruecolor rglIsClipped rglIsFast rglLightingAdjust rglList rglListSpec rglMeshRender rglNormal rglNumPolys rglRestoreCursorUnder rglSaveCursorUnder rglSelectDevice rglSetAllocs rglSetExtensionString rglSetRendererString rglSmoothTexturedTriangle rglSmoothTriangle rglSpecExp rglTexturedTriangle rglTriangle`, plus `glu*`/`glut*` helpers (`RGL/kgl.h:810-841`).

### 6.3 Start-up protocol (game -> rgl.dll -> driver DLL)
1. `rglSetAllocs(MemAllocFunc, MemFreeFunc)`: the game hands its allocator (`void*(*)(GLint len,char* name,GLuint flags)`, `RGL/kgl.h:684-685`; used as `gAllocFunc(size,"RGL alloc",8)`, `kgl.c:179-205`).
2. `rglCreateWindow(a,b,c)` stores three ints. `rglCreate0()` is the **HWND** the D3D driver renders to (`kgl.c:6397-6407`, `RGL/D3D9/d3dinit.cpp:27`).
3. `rglSelectDevice(name, data)` sets `deviceToSelect` and the D3D device string (`kgl.c:4693-4722`).
4. `rglFeature(RGL_FULLSCREEN|RGL_WINDOWED|RGL_TRUECOLOR|...)` sets global modes (`kgl.c:4725-4953`; constants `kgl.h:622-680`).
5. `rauxInitPosition(x,y,w,h,depth)` creates the single `GLcontext`, `gl_init_context`, then `_rgl_init` -> `gl_driver_init` (`kgl.c:2980-3003, 2781-2846`).
6. `gl_driver_init` loads `"rgl"+devices[active].name` (`rglsw` or `rgld3d`; device list hard-coded `{"sw","software_kgl"}`, `{"d3d","d3d_direct3d_directaxe_ms"}`, `kgl.c:352-367`) via `GetModuleHandle`/`LoadLibrary`, then `GetProcAddress(lib,"init_driver")` and `init_driver(ctx)` (`kgl.c:2808-2836`). Renderer string becomes `"rgl"+2 chars`. **hw_rgl does not build `rglsw.dll`**, so the software device needs the game's original DLL (UNVERIFIED).
7. `gl_driver_postinit` calls `post_init_driver`, `allocate_colorbuffer/depthbuffer` if non-NULL (`kgl.c:2853-2891`).

### 6.4 Driver dispatch table (`gl_driver_funcs`, `RGL/kgl.h:246-434`, embedded as `gl_context.DriverFuncs`, `kgl.h:577`)
Members in order: `init_driver post_init_driver shutdown_driver create_window delete_window set_save_state get_scratch driver_caps allocate_depthbuffer allocate_colorbuffer clear_depthbuffer clear_colorbuffer clear_both_buffers lock_buffer unlock_buffer get_framebuffer setup_triangle setup_line setup_point setup_raster set_monocolor flush clear_color scissor flush_batch draw_triangle draw_triangle_array draw_quad draw_triangle_fan draw_triangle_strip draw_polygon draw_line draw_pixel draw_bitmap draw_pixels read_pixels draw_point draw_triangle_elements draw_clipped_triangle draw_clipped_polygon bind_texture tex_param tex_del tex_palette tex_img tex_env deactivate activate screenshot gamma_up gamma_dn chromakey super_clear fastbind_set draw_background fog_vertices begin end vertex update_modelview update_projection draw_c4ub_v3f feature_exists fullscene get_animaticbuffer draw_pitched_pixels`. Most are untyped `void(*)()` (K&R style), so the ABI is "whatever the caller pushes".

The D3D9 driver (`RGL/D3D9/d3driver.cpp:2184-2322`, exported `extern "C" init_driver(GLcontext*)`) sets: `post_init_driver, shutdown_driver, clear_*` (3), `setup_raster, set_monocolor, flush, scissor, begin, end, vertex, update_modelview, update_projection, draw_c4ub_v3f, flush_batch, draw_triangle, draw_quad, draw_line, draw_pixels, read_pixels, draw_pixel, draw_point, draw_triangle_elements, bind_texture, tex_param, tex_del, tex_palette, tex_img, activate/deactivate (no-ops), lock_buffer/unlock_buffer (Begin/EndScene), draw_pitched_pixels, fullscene (asserts unimplemented), driver_caps, feature_exists`. All others are NULL. `DriverTransforms=FALSE` (rgl.dll transforms and clips on the CPU, `RGL/kvb.c`), `RequireLocking=FALSE` (`IMPLICIT_BEGINS 1`).

`feature_exists` answers (`d3driver.cpp:2155-2171`): `GL_SHARED_TEXTURE_PALETTE_EXT` (if a COLORINDEX format exists), `RGL_BROKEN_MIXED_DEPTHTEST` (TRUE when `D3DReference==0`), `RGL_COLOROP_ADD` (`canTexAdd`), `RGL_D3D_FULLSCENE` (`canAntialiasTriIndep`), `RGL_D3D_D3D9` (always TRUE; new constant 0x4632, `kgl.h:677`).

**ABI coupling warning:** the driver reads and writes `gl_context` fields directly (`CTX->Buffer`, `TexBoundObject`, `ModelViewMatrix`, `D3DReference`, ...; struct at `RGL/kgl.h:439-607`), so `rgl.dll` and `rgld3d.dll` must come from the same build. hw_rgl's context is **not** layout-compatible with Relic's: for example `MAX_WIDTH 3840 / MAX_HEIGHT 2160` arrays (`kgl.h:112-113`) and the added `AttribStack` (`kgl.h:566-567`) change its size, so hw_rgl's `rgl.dll` cannot drive the GOG `rgld3d.dll` or the reverse (UNVERIFIED, but follows from the struct).

### 6.5 D3D9 specifics (`RGL/D3D9/`)
| Topic | Behaviour | Source |
|---|---|---|
| Device | `Direct3DCreate9`, adapter 0, `D3DDEVTYPE_HAL`, `PUREDEVICE\|HARDWARE_VERTEXPROCESSING\|MULTITHREADED\|FPU_PRESERVE`. FPU_PRESERVE is required: "Audio and various other things will break in Cataclysm if D3D changes the FPU precision" | `d3dinit.cpp:134-143` |
| Swap chain | Windowed = `!rglGetFullscreen()`, `D3DFMT_X8R8G8B8` only, 1 back buffer, `DISCARD`, `D3DPRESENT_INTERVAL_IMMEDIATE` (no vsync), auto depth `D24X8` | `d3dinit.cpp:120-131` |
| Caps gate | Fails if `D3DPTEXTURECAPS_POW2` (needs non-pow2 textures) | `d3dinit.cpp:167-172` |
| Geometry | Pre-transformed `XYZRHW\|DIFFUSE(\|TEX1)` vertices via `DrawPrimitiveUP` (rgl.dll does T&L); `XYZ` path only if `DriverTransforms` | `d3driver.cpp:1291-1319,1423-1460,145-160` |
| Textures | `CreateTexture(..., D3DUSAGE_DYNAMIC, D3DPOOL_DEFAULT)`; paletted (`GL_COLOR_INDEX`) textures expanded by `d3d_blt_COLORINDEX`; shared palette emulated | `d3dtex.cpp:470,244,666-718` |
| 2D/pixel blits | `glDrawPixels`/`rglDrawPitchedPixels` go through a dynamic texture plus a screen-space quad (the "UI slowdown" fix) and restore the bound texture after | `d3dblt.cpp:582-640` |
| Alt-tab | `activate/deactivate` no-ops (D3D9 resources survive) | `d3driver.cpp:2173-2182` |
| Max resolution | 3840x2160 (static arrays) | `RGL/kgl.h:111-113` |
| Logging | `rgld3d9.log` (spdlog) | `d3driver.cpp:89,2188` |
| Cataclysm-specific | `glPushAttrib/glPopAttrib` with a minimal 16-deep stack for `GL_POINT_BIT`, `GL_LINE_BIT`, **`GL_BLEND`, `GL_LIGHTING`** (enable caps passed as attrib masks) | `RGL/kgl.c:1224-1318` (HEAD commit message) |
| README claims | D3D7->D3D9, resolutions up to 4K, fixes UI slowdown (options/build manager), fixes Z-fighting on distant ships; supports HW1 Classic 1.05/1.06 **and** Cataclysm with the same DLL pair. The Z-fighting fix is presumably the 24-bit depth buffer (`D3DFMT_D24X8`, `d3dinit.cpp:50,130`), UNVERIFIED | `RGL/README.md:3,17-21` |
| D3D7 tree | `RGL/D3D7/` = Relic's original DirectDraw7/D3D7 driver (`TargetName rgld3d`), reference for the stock `rgld3d.dll` | `RGL/D3D7/rgld3d7.vcxproj:111` |

For the native port, rGL is irrelevant: HomeworldSDL renders straight to OpenGL, and HW1's `glcaps.c`/`gldll.c`/`rinit.c` DirectDraw paths are gone. rGL only matters for running the stock exe (e.g. under Proton) as a behavioural reference.

---

## 7. Configuration (`CET/CataclysmEngineTweaks.ini`, parsed by `CET/Config.cpp` with inih `INIReader`)

INIReader lower-cases section and key names (standard inih C++ wrapper behaviour; vcpkg version not pinned, UNVERIFIED), so the ini's `DisableFrameRateLimit` matches the code's `"DisableFramerateLimit"`.

| Section / key | Code default | Shipped ini | Effect | Code |
|---|---|---|---|---|
| `[Engine] UpdateRateShift` | 2 (clamped 0..3) | 2 | Universe rate = 16<<n (16/32/64/128 Hz). Gates **all** of `PatchUniverse.cpp` (n=0 -> none) | `Config.cpp:5,15-16`; `PatchUniverse.cpp:428-431` |
| `[Engine] DisableFrameRateLimit` | false | false | NOP the render frame limiter (2 sites) | `Config.cpp:17`; `PatchRGL.cpp:96-100` |
| `[Engine] UnlimitedVideoMemory` | true | true | Texture RAM pool = INT32_MAX (M17 shrink is skipped regardless) | `Config.cpp:18`; `GameHooks.cpp:60-67` |
| `[Display] HighDetailMode` | true | true | LOD 0 always + no effect cap + (side effect) particle-block scaling | `Config.cpp:23`; `PatchLOD.cpp:29`, `PatchETG.cpp:84-95` |
| `[Display] NewRendererSelection` | true | true | New mode enumeration/sort, OpenGL default at desktop res, no 16-bit modes | `Config.cpp:24`; `PatchRGL.cpp:80-88` |
| `[Display] BorderlessWindow` | true | true | `fullScreen=showBorder=FALSE` (also enables the ActivateMe skip) | `Config.cpp:25`; `Main.cpp:21-25` |
| `[Display] DisableIntroMovies` | true | true | `enableAVI=FALSE` | `Config.cpp:26`; `Main.cpp:27-30` |
| `[Display] DisableNLips` | false | false | Zero ship N-LIPS and trail scale caps (**only applied when UpdateRateShift>0**) | `Config.cpp:27`; `PatchUniverse.cpp:119-132,449-453` |
| `[Game] AutoSaveInterval` | 5 (min, >=0) | 5 | Autosave period; 0 disables | `Config.cpp:32`; `GameState.cpp:6-16`, `GameHooks.cpp:22-39` |
| `[OpenGL] FixWindowsNTBug` | true | true | `glCapNT()` returns TRUE | `Config.cpp:37`; `glcaps.cpp:25-28` |
| `[OpenGL] NoCompiledVertexArrays` | false | false | `glCapCompiledVertexArray=FALSE` | `Config.cpp:38`; `PatchRGL.cpp:70-73` |
| `[Audio] ForceWaveOut` | false | false | Do not hook `soundStartDSound` | `Config.cpp:42`; `PatchSound.cpp:14-17` |
| `[Audio] DisableEqualizer` | false | false | `fqEqualize` -> `ret` | `Config.cpp:43`; `PatchSound.cpp:24-27` |
| `[AI] EnableNewAI` | false | false | Replace `aisFleetUpdate`; load `aiskirmish.script` | `Config.cpp:47`; `PatchAI.cpp:20-23`, `GameHooks.cpp:102-105` |
| `[Debug] SensorHack` | false | (absent) | `_DEBUG` builds only: NOP 2 B at 0x0050A43B | `Config.cpp:49-53`; `GameHooks.cpp:160-168` |

Command-line equivalents in the exe (from CET comments): `/window`, `/noBorder`, `/disableAVI`, `/safeGL` (disables CVA among other things) (`CataclysmEngineTweaks.ini:29-53`). The loader passes its own argv straight through (`CET/loader/Main.cpp:28-38`).

CET data overrides shipped in `Update102.big` (`CET/assets/`, purpose undocumented in source, UNVERIFIED): `tweak.script`, `english.script`, `aiskirmish.script`, `feman/multiplayer_game.fib`, `soundfx/shipcmnevents.lut`, 11 `multiplayer/*/description.txt`, Beast `.shp` (bCarrier, bCruiseMissile, bDFGFrigate, bHeavyCorvette, bHeavyCruiser, bHiveFrigate, bIonArrayFrigate, bMissileCorvette, bMultiBeamFrigate, bMultiGunCorvette, bProcessor, bswarmer, bWorker), Sect `.shp` (sCarrier, sCarrierSupport, sDestroyer, sDreadnought, sHiveFrigate, sMCV, sMimic, sMothership, sMothershipDockingBay, sMothershipSpecial, sMothershipSupport, sMultiBeamFrigate, sProcessor, sswarmer), and LOD0-4 `.mex` for b/s hive frigate and swarmer. These are the only plain-text Cataclysm data in hand before the installer arrives. They show `.shp` keys such as `supportUnits`, `providesSU`, `isCapitalShip`, `regenerateRUrate` (as a `(2^n)-1` frame mask), `hyperspaceBoxScale`, `pilotViewOffset*`, `GUN_HiddenGimble`, `StartUpgradeLevel` (`assets/RACE_SECT/sMothership.shp`), and `tweak.script` frame-mask tunables (`assets/tweak.script:749-804`).

---

## 8. Loader start-up sequence (`CET/loader/Main.cpp`, console app, Win32 MultiByte)

1. `CheckCurrentDirectory()`: `CataclysmEngineTweaks.dll` and `cataclysm.exe` must exist in the **CWD** (`:9-26`).
2. `KillProcessesByName("cataclysm.exe")`: terminates every running instance via a Toolhelp snapshot (`:91`, `loader/Utilities.cpp:3-42`). The match is a case-sensitive `strcmp` on `szExeFile` (`Utilities.cpp:28`), so an instance named e.g. `Cataclysm.exe` survives.
3. Command line = `"cataclysm.exe " + argv[1..] joined with spaces` (no quoting, `:28-38`).
4. `CreateProcess(NULL, cmdline, ..., CREATE_SUSPENDED, ..., CWD)` (`:96-110`).
5. `InjectDll`: `VirtualAllocEx` + `WriteProcessMemory` of the string `"CataclysmEngineTweaks.dll"`, then `CreateRemoteThread(LoadLibraryA)` and `WaitForSingleObject(INFINITE)` (`:40-77`). The remote thread's exit code (the HMODULE) is **not** checked, so the game is resumed even if DllMain fails. It runs cleanly unpatched only when the failure is the image-base check or `MH_Initialize` (before any patch); for a later failure see the DllMain step 5 note below. If `VirtualAllocEx`/`WriteProcessMemory`/`CreateRemoteThread` fails, the loader returns the error and leaves the child **suspended** (never resumed or terminated); the next loader run kills it in step 2 (comment `:90`, `:112-116`).
6. `ResumeThread(main thread)` and exit (`:118-120`).

Inside the game process (`CET/dllmain.cpp:27-54`), all while the exe's main thread is still suspended. By standard Windows loader behaviour (not stated in CET), the remote thread first finishes process initialization, so the exe's static imports are loaded and DllMain runs before the exe entry point. The .data defaults are therefore patched before WinMain parses the command line:
1. `CheckModuleLoadAddress()` (image base == 0x400000).
2. `MH_Initialize()`.
3. `InitLogger("CataclysmEngineTweaks.log")` (truncating file sink, dup-filter 100 ms, flush on every message; `Main.cpp:51-78`).
4. `Config::FromFile("CataclysmEngineTweaks.ini")` (`Main.h:5`).
5. `ApplyPatches(config)` (order in section 2.1). Any exception logs, then DllMain returns FALSE (already-applied patches stay in place). By Microsoft's DllMain contract (not in the repo; UNVERIFIED here), a FALSE from `DLL_PROCESS_ATTACH` under `LoadLibrary` makes the loader call `DLL_PROCESS_DETACH` and unmap the DLL. `MH_Uninitialize()` then removes the MinHook detours (standard MinHook behaviour), but Keystone writes and data pokes that point **into the DLL** stay behind dangling: the `Update102.big` string pushed at 0x005ABB21/3E/5C, the ETG/NIS operand statics, `songNumberDummy`, and the N-LIPS callbacks. A mid-`ApplyPatches` failure should therefore crash later rather than "start unpatched".
6. `DLL_PROCESS_DETACH`: flush log, `MH_Uninitialize()`.

On Linux under Proton/Wine, the same `CREATE_SUSPENDED` + `CreateRemoteThread(LoadLibraryA)` pattern generally works, but it is untested here (UNVERIFIED).

---

## 9. Known issues and update-rate caveats

From CET itself:
- README Known Issues: the higher update rate is "beta and may affect game balance/functionality due to timers running too quickly" (`CET/README.md:42-44`). Supported 32/64, while 128 is "untested" (`CataclysmEngineTweaks.ini:2-11`). **CET ships 64 Hz on by default** (`UpdateRateShift=2` in both code and ini), so a CET install is *not* vanilla timing. A 1-to-1 baseline must use shift 0.
- `etgNParticleBlocksSet` scaling is described as "more of a hack than a proper fix" (`CET/PatchETG.cpp:53-56`).
- `realtimeelapsed` should really be a double (`CET/PatchUniverse.cpp:301-302`).

Derived from reading the code (each is a consequence of the source; runtime behaviour is UNVERIFIED):
1. **Things that only exist when `UpdateRateShift>0`:** the mothership friendly-fire fix (`univBulletCollidedWithTarget` hook), the QPC uptime fix, `DisableNLips`, MP resource-interval UI conversion, and the `ResetAutosaveTimer()` call in `univUpdateReset`. With shift 0, `GameState.autosaveTimer` starts at `0.0f` (`CET/GameState.h:5`), so the **first universe tick triggers an autosave even when `AutoSaveInterval=0`**, and only then does `ResetAutosaveTimer` disable it (`CET/GameHooks.cpp:22-39`).
2. **Particle-block scaling depends on HighDetailMode**: `ApplyETGPatches` returns early when HighDetailMode is off (`CET/PatchETG.cpp:90-96`), so shift>0 without high detail can overflow effect particle arrays.
3. **16 Hz frame masks CET does not touch** (they run 4x as often at 64 Hz): every `*_RATE`/`*_FRAME`/`*_MASK` pair in `assets/tweak.script:749-804` except research (handled): `TUTORIAL_KAS_UPDATE_MASK`, `MINE_DO_SEARCH_MASK 15`, `SPHERE_CHECK_TARGET_MASK 31`, `CHECK_ATTACKMOVE_TO_MOVE_RATE 31`, `FORMATION_ERROR_CALCULATE_RATE 7`, `EnemyNearByCheckRate 15`, `RetreatCheckRate 31`, `ATTACKMEMORY_CLEANUP_RATE 255`, `TW_ProximitySensorSearchRate 31`, `CHECK_PASSIVE_ATTACK(_WHILEMOVING)_RATE 31`, `REFRESH_RENDERLIST/MINORRENDERLIST/COLLBLOB_RATE 7`, `CHECK_BOUNTIES_RATE 127`, `CLOAKGENERATOR_CLOAKCHECK_RATE 15`, `tweakCheckMissileTargetLossRate 3` (`:516`). Frame counts are also untouched: `AUTOSTABILIZE_FRAME_KICK_IN 8` ("16 == 1 second", `:15-16`), compiled-in `RECENT_ATTACK_DURATION = (ubyte)(5*16)` (`CET/game/Game/spaceobj.h:1408`), and the alliance-proposal timeout `univUpdateCounter + 16*90` (`HW1/Game/Alliance.c:57`; CET defines the site 0x0042090D but never patches it). Gardens of Kadesh fixed most of these in HW1 with `UNIVERSE_WOODPECKER` (`gardens-of-kadesh/src/Game/Tactics.c:1674,1885,1992`, `UnivUpdate.c:5582,5966,6095,6166,7384`). Use that list as the checklist.
4. `regenerateRUrate` is also a **per-ship** `.shp` key (`assets/RACE_SECT/sMothership.shp:89`, 511). CET only rescales the global `RegenerateRURate` (0x0088F580). **[exe 12948]** The key is not among the 178 `ShipStaticScriptTable` entries, so another table binds it: a ship-specific custom table, or the global as in HW1 `ResCollectTweaks` (`ResCollect.c:116`). Whether it is really per-ship is UNVERIFIED. The global has exactly one code ref: `test [0x0088F580],ecx` at 0x0054C752 inside `univUpdate`, with ecx = `univUpdateCounter`, i.e. the 16 Hz `WOODPECKER(regenerateRUrate,0)` of HW1 `UnivUpdate.c:7405`.
5. Savegames store frame-based counters (`univUpdateCounter`, frames-based `buildTime`, MP intervals in `tpGameCreated`). Mixing saves or MP peers across rates is unsafe. CET's `"v1.02"` network version prevents MP mixing with stock clients.
6. `univBulletCollidedWithTarget` beam gating: `if (!(counter % factor) == 0) return;` parses as `(!(x)) == 0`, i.e. return when `x != 0`. The behaviour is what was intended, but the expression is easy to misread (`CET/PatchUniverse.cpp:246`).
7. `soundStartDSound` failure path sets CET's own `Globals::lpPrimaryBuffer` pointer variable to NULL rather than `*lpPrimaryBuffer` (`CET/game/Win32/soundlow.cpp:104`), so after a failure CET's global no longer points at the game's slot. Harmless today because nothing reads it again.
8. Structs not re-based for the Cataclysm SpaceObj header (`SpaceObjRot*`, `Missile`, `Derelict`, `Resource*`, `Bullet` beyond its two asserts) are wrong in CET. Do not copy their offsets.
9. The `LoadMission` detour skips the M17 adjustment even when `UnlimitedVideoMemory=false` (`CET/GameHooks.cpp:69-70`).
10. `ActivateMe` re-init skip is decided from the `.data` default of `fullScreen` at attach time, not from the effective command-line mode (`CET/PatchRGL.cpp:91-94`).
11. Possible side effect of the SongNumber redirect: if slot 0x00892670 is also how a fresh mission applies the `.level` file's `SongNumber` (as HW1's `SMMissionTweaks` does, `HW1/Game/LevelLoad.c:133,236`), new missions would no longer pick up their scripted track. **[exe 12948]** Mostly ruled out: the slot belongs to the pre-game table at 0x00892660. Only two functions reference it (code refs to 0x00892664/0x0089267C): FUN_004FDA00, which packs the three values into a chunk (presumably on save), and FUN_004FE0E0, which copies them back on load. Fresh-mission code writes 0x00883960 directly (FUN_004A3A10, `decompiled.c:105708`). If FUN_004FDA00 is the save side, the redirect also makes CET saves record the dummy's value in that field, not the real `SongNumber`. A CET save loaded without CET would then restore that value over the track (UNVERIFIED).
12. **`ETG_UpdateRoundOff` write is a no-op (CET bug, found by the byte check).** 0x0072A818 is a **double** 0.03125 (`00 00 00 00 00 00 A0 3F`), the `(double)ETG_UpdateRoundOff` divisor of `fmod` in `etgAtDefine` (`HW1/Game/ETG.c:5255`; sole ref `fld qword` at 0x0046E037). CET stores a 4-byte float `P/2` there (`PatchUniverse.cpp:53`, `Cataclysm.h:194` declares `float*`), which only changes the low dword, so the double stays ~0.03125. At shift>0, `EOP_At` times therefore snap to k/32 s + P/4 (64 Hz: k/32 + 1/256) instead of the intended k*(P/2) + P/4 (k/128 + 1/256). Vanilla snaps to k/32 + 1/64. Effect events land 3/256 s (~11.7 ms) earlier than vanilla, at the coarse 1/32 s grid. Fix: write a `double`, or patch the `fld qword` operand the way CET patches the three `dword` operands.
13. **`TimerResolutionMax` (0x0072A614, 240.0f) is a pooled constant.** It has 8 `fmul dword` users in FUN_0042ED90, FUN_0042F1C0, FUN_0042F270 (x3), FUN_0052EC50 (x2) and FUN_0058A000, and CET's write (`PatchUniverse.cpp:448`) rescales all of them. One is visibly a seconds-to-ticks timeout conversion (`decompiled.c:196735`, cf. HW1 `TimeoutTimer.c:95`, `Captaincy.c:211`). Whether all 8 are timer conversions is UNVERIFIED, and any that is not changes behaviour 4x at 64 Hz.
14. **Research and regrow gating ignores script overrides.** The exe reads the mask/frame pairs from globals (0x008AC5C4/0x008AC5C8 = `REFRESH_RESEARCH_RATE/FRAME`, 0x0088F558/0x0088F55C = `REGROW_RESOURCES_CHECK_RATE/FRAME`). CET's detours hard-code 15/13 and 255/86 (`PatchUniverse.cpp:143-177`), so with shift>0 a `tweak.script`/`Resource.script` change to those values has no effect.
15. The `univUpdate` hook is declared `static bool univUpdate(real32)` (`GameHooks.cpp:17`) while the original returns `bool32` (`Cataclysm.h:179`), and the exe's `universeUpdateTask` tests the full EAX (`85 C0` after the calls at 0x0053E8AA/0x0053E8E0). This is benign with MSVC's `test eax,eax; setne al` conversion when the original returns 0/1, but it is an ABI mismatch to avoid when porting the hook (UNVERIFIED codegen).
16. **The HW1 reference is not 16 Hz either.** The `HW1/` mirror (HomeworldSDL `1bed493`) and GoK both ship `ubyte UNIVERSE_UPDATE_RATE_SHIFT = 2` (`HW1/Game/Universe.c:75`, `gardens-of-kadesh/src/Game/Universe.c:75`). `UNIVERSE_UPDATE_RATE`, `UNIVERSE_UPDATE_PERIOD` and `UTY_TimerResolutionMax` are derived from it (`HW1/Game/Universe.h:44-49`, `HW1/SDL/utility.h:39`). Only the `/vanilla` switch resets it to 0 (`HW1/SDL/main.c:848`, applied at `:2166-2168`; the `Universe.h:46` comment still calls it `/legacy`, which is stale). Code lifted from `HW1/` therefore runs at 64 Hz unless the shift is forced to 0, and `HW1/` line numbers quoted here for `*_RATE`/`WOODPECKER` sites describe the generalized code, not Relic's 2003 originals. Where this doc says "HW1 `utility.h:39` = 240" or "16*90", that is the shift-0 value.

---

## 10. Address anchors for Ghidra

Goal: once `cataclysm.exe` is extracted (innoextract on `setup_homeworld_emergence_*.exe`), prove it is CET's build and bulk-import names.

### 10.1 First checks
1. PE `ImageBase == 0x00400000` and no `IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE` / no `.reloc` dependence (CET requires it, `CET/dllmain.cpp:11-17`). **[exe 12948]** Pass: ImageBase 0x00400000, `DllCharacteristics` 0, empty base-relocation directory.
2. Record SHA-256, size and PE TimeDateStamp in `docs/`, since CET pins none of them. **[exe 12948]** Done in `docs/binary-fingerprint.md` (sha256 `f03d9a3e…`, 5,369,856 B, 2000-12-21).
3. Check the sections: VAs 0x0072A614/0x0072A818/0x0072A82C must be in a **writable** section (section 1). **[exe 12948]** Pass: they are in `.rdata`, which is writable in this exe (0xC0000040).

### 10.2 Byte-pattern anchors (high confidence if they match; written as predictions)

**[exe 12948]** Result: 35 of the 36 addresses below match. The exception is 0x0072A818, a double, not a float (see its row and section 9, item 12). The `ShipStaticScriptTable` names are confirmed: entry 119 = `"N-LIPS"`, entries 126/133/140/147 = `"trailScaleCap"`..`"trailScaleCap4"`, 178 named entries in all.
| VA | Expect | Why |
|---|---|---|
| 0x00524841, 0x005256B7, 0x0053E8A5, 0x0053E8DB, 0x0053E9B5, 0x005AC174, 0x004CC8F2, 0x004CC801, 0x0052AF12 | `68 00 00 80 3D` (push 0.0625f) | CET overwrites each with a same-length `push imm32` |
| 0x004CC8BE | `C7 05 B4 CA A4 00 00 00 80 3D` | `mov [0x00A4CAB4], 0.0625f` |
| 0x0054ACDB | `C7 05 BC 2A B6 00 00 00 80 3D` | `mov [universe.phystimeelapsed], 0.0625f` |
| 0x0048CBD0, 0x0048CC57 | `C7 44 24 1C 00 00 80 3D` | `mov [esp+1C], 0.0625f` |
| 0x005ABF68 | `68 F0 00 00 00` | push 240 (timer resolution) |
| 0x0046E042 / 4F / 5B | `D8 15 xx xx xx xx` / `D8 25 ...` / `D9 05 ...` | fcom/fsub/fld m32 (operand -> float 0.015625) |
| 0x004CC634 / 0x004CC701 / 0x004CC72C / 0x004CC7B8 | `D8 05` / `D8 1D` / `D8 0D` / `D8 05` + abs32 | fadd/fcomp/fmul/fadd m32 (operand -> float 0.0625) |
| 0x005ABB21 / 3E / 5C | `68 <abs32>`, operand pointing at an ASCII bigfile name | push string |
| 0x00562401 | `0F 84 <rel32>` | 6-byte `je` |
| 0x0072A614 | float 240.0 (`00 00 70 43`) | TimerResolutionMax |
| 0x0072A818 | float 0.03125 (`00 00 00 3D`). **[exe 12948]** **Wrong**: the bytes are `00 00 00 00 00 00 A0 3F` = **double** 0.03125 | ETG_UpdateRoundOff |
| 0x0072A82C | float -16.0 (`00 00 80 C1`) | etgEffectorCI constant |
| 0x0089410C | float (HW1 default 0.075 before scripts run) | HYPERSPACE_SLICE_RATE |
| 0x0088F580 | dword 255 (HW1 default) | regenerateRUrate |
| 0x008B8850 | ASCII network version, buffer 16 B | networkVersion |
| 0x008B2428 + 16*k | `{char* name, setVarCB, offset1, offset2}` records; entry 119 name likely `"N-LIPS"`; entries 126/133/140/147 = `"trailScaleCap"`, `"trailScaleCap2..4"` | ShipStaticScriptTable |

### 10.3 Function anchors with unique HW1 twins (diff against `HW1/` decompile-by-eye)
Best first targets, because they are small, have distinctive constants or strings, and HW1 source exists: `univUpdate 0x0054C3F0` (calls `rmUpdateResearch 0x004F2A20` and `univCheckRegrowResources 0x0054C270` between 0x0054C83D-0x0054CD8F), `univUpdateReset 0x0054ACD0`, `CheckPlayerWin 0x005485E0`, `CollectResources 0x004F17A0` (both: Ghidra containers of the CET sites, `ghidra/exports/cataclysm_gog/cet_apply_report.csv`), `scriptSet 0x00526420`, `scriptSetReal32CB 0x005259D0`, `scriptSetUdwordCB 0x00525C20`, `scriptSetTweakableGlobals 0x00527820` (string xrefs to `"tweak.script"`, UNVERIFIED), `ShipTypeToStr 0x004D6BC0` (string table of ship names), `etgNParticleBlocksSet 0x0046E110` (`"%d"` sscanf), `trailSegmentsRead 0x005A7A70`, `opOptionsAccept 0x004D7E30`, `rinDeviceCRC 0x0057DCF0`, `rinSortModes 0x0057DE00`, `mainRescaleMainWindow 0x0055FC70`, `SaveGame 0x004FDAF0`, `memAlloc 0x006725D0` / `memFree 0x00673080`. These were expected to be among the most-called functions. **[exe 12948]** Wrong: they have only 39/30 call sites and are the debug-CRT `malloc`/`free`, not Relic's allocator (section 3b). Relic's are probably 0x004ADEE0/0x004AE130 (section 3d).

Struct anchors for typing `universe` (0x00B62A00): `+0xBC phystimeelapsed`, `+0xC0 totaltimeelapsed`, `+0xC4 realtimeelapsed`, `+0x1AC univUpdateCounter`, `+0x4AF8 numPlayers`, `+0x4B00 players`. `Ship`: `+0x20 staticinfo`, `+0x2B0 shiptype`, `+0x32C playerowner`, `+0x370 command`. `ShipStaticInfo`: `+0x1E8 shipclass`, `+0x238 buildCost`, `+0x23C buildTime`, `+0x25C harvestRate`, `+0x410 supportUnits`.

### 10.4 Import procedure
`ghidra/cet_symbols.json` follows the schema `{target, symbols[{address,name,kind,type,source,hw1_equivalent,notes}]}`. There is no `derived` flag. The 25 derived entries are the `data` entries named `universe_*`, `tpGameCreated_*` or `*_hyperspaceState`, and 23 of them have `notes` starting `Derived`. `ghidra/scripts/ImportSymbolsJson.py` does not skip them; it labels them like any other `data` entry. Suggested Ghidra script behaviour: for `function`, create the function and set its name/signature (parse `type` leniently); for `data`, label and apply the type if known; for `patch_site`, add a pre-comment holding `notes` and a bookmark `CET`. Skip `derived` entries until the struct types are applied.

---

## 11. Gaps: what CET does not know

- **Executable identity**: no hash/size/timestamp/version string of the target. CET's address map applies to whatever exe the author had (presumably the 2017 GOG build) (section 1). **[exe 12948]** The byte check shows the map fits GOG build 12948 (sha256 `f03d9a3e…`).
- **Most of the exe**: CET names about 56 functions and about 50 globals in a ~2.5 MB code range. Most Cataclysm-new systems are unnamed: support units, modules/upgrades (`ModuleDefs.h` has only 5 IDs), Beast infection (`beastSystemType`, `beastAttack*`), leech/EMP/cutting beam/ramming, mimic, DFG, pilot view, Worker/Processor repair, external construction (`aiUpdateExternalShipBuild` known only by two instruction addresses), crates, the skirmish AI apart from `aisFleetUpdate`/`aisRequestShip`/`sub_53F9F0`.
- **Struct sizes**: true sizes of `Player` (CET's commented 0x6530; **[exe 12948]** confirmed as the `players[]` stride, but the layout of the ~0x6250 inserted bytes is unknown, section 4.1), `Universe` (>= 0x3DA3C once `players` is 9 x 0x6530), `Ship` (>=0x660 plus ShipSpecifics), `ShipStaticInfo` tail (>0x454 "do not use"), `CommandToDo` (0x330), `CaptainGameInfo` (688), `GameStats`/`unk3`, `AIPlayer` (+8 somewhere before 0xB9C), `Effect`, `Bullet`. Large `unk*` blocks: `ShipStaticInfo.unk/unk2/unk6/unk7/unk8/unk9-11`, `Ship.unk_18/1C/164-16C/unk2[0x50]/38c-398/3D0-3D8/3E0-3E4/3EC-3FC/410-418/4C8-5B0/5C4-64C`, `Universe.unknown1/unk1/unk3`, `particleAttribs.unknown[3]` and the two new particle types.
- **`Ship` 0x174-0x23C**: CET's commented-out asserts (`spaceobj.h:1820-1822`) put `putOnDoor`/`dockingship`/`specialFlags` 8 B later than its compiled layout; CET could not make them pass, so `nonstatvars`, `clampInfo`, `salvageInfo`, `aistate`, `specialFlags*`, `salvageNumTagged`, `navLightInfo` offsets are all unconfirmed.
- **Which of `RetreatList`/`AttackMemory` was removed** from `Universe` (`universe.h:185-186`).
- **`SPECIAL_*` flag values** may have shifted ("`SPECIAL_rowGettingOutOfWay` likely 0x00010000 now", `spaceobj.h:1489`).
- **Callers/owners not identified**: `univUpdate1/2` caller (Ghidra: unnamed FUN_00524790 / FUN_00525640), `SensorHack` purpose (the site is a `jle short`), the meaning of `push 0x28` at 0x00589115, `LoadMission` naming vs `trCramRAMComputeAndScale`, the meaning of `rmCanBuildShip`'s third arg, the `clCommandMessage` flags arg, the meaning of global 0x00A74D50 (`ActivateMe`). Resolved by the byte check: the frame limiter is `Sleep(1)` x2 in FUN_0057B760, and `memAlloc` is CRT `malloc(size)` (1 arg).
- **Original bytes at patch sites**: never verified by CET (section 2/10). **[exe 12948]** Now verified for every Keystone and mid-function site (## Verification).
- **File formats**: CET touches none of `.big`, `.mex`, `.lif`, `.fib`, `.lut`, `.shp`, savegames, or network packets beyond what HW1 already documents.
- **Renderer**: hw_rgl replaces `rgl.dll`/`rgld3d.dll` wholesale. It documents the DLL ABI but not how `cataclysm.exe` drives it (call sites of `rglFeature`, `glDLLGetProcs`, `glToSelect` choices) or the stock `rglsw.dll`.
- **Audio/video**: nothing on the Cataclysm sound engine beyond `soundStartDSound`/`fqEqualize`, nothing on AVI/Bink intro playback beyond `enableAVI`.
- **Networking**: only the version strings and `tpGameCreated` intervals; nothing on the WON/Titan lobby (`TitanInterfaceC.h` is an HW1 copy with a "struct has changed" note).
- **Scripting**: KAS mission scripting for Cataclysm (KAS.h/KASFunc.h are HW1 copies; Cataclysm missions differ).
- **Correctness**: CET is a working patch, not a decompilation. Its asserted offsets are reliable for the fields it uses, but its unasserted HW1-copied layouts and reimplemented functions (`aisFleetUpdate`, `mgResource*`, `soundStartDSound`, `rinSortModes`, `glCapNT`) are behavioural approximations.


---

## Verification

Skeptical re-check, 2026-10-08. Sources: the reference clones (CET `19a08c3`, hw_rgl `41ff3c3`, HomeworldSDL, GoK); the real GOG exe that has meanwhile been extracted to `original/game/cataclysm.exe` (build 12948, sha256 `f03d9a3e…`, matching `docs/binary-fingerprint.md`), read-only via Python byte reads, with nothing executed or modified; and the existing Ghidra export `ghidra/exports/cataclysm_gog/` produced by another agent. Struct offsets were re-derived with `clang++ -target i686-pc-windows-msvc -fms-extensions` over CET's headers (case-insensitive include links plus stub `windows.h`). Keystone encodings were re-assembled with `keystone-engine` at each target VA. The installed CET (`original/game/CataclysmEngineTweaks.ini`) is byte-identical to the repo ini after CRLF stripping. The installed DLL is dated 2024-08-22, the same day as commit `19a08c3`; that the release build equals this commit is UNVERIFIED. The M0 run log (`original/game/CataclysmEngineTweaks.log`) shows `RGL patches applied` / `Update rate patches applied` / `Patches applied` / `DSound init OK` / `Universe reset called` (`PatchRGL.cpp:110`, `PatchUniverse.cpp:457`, `Main.cpp:48`, `game/Win32/soundlow.cpp:87`, `PatchUniverse.cpp:45`). So every MinHook target and Keystone write in the default config was accepted by this exe under Proton.

### Claims checked

| # | Claim (section) | How checked | Result |
|---|---|---|---|
| 1 | Image-base-only pinning; `LogAndThrow` format bug; fires before `InitLogger` (1) | `dllmain.cpp:9-41`, `Main.h:13-21` | Correct |
| 2 | No ASLR in the exe (1) | PE header: `DllCharacteristics` 0, empty reloc directory | Correct (was UNVERIFIED) |
| 3 | 0x0072Axxx constants live in `.data`, not `.rdata` (1) | PE section table | **Wrong**: they are in `.rdata`, which this exe flags writable (0xC0000040). Fixed |
| 4 | Patch order, shared Keystone instance (2.1) | `Main.cpp:33-49` | Correct |
| 5 | Assembler: no length/orig-byte check, no `FlushInstructionCache` (2.1) | `Assembler.cpp:19-49` | Correct |
| 6 | Keystone bytes for all 17 distinct CET asm strings (2.2) | Keystone re-assembly at each VA | Correct; each equals the original instruction length in the exe |
| 7 | 64 Hz constants (0x3C800000, P/2, P/4, 960, 1023, 0.00625) (2.2) | `Config.h`, `PatchUniverse.cpp:276-289` | Correct |
| 8 | "Expected original" bytes of all Keystone and mid-function sites (2.2, 10.2) | Exe bytes | 35/36 10.2 predictions match. 0x0072A818 is a **double**, not float (**wrong**; fixed). New facts recorded at 0x0054860C, 0x00543F48, 0x00562084, 0x0057B78B/A1, 0x004DD43E, 0x0050A43B, 0x00589115, 0x00589AEA, 0x004F17C9, 0x0054C83D, 0x0054CD73, 0x0042090D |
| 9 | CheckPlayerWin original is "the `singlePlayerGame` test" (2.2) | Exe bytes `A1 94 3C A4 00 85 C0 0F 84 …` | **Wrong**: it tests `multiPlayerGame`. Fixed in 2.2, 3a, 5 |
| 10 | 0x00562084 skips a "renderer re-init call" (2.2, 3a) | Exe bytes | **Imprecise**: it is `mov dword [0x00A74D50], 2`. Fixed |
| 11 | Every CET address literal is in the JSON; JSON citations point at the literal/assert (3) | grep of all 199 `0x00xxxxxx` literals (the 40 not in the JSON are flag constants); per-entry line check | Correct (161 literal lines, 23 derived assert lines) |
| 12 | "Section 3 is generated from the JSON, so the two always agree" (3) | Regenerated every row from the JSON and diffed | **Two rows had drifted** (`universe_realtimeelapsed`, `sub_53F9F0`). Fixed by regenerating; JSON and doc now agree again (empty `type` renders as `-`) |
| 13 | "25 derived" `universe_*`/… entries are not CET literals (header, 3) | JSON sources | **Partly wrong**: 2 of the 25 are literals. Fixed |
| 14 | `memAlloc`/`memFree` = Relic `Memory.c`, "among the most-called" (3b, 10.3) | `functions.csv`, `decompiled.c:306937-306951,307485-307489`, HW1 `rinit.c:45-58` | **Wrong**: debug-CRT `malloc`/`free` (39/30 call sites), consistent with HW1 `rinit.c`. Fixed in JSON and doc |
| 15 | SongNumber slot is HW1 `SMMissionTweaks` entry 9, base 0x008925FC (3c, 9.11) | Exe data + `decompiled.c` | **Wrong**: `{ptr,size}[3]` pre-game table at 0x00892660, and 0x008925FC is ASCII. Fixed; `SongNumber` = 0x00883960 |
| 16 | `ShipStaticScriptTable` entry 119 = `"N-LIPS"`, trail entries, `nlips` 0x30C (3c, 10.2) | Exe table (178 entries) | Correct |
| 17 | `StaticHeader` "unchanged vs HW1" (4.1) | 178 exe script-table offsets vs clang-computed CET offsets | **Wrong past 0x4C**: 171/178 match; `preciseSelection`/`collBox*` are +0xC in the exe. CET's `ShipStaticInfo.unk[12]` belongs inside `StaticCollInfo`. Fixed in 4.1 |
| 18 | CET struct sizes and the commented-out `Ship` asserts (4) | clang: `Ship` 0x660, `ShipStaticInfo` 0x4CC, `Universe` 0x65C4; `putOnDoor`/`dockingship`/`specialFlags` 0x1BB/0x1BC/0x1C0 | Correct; all CET asserts pass |
| 19 | Asserted-offset citations in 4.2 (228 rows) | Script: cited line must name the field and offset | 1 **wrong line** (`gameStats` :254 → :253, fixed); 21 rows cite a sub-field assert (note added) |
| 20 | Bare-`bool` fields only match "by luck" (4) | Exe script callbacks | Confirmed and sharpened: `repairBeamCapable` is bool32, `externalBuild` is bool8 in the exe |
| 21 | Config keys, defaults, INIReader lower-casing (7) | `Config.cpp:15-53`, `Config.h`, ini | Correct |
| 22 | First tick autosaves at shift 0 even with interval 0 (9.1) | `GameState.h:5`, `GameHooks.cpp:22-39`, `PatchUniverse.cpp:428-431` | Correct |
| 23 | Beam-gating precedence (9.6) | `PatchUniverse.cpp:246` | Correct |
| 24 | Loader sequence, suspended-child failure path (8) | `loader/Main.cpp:9-121`, `Utilities.cpp:5-43` | Correct; added case-sensitive kill and the dangling-pointer consequence of a mid-`ApplyPatches` failure |
| 25 | hw_rgl ABI claims (6) | `wgl.c:6-13`, `draw.def` (105 gl + 4 wgl), `kgl.c:352-367,1224-1236,2808-2812,4693,6397`, `kgl.h:112-113,566-567,677,684-685`, `d3dinit.cpp:120-143,167-172,316`, `d3driver.cpp:2155-2171` | Correct |
| 26 | HW1 file:line citations in 3 and 5 (47 spot checks) | `sed -n` on each line | Correct |
| 27 | tweak.script frame-mask lines, `sMothership.shp:89` (7, 9) | `assets/tweak.script:15-16,516,754,778,801`, `.shp:89` | Correct; per-ship `regenerateRUrate` binding is UNVERIFIED (not in the 178-key table) |
| 28 | Status "binary not in hand" (header) | `original/game/`, `docs/binary-fingerprint.md` | **Stale**. Fixed |
| 29 | Version strings "presumably 1.01" (1) | Exe data | Originals are `"Emergence 1"` / `"01"`. Filled in |
| 30 | Research/regrow masks are constants (2.2, 3a) | Exe bytes | Incomplete: the exe reads them from globals. Added (9.14) |

**Wrong or stale claims found: 11** (rows 3, 8, 9, 10, 12, 13, 14, 15, 17, 19, 28), all fixed in place. JSON changes: 59 entries changed. `notes` were extended or corrected on all 59 (tagged `[exe 12948]`), `type` on 5, and `hw1_equivalent` on `memAlloc`/`memFree`; `target.notes` was also extended. Addresses, names, kinds, sources and the 184-entry count are unchanged, so `docs/ghidra-pipeline.md`'s references still hold.

### Added content
- Section 3d: 29 exe addresses discovered by the check (SongNumber, pre-game table, the `"Update.big"` string, pooled FPU constants, tweakable research/regrow globals, the Sleep IAT slot, script-callback functions, probable Relic allocator).
- Section 9 items 12-15. Item 12 is a new CET bug: the `ETG_UpdateRoundOff` write is a no-op because the target is a double. Item 13: the pooled `TimerResolutionMax` constant. Item 14: script overrides ignored. Item 15: the `bool` vs `bool32` hook return.
- Section 8: loader kill is case-sensitive; a DllMain failure after patching leaves pointers into the unloaded DLL.

### Remaining issues
- The exe evidence comes from one build (GOG 12948). The CD release and any other GOG revision are unchecked.
- Some readings rest on single-opcode decodes and on Ghidra's decompile, without full disassembly review: 0x00A74D50's meaning, the glTexParameteri name for slot 0x00B3A444, the save-side role of FUN_004FDA00, and the Relic allocator guesses 0x004ADEE0/0x004AE130.
- Where the 12 extra `StaticCollInfo` bytes sit (0x50-0x87) is not pinned down. Also open: whether other static-info structs shift with it, and whether the 8 users of 0x0072A614 are all timer conversions.
- The `Ship` 0x174-0x23C region is still unresolved. The 0x1CC flag test is only weak support for the commented +8 asserts.
- `Player` true size, `AIPlayer` +8, `CaptainGameInfo` 688-B layout and `CommandToDo` 0x330 are untouched by this pass (section 11). (Pass 2 settled the `Player` size as 0x6530. The rest are still open.)
- Behaviour claims about Windows/MinHook internals (DllMain FALSE → DETACH/unload, `MH_Uninitialize` reverting hooks, MSVC bool codegen) are from documentation/knowledge, not from code in the repo.
