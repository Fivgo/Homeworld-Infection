# Homeworld 1 engine: presentation layer

A reference map of the HW1 presentation code that Cataclysm forked: renderer, asset formats, effects, front end, in-game UI,
input, sound, cutscenes and networking. It is written for the engineer who decompiles the Cataclysm executable (GOG
"Homeworld: Emergence", `cataclysm.exe`) and builds the native Windows 11 / Linux port. The companion documents are
`docs/hw1-engine-gameplay.md` (simulation), `docs/hw1-port-lessons.md` (what the SDL port had to change) and
`docs/cet-knowledge.md` (everything CET knows about the binary).

## 0. Sources, conventions, caveats

| Prefix | Location | Version read |
|---|---|---|
| `GoK:` | `reference/gardens-of-kadesh/` | HEAD `3af31fb`. **Primary source for line numbers** (task brief: prefer GoK). |
| `ORIG:` | Relic's untouched 2003 source = **root commit `ad1bb82`** of the GoK history ("Original Relic code, as published in 2003"). Not checked out in `reference/` (the clones are depth 1). | Read from a blobless clone in scratch: `git clone --filter=blob:none https://gitlab.com/gardens-of-kadesh/gardens-of-kadesh.git; git show ad1bb82:src/Win32/glcaps.c`. The files are Latin-1, so use `grep -a`. File names are lower-case in ORIG (`src/Game/mesh.c`, `src/Win32/render.c`). |
| `SNAP:` | `GoK:zarchive/original-release-snapshot/` | In tree. It holds Relic's `documents/technical/*.doc`, `tools/`, `exe/Homeworld.exe` (symbolized, see 0.1) and a partial `src/Win32/`. |
| `RGL:` | `reference/hw_rgl/` | HEAD `41ff3c3` ("Handle Cataclysm attrib stack calls"). `CET:game/rgl` is an **empty git submodule** that points at this repo (`CET:.gitmodules`), so `RGL:` stands in for "CET/game/rgl". |
| `CET:` | `reference/CataclysmEngineTweaks/` | HEAD `19a08c3`. Binary addresses are in `CET:Cataclysm.h`. They are valid for the GOG `cataclysm.exe` at image base 0x400000 only (`CET:dllmain.cpp:9-18`). |
| `DOC:` | `SNAP:documents/technical/*.doc` | Converted to text with headless LibreOffice. These files have no line numbers, so they are cited by document and section. |

- `file:N` means line N; `file:N-M` means a range. Where a GoK and an ORIG anchor are both given, the ORIG anchor is the one that matches what Cataclysm still does.
- **UNVERIFIED** marks claims I could not ground in a file I read. **Inferred** marks a conclusion drawn from grounded facts.
- `reference/HomeworldSDL` (HEAD `1bed493`) is the same lineage as GoK. GoK supersedes it, and it was used only where noted.
- **Tick-rate warning.** GoK runs the universe at 64 Hz by default (`GoK:src/Game/Universe.c:75` `UNIVERSE_UPDATE_RATE_SHIFT = 2`, `/vanilla` restores 16 Hz, `GoK:src/SDL/main.c:848,2195-2197`). ORIG and Cataclysm run at **16 Hz** (`ORIG:src/Game/universe.h:57`). Every presentation timer below (NIS task, ETG `at()/every()`, trails) assumes 16 Hz.

### 0.1 Highest-value facts for the decompiler

1. `SNAP:exe/Homeworld.exe` (11,462,804 bytes, PE32, linker 5.10, PE timestamp 2003-08-22, image base 0x400000) carries **embedded CodeView NB11 debug info** at file offset 0x4A9C00. Its symbol strings include `_glCapStartup`, `_rndRenderTask`, `_etgFunctionCall`, `_feScreensLoad`, `_lodLevelGet`, `_soundStartDSound`, `_partRenderBillSystem`, `_WindowProc` and `_kbCheckBindings` (verified with `grep -a`). It is a **symbolized HW1 build of the same engine**, so it can drive function matching (BinDiff/FunctionID/BSim) against `cataclysm.exe`.
2. The engine is full of **name→function-pointer tables** that survive into release builds. Each one names functions and globals in the binary:
   - ETG: `etgParseTable` (GoK:src/Game/ETG.c:258), `etgFunctionTable`, which holds 153 active script-callable entries, 150 distinct names (GoK:src/Game/ETG.c:501-694; ORIG has 154 because `modifyLength` is listed twice, ORIG:src/Game/etg.c:518-519), and `etgHandleTable` (GoK:src/Game/ETG.c:127).
   - Front end: `fecallback` tables `{function, "Name"}` (e.g. `utyCallbacks`, GoK:src/SDL/utility.c:431; `smCallbacks`, GoK:src/Game/Sensors.c:3917; `cmCallback`, GoK:src/Game/ConsMgr.c:178).
   - Scripts: `makeEntry(var, cb)`, which expands to `{ "var", cb, &var }` (GoK:src/Game/StatScript.h:45, `strdollar` = `#x`, GoK:src/Game/Types.h:262). Every tweakable global therefore appears as a string next to its address.
   - rGL: `glCapGetRGLAddresses` (ORIG:src/Win32/glcaps.c:217-249).
3. Cataclysm keeps HW1's **asset magics and versions**. A Cataclysm `.fib` taken from CET parses with the HW1 layout: `"Bananna"`, version 0x0212, 41 screens (`CET:assets/feman/multiplayer_game.fib`, see §4.2). A Cataclysm `.lut` matches HW1's `SFXLUT` (see §7.4).
4. rGL (`rgl.dll` plus `rgld3d.dll`) is a **separate DLL that exports an OpenGL-1.1 subset**. The game binds every `gl*` call through function pointers, so a port can replace both DLLs with a real GL (or GLES/Vulkan) context without touching the game's GL call sites (§1).

---

## 1. Renderer abstraction (rGL vs native OpenGL)

### 1.1 Architecture

```
game code (Game/*.c, Win32/*.c)
   │  calls gl*() through GLOBAL FUNCTION POINTERS (gldll.h typedefs; ORIG:src/Win32/gldll.c:46-166)
   │  calls rgl*() through pointers fetched with wglGetProcAddress (ORIG:src/Win32/glcaps.c:84-249)
   ▼
glDLLGetProcs(dllName)  ── LoadLibrary(dllName) + GetProcAddress for each name  (ORIG:src/Win32/gldll.c:405-629)
   ├── "opengl32.dll" → native ICD (RGLtype = GLtype = 3).        Context: wgl via setupPixelFormat (ORIG:src/Win32/render.c:813-872)
   ├── "<cwd>\3dfx\<subdir>\opengl32.dll" / "3dfxvgl.dll" → 3dfx MiniGL (RGLtype = GLtype, gl3Dfx=TRUE)  (ORIG:src/Win32/main.c:1865-1890, gldll.c:419-452)
   └── "rgl.dll" → Relic GL ("RelicGL rendering library", RGL:draw.def:1-2; Mesa-like internals such as GLcontext/gl_problem/NewMask: Inferred)
          │ rgl.dll keeps the GL state machine, transform, clipping and lighting (RGL:kgl.c, kvb.c, clip.c)
          │ then rasterises through a driver DLL loaded by name "rgl"+device:  (RGL:kgl.c:2781-2840)
          ├── rglsw.dll  ("sw", software rasteriser; RGLtype = SWtype = 0)
          ├── rgld3d.dll ("d3d", Direct3D; RGLtype = D3Dtype = 2)
          └── rglfx.dll  ("fx", Glide; table entry commented out in both ORIG and RGL: RGL:kgl.c:364)
```
`RGLtype` constants: `SWtype 0`, `GLIDEtype 1`, `D3Dtype 2`, `GLtype 3` (ORIG:src/Win32/glext.h:103-106).

### 1.2 Startup / selection flow (Win32 original)

| Step | What happens | Anchor |
|---|---|---|
| 1 | `WinMain` sets `glNT = glCapNT()` (TRUE on NT ≤ 4, or if `GetVersionEx` fails) and `gl95 = glCap95()` | ORIG:src/Win32/main.c:3352-3353, glcaps.c:533 |
| 2 | `InitWindow()` calls `mainDevStatsInit()` and then `rinEnumerateDevices()`. The device list is the D3D devices (`DirectDrawEnumerate` → `IDirectDraw4` → `IDirect3D3::EnumDevices`, rinit.cpp:798-828, 923), then "Default OpenGL" (localized names), an optional "3dfx OpenGL", and "Software" | ORIG:src/Win32/main.c:3062-3063; rinit.cpp:1017-1198 |
| 3 | Per-card capability bits come from **`devstats.dat`**: lines of `CRC0 devcaps CRC1 devcaps2 name`, keyed by a CRC of the `D3DDEVICEDESC`. `mainDevStatsInit` loads the table and `rinFindDevCaps` looks entries up | ORIG:src/Win32/main.c:1550-1600; rinit.cpp:725-776, 861; SNAP:exe/devstats.dat (bit legend at top of file) |
| 4 | Mode filter: only 16/32-bit; only 640×480, 800×600, 1024×768, 1280×1024, 1600×1200; width ≤ 1600; devstats bits can veto modes | ORIG:src/Win32/rinit.cpp:178-230, 440-470 |
| 5 | If the saved `opDeviceCRC != rinDeviceCRC()` **and `mainAutoRenderer` is TRUE** (the default, main.c:174; `/noSavedMode` clears it, main.c:1198), the game stores the new CRC and **forces Software 640×480×16** with `rgl.dll` + device `"sw"`. `/sw` (`mainForceSoftware`, main.c:1197) forces the same path unconditionally | ORIG:src/Win32/main.c:3086-3110 (CET hooks this spot, §11) |
| 6 | `glCapLoadOpenGL(glToSelect)`. The default `glToSelect` is `"rgl.dll"` (main.c:218). The live command-line switch is `/device <dev>` (`SelectDevice`, main.c:644-653, table entry :1215): it copies the rGL device name and, for `d3d`, sets `mainReinitRenderer = 2`. The `/gl` and `/d3d` entries are **commented out** of the option table (main.c:1216-1217), so `SelectMSGL` (sets `"opengl32.dll"`, main.c:655-659) and `SelectD3D` (rgl.dll + `"d3d"`, main.c:661-668) have no callers in the ORIG tree (`git grep` at ad1bb82). Switching to OpenGL at run time goes through `mainStartupGL`, which sets `"opengl32.dll"` (main.c:1969-1990). If rgl.dll is loaded, `glCapGetRGLAddresses()` runs (glcaps.c:486, a case-insensitive `_stricmp` on the DLL name) | ORIG:src/Win32/main.c:3135-3156; glcaps.c:481-492 |
| 7 | `glCapStartup()` reads `GL_EXTENSIONS`. **`GL_RGL_rgl_feature` present ⇒ RGL=TRUE**, and then `"direct3d"` in the string ⇒ D3Dtype, otherwise SWtype | ORIG:src/Win32/glcaps.c:616-662 |
| 8 | With rGL: `rglSetAllocs`, `rglFeature(RGL_FULLSCREEN/WINDOWED, RGL_HICOLOR/TRUECOLOR, RGL_SLOWBLT/FASTBLT)`, `rglSelectDevice(deviceToSelect, d3dToSelect)` | ORIG:src/Win32/main.c:3165-3207 (and `mainContinueRGL` :2053-2093) |
| 9 | `rndInit()`. Native GL: `GetDC`, `setupPixelFormat`, `setupPalette`, `rwglCreateContext`, `rwglMakeCurrent`. rGL: `rglCreateWindow(hwnd,w,depth)` then `rauxInitPosition(0,0,w,h,depth)` (fetched via `rwglGetProcAddress`). After that both paths run `glCapStartup()` again, the default GL state, and `glLockArraysEXT/glUnlockArraysEXT = rwglGetProcAddress(...)` | ORIG:src/Win32/render.c:958-1066 |
| 10 | Run-time switches: `mainLoadParticularRGL`, `mainShutdownRGL`, `mainReinitRGL` (D3D only: `rglFeature(RGL_REINIT_RENDERER)`); `mainFreeLibraries` unloads `rglsw.dll`, `rgld3d.dll`, `rgl.dll`, `opengl32.dll` | ORIG:src/Win32/main.c:1904-1926, 2102-2120, 2256, 2529-2586 |
| 11 | `ActivateMe` (WM_ACTIVATEAPP) with rGL: `rglFeature(RGL_ACTIVATE); mainReinitRenderer = 2`, which re-initialises the renderer two frames later inside `rndRenderTask` | ORIG:src/Win32/main.c:1775-1787; render.c:4124-4131 |

### 1.3 The GL function table the engine expects

`ORIG:src/Win32/gldll.c:405-629 glDLLGetProcs` resolves **103 core GL entry points** by exact export name with
`GetProcAddress(lib, "glXxx")`. It also resolves `glLockArraysEXT`/`glUnlockArraysEXT`, takes `glColorTable` from the
export `"glColorTableEXT"` (gldll.c:485), and resolves **9 `wgl*`** functions into `rwgl*` pointers (gldll.c:611-619).

- If any core function is missing, the load fails (gldll.c:606).
- For `wgl*`, failure is fatal only when `wglSwapBuffers` was found (gldll.c:621). This is what lets `rgl.dll`, which exports only 4 `wgl*`, pass.
- `glBegin`, `glBindTexture`, `glDeleteTextures`, `glGenTextures`, `glEnable` and `glDisable` go through `_gl*` indirections. The point is optional debug wrappers (gldll.c:19-24), plus devstats `DEVSTAT2_NO_DALPHA`, which swaps `glEnable`/`glDisable` for dither-suppressing versions (gldll.c:497-507).
- The typedef list (`__stdcall`, 115 prototypes) is reproduced in `CET:game/Win32/gldll.h:32-147`.

The core set is these 103 functions (identical to `RGL:draw.def:4-108`, minus the `glColorTable(EXT)` alias):
`AlphaFunc Begin BindTexture Bitmap BlendFunc Clear ClearColor ClearDepth ClearIndex Color3f Color3ub Color4f Color4ub ColorMask CullFace DeleteTextures DepthFunc DepthMask DepthRange Disable DrawArrays DrawElements DrawBuffer DrawPixels Enable End EvalCoord1f EvalCoord2f EvalMesh1 EvalMesh2 EvalPoint1 EvalPoint2 Flush Fogf Fogfv Fogi Frustum GenTextures GetDoublev GetError GetFloatv GetIntegerv GetBooleanv GetString GetTexLevelParameteriv Hint InterleavedArrays IsEnabled LightModelf LightModelfv LightModeli Lightfv LineStipple LineWidth LoadIdentity LoadMatrixf Map1f Map2f MapGrid1f MapGrid2d Materialfv MatrixMode MultMatrixd MultMatrixf Normal3f Normal3fv Ortho PixelStorei PixelTransferf PointSize PolygonMode PopAttrib PopMatrix PushAttrib PushMatrix RasterPos2f RasterPos2i RasterPos4f ReadBuffer ReadPixels Rotatef Scalef Scissor ShadeModel TexCoord2f TexEnvi TexImage1D TexImage2D TexParameteri Translated Translatef Vertex2f Vertex2i Vertex3f Vertex3fv Vertex4fv Viewport ClipPlane GetTexImage VertexPointer EnableClientState DisableClientState ArrayElement`.

The 9 wgl functions are `CreateContext DeleteContext GetProcAddress MakeCurrent ChoosePixelFormat SetPixelFormat GetPixelFormat DescribePixelFormat SwapBuffers`. `rgl.dll` exports only `wglCreateContext/DeleteContext/GetProcAddress/MakeCurrent` (RGL:draw.def:109-112; stubs in RGL:wgl.c), and its `wglGetProcAddress` serves the table in RGL:kgl.c:6457-6505.

**Port implication.** In a reconstructed `cataclysm.exe` these pointers are globals in `.data` that get written once by
`glDLLGetProcs`. A port can fill them from SDL/GLAD instead. The game never links `opengl32.lib`.

### 1.4 rGL extension entry points (fetched via `wglGetProcAddress`)

`ORIG:src/Win32/glcaps.c:84-208` declares these entry points. Each one has a `NULL_rgl*` stub, installed by `glCapResetRGLAddresses` (glcaps.c:259-294) when the native GL is active. `glCapGetRGLAddresses` (glcaps.c:217-249) fills them in.

| Function | Signature | Used for |
|---|---|---|
| `rglFeature` | `int (unsigned feature)` | Mode toggles and queries (table 1.5) |
| `rglSpecExp`, `rglLightingAdjust` | `(GLint, GLfloat)`, `(GLfloat)` | Software specular / lighting tweak |
| `rglSaveCursorUnder`, `rglRestoreCursorUnder` | `(GLubyte*, w, h, x, y)` | Software mouse cursor save-under (ORIG:src/Win32/mouse.c:883-978) |
| `rglIsFast` | `uchar (unsigned)` | `glCapFastFeature(GL_BLEND)` (glcaps.c:303-321) |
| `rglCreateWindow`, `rglDeleteWindow` | `(hwnd, w, depth)`, `(GLint)` | Window/surface creation |
| `rglIsClipped` | `(GLfloat*, GLfloat, GLfloat, GLfloat)` | Bounding-sphere clip test |
| `rglNumPolys`, `rglCulledPolys` | `GLuint ()` | Statistics |
| `rglBackground` | `(GLubyte*)` | Blit a full-screen background |
| `rglSetAllocs` | `(MemAllocFunc, MemFreeFunc)` | rGL uses the game's heap |
| `rglSuperClear`, `rglEnable`, `rglDisable` | | rGL-private state |
| `rglListSpec`, `rglList`, `rglNormal`, `rglTriangle`, `rglTexturedTriangle`, `rglSmoothTriangle`, `rglSmoothTexturedTriangle`, `rglMeshRender` | | Fast mesh path for software rGL (RGL:rglext.c:57-640; ORIG:src/Game/mesh.c:47-66 `MESH_OBJECT_RENDER`) |
| `rglSelectDevice`, `rglD3DSetDevice`, `rglD3DGetDevice` | `(char* name, char* data)` | Pick the `"sw"` or `"d3d"` driver and the D3D device GUID |
| `rglGetFramebuffer`, `rglDrawPitchedPixels` | | Direct framebuffer access for the fast front end (glcompat) |
| `rauxInitPosition` | `int (x, y, w, h, depth)` | Creates the context (RGL:kgl.c:2980) |
| `glColorTableEXT`, `glLitColorTableEXT` | | Paletted textures (ORIG:src/Win32/texreg.c:213-220, 280-287) |

### 1.5 `rglFeature` codes (RGL:kgl.h:622-680, identical in ORIG:src/rgl/kgl.h)

| Code | Name | Code | Name |
|---|---|---|---|
| 0x1000 | RGL_ISA_BACKGROUND | 0x4000-0x4003 | RGL_SPECULAR_RENDER / NORMAL_RENDER / SPECULAR2 / SPECULAR3 |
| 0x1001 | RGL_FULLSCREEN | 0x5000/0x5001 | RGL_SANSDEPTH / RGL_NORMDEPTH |
| 0x1002 | RGL_WINDOWED | 0x6000/0x6001 | RGL_GAMMA_UP / DN |
| 0x1003 | RGL_TRUECOLOR | 0x7000/0x7001 | RGL_CHROMAKEY_ON / OFF |
| 0x1004 | RGL_SLOWBLT | 0x8000/0x8001 | RGL_FASTBIND_ON / OFF |
| 0x1005 | RGL_NEXT_RENDERER | 0x9000-0x9004 | RESET/GET_LINECOUNTER, RASTERIZE_ONLY, BROKEN_MIXED_DEPTHTEST, COLOROP_ADD |
| 0x1006 | RGL_ACCELERATED | 0x3000/0x3001 | RGL_FEATURE_ALPHA / BLEND (also RGL_EFFECTPOINT / MAPPOINT); 0x3010/0x3011 RGL_FEATURE_NOLINES / LINES (RGL:kgl.h:662-663, also in ORIG) |
| 0x1007/0x1008 | RGL_ACTIVATE / DEACTIVATE | 0x4500/0x4501 | RGL_2D_QUADS / RENDER_2D_QUAD |
| 0x1009/0x100A | RGL_LOCK / UNLOCK | 0x4600/0x4601 | RGL_SAVEBUFFER_ON / OFF (cursor save-under; set every frame in `rndRenderTask`, ORIG:src/Win32/render.c:3913-3940) |
| 0x100B/0x100C | RGL_FASTBLT / HICOLOR | 0x4610 | RGL_TEXTURE_LOG |
| 0x100D | RGL_REINIT_RENDERER | 0x4620 | RGL_SHUTDOWN |
| 0x100E | RGL_SPEEDY | 0x4630/0x4631 | RGL_D3D_SHUFFLE / RGL_D3D_FULLSCENE |
| 0x2000-0x2002 | RGL_SCREENSHOT / MULTISHOT_START / END | 0x4632 | RGL_D3D_D3D9 (**hw_rgl only**, new) |
| | | 0x4640/0x4641 | RGL_SKIP_RASTER / NOSKIP_RASTER (`/rasterSkip`) |

### 1.6 rGL internals (rgl.dll ↔ driver DLL)

- **Device table.** `gl_load_devices` hard-codes `sw` (aliases `software_kgl`) and `d3d` (`d3d_direct3d_directaxe_ms`). The `fx` entry (glide) is commented out, and the parser for `rparams.dat` exists (`gl_parse_devices`) but is disabled. Default renderer: `"sw"` (RGL:kgl.c:280-368).
- **Driver load.** `gl_driver_init` loads `"rgl" + devices[activeDevice].name` (→ `rglsw`, `rgld3d`) and calls the export **`init_driver(ctx)`**. It then copies the first two letters of the device name into the renderer string `"rgl   "` (RGL:kgl.c:2781-2843). `_rgl_init` allocates the buffers and calls `driver_caps` and `post_init_driver` (RGL:kgl.c:2900-2975).
- **Driver vtable.** `gl_driver_funcs_s` = `init_driver, post_init_driver, shutdown_driver, create_window, delete_window, set_save_state, get_scratch, driver_caps (required), allocate_depthbuffer/colorbuffer, clear_depthbuffer/colorbuffer/both, lock_buffer, unlock_buffer, get_framebuffer, setup_triangle/line/point/raster, set_monocolor, flush, clear_color, scissor, flush_batch, draw_triangle/_array, draw_quad, draw_triangle_fan/_strip, ...` (RGL:kgl.h:246-330+).
- **Strings.** `STR_VENDOR = "Relic Entertainment, Inc. [1.1a]"` in ORIG and `[1.3a]` in hw_rgl. `STR_VERSION = "1.1   "` in ORIG and `"1.3   "` in hw_rgl. `STR_EXTENSIONS = "xxxxxxxx GL_RGL_rgl_feature GL_EXT_rescale_normal GL_EXT_paletted_texture [GL_EXT_shared_texture_palette] GL_RGL_lit_texture_palette"` (RGL:kgl.c:70-77). The D3D driver overwrites the `xxxxxxxx` with `"direct3d"` through `rglSetExtensionString` (RGL:D3D9/d3driver.cpp:2319; D3D7/d3driver.cpp:2683), and that word is the token `glCapStartup` keys on. `STR_VERSION` is not constant at run time: context init overwrites its last three characters with `MMX` (MMX CPU) or `XMM` (Katmai/SSE) (ORIG:src/rgl/kgl.c:2477-2488; RGL:kgl.c:2507-2518), so a live `GL_VERSION` reads e.g. `"1.1MMX"`.
- **Original D3D driver.** It uses DirectDraw4 + `IID_IDirect3D3`, i.e. the DX6 interfaces (ORIG:src/rgl/d3dinit.cpp:35-48, 493). hw_rgl ships two replacements: `RGL:D3D7/` (legacy) and `RGL:D3D9/` (`TargetName rgld3d`, RGL:D3D9/rgld3d9.vcxproj:108).
- **hw_rgl D3D9 specifics:**
  - `D3DFMT_D24X8` depth, which fixes distant-ship Z-fighting (RGL:D3D9/d3dinit.cpp:50,130).
  - Windowed-capable device; `PresentationInterval = IMMEDIATE` (d3dinit.cpp:121-130).
  - **`D3DCREATE_FPU_PRESERVE` is mandatory**: "Audio and various other things will break in Cataclysm if D3D changes the FPU precision" (RGL:D3D9/d3dinit.cpp:141).
- **Cataclysm-specific rGL requirement.** Cataclysm calls `glPushAttrib(GL_BLEND)` and `glPushAttrib(GL_LIGHTING)`. These are capability enums, not attrib bits, and HW1's rGL ignored them: it saved only `GL_POINT_BIT`/`GL_LINE_BIT` (ORIG:src/rgl/kgl.c:1224-1290). hw_rgl now saves and restores blend enable+func and lighting enable (RGL:kgl.c:1230-1316; commit "Handle Cataclysm attrib stack calls"). **A native-GL port must do the same.** Real OpenGL raises `GL_INVALID_VALUE` or misbehaves on `glPushAttrib(GL_BLEND /*0x0BE2*/)`, so wrap `glPushAttrib/glPopAttrib`.

### 1.7 glcaps: capability policy by renderer

`glCapFeatureExists` (ORIG:src/Win32/glcaps.c:328-425) and `glCapStartup` (glcaps.c:616-922) produce these values:

| Capability | Native GL (GLtype) | rGL software (SWtype) | rGL D3D (D3Dtype) |
|---|---|---|---|
| Paletted textures (`trNoPalettes == FALSE`) | Only if vendor contains "3dfx"/"3Dfx" and both palette extensions exist (glcaps.c:714-743). `/safeGL` disables it | Yes (`GL_EXT_shared_texture_palette` via `rglFeature`) | **Never** (glcaps.c:706-713) |
| Vertex arrays (`GL_VERTEX_ARRAY`) | TRUE unless renderer contains "Savage" or `/safeGL` (glcaps.c:750-790) | TRUE | TRUE |
| Compiled vertex arrays (`GL_COMPILED_ARRAYS_EXT` = 0xffe02) | `GL_EXT_compiled_vertex_array` in the extension string, and not `/safeGL` (glcaps.c:792-799) | not advertised | not advertised |
| Depth func (`glCapDepthFunc`) | `GL_LEQUAL` | `GL_LESS` | `GL_LEQUAL` (glcaps.c:815,837) |
| Line/point smoothing | Lines ON unless devstats `NO_AA` or `/safeGL`; points additionally OFF for renderer "Savage" or devstats `NO_POINTSMOOTH` | OFF | OFF (glcaps.c:847-890) |
| `GL_SCISSOR_TEST` usable | yes | yes | **no** (glcaps.c:394-395) |
| Clear-colour backgrounds (`GL_COLOR_CLEAR_VALUE`) | `!gl3Dfx` | yes | no |
| "Swap friendly" fast front end (`GL_SWAPFRIENDLY` = 0xffe01) | `mainFastFrontend` (default TRUE, `/noFastFE` clears it; ORIG:src/Win32/main.c:172,1199) unless devstats `NOFFE[_GL]` | always | `mainFastFrontend` unless devstats `NOFFE[_D3D]` (glcaps.c:887-922) |
| Buffer count for FE redraw (`glCapNumBuffers`) | 3 | 2 (3 with `/triple`) | 2 (3 with `/triple`) (glcaps.c:460-473) |
| Texture formats | `GL_PROXY_TEXTURE_2D` probe of `GL_RGB`, `GL_RGBA`, `GL_RGBA16` | all except RGBA16 | all (glcaps.c:35-41, 670-700) |

`gl3Dfx` is set when `GL_VENDOR == "3Dfx Interactive Inc."` and the extension string equals one of two exact Voodoo strings. In that case Glide (`sstStartup`) is loaded for 2D (glcaps.c:611-614, 820-845; ORIG:src/Win32/sstglide.c).

### 1.8 GL features and extensions actually used

| Feature | Where (ORIG file names) | Notes for a modern port |
|---|---|---|
| Immediate mode `glBegin/glEnd`, `glVertex3f[v]`, `glColor*`, `glTexCoord2f`, `glNormal3f` | everywhere (prim2d.c, prim3d.c, mesh.c, particle.c) | Needs a compatibility profile, or an immediate-mode emulator like GoK's GLES shim (GoK:src/SDL/glinc.h:4-213) |
| `glVertexPointer` + `glArrayElement` inside `glBegin(GL_TRIANGLES)` | mesh.c:1674-1830 | Mesh path; lit vertex colours are computed on the CPU (`Shader.c`, `meshObjectRenderLit`) |
| `glInterleavedArrays(GL_C4UB_V3F)` + `glDrawElements(GL_TRIANGLES, …, GL_UNSIGNED_INT)` | btg.c:1044-1062 (rGL only), render.c | Background (BTG) polygons |
| `GL_EXT_compiled_vertex_array` (`glLockArraysEXT/glUnlockArraysEXT`) | mesh.c:1674-1696, btg.c:1050-1062 | §1.9 |
| `GL_EXT_paletted_texture`, `GL_EXT_shared_texture_palette` (`glColorTableEXT`), `GL_RGL_lit_texture_palette` (`glLitColorTableEXT`) | texreg.c:50-287 | Only 3dfx and rGL-sw used palettes. Everything else takes the "NoPal" RGBA path (`trNoPal*`) |
| `GL_EXT_rescale_normal` (`GL_RESCALE_NORMAL`) | glcaps.c:704 | |
| `GL_EXT_clip_volume_hint` | glcaps.c:793 | |
| `glDrawPixels` / `glRasterPos*` / `glBitmap` | animatic.c, glcompat.c, btg.c, render.c, HorseRace.c, FeReg.c, font.c, mouse.c, Subtitle.c | 2D. D3D rGL maps it through glcompat (DrawPixels → texture) |
| `glReadPixels` | render.c | Screenshots |
| `glScissor` | consMgr.c, shipview.c, TradeMgr.c, mainrgn.c | Not on D3D (see above) |
| `glClipPlane` | hs.c | Hyperspace gate slice. With rGL it selects `meshObjectRenderLitRGL` (mesh.c:56) |
| `glLineStipple`, `glPolygonMode` | pieplate.c, mesh.c | Wireframe hack, pie-plate dashes |
| Fog (`glFog*`) | clouds.c, nebulae.c, render.c | |
| `GL_SRC_ALPHA_SATURATE` blending | render.c, glcaps.c | Gated by `glCapFastFeature` |
| `glGetTexImage` | particle.c, texreg.c | Reading textures back (devstats `teximage`) |
| `GL_LIGHT0/1`, `glLightModelf(GL_LIGHT_MODEL_TWO_SIDE,1)`, `glMaterialfv` | render.c:1019-1036, light.c, mesh.c | Fixed-function lights. The HSF light sets drive them (§2.6) |
| `glMap*`/`glEval*` | none found in Game/Win32 except gldll.c | Bound but unused by HW1 |

`GL_RGBA16` as an internal format and `GL_SGIS_multitexture` occur only in capability probing (glcaps.c:35-41, 611).

### 1.9 Compiled vertex arrays (CVA)

- `meshEnableVertexArrays(vlist, first, count)` calls `glVertexPointer(3, GL_FLOAT, 16, vlist)` (4-float stride), enables only `GL_VERTEX_ARRAY`, and calls `glLockArraysEXT(first,count)` if `glCapFeatureExists(GL_COMPILED_ARRAYS_EXT)`. `meshDisableVertexArrays` unlocks (ORIG:src/Game/mesh.c:1674-1696). Indices are then fed per vertex with `glArrayElement` inside `glBegin(GL_TRIANGLES)` (mesh.c:1729-1830).
- In the BTG background, CVA is used **only when `RGLtype != GLtype`**. In practice it never happens: rGL does not advertise the extension, so `compiledArrays` stays FALSE there (ORIG:src/Game/btg.c:1044-1062).
- Resolution of the entry points: `glDLLGetProcs` first tries a DLL export (gldll.c:626-627). That fails for both opengl32 and rgl, because rgl's `draw.def` and `wglGetProcAddress` table lack it. `rndInit` then resolves it **after context creation** via `rwglGetProcAddress("glLockArraysEXT")` (ORIG:src/Win32/render.c:1060-1061). With rGL it stays NULL, which is safe because `glCompiledVertexArrays` is FALSE.
- GoK removed CVA (the lock calls are commented out, GoK:src/Game/Mesh.c:2053-2081) and uses VBOs for stars (GoK:src/SDL/render.c:1029-1041).
- CET exposes `NoCompiledVertexArrays`, which zeroes `glCapCompiledVertexArray` (Cataclysm global 0x00A72548) after `glCapStartup` to work around AMD corruption (CET:PatchRGL.cpp:63-74; CET:CataclysmEngineTweaks.ini:51-53).

### 1.10 Native-GL specifics that a 1:1 port must reproduce or consciously drop

| Item | Original | Anchor |
|---|---|---|
| Pixel format | `PFD_DOUBLEBUFFER\|PFD_SUPPORT_OPENGL\|PFD_DRAW_TO_WINDOW`, `cColorBits = MAIN_WindowDepth`, **16-bit depth**, no stencil | ORIG:src/Win32/render.c:813-871 |
| `glNT` switch | If NOT NT≤4, the game uses `rwglChoosePixelFormat/rwglSetPixelFormat/rwglSwapBuffers` **taken from the GL DLL's exports**; otherwise GDI `ChoosePixelFormat/SetPixelFormat/SwapBuffers` | render.c:851-870, 4952-4968 |
| Swap | `rndFlush`: `glFlush()`. Native GL then swaps (as above); rGL presents inside its driver. Every frame ends with **`Sleep(1)`** | render.c:4952-4968 |
| Clear | `rndClear` runs `glClear(COLOR)` + `rndFlush()` three times (Inferred: to blank every swap buffer). Software: `rglFeature(RGL_SPEEDY)` | render.c:4929-4944 |
| Default state | Clear depth 1.0; blend `SRC_ALPHA, ONE_MINUS_SRC_ALPHA`; depth test; lighting with LIGHT0 ambient {0.9,0.5,0.5,1}, diffuse 0.8, specular 1; two-sided light model; back-face culling; `rndAspectRatio = w/h` (**set only here**) | render.c:958-1060 (aspect: :1038) |
| Fast front end ("swap-friendly") | `glcompat.c`: the front end draws 2D into a scratch buffer and `glcPageFlip` uploads changed extents as textures / `rglDrawPitchedPixels` instead of a full redraw. It is driven from `rndRenderTask` | ORIG:src/Win32/glcompat.c:1-30, 1305-1430; render.c:4136-4151 |

### 1.11 Frame loop: `rndRenderTask`

`rndRenderTask` is started by `taskStart(rndRenderTask, 1.0f, TF_OncePerFrame)` (ORIG:src/Win32/utility.c:4218; GoK:src/SDL/utility.c:4027). Order of operations (ORIG:src/Win32/render.c:3871-4183):

1. `speechEventUpdate()`, `subTitlesUpdate()`
2. Cursor save-buffer feature toggles (rGL), then the clear (`glClear(COLOR|DEPTH)`, or black-only when the Launch Manager is active)
3. **`regFunctionsDraw()`**, which renders every region. The 3D view is a region draw callback, `rndMainViewRender` → `rndMainViewRenderFunction`
4. Letterbox/scissor bars (`rndDrawScissorBars`) for NIS
5. **`soundEventUpdate()`** (audio is pumped from the render task)
6. Subtitles and tutorial pointers
7. Debug overlays
8. `mainReinitRenderer` countdown → `mainReinitRGL()`, otherwise the glcompat page flip and `rndFlush()`
9. `taskYield(0)`

HW1 has **no frame limiter** beyond `Sleep(1)`. Cataclysm added one in this task: CET NOPs two 8-byte instructions at 0x0057B78B/0x0057B7A1 (CET:PatchRGL.cpp:96-100). The exact form of the limiter is UNVERIFIED.

**How the render task interleaves with the 16 Hz tasks.** Each pass of `utyTasksDispatch` (ORIG:src/Win32/utility.c:4846) reads QPC, converts the delta into 1/240 s ticks (`utyTimerDivisor = F / UTY_TimerResloutionMax`, `UTY_TimerResloutionMax 240`, ORIG:src/Win32/utility.h:39, utility.c:4100-4109, 4851-4855) and calls `taskExecuteAllPending(ticks)`. A `TF_OncePerFrame` task (`rndRenderTask`, `regProcessTask`) runs exactly once per pass. A periodic task such as the universe or NIS task (period 1/16 s, so `ticksPerCall = 15`, ORIG:src/Game/task.c:251-252) runs `(leftover + ticks) / ticksPerCall` times and carries the remainder (ORIG task.c:460-483). So one rendered frame can follow 0, 1 or several sim/NIS ticks, and nothing interpolates between them: that is the 16 Hz "choppiness" CET's README describes. The Cataclysm globals are `TimerResolutionMax` (float 240.0 at 0x0072A614, plus an immediate `push` of the integer resolution at 0x005ABF68 that CET rewrites, CET:PatchUniverse.cpp:374-382), `utyTimerDivisor` 0x00AB7520, `realtimeremainder` 0x00A6D9A0 and `taskFrequency` 0x00BB3260 (CET:Cataclysm.h:71, 190, 212-218). Full treatment: docs/hw1-engine-core.md §2.2 and §3.

Main-view render order (`rndMainViewRenderFunction`, GoK:src/SDL/render.c:2393-3255):
camera setup / `rndScaleCameraOffsets` → `rndBackgroundRender` (BTG + stars, :1303) → lighting → `rndRenderAHomeworld` / world effects → effects (`etgEffectDraw`) → per-object `lodLevelGet` → `rndShipVisible` → `rndFade` → `meshRender` → clouds (`cloudRenderAndUpdateLightning`, `cloudRenderSystem`) → `trailDraw` → `mistrailDraw` → `alodAdjustScaleFactor` → `nebRender` → `rndPostObjectCallback` (UI overlays such as move lines / TO hooks).

### 1.12 What the SDL port (GoK) changed in the renderer

| Area | GoK | Anchor |
|---|---|---|
| DLL loading | Gone; direct GL linking via `SDL2/SDL_opengl.h`; optional GLES (`HW_ENABLE_GLES`, immediate-mode shim) | GoK:src/SDL/glinc.h:1-241 |
| Devices | Single "OpenGL" device. Modes = desktop size ÷ integer factors ≥ 640×480 (windowed-fullscreen) | GoK:src/SDL/rinit.c:399-502 |
| Context | `SDL_GL_*`: **24-bit depth**, double buffer, GL 2.0, `SDL_WINDOW_FULLSCREEN_DESKTOP`, optional 8×MSAA, vsync on | GoK:src/SDL/render.c:854-1043 |
| Extensions | `glCheckExtension`; VBO for stars; CVA removed | GoK:src/SDL/render.c:1029-1061; GoK:src/Game/Mesh.c:2053-2081 |
| rGL symbols | `mainStartupParticularRGL/LoadParticularRGL` still exist by name but only start GL; `RGLtype` survives in Animatic.c | GoK:src/SDL/main.c:1300-1601; GoK:src/Game/Animatic.c:188,513 |
| HiDPI | Separate UI resolution and render resolution (`MAIN_WindowWidthActual`) | GoK:src/SDL/render.c:1019-1023 |

---

## 2. Mesh, geometry and texture formats

All of these load through the file layer (`.big` archives first, then loose files; header `BF_FILE_HEADER "RBF"` + `BF_VERSION "1.23"`, i.e. the file starts with `RBF1.23`, GoK:src/Game/BigFile.h:67, 76; BigFile.c:485, 504).
On disk, pointers are 32-bit offsets that get fixed up in place after load. GoK added `*_disk` twin structs plus `…LoadAndConvertTo64Bit`
helpers for x86-64. Cataclysm is 32-bit, so the in-memory structs **are** the disk structs plus fixups.

### 2.1 `.geo` meshes ("RMF")

| Field | Value / layout | Anchor |
|---|---|---|
| Magic | `"RMF97ba"` (old) or `"RMF99ba"` (new format: `newFormat`, `localSize = 0`), 8 bytes | GoK:src/Game/Mesh.h:51-53; Mesh.c:908-927 |
| Version | `0x00000402` (fatal mismatch in error-checking builds) | Mesh.h:54; Mesh.c:915-918 |
| `GeoFileHeader` (68 B on disk) | `identifier[8], version, pName(off), __obsolete(fileSize), localSize, nPublicMaterials, nLocalMaterials, oPublicMaterial, oLocalMaterial, nPolygonObjects, reserved[24]` | Mesh.h:113-127 (disk: :261-275) |
| `polygonobject` | `pName, flags(u8), iObject(u8), nameCRC(u16), nVertices, nFaceNormals, nVertexNormals, nPolygons, pVertexList, pNormalList, pPolygonList, pMother, pDaughter, pSister, hmatrix localMatrix` (mother/daughter/sister hierarchy) | Mesh.h:180-198 |
| `vertexentry` | `x,y,z, iVertexNormal` (16 B; this stride is why `glVertexPointer(3,GL_FLOAT,16,…)`) | Mesh.h:147-154 |
| `normalentry` | `x,y,z, pad[4]` | Mesh.h:156-163 |
| `polyentry` | `iFaceNormal(s32), iV0..iV2(u16), iMaterial(u16), s0,t0,s1,t1,s2,t2, flags(u16), reserved[2]` | Mesh.h:129-145 |
| `materialentry` | `pName, ambient, diffuse, specular, kAlpha, texture(trhandle*), flags(u16), nFullAmbient, bTexturesRegistered, textureNameSave` | Mesh.h:165-178 |
| Material flags | `MDF_AnimatingTextureMap 1, Smoothing 2, AllowFlatShading 4, 2Sided 8, BaseColor 16, StripeColor 32, SelfIllum 64` | Mesh.h:62-68 |
| Polygon modes | `MPM_Flat 0, Texture 1, Smooth 2, SmoothTexture 3` | Mesh.h:71-74 |
| Hierarchy bindings | `HBF_Azimuth 0x100, HBF_Declination 0x200, HBF_Recoil 0x400`, update-frequency mask 0xff; `mhbinding{function,userData,userID,flags,object}` for gun turrets and anims | Mesh.h:87-91, 221-245 |
| UV seam fixups | `PAC_*` flags for paco-packed texture pages. A "paged" mesh variant **`.peo`** is preferred when it exists | Mesh.h:94-107; Mesh.c:430-442, 865-872 |
| Loader | `meshLoad(fileName)`, which allocates `fileLength - sizeof(header) + sizeof(meshdata) - sizeof(polygonobject)` and fixes up offsets | Mesh.c:839-960 |

**Rendering.** `meshRender` → per-object `MESH_OBJECT_RENDER`. With `usingShader && lighting`, the lit vertex colours are computed on
the CPU (`Shader.c`, `meshObjectRenderLit`). On rGL this goes to `meshObjectRenderLitRGL[vx]`; otherwise it is plain `meshObjectRender`
(ORIG:src/Game/mesh.c:47-66; GoK:src/Game/Mesh.c:1803, 2093, 2824). Global light sets: `shSetLighting`, `shDockLight` (GoK:src/Game/Shader.c:237, 320).

### 2.2 `.mex` mesh extensions

There is one `.mex` per mesh. Header: `identifier[8], version(u16), nChunks(u16)`. Each chunk is `type[4], name[8], chunkSize(u32)` followed by data
(GoK:src/Game/MEX.h:15-28). Chunk types parsed: Gun, Engine (nozzle), Collision sphere, Collision rectangle, Docking, Salvage, NAV light
(MEX.h:30-145; accessors MEX.h:147-162; `mexLoad` MEX.c:33). The magic string is `"Mannngo"` per DOC:MeshExtensions.doc. **The code does
not check it** (`mexLoad` only byte-swaps the version, MEX.c:33-50), so the magic is UNVERIFIED in code. It is also absent from `SNAP:exe/Homeworld.exe` (0 `strings -a` hits, like `rrasberry`), so it cannot be used as a search anchor.

### 2.3 LOD system

- **File.** Each ship's `.lod` is a **text script** read with `scriptSetStruct` (`lodTableReadScript`, GoK:src/Game/LOD.c:106-154). Keys
  per level 0..5: `type<n>` (`Mesh`/`TinySprite`/`SubPixel`/`Function`/`NULL`), `mOn<n> bOn<n> mOff<n> bOff<n>`, `pMeshFile<n>` (a `.geo`,
  default mesh if missing), `pSpriteFile<n>`, plus `baseScalar`, `stripeScalar` (`lodScriptTable`, LOD.c:35-84). Called from the ship static loader
  (GoK:src/Game/Universe.c:1368). The fallback is `DefaultShip.lod` (Universe.c:3085).
- **Limits.** `LOD_NumberLevels 6`; types `LT_Mesh 1, LT_TinySprite 2, LT_SubPixel 3, LT_Function 4, LT_NULL 5` (GoK:src/Game/LOD.h:40-49).
- **Selection.** `lod *lodLevelGet(spaceObj, camera, ship)` stores `cameraDistanceVector/Squared` on the object. It steps `currentLOD` down while
  `dist² > level[cur].bOff * lodScaleFactor`, and up while `dist² < level[cur-1].bOn * lodScaleFactor`. The hysteresis uses `bOn/bOff`;
  `mOn/mOff` are unused in selection (LOD.c:268-339).
- **Scale.** `lodScaleFactor` defaults to `LOD_ScaleFactor = 0.6f` (LOD.h:51). It is forced to 1.0 for every non-software renderer in ORIG
  (ORIG:src/Win32/main.c:2555, 3192-3204). AutoLOD (`alod*`, GoK:src/Game/AutoLOD.h:15-23) adjusts the factor each frame toward a polygon
  budget (`alodSetPolys`, `alodAdjustScaleFactor` in the render loop).
- **Cataclysm difference (from CET).** `lodLevelGet` at 0x004A4D70 "was changed significantly from HW1 and just returns an int LOD level now".
  Its signature is `int (void* spaceObj, vector* camera, vector* ship)`, and it still writes `currentLOD`, `cameraDistanceVector` and
  `cameraDistanceSquared` (CET:PatchLOD.cpp:5-17; CET:Cataclysm.h:145).

### 2.4 Textures: `.lif`, `textures.ll`, registry and memory pools

| Item | Detail | Anchor |
|---|---|---|
| `.lif` header (`lifheader`, 48 B) | `ident[8]="Willy 7"`, `version=0x104`, `flags` (= texreg flags), `width,height`, `paletteCRC`, `imageCRC`, offsets `data, palette, teamEffect0, teamEffect1` | GoK:src/SDL/texreg.h:114-116, 260-275 |
| Texel data | 8-bit indexed image + 256-entry `color` palette + two team-effect byte planes (team colour masks), **or** RGBA (alpha images) | texreg.h:260-275; `trBufferColorRGB` texreg.h:377 |
| Listing file | `textures.ll`: `ident="Event13"`, `version=0x104`, ≤12000 `llelement{name, w, h, flags, imageCRC, nShared, sharedTo, sharedFrom}`. It enables texture **sharing by CRC** and faster load | texreg.h:108-112, 219-257; `trListFileLoad` texreg.c:2677 |
| Flags | `TRF_Pending 1, Paletted 2, Allocated 4, Alpha 8, TeamColor0 0x10, TeamColor1 0x20, NoPalPending 0x40, SharedFileName 0x80` | texreg.h:58-66 |
| `texreg` entry | `diskWidth,diskHeight (s16), scaledWidth,scaledHeight (s16), nUsageCount, flags(u8), nPalettes, currentPalette, nSharedTo, paletteCRC, palettes, meshReference, fileName, handle, baseScalar, stripeScalar, sharedFrom`. **CET asserts the same offsets in Cataclysm** (scaledWidth 0x4, scaledHeight 0x6, flags 0xC) | texreg.h:152-175; CET:game/Win32/texreg.h:179-181 |
| Registration | `trTextureRegister(name, colorinfo, meshRef)` defers the load (`TRF_Pending`); `trRegistryRefresh()` loads all pending textures in one batch after level load | texreg.c:624, 2914 |
| Sizes | Max 256×256 (`TR_TextureWidthMax/HeightMax`); power-of-two sizes 1..512 for the size tables | texreg.h:96-104, 131-133 |
| **VRAM budget** | One pool, `trRamPoolList[0] = 10 MB` (`1024*1024*10`), granularity 16 B, max 8 pools. ORIG Win32 is the same (12 MB only under a developer `#ifdef khentschel`), and its no-palette RGBA cache defaults to `trNoPalMaxBytes = 20 MB` | texreg.c:59-70; texreg.h:96; ORIG:src/Win32/texreg.c:73-85, 121 |
| Cram algorithm | `trCramRAMScaleCompute` iterates a 16.16 fixed-point scale factor (start 65536 = 1.0) up to `TR_MaxCramIterations=5`, aiming to leave margin ≤ 1/6 free. `trCramIntoRAM(scale)` then shrinks textures to fit. Textures are **never scaled up** | texreg.c:2392-2462, 2464-2560 |
| No-palette path | `trNoPal*` (RGBA GL textures made from palette + team colours, separate pool `trNoPalResizePool(opNoPalMB)`) is used when `trNoPalettes`, i.e. always on D3D and modern GL | texreg.h:389-398; GoK:src/Game/Options.c:1420, 2287 |
| Cataclysm change | `trCramRAMComputeAndScale` @0x00589A50. The `LoadMission` code at 0x00589AEA reduces the texture-memory limit on **SP mission 17**. CET jumps over it and can set the limit to INT32_MAX (by writing `[esp+0x18]`). The hook is always installed; the INT32_MAX write needs ini `UnlimitedVideoMemory` (default true). Note that 0x00589AEA is only 0x9A bytes past `trCramRAMComputeAndScale` (0x00589A50), so this "LoadMission" check is probably inside that function, with the limit held in a stack local (Inferred; the function's size is unknown) | CET:Cataclysm.h:106-107,173; CET:GameHooks.cpp:56-71, 126; CET:Config.cpp:18 |

### 2.5 `.mad` mesh animations

| Field | Value | Anchor |
|---|---|---|
| Magic / version | `"Mellonn"`, `real32 version = 0.91f` (DOC:MeshAnim.doc says 1.0; the code is authoritative) | GoK:src/Game/MeshAnim.h:36-37 |
| `madheader` | `identifier[8], version(f32), stringBlockLength, stringBlock(off), length(f32), framesPerSecond, nObjects, objPath(off), nAnimations, anim[]{name, startTime, endTime, flags}` | MeshAnim.h:60-88 |
| Keyframes | Per object, `times[]` + 6 paths (x,y,z,h,p,b); string offsets are relative to the string block | DOC:MeshAnim.doc; `madobjpath` MeshAnim.h:49-60 |
| Runtime | `madFileLoad`, `madAnimationStart/Pause/Update/Stop`, binding lookup by name; context animations (gun open/close, dock, special, door) in `MadLinkIn.c` | MeshAnim.h:197-217; MeshAnim.c:196, 630; GoK:src/Game/MadLinkIn.h:16-36 |
| NIS link | NIS scripts trigger `meshAnimStart/Stop/Pause/Seek` | DOC:NISPipeline.doc ("Mesh animations") |

### 2.6 Other presentation assets

| Format | Magic / version | Anchor |
|---|---|---|
| `.btg` background ("BTG" tool) | `btgHeader{btgFileVersion=0x600, numVerts, numStars, numPolys, xScroll, yScroll, zoomVal, pageW/H, mRGB, mBG RGB, bVerts..bBlends, renderMode}`; the loader is `btgLoad`, the renderer `btgRender` | GoK:src/Game/BTG.h:14-33; BTG.c:521, 664, 1259 |
| `.hsf` **lighting** file (not fonts) | `HSFFileHeader{identifier[12]="rrasberry", version, nLights}` + `HSFLight{type, x,y,z, h,p,b, coneAngle, edgeAngle, r,g,b (u8), intensity}`; light types Distant/Point/Spot/Linear/Area/Ambient. The magic is defined but **never checked** (no use of `HSF_FILE_ID` in any `.c`), so `rrasberry` is absent from HW1's exe. The level loader appends `.hsf`; NIS environments load `hsf\%s.hsf`, with `hsf\default.hsf` as fallback | GoK:src/Game/Light.h:44-71; Light.c:292; LevelLoad.c:1481-1503; ORIG:src/Win32/light.h:53 (`light.c` lives in Win32 in ORIG); ORIG:src/Game/nis.c:3241 |
| Tactical overlay icons | text `Tactical.script` (`IconClass`, `nPoints`, `vertex`, `playerColor0..`) | GoK:src/Game/Tactical.c:51-70, 105-117 |

---

## 3. Effects: ETG ("Effect Template Generator")

### 3.1 Pipeline

DOC:ETGUserGuide.doc ("Overall layout"): artists write `.etg` text. **nmake + the C preprocessor** (`#include/#define/#if`) produce **`.ebg`**,
and the game loads `.ebg` (still text: `etgEffectCodeLoad` opens it with `FF_TextMode`) and **compiles it to bytecode at load time**
(GoK:src/Game/ETG.c:3426-3640). Effects are named from per-race scripts in `ETG\`: `general.script, R1, R2, P1, P2, P3, Traders.script`
(ETG.c:762-771) and parsed by `etgStartup` (ETG.c:1674-1715). Those scripts bind effects to events (`setDeathEvent`, `setFireEvent`,
`setHitEvent`, `setBulletEffect`, `setTractorBeamEffect`, `setSpecialPurposeEffect`, `LoadEffect`, `SoftwareVersion`, `bigBoomFactor`, …).
Defaults: `ETG\genericBOOM.ebg`, `ETG\defaultBlast.ebg` (GoK:src/Game/ETG.h:208-244).

### 3.2 The language (DOC:ETGUserGuide.doc + parse table)

- One statement per line; `{ }` count as statements; `[expr]` holds load-time constant expressions; `label(x)`, `goto/call/return`, `end()`, `delete()`, `yield()`.
- Blocks: `effectStart`/`eventStart` (parameters come from the game), `variable`, `startup`, `eachFrame`, `timeIndex`.
- Types: `int float RGB RGBA vector`, constants such as `subTextureDWORD(...)` and `morphAnimDWORD(...)`.
- Conditionals: `if/else`, `alternate { alt<n>(weight): }`, `caseof/case`, time effectors `timeBlock(var){ time(t, v) }`, `at(t)`, `every(t)`, `before/after/between`.
- **Parse-table keywords** (GoK:src/Game/ETG.c:258-333): `nop if else label exlabel eventStart variable { } startup eachFrame timeIndex particleBlocks maxFrequency worldRender int float RGB RGBA subTextureDWORD morphAnimDWORD setTexture modifyTexture setMesh vector at every before after between effect timeBlock time goto call return end delete yield alternate alt #line spawn createSprites createCircles createPoints createLines createMeshes createEffects`. `setForceVisible` is **commented out** of the parse table in both GoK (ETG.c:280) and ORIG (etg.c:265), and `nop` is listed twice (ETG.c:260, 320).
- **Function table**: 153 active `funcEntryN("name", returnType, fn, paramTypes…)` entries, 150 distinct names (`modifyAlphaMode`, `modifyMesh` and `setSpriteDeltaRot` each appear twice) (ETG.c:501-694). About 20 more are commented out (e.g. `modifyAnimFrame`, `modifyDepthWrite`, `effOffsetLOF`). Examples: particle parameter setters `setDefaults setDrag setScale setXYZScale … setLighting setAnimation setMeshAnimation`, effect modifiers `setEffOffsetLOF setEffChangeLOF setEffOffsetR setEffOffsetXYZ setEffOffsetVelLOF setEffOffsetSpin setEffOffsetTime setEffDrag setEffDefaults setEffAttachParent setEffSpin setEffDeltaSpin`, and modifiers on created systems `modifyLighting modifyAnimation modifyMesh modifyMeshAnimation modifyMeshFramerate`. The full list with defaults and ranges is in DOC:ETGUserGuide.doc ("Setting parameters to particle systems").

### 3.3 Compiled representation

- `etgeffectstatic`: `name, softwareVersion, nParticleBlocks, variableSize, rateSize, effectSize, variableOffsets, variableInitData, initLength, constData, constLength, nDecisions, decisions, alternateOffsets, codeBlock[3] (startup / eachFrame / timeIndex), nHistoryList, iHistoryList, historyList, specialOps` (GoK:src/Game/ETG.h:536-560).
- `etglod {nLevels; level[]}` holds alternate effect LODs, one effect file per level (`etgLODLoad`, ETG.c:901-936).
- Opcodes: `enum EOP_*` (ETG.h:57-…). Each opcode starts with a `udword` id. `etgHandleTable[opcode] = {size, handler}` (ETG.c:127-210).
- Function-call opcode `etgfunctioncall{opcode, function ptr, [wrap_function: GoK only], nParameters, returnValue (variable offset or -1), passThis, parameter[]{type, param}}` (ETG.h:401-419). CET reuses this layout for Cataclysm (`CET:PatchETG.cpp:9-36`).

### 3.4 Execution

| Stage | Function | Anchor |
|---|---|---|
| Create | `etgEffectCreate(stat, owner, pos, vel, coordsys, nLips, flags, nParams, ...)` allocates the `Effect` with its variable block, sets attach flags (`EAF_Position/Coordsys/Velocity/NLips`) and links it into `universe.effectList` | GoK:src/Game/ETG.c:7459; ETG.h:269-274 |
| Throttle | `etgFrequencyExceeded(stat)`: per-effect history ring, `max = nHistoryList * etgHistoryScalar / 256`. If more effects than that spawned in the last `EH_TimeElapsed`, it refuses, otherwise refuses at random (`randyrandombetween(RANDOM_ETG, …)`) | ETG.c:7609-7647 |
| Update (sim, 16 Hz) | `univUpdate` → `etgEffectUpdate(effect, phystimeelapsed)`: first tick runs `EPM_Startup`, then each tick `EPM_TimeIndex`, then `EPM_EachFrame` (unless deleted). Afterwards it inherits position, coordsys, velocity and N-LIPS from the owner | GoK:src/Game/UnivUpdate.c:2081; ETG.c:2192-2280 |
| Interpreter | `etgEffectCodeExecute(stat, effect, block)`: `while (offset < length) { op = *(udword*)pc; offset += etgHandleTable[op].function(effect, stat, pc); }` | ETG.c:1977-2010 |
| Native calls | **ORIG x86:** push the parameters in reverse (constants, `constData+off`, `variable+off`, or variable values), optionally push `effect` as `this`, then `call function`. The `eax` return value is stored at `variable+returnValue` (ORIG:src/Game/etg.c:5647-5697, inline `_asm push`). GoK replaces this with generated wrappers (`GENERIC_ETGCALLFUNCTION`) or an x86-64 register shim (GoK ETG.c:5739-6015) | |
| Draw (render task) | `etgEffectDraw(effect)` → per particle block → `partRenderSystem` (types points/cubes/lines/mesh/billboard) | ETG.c:2282-2427; GoK:src/Game/Particle.c:1712; render.c:2630 |

**Cataclysm particle changes (CET).**
- Two new particle types after `PART_BILLBOARD`: `PART_UNK1` (record size 0x24) and `PART_UNK2` (0x1C).
- `particleAttribs` is 456 bytes, with `effectVelocity` moved from a static into it.

Sources: CET:game/Game/particle.h:31-40, 200-305.

**Timing coupling.**
- `at()/every()` round off against `ETG_UpdateRoundOff = UNIVERSE_UPDATE_PERIOD/2 = 0.03125` (CET:Cataclysm.h:192-194).
- Effects that create sprites every frame consume more particle blocks at higher tick rates (CET:PatchETG.cpp:49-66).

Section 11 covers what CET does about both.

---

## 4. Front end: FEMan / FEFlow, `.fib`, regions, fonts

### 4.1 Layers

```
WindowProc / SDL events ──► key.c / mouse.c state ──► regProcessTask (once per render frame)
      ──► Region tree (Region.c) ──► region process callbacks (buttons, list windows, main game region mrRegionProcess)
                                  └► region draw callbacks (regFunctionsDraw() from rndRenderTask)
FEFlow (FEFlow.c) = builds region subtrees from .fib screens; binds atoms to C callbacks by NAME
```

- Region events are bit masks: `RPE_Enter 0x1 … RPE_PressLeft 0x2000, RPE_ReleaseLeft 0x10000, RPE_KeyDown 0x80000, RPE_KeyUp 0x100000, RPE_KeyRepeat 0x200000, RPE_KeyHold 0x400000, RPE_DoubleLeft 0x800000, RPE_ModalBreak 0x40000000, RPE_DrawEveryFrame 0x80000000` (GoK:src/Game/Region.h:66-106).
- API: `regChildAlloc`, `regSiblingAlloc`, `regKeyChildAlloc(parent, ID, filter, fn, nKeys, …)`, `regRegionProcess`, `regFunctionsDraw`, `regDrawFunctionSet`, `regFunctionSet` (GoK:src/Game/Region.c:563, 901, 1088, 1110-1301, 1575-1600).
- `regProcessTask` is started with `REG_TaskFrequencyOPF = 1`, `TF_OncePerFrame`, i.e. once per rendered frame (Region.c:984; Region.h:116-117).

### 4.2 `.fib` binary format (verified against a Cataclysm file)

DOC:FEManBinary.doc specifies it. The code is `feScreensLoad` (GoK:src/Game/FEFlow.c:1839-2035). The FEMan tool
(`SNAP:tools/win32/FEMan`) exports it.

| Struct | Layout (32-bit, little endian) | Size | Anchor |
|---|---|---|---|
| `fibfileheader` | `char identify[8] = "Bananna"`, `uword version = 0x0212`, `uword nScreens` | 12 | GoK:src/Game/FEFlow.h:40-41, 150-156 |
| `fescreen[nScreens]` (immediately after header) | `name(off), flags, uword nLinks, uword nAtoms, links(off), atoms(off)` | 20 | FEFlow.h:159-167 |
| `felink` | `name(off), flags (FL_Enabled 1, FL_DefaultPrev 2, FL_DefaultNext 4, FL_RetainPrevious 8), linkToName(off)` | 12 | FEFlow.h:42-50, 171-177 |
| `featom` | `name(off), flags, status, ubyte type, ubyte borderWidth, uword tabstop, color borderColor, color contentColor, sword x,loadedX,y,loadedY,width,loadedWidth,height,loadedHeight, pData(off), attribs(off), char hotKeyModifiers, char hotKey[5], pad2[2], udword drawstyle[2], region, pad[2]` | **76** | FEFlow.h:188-211 |

- Offsets are relative to the file start and fixed up at load. Link names are fixed up only when `FL_Enabled` is set (FEFlow.c:1903-1909).
- **On disk, x/y/width/height hold a 2-point rectangle** (x0,y0,x1,y1). The loader converts it to x,y,w,h and keeps `loaded*` copies for resolution rescaling (FEFlow.c:1948-1975; `feResRepositionScaledX/Y` :1573-1596).
- The screen named `"HyperspaceRollCall"` is special-cased as a menu (FEFlow.c:1945-1948).
- **Verification.** I parsed `CET:assets/feman/multiplayer_game.fib` (Cataclysm data, 229,460 bytes) with exactly this layout: ident `Bananna`, version 0x212, 41 screens. The first screens are `Load_a_scenario` (16 atoms, including `CS_ScenarioWindowInit` type 16 = ListWindow, `CS_Back` type 3 = Button, flags 0x942), `Connection_Method` (2 links `Connection_Method-TO-Recorded_Game`, `…-TO-Skirmish_Load`; atoms `MG_Skirmish`, `MG_InternetWON`), `Internet_Login`, `Password_Change`. **Cataclysm's FIB format is HW1's.** Decorative atoms carry texture names (`Feman\TexDecorative\arrow_back_medium.lif`) in `name`.

Atom types (`FA_*`, FEFlow.h:86-110): `UserRegion 1, StaticText 2, Button 3, CheckBox 4, ToggleButton 5, ScrollBar 6, StatusBar 7,
TextEntry 8, ListViewExpandButton 9, TitleBar 10, MenuItem 11, RadioButton 12, CutoutRegion 13, DecorativeRegion 14, Divider 15,
ListWindow 16, BitmapButton 17, HorizSlider 18, VertSlider 19, DragButton 20, OpaqueDecorativeRegion 21`.

Atom flags (`FAF_*`, FEFlow.h:54-76): `Link 0x1, Function 0x2, Bitmap 0x4, Modal 0x8, Popup 0x10, CallOnCreate 0x20, ContentsVisible 0x40,
DefaultOK 0x80, DefaultBack 0x100, AlwaysOnTop 0x200, Draggable 0x400, BorderVisible 0x800, Disabled 0x1000, DontCutoutBase 0x2000,
CallOnDelete 0x4000, Hidden 0x8000, justification mask 0x30000, Background 0x40000, drop shadow 0x0ff00000`.

### 4.3 Binding screens and callbacks

- **By name.** `feCallbackAdd(controlName, fefunction)`/`feCallbackAddMultiple(fecallback[])` put entries into a table of up to 400 (`FE_NumberCallbacks`).
  `feDrawCallbackAdd*` registers up to 150 draw callbacks (FEFlow.c:2039-2133; FEFlow.h:125-127). The callback type is
  `void (*fefunction)(char *name, featom *atom)` (FEFlow.h:262).
- **Dispatch.** `feButtonProcess` → if `FAF_Link`, `feLinkFindInScreen(current, atom->name)` → `feScreenStart(parent, link->linkToName)`.
  If `FAF_Function`, `feFunctionExecute(atom->name, atom, FALSE)`, which does a **linear strcmp search** of the callback table. It
  also calls `tutGameMessage(name)`, so tutorials can hook any button by name (FEFlow.c:153-176, 627-688).
- **Screens.** `feScreenStart(parentRegion, "Screen_Name")` → `feRegionsAdd` builds the regions per atom and fires the `FAF_CallOnCreate`
  callbacks (`FAS_OnCreate` status bit). The stack (`feStack[32]`) is driven by `feScreenPush/Pop`; `feCurrentScreenDelete`,
  `feAllScreensDelete` (FEFlow.c:1171, 1446, 2301-2414, 2984-3214).
- **Files loaded at startup** (`utyFrontEndFiles`): `FEMan\Front_end.fib`, `Sensors_manager.fib`, `Research_manager.fib`,
  `In_game_ESC_menu.fib`, `CSM-All.fib`, `TaskBar.fib`, `single_player_objective.fib` (GoK:src/SDL/utility.c:226-238, loaded at :3163).
  Modules load their own on demand: `Construction_Manager.fib` (ConsMgr.c:43, 3591), `Launch_Manager.fib` (LaunchMgr.h:21),
  `Research_Manager.fib` (ResearchGUI.h:22), `Trader_Interface.fib` (TradeMgr.c:67), `Horse_Race.fib` (HorseRace.h:19),
  `Multiplayer_LAN_Game.FIB` (MultiplayerLANGame.h:21), `Game_Chat.FIB` (GameChat.c:45), `Hyperspace_Roll_Call.fib` (SinglePlayer.c:944).
- **Root screen** `"Main_game_screen"` (GoK:src/SDL/utility.c:809, 2387, 2701). Example base-callback table `utyCallbacks`: `"OP_TroubleShoot"`,
  `"TA_StartSinglePlayer"`, `"QuitGame"`, `"QuitToMain"`, `"CSM_Dock"`, `"CSM_Harvest"`, `"CSM_Move"`, … (GoK:src/SDL/utility.c:431-520).
- **Front-end textures** (cutouts, corner pieces) come from `FEReg.c` (`ferTextureRegister*`, `ferDrawBox`; GoK:src/Game/FEReg.c:484-990).
- **Cataclysm keeps the callback signature.** CET calls Cataclysm FE handlers with HW1's `fefunction` shape `(char *name, featom *atom)`: `opOptionsAccept` 0x004D7E30, `mgResourceInjectionInterval` 0x004BDE80, `mgResourceLumpSumInterval` 0x004BE090 (CET:Cataclysm.h:150-153; HW1 `opOptionsAccept` GoK:src/Game/Options.c:1309). `feToggleButtonSet(char*, sdword)` 0x00475990 also matches HW1 (GoK:src/Game/FEFlow.c:3263). One known change is `clCommandMessage` (on-screen command text), which takes `(const char[], udword flags)` in Cataclysm (CET:Cataclysm.h:131) but only `(char[])` in HW1 (GoK:src/Game/CommandWrap.c:33).
- **Localization.** Strings come from `English.script` (`scriptSet("", "English.script", LanguageStrings)`, GoK:src/Game/StringSupport.c:768). Cataclysm ships its own `english.script` with the same `strXxx  TEXT` format (`CET:assets/english.script`).

### 4.4 Fonts (`.hff`, "Orannge")

| Item | Detail | Anchor |
|---|---|---|
| File | `fontfileheader{identification[8]="Orannge", uword version=0x100, uword flags (FF_Color 1, FF_Alpha 2), fontheader}` | GoK:src/SDL/font.h:44-51, 114-121 |
| `fontheader` | `nCharacters, spacing, fullHeight, baseLine, name, imageWidth, imageHeight, nColors, palette(off), image(off), glFont, reserved[12], charheader* character[256]` | font.h:95-111 |
| Char record | `charfileheader{sword u, v, width, height, offsetX, offsetY}` (in-memory `charheader` holds a bitmap ptr) | font.h:79-92; DOC:FontSpec.doc |
| Image | One 8-bit paletted atlas; palette entries are RGBA (alpha for AA) | DOC:FontSpec.doc |
| Registry | `frFontRegister("name.hff")` → path `fonts\<Language>\name.hff` (English/French/German/Spanish/Italian); 32 usable slots with ref-counting (`FR_NumberFonts 32+1`; the comment calls the +1 a hack against a NULL error) | GoK:src/Game/FontReg.c:116-184; FontReg.h:33-40 |
| GL rendering | `glfontCreate` packs glyphs into GL texture pages, then `glfontDisplayString` draws textured quads. ORIG used `glDrawPixels` per glyph on some paths (`ORIG:src/Win32/font.c:1124-1233`) | GoK:src/SDL/font.c:290-640 |
| Font names seen | `hw_eurosecond_11.hff`, `default.hff`, `Arial_12.hff`, `Small_Fonts_8.hff` | GoK:src/Game/KeyBindings.c:717; ScenPick.c:1112; HorseRace.c:73; MultiplayerLANGame.c:128-140 |

**Note.** `.hsf` is **not** a font format. It is the lighting file (§2.6).

---

## 5. In-game UI

| Module | Purpose | Screen / data | Entry points |
|---|---|---|---|
| **mainrgn.c** (main game region) | Owns the full-screen 3D region. Selection band box, right-click menu (`CSM-All.fib`), orders, hot keys, move/attack lines, field spheres, big messages | `mrRegionProcess` (all mouse/key events) | GoK:src/SDL/mainrgn.c:489 `mrStartup`, :1560 `mrKeyPress`, :1347 `mrKeyRelease`, :3078 `mrRightClickMenu`, :3643 `mrRegionProcess`, :5402 `mrRegionDraw`; move-line/TO drawing helpers :4270-5400 |
| **Tactical overlay** (TO) | Per-class 2D icons and a legend over ships. **CAPS LOCK** toggles it (`kbTACTICAL_OVERLAY`, mainrgn.c:2489) | `Tactical.script` (`toIconTweaks`) | GoK:src/Game/Tactical.c:105 `toStartup`, :190 `toAllShipsDraw`, :435 `toLegendDraw` |
| **Sensors manager** (SM) | Strategic zoomed-out map: blobs, world plane, horizon ticks, movement pie, fleet intel. **SPACE** opens it (mainrgn.c:1802) | FE screens `"Sensors_manager"` / `"FleetIntel"` (Sensors.h:36-37); `sensors.script` (`smTweaks`) | GoK:src/Game/Sensors.c:3932 `smStartup`, :3984 `smSensorsBegin` (→ `feScreenStart(ghMainRegion, screenName)` :4008-4010), :2909 `smViewportRender`, :3260 `smViewportProcess`, :1737 `smBlobsDraw`, :2728 `smTacticalOverlayDraw`, :3917 `smCallbacks` |
| **Task bar** | Auto-hiding bottom bar with buttons for running managers, objectives list, bumper region | `TaskBar.fib`, screen `"Task_Bar"` (TaskBar.h:39-43) | GoK:src/Game/TaskBar.c:509 `tbStartup`, :748 `tbButtonCreate`, :189 `tbBumperProcess` |
| **Construction manager** (CM, build manager) | Ship build queues, RU display, arrows, ship info; runs in its own task | `Construction_Manager.fib`, screen `"Construction_manager"` (ConsMgr.c:43-47) | GoK:src/Game/ConsMgr.c:1426 `cmStartup`, :178 `cmCallback[]`, :1105 `cmBuildTaskFunction`, draw callbacks :1493-3155 |
| Launch manager | Launch docked ships | `Launch_Manager.fib` | GoK:src/Game/LaunchMgr.c:136 `lmCallback[]`, :1713 |
| Research manager | Research GUI | `Research_Manager.fib` | GoK:src/Game/ResearchGUI.c:178, :2171 |
| Info overlay | Selected-ship list (top-right) | — | GoK:src/Game/InfoOverlay.c:151, 282 |
| Pie plate | 3D movement disc and height cursor | — | GoK:src/Game/PiePlate.c:118, 732, 806 |
| Ship view | Rotating ship preview in managers (uses `glScissor`) | — | GoK:src/Game/ShipView.c |
| Objectives / Trade / Horse race (load screen) / Game chat | | own `.fib` files (§4.3) | Objectives.c:66, TradeMgr.c:140, HorseRace.c:174, GameChat.c |

**Draw path.** Each manager is FE regions with draw callbacks (`featom` + `regionhandle`). The 3D main view is itself a region callback
(§1.11). Any Cataclysm-only screens are UNVERIFIED until the real install is extracted. Their `.fib` screen and callback names can be
recovered from the `.fib` files (format above) and from the callback-name strings in the exe.

---

## 6. Camera and input

### 6.1 Keyboard routing

| Stage | Win32 original (what Cataclysm does) | SDL port (GoK) |
|---|---|---|
| OS event | `WindowProc`: `WM_KEYDOWN` → `keyPressDown(keyLanguageTranslate(wParam))` (or `keyRepeat` when lParam bit 30 is set); `WM_KEYUP`/`WM_SYSKEY*` similar (ORIG:src/Win32/main.c:2663-2823) | `HandleEvent`: `SDL_KEYDOWN/UP` → `keyPressDown(keyLanguageTranslate(scancode))` (GoK:src/SDL/main.c:1685-1780; `keyLanguageTranslate` :1623) |
| Key codes | **Win32 virtual-key codes** (`TABKEY 0x09`, `CAPSLOCKKEY 0x14`, `ESCKEY 0x1b`, `HOMEKEY 0x24`, letters = ASCII) (ORIG:src/Game/KEY.H:31-56) | **SDL scancodes** (`TABKEY SDL_SCANCODE_TAB`, …) (GoK:src/Game/Key.h:34-41) |
| Layout | `keyLanguageTranslate`; `keyGermanToEnglish/French/Spanish/Italian` remap for localized layouts (GetKeyboardLayout in ORIG Strings.c) | same functions on scancodes (Key.h:223-226) |
| State | `key.c`: `keyPressDown/Up/Repeat`, stuck-key bits, a typed-key ring buffer (`keyBufferedKeyGet`) | GoK:src/Game/Key.c:79, 189, 290 |
| Dispatch | `regProcessTask` sends `RPE_KeyDown/Up/Repeat/Hold` to regions whose `regKeyChildAlloc` filter lists the key → main game region `mrRegionProcess` → `mrKeyPress(ID)` | Region.c:563-947; mainrgn.c:3643 |
| Binding | `mrKeyPress`: `ID = kbCheckBindings(ID)` maps the user's key back to the **default** key of the bound command, and the big `switch(ID)` handles defaults (`case SPACEKEY`, `CAPSLOCKKEY`, `BKEY`, `F1KEY`…) | GoK:src/SDL/mainrgn.c:1560-2848 (binding at :1595), release at :1352 |

**Key binding table.** `kbBoundKeys {command, defaultkey, primarykey, resettokey, status}` (status `KB_NormalBound 0, KB_NotBound 1,
KB_GetKeyPress 2`). The default `kbKeyTableEnglish[19]` (GoK:src/Game/KeyBindings.c:54-75; enum KeyBindings.h:25-48) is:

| Command | Default | Command | Default |
|---|---|---|---|
| NEXT_FORMATION | TAB | NEXT_TACTIC / PREVIOUS_TACTIC | `]` / `[` |
| BUILD_MANAGER | B | SCUTTLE | S |
| PREVIOUS_FOCUS / NEXT_FOCUS | C / V | SHIP_SPECIAL | Z |
| DOCK | D | TACTICAL_OVERLAY | CAPS LOCK |
| SELECT_ALL_VISIBLE | E | MOTHERSHIP | HOME |
| FOCUS | F | KAMIKAZE | K |
| RESEARCH_MANAGER | R | CANCEL_ORDERS | ~ (TILDE) |
| HARVEST | H | LAUNCH_MANAGER | L |
| MOVE | M | (SENSORS_MANAGER, SPACE) | commented out, hard-wired |

`kbCheckBindings` (KeyBindings.c:587-607) returns 0 for a mappable key that is bound to nothing (`kbCanMapKey[]`, :178). UI display
names live in `GoK:src/Game/Options.c:179-200` (command names such as `"NEXT FORMATION"`) and :297-330 (key names `"BKSPC"`, `"TAB  "`, `"CAPS "`, `"SPACE"`, `"TILDE"`, `"NUM0"`…).

**Persistence caveat.** Saved bindings hold **raw key codes**, i.e. VK codes in the Win32 build (`kbKeySavedKeys[20]`, KeyBindings.c:29-49).
A port that reads Cataclysm settings must translate VK → its own key space.

**F12 behaviour (Win32).** `WM_KEYUP VK_F12` never reaches `keyPressUp`. It is a debug hook: Ctrl+Shift+F12 switches to the software renderer,
and otherwise the event is dropped (ORIG:src/Win32/main.c:2744-2788). F12 therefore stays "down" after its first press. That is
the Cataclysm bug CET fixes (§11).

**ESC fall-through (Win32).** In the same `WM_KEYUP` switch, `case VK_ESCAPE` has no `break`, so it falls into `case VK_F11` (`MAIN_MOUSE_FREE` is 1, ORIG:src/Win32/main.h:20). After a Bink movie has finished, an ESC key-up therefore calls `keyPressUp` twice, and Ctrl+Esc also toggles the cursor clip (ORIG:src/Win32/main.c:2720-2743). This is harmless, but a bit-exact port may want to keep it.

### 6.2 Mouse

- Win32: `WM_[LRM]BUTTON*` and `WM_MOUSEWHEEL` → key-state pseudo keys (`LMOUSE_BUTTON`, `LMOUSE_DOUBLE`) plus the `mouse.c` state (ORIG:src/Win32/main.c:2828-2960). The position is polled with `GetCursorPos` and warped with `SetCursorPos` (ORIG:src/Win32/mouse.c:274, 489). The cursor is clipped with `ClipCursor` (ORIG:src/Win32/utility.c), and the OS cursor is hidden (`ShowCursor(FALSE)`, mouse.c:1907).
- The game **draws its own cursor** (`mouseDraw`, cursor types, `.lif` cursor bitmaps). On software rGL it saves and restores the pixels under the cursor through `rglSaveCursorUnder` (ORIG:src/Win32/mouse.c:883-978).
- GoK: `SDL_GetMouseState`, `SDL_WarpMouseInWindow` with UI→render scaling, `SDL_SetRelativeMouseMode` (GoK:src/SDL/mouse.c:280, 462-530, 909, 1829).

### 6.3 Camera

- `Camera.c`: `cameraInit`, `cameraRotAngle/Declination`, `cameraZoom` (enforces ship distances), `cameraControl` (mouse drag and keys, joystick reset), `cameraSensitivitySet` (GoK:src/Game/Camera.c:197, 303, 339; Camera.h:60-70).
- `CameraCommand.c`: a focus stack (`ccFocus`, `ccFocusFar/Close`, `ccFocusOnMyMothership`, `ccForwardFocus`, `ccCancelFocus`, `ccControl`) with cubic-blended transitions (`EvalCubic`). The NIS uses it for the "focus at end" cut (GoK:src/Game/CameraCommand.h:101-141; CameraCommand.c:1110, 1577).
- Camera motion polling happens in `mrCameraMotion` (GoK:src/SDL/mainrgn.c:1297).

---

## 7. Sound

### 7.1 Layers

```
SoundEvent.c / SoundEventPlay.c / SoundEventStop.c   game-level SFX events (guns, ships, UI, specials) → .lut lookup → bank patch
SpeechEvent.c                                         speech (actors, sentence LUT), music tracks, NIS/animatic speech
        │
soundlow.c   channels, patches, banks, volume/pan/pitch fades, per-channel EQ (soundequalize), "ship heading" doppler
sstream.c    streaming thread: .vce speech / .wxd music read in blocks, subtitles from INFO chunks
smixer.c     mixer thread: decode FQ blocks, mix in the frequency domain, iDCT, write PCM to the device
fqcodec/fquant/fqeffect/dct/mixfft   Relic "FQ" transform codec + spectral effects (EQ, pitch shift, delay, noise...)
        │
Win32: DirectSound primary buffer (if "certified") or waveOut            SDL port: SDL_OpenAudio callback
```

Mixing happens in the frequency domain. Each channel's spectra go through `fqMix` (and `fqEqualize` for stream EQ), then `fqDecBlock` turns the
summed blocks into PCM (GoK:src/SDL/smixer.c:199-204, 451-554).

### 7.2 Codec and mixer constants

| Constant | Value | Anchor |
|---|---|---|
| Output rate / format | **22,050 Hz, 16-bit, stereo** (`FQ_RATE`); `FQ_DRATE` 44,100 | GoK:src/SDL/fqcodec.h:24-25; ORIG:src/Win32/soundlow.c:335-341 (`WAVEFORMATEX`) |
| Block | `FQ_SIZE 256`, `FQ_HSIZE 128` samples; mix block 1024 bytes | fqcodec.h:29-30; GoK:src/SDL/smixer.c:23 |
| Block time | `SND_BLOCK_TIME = FQ_SLICE/1000 ≈ 11.6 ms` | GoK:src/SDL/soundcmn.h:13-14 |
| Patch | `PATCH{id, priority, pitch, dataoffset, datasize, loopstart, loopend, bitrate, flags, volume, pan, waveformat}`; `BANK{id, checksum, numpatches, firstpatch}` | soundcmn.h:31-79 |

- The codec (`fqcodec`, `fqeffect`, `fquant`, `dct`) shipped in the 2003 release **only as `.obj`** (`SNAP:obj/{Release,Interim,Debug}/fq*.obj`, `dct.obj`; also `GoK:src/obj/*.obj`). GoK carries a C reconstruction (GoK:src/SDL/fqcodec.c, fquant.c, fqeffect.c, dct.c).
- Cataclysm statically links its own copy. CET locates `fqEqualize` at 0x00558F20 (CET:Cataclysm.h:141). The signature is `int fqEqualize(float *aBlock, float *aEq)` (GoK:src/SDL/fqeffect.h:83).

### 7.3 Device back ends

| | Win32 original (Cataclysm) | SDL port |
|---|---|---|
| Init | `soundinit(hWnd, mode)`: builds the `WAVEFORMATEX`, then `soundStartDSound(hWnd)`. On failure it falls back to **waveOut** (`useWaveout = TRUE`), then `isoundmixerinit` (ORIG:src/Win32/soundlow.c:313-361) | `soundinit`: `SDL_OpenAudio({22050, AUDIO_S16, 2ch, SDL_BUFFERSIZE, soundfeedercb})` (GoK:src/SDL/soundlow.c:192-245) |
| DirectSound | `DirectSoundCreate` → `SetCooperativeLevel(PRIORITY or EXCLUSIVE, depending on /dsoundCoop)` → `GetCaps` → **requires `DSCAPS_CERTIFIED` or `/dsound`**, otherwise it releases the device and returns `SOUND_ERR` → primary buffer `SetFormat` (ORIG:src/Win32/soundlow.c:197-291, test at :249) | n/a |
| Mixer thread | `_beginthread(isoundmixerthreadDSOUND)` if certified, else `_beginthread(isoundmixerthreadWAVEOUT)` with `waveOutOpen/PrepareHeader/Write` (ORIG:src/Win32/smixer.c:211-330, 1267) | SDL audio callback + `isoundmixerthreadSDL` (GoK:src/SDL/smixer.c:39, 182) |
| Stream thread | `_beginthread(isoundstreamupdate)` (ORIG:src/Win32/sstream.c:84) | `SDL_CreateThread` (`streamStartThread`, GoK:src/SDL/sstream.c:84) |
| Switches | `/noSound /noSpeech /dsound /dsoundCoop /waveout /reverseStereo` (ORIG:src/Win32/main.c:1166-1172) | same names |

### 7.4 SFX events, banks and LUTs

- Directory `SOUNDFXDIR = "SoundFX\"` (GoK:src/Game/SoundEventPrivate.h:32).
- `SEloadbank` loads the LUTs `GunEvents.lut, ShipCmnEvents.lut, ShipEvents.lut, DerelictEvents.lut, SpecExpEvents.lut, SpecHitEvents.lut, UIEvents.lut` and the banks `Guns.bnk, Ships.bnk, SpecialEffects.bnk, UI.bnk`. Each bank is checked: `soundbankadd(bank) == lut->checksum` (GoK:src/Game/SoundEvent.c:2680-2885).
- Global tables: `Volume.lut`, `Range.lut`, `Frequency.lut` (SoundEvent.c:750-772).
- `SFXLUT{sdword ID; udword checksum; sword numvariations, numevents, numobjects; sword lookup[]}` (GoK:src/Game/SoundStructs.h:45-53). Index = `object*numevents*(numvariations+1) + (event & SFX_Event_Mask)*(numvariations+1)` (SoundEvent.c:344). Slot 0 of each group is the variation count (`numvars = lut->lookup[index]`, SoundEvent.c:345), followed by patch numbers.
- **Verified on Cataclysm data.** `CET:assets/soundfx/shipcmnevents.lut` (32,134 bytes) has `ID = "PKG0"`, `checksum = 0x9C7C3600`, numvariations 10, numevents 10, numobjects 146. 14 + 2·(10+1)·10·146 = 32,134 exactly, so **the Cataclysm LUT format equals HW1's**. CET also asserts `numvariations` at 0x8 and `numevents` at 0xA (CET:game/Game/soundstructs.h:46-57).
- Runtime: `soundEventInit` (SoundEvent.c:719), `soundEventUpdate` (called from `rndRenderTask`, §1.11), ship ambient/doppler (`soundshipheading`), damage and status sounds. The CD-audio helper `soundEventPlayCD` exists (SoundEvent.c:2886).

### 7.5 Speech (`.vce`) and music (`.wxd`/`.wxh`)

| Item | HW1 value | Anchor |
|---|---|---|
| Voice file | `HW_comp.vce` (demo `DL_Demo.vce`) | GoK:src/SDL/utility.c:198-209 |
| Music file / header | `HW_Music.wxd` + `HW_Music.wxh` (demo `DL_Music.*`, OEM `OEM_Music.*`) | utility.c:198-202; GoK:src/Game/SpeechEvent.c:244-250 |
| Sentence LUT | `SoundFX\speechsentence_comp.lut` → `SENTENCELUT{ID, checksum, numcolumns, numevents, numactors, compbitrate[4], …}` + `PhraseLUT`: offsets into the `.vce` per (actor, event, variation) | SpeechEvent.c:186-187, 423-470; SoundStructs.h:56-75 |
| Stream chunks | `STREAMHEADER{sdword ID, sdword size}`, with `ID_STREAM_DATA = 'DATA' (0x41544144)` and `ID_STREAM_INFO = 'INFO' (0x4f464e49)`. INFO carries the **subtitle text** read by `ssSubtitleRead`. Old files have DATA before INFO (`VCE_BACKWARDS_COMPATIBLE`) | GoK:src/SDL/soundlow.h:91-92; soundcmn.h:12, 175-178; sstream.c:721-842, 867-970 |
| Queueing | `speechEventQueue(obj, event, var, variation, actor, player, linkto, timeout, volume)` → priority queue → `speechPlayQueue` → `soundstreamqueuePatch` | SpeechEvent.c:1329, 751; sstream.c:372 |
| Music | `musicEventPlay(track)`, `musicEventStop(track, fade)`, `musicEventNextTrack/PrevTrack` | SpeechEvent.c:2527-3110 |
| Volumes | `volSpeech`, `volMusic`, `volNIS`; NIS scripts override with `musicVolume/speechVolume/SFXVolume` | SpeechEvent.c:255-259; NIS §8 |

The Cataclysm file names for `.vce`, `.wxd` and `.wxh` are UNVERIFIED. Search the exe for `.vce`, `.wxd` and `_Music` (§12). The CET
symbol `speechEventQueue` @0x00522560 has the HW1 signature (CET:Cataclysm.h:171).

---

## 8. Cutscenes: NIS and animatics

### 8.1 NIS ("Non-Interactive Sequence", in-engine)

| Item | Detail | Anchor |
|---|---|---|
| Files | `nis\nNN[r1\|r2].nis` + `nis\nNN….script`, a table per race (HW1: `nisR1Names`, `nisR2Names`) | GoK:src/Game/SinglePlayer.c:3066-3110, 3357 |
| Header | `nisheader{identifier[12]="Crannnberry", version=0x101, flags, stringBlock(off), stringBlockLength, length(s), loop, nObjectPaths, objectPath(off), nCameraPaths, cameraPath(off), nLightPaths, lightPath(off), nEvents, events(off), nGenericObjects, genericObject, iLookyObject}` + `NIS_HeaderExtra` 64 bytes of runtime space | GoK:src/Game/NIS.h:64-79, 282-306 |
| Keyframes | `NIS_FrameRate 30.0f`; `spaceobjpath`/`camerapath` hold per-object time + x,y,z,h,p,b tracks (hierarchical, like `.mad`) | NIS.h:66-68, 137-170 |
| Load | `nisLoad(nis, script)` validates magic and version, copies the body, loads the string block separately, then parses the **localized** script (`nisLanguageSubpath`) with `scriptSet(…, nisScriptTable)` into events | GoK:src/Game/NIS.c:4698-5030 |
| Script keywords (`nisScriptTable`) | `with at deathDamage deathProjectile deathBeam attack haltAttack fire hide show invincible vincible soundEvent speechEvent fleetSpeechEvent animaticSpeechEvent startMusic stopMusic remainAtEnd cameraFOV swapRaces cameraCut fadeSet fadeTo colorScheme textFont textColor textPosition textMargin textDuration textScroll textFadeIn textFadeOut textCard cameraBlendIn wideScreenIn wideScreenOut focusAtEnd focus centreShip meshAnimStart meshAnimStop meshAnimPause meshAnimSeek blendCameraToFocus musicVolume speechVolume SFXVolume trailMove trailZero pauseUniverseToggle hideUniverseToggle environment midPointLookAt SMPTEOn SMPTEOff staticOn staticOff hideDerelict showDerelict newNIS attributes attackKasSelection keepMovingAtEnd numberLODs health` (the last four are present in ORIG too, ORIG:src/Game/nis.c:4388-4391; `numberLODs` → `nisMinimumLODSet`, `health` → `nisDamageLevelSet`). 66 active entries. `startBlackScreen`, `endBlackScreen` and `cameraBlendOut` are **commented out** in GoK (NIS.c:4398-4399, 4413) and ORIG (nis.c:4345-4346, 4360), so they are not script keywords | NIS.c:4373-4445; semantics in DOC:NISPipeline.doc |
| Playback | `nisStart(header, position, coordsys)`; `nisUpdateTask` runs at `UNIVERSE_UPDATE_PERIOD` (16 Hz) and calls `nisUpdate(NIS, dt)`; events dispatch through `nisEventDispatch[]`, indexed by `NEO_*` event opcode (61 opcodes + NULL `NEO_LastNEO`, NIS.c:133-196; enum NIS.h:165-228); `nisStop`, `nisGoToEnd`, text cards drawn by `nisTextCardListDraw` | NIS.c:133-197, 424, 580, 906, 1173, 1610, 1714, 5153 |
| Letterbox | `NIS_LetterHeight`. ORIG always uses 1.85:1: `(H - W*20/37)/2` (ORIG:src/Game/nis.h:69). **GoK changed it** to 1.85:1 below 1.5 aspect and 2.39:1 above, with a minimum of H/15 (GoK:src/Game/NIS.h:70-76). See also `nisScissorFade`, `nisBlackFade`, `rndDrawScissorBars` | NIS.h:450-455; render.c §1.11 |
| Test hooks | `/testNIS <file>`, `/testNISScript <file>`, `/nisCounter`, `/nisNoLockout`, numpad debug keys | DOC:NISPipeline.doc; ORIG:src/Win32/main.c (switch list) |
| Cataclysm timing | CET patches the NIS update-task registration and the `UNIVERSE_UPDATE_PERIOD` constants inside `nisUpdateTask` (0x004CC8F2, 0x004CC801, 0x004CC8BE, 0x004CC634-0x004CC7B8). Mapping to HW1 (Inferred from the CET comments and the x87 opcodes CET writes): 0x004CC8F2 = `taskStart(nisUpdateTask, UNIVERSE_UPDATE_PERIOD, 0)` (ORIG:src/Game/nis.c:575); 0x004CC8BE = `timeElapsed = UNIVERSE_UPDATE_PERIOD` (nis.c:506), whose `static real32 timeElapsed` (nis.c:418) is the global **0x00A4CAB4** that CET writes; 0x004CC801 = the period pushed to `nisUpdate(thisNisPlaying, …)` (nis.c:484); and 0x004CC634/701/72C/7B8 (`fadd`/`fcomp`/`fmul`/`fadd`) = the scissor/black-fade steps at nis.c:428, 447, 454, 473 | CET:Cataclysm.h:54-94; CET:PatchUniverse.cpp:364-395 |

### 8.2 Animatics and movies

- **Bink** (`binkw32.dll`, RAD; shipped in `SNAP:exe/binkw32.dll`).
  - Intro movies: `Movies\sierra.bik`, `Movies\relicintro.bik` (ORIG:src/Win32/utility.c:4167-4171).
  - Per-mission animatics: `Movies\<name>.bik` + `Movies\<name>.script` (subtitle/speech cues) (GoK:src/Game/Animatic.c:715-760).
  - Player: `ORIG:src/Win32/bink.c` (`BinkOpen(name, BINKNOTHREADEDIO)` with CD fallback, :189-197). Frames are blitted per `RGLtype` (S_RGB555 etc.; GoK:src/Game/Animatic.c:188-197, 513-522).
- **AVI** via Video for Windows (`AVIFileInit/AVIStreamOpenFromFile/AVIStreamGetFrame*`, `ORIG:src/Win32/avi.cpp`). `/disableAVI` turns movies off; CET's `DisableIntroMovies` clears the Cataclysm global `enableAVI` @0x008B8610 (CET:Main.cpp:20-32; CET:Cataclysm.h:204).
- GoK decodes Bink with **FFmpeg libavcodec** (`HW_ENABLE_MOVIES`, GoK:src/SDL/avi.c:38-60; meson option `movies`).

---

## 9. Networking

### 9.1 Transport

| | Win32 original | GoK |
|---|---|---|
| Internet | **WON "Titan" SDK** (`ORIG:src/Titan/*`, 183 files; `ORIG:src/Win32/TitanInterface.cpp` 6,556 lines). Directory server dir `L"/Homeworld"`, services `L"AuthServer"`, `L"TitanRoutingServer"`, `L"TitanFactoryServer"`; patch URL `http://homeworld.won.net` (TitanInterface.cpp:132-151). CD-key/auth via `ClientCDKey`, `HomeworldCRC[4]` | `TitanInterfaceC.stub.c` "simulate a WONnet connection that never works" (GoK:src/SDL/TitanInterfaceC.stub.c:1-40) |
| LAN | Titan in LAN mode: no routing server, fixed `LAN_GAMEKEY`; **IPX/SPX** (`AF_IPX`, `NSPROTO_SPX`) or TCP/IP; game port **2500**, advertisement port **2501** (`GAME_PORT`, `AD_PORT`) (TitanInterface.cpp:160, 642-830; GoK:src/Game/TitanNet.c:89-90) | `NetworkInterface.c` on **SDL_net**: TCP 10500, UDP broadcast 10600 (GoK:src/SDL/NetworkInterface.h:48-49). Compiled only with `HW_ENABLE_NETWORK`, which the GoK meson build does **not** define (grep of all `meson.build`); the old autotools option was `HomeworldSDL/Linux/stuff/configure.ac:218` |
| Tweaks | `NetTweak.script` → `NetTweaks[]` (`DIRSERVER_IPSTRINGS`, `PATCHSERVER_*`, `GAME_PORT`, `AD_PORT`, `ROUTING_SERVER_NAME "routingserv"`, `PATCHNAME "HomeworldPatch.exe"`, LAN advertise timers, keep-alive timers, `T1_Timeout/T2_Timeout`) | GoK:src/Game/TitanNet.c:60-160; StatScript.c:1883 |

Cataclysm: CET reports that the `CaptainGameInfo` struct "has changed. New size is 688" (CET:game/Win32/TitanInterfaceC.h:148-149).
The global `tpGameCreated` is @0x00886028 (CET:Cataclysm.h:199). The version strings `networkVersion` @0x008B8850 and `minorBuildVersion`
@0x008B8860 gate multiplayer compatibility. CET overwrites them with `"v1.02"` / `"0"` (CET:GameHooks.cpp:9-10, 133-136; CET:Cataclysm.h:243-244).

### 9.2 Lockstep model (captain = host)

- **Packet header** `HWPacketHeader{uword type; uword from; udword frame; [NetSyncChecksums checksums]; uword numberOfCommands; commands…}`, where `NetSyncChecksums{uword packetnum; uword randcheck; real32 univcheck; udword blobcheck}` exists when `SYNC_CHECK` is 1, which is the default (GoK:src/Game/CommandNetwork.h:211-230; GoK:src/Game/Switches.h:14).
- **Packet types:** `COMMAND 0xcccc, SYNC 0x5555, REQUESTEDSYNC 0xe555, NONCAPTAINREADY 0xaaaa, HORSERACE 0x0911, CHAT 0xb1ab, TRANSFERCAPTAINCY 0xca00, DROPPINGOUTOFLOAD 0x10ad, REQUESTSYNCPKTS 0xe155, IAMALIVE 0xa1a1, ALIVESTATUS 0xa155, FILE 0xf11e, INGAMEQUITING 0x15ed, LAGCHECK 0x1A5C, CHEATDETECT 0xcea1` (CommandNetwork.h:166-193). Captaincy-transfer subtypes are at :195-205.
- **Flow.**
  1. Every client's UI turns orders into `clSend*` command packets (`clSendMove`, `clSendAttack`, `clSendBuildShip`, …, CommandNetwork.c:667-1440) and sends them to the captain (`packetSendToCaptain`, :606).
  2. `captainServerTask` runs at `CAPTAINSERVER_PERIOD = 1/8 s` (two 16 Hz frames). It drains `ProcessCmdPktQ` and broadcasts one **SYNC** packet holding all queued commands plus frame number and checksums (`packetBroadcastSync`) (CommandNetwork.h:126; CommandNetwork.c:645, 2024-2160).
  3. Every machine (captain included) executes only what arrives in SYNC packets: `clWaitSyncPacket` blocks the universe update until the next frame's sync has arrived, and `clProcessSyncPacket` applies the commands (CommandNetwork.c:1494, 1873-2020).
  4. Lost packets are re-requested (`REQUESTSYNCPKTS`, `LastSyncPktsQ`, :1804-1870).
  5. Captaincy migrates on drop-out (`Captaincy.c`, `TRANSFERCAPTAINCY`).
- **Desync detection.** On each sync, `netcheckShow` computes `univGetChecksum()` (the **float sum of x+y+z of every live ship**, GoK:src/Game/UnivUpdate.c:5004-5274, sums at :5024-5026, `return (x+y+z)` at :5273; derelict and resource positions are added only in non-release builds: `#ifndef HW_Release` at ORIG:src/Game/univupdate.c:5233-5361, `#ifdef HW_BUILD_FOR_DEBUGGING` at GoK :5141-5269), `statsGetStatChecksum()` (blob/stat checksum) and `gamerand()`. It stores them in a ring and compares them with the captain's values (`netcheckCheckChecksum`). Mismatches bump `univSyncErr/blobSyncErr/randSyncErr` (GoK:src/Game/NetCheck.c:24-30, 854-960; `netcheckCheckChecksum` :854, `netcheckShow` :918). The extended "god sync" adds `univCalcShipChecksum` (UnivUpdate.c:5283; NetCheck.c:457-480; `clSendGodSync` CommandNetwork.c:1058).
- **Replay.** Recorded games (`recPack*`) record and replay exactly these packets (NetCheck.c:527-760). The `.fib` above has a `Recorded_Game` screen.
- **Determinism prerequisites.** The presentation layer must never touch simulation RNG streams. ETG, for example, uses its own `RANDOM_ETG` stream (ETG.c:7642), and the sync check includes `gamerand()`. See `docs/hw1-engine-gameplay.md` for the sim side.

---

## 10. Win32 original vs SDL port: what Cataclysm's binary still does the Win32 way

Cataclysm (1999-2000) predates the 2003 source release. The GOG build is still a 32-bit Win32 binary that loads `rgl.dll`/`rgld3d.dll`
(CET:README.md "Back up rgl.dll and rgld3d.dll"). Every left-hand column item below is therefore expected in `cataclysm.exe`.

| Subsystem | Win32 original (expect in Cataclysm) | GoK SDL port | Port note |
|---|---|---|---|
| Entry / pump | `WinMain` → `InitWindow` → loop `PeekMessage/GetMessage/DispatchMessage` + `utyTasksDispatch()` (ORIG:src/Win32/main.c:2989, 3282, 3439-3448) | `main` → `SDL_PollEvent` + `utyTasksDispatch()` (GoK:src/SDL/main.c:2037-2099, 2135-2300) | Same task model |
| Tasks | x86 **stack-switching asm** scheduler (ORIG:src/Game/task.c:255-310) | `switch(__LINE__)` coroutines (GoK:src/Game/Task.h:149-180); locals do not survive `taskYield` | See docs/hw1-port-lessons.md |
| Timer | `QueryPerformanceFrequency/Counter` (ORIG:src/Win32/utility.c:4095, 4851-4871). Cataclysm also keys `universe.realtimeelapsed` off QPC (CET:game/Game/universe.h:162) | `SDL_GetTicks` (ms) (GoK:src/SDL/utility.c:4676-4694) | GoK has coarser resolution. Prefer `SDL_GetPerformanceCounter` |
| GL binding | `LoadLibrary` + `GetProcAddress` of the GL subset (gldll.c); `rgl.dll`/`opengl32.dll`/3dfx MiniGL | Linked GL (SDL_opengl.h) or a GLES shim | §1 |
| Context | `wglCreateContext` with `ChoosePixelFormat` (16-bit Z), `glNT` quirk; rGL `rglCreateWindow`/`rauxInitPosition` | `SDL_GL_CreateContext` 2.0 compat, 24-bit Z | §1.10 |
| Display modes | DirectDraw enumeration (`DirectDrawCreate`, `EnumDisplayModes`) in `rinit.cpp`; `ChangeDisplaySettings` in `ddraw.cpp` (ORIG:src/Win32/ddraw.cpp:63-80, 182); DirectDraw window for movies (`dxdraw.cpp`) | `SDL_GetCurrentDisplayMode` integer-divided modes | CET replaces the enumeration callbacks (§11) |
| Device caps DB | `devstats.dat` CRC table → `gDevcaps/gDevcaps2` bits (ffe, 32bit, pal, d3d, aa, …) | none | Drop it; hard-code modern answers |
| Software renderer | `rglsw.dll` + `glcompat` fast front end + `rglSaveCursorUnder` | none | Drop |
| Glide | `sstglide.c` (3dfx 2D) | none | Drop |
| Keyboard | `WM_KEY*` VK codes (`KEY.H` values = VK) | SDL scancodes | Translate saved bindings |
| Mouse | `GetCursorPos`, `SetCursorPos`, `ClipCursor`, `ShowCursor(FALSE)` + game-drawn cursor | SDL mouse, relative mode, UI→render scaling | |
| Audio out | DirectSound (certified only) / waveOut, `_beginthread` mixer + streamer | `SDL_OpenAudio` callback + SDL threads | §7.3 |
| Audio codec | Relic FQ codec linked from `.obj` | C reimplementation | The reimplementation must be bit-compatible with Cataclysm's `.vce/.wxd/.bnk` (UNVERIFIED for Cataclysm data) |
| Movies | Bink SDK (`binkw32.dll`), VfW AVI | FFmpeg (optional) | |
| Network | WON Titan SDK (internet), Titan LAN over IPX/TCP | WON stub; SDL_net LAN disabled in the meson build | §9 |
| Settings | Registry `HKLM\SOFTWARE\Sierra On-Line\Homeworld` (value `CmdLine`, option table) (ORIG:src/Win32/regkey.h:9-13; main.c:20-36; utility.c:66-196) | `Homeworld.cfg` in the user dir (GoK:src/SDL/utility.c:142, 884-901) | Cataclysm's key name is UNVERIFIED (search §12) |
| Tick rate | 16 Hz | 64 Hz default, `/vanilla` = 16 Hz | 1:1 needs 16 Hz |
| Paths | Backslash literals everywhere (`"FEMan\\…"`, `"SoundFX\\"`, `"nis\\"`, `"ETG\\"`) | `#ifdef _WIN32` pairs with `/` | Normalize in the file layer, not at call sites |

---

## 11. What CET changes in presentation

CET is injected by `cataclysm_loader.exe` (`CreateProcess("cataclysm.exe …")` + `CreateRemoteThread(LoadLibraryA)`,
CET:loader/Main.cpp:6-7, 40-112). It patches through MinHook detours and in-place assembly (`Assembler::Write`).
All addresses are for the GOG `cataclysm.exe`.

### 11.1 PatchRGL (CET:PatchRGL.cpp)

| Patch | Cataclysm address | Original behaviour / bug | HW1 anchor |
|---|---|---|---|
| Replace `rinSortModes`, `rinEnumDisplayModes_cb`, `rinEnumPrimaryDisplayModes_cb` (when `NewRendererSelection`) with versions that accept **any ≥640×480 32-bit mode** (CET:game/Game/rinit.cpp:59-181) | 0x0057DE00, 0x0057E5C0, 0x0057E710 | Original whitelist: 640×480, 800×600, 1024×768, 1280×1024, 1600×1200, width ≤ 1600, plus devstats vetoes, so no widescreen or high resolutions | ORIG:src/Win32/rinit.cpp:178-230, 440-470 |
| `InitWindow` device-CRC check (naked hook at 0x00563563 → continue at 0x00563645; installed only with `NewRendererSelection`, CET:PatchRGL.cpp:29-61, 80-88) | 0x00563563 | When the saved `opDeviceCRC` differs from the current `rinDeviceCRC()` (always true on first run or new hardware) and `mainAutoRenderer` is set (default), the game **forces the Software renderer at 640×480×16**. CET instead stores the new CRC, uses the desktop resolution at 32 bpp, calls `mainRescaleMainWindow` and selects `"opengl32.dll"` | ORIG:src/Win32/main.c:3086-3110 |
| `jmp 0x0056208E` at `ActivateMe_ReInitRenderer` 0x00562084 (windowed only) | 0x00562084 | `ActivateMe` (WM_ACTIVATEAPP) runs `rglFeature(RGL_ACTIVATE); mainReinitRenderer = 2`, a full renderer reinit on every alt-tab, which is slow | ORIG:src/Win32/main.c:1775-1787; render.c:4124-4131 |
| NOP×8 at 0x0057B78B and 0x0057B7A1 (`DisableFramerateLimit`) | 0x0057B78B, 0x0057B7A1 | Cataclysm-added frame limiter in the render task. HW1 has none (only `Sleep(1)` in `rndFlush`) | ORIG:src/Win32/render.c:4952-4968 |
| Hook `glCapNT` → return TRUE (`OpenGLFix` / ini `FixWindowsNTBug`) | 0x00559AC0 | On anything but NT ≤ 4, the game calls the **ICD DLL's own exported `wglChoosePixelFormat/wglSetPixelFormat/wglSwapBuffers`** (through `rwgl*`) instead of GDI. On modern Windows the OpenGL renderer then fails ("OpenGL renderer being unavailable on newer operating systems") | ORIG:src/Win32/glcaps.c:533-552; render.c:851-870, 4952-4968; main.c:3352 |
| Post-hook `glCapStartup`: `glCapCompiledVertexArray = FALSE` if `NoCompiledVertexArrays` | 0x00559BE0, global 0x00A72548 | `GL_EXT_compiled_vertex_array` corrupts rendering on some AMD drivers | ORIG:src/Win32/glcaps.c:792-799; mesh.c:1674-1696 |

The bundled **hw_rgl D3D9 `rgld3d.dll`** replaces the DX6 driver: 4K modes, D24X8 depth (Z-fighting), faster DrawPixels-heavy UIs,
`FPU_PRESERVE`, and the Cataclysm `glPushAttrib(GL_BLEND/GL_LIGHTING)` support (§1.6; RGL:README.md).

### 11.2 PatchSound (CET:PatchSound.cpp; CET:game/Win32/soundlow.cpp)

| Patch | Address | Original bug | HW1 anchor |
|---|---|---|---|
| Replace `soundStartDSound(HWND)` unless `ForceWaveOut` | 0x005808E0 (globals `lpDirectSound` 0x00A75F28, `lpPrimaryBuffer` 0x00A75F2C, `bDirectSoundCertified` 0x00A75F3C, `coopDSound` 0x00A74DC8, `waveFormatEX` 0x00B06C80) | The original accepts DirectSound only when `GetCaps` reports **`DSCAPS_CERTIFIED`** (or `/dsound`). Modern drivers are not "certified", so the game falls back to **waveOut** with higher latency. CET's copy treats every driver as certified (`if (true /* || DSCAPS_CERTIFIED || useDSound */)`, CET:game/Win32/soundlow.cpp:65) and logs the result. It also **drops the original's `if (useWaveout) return SOUND_ERR;` prologue** (ORIG:src/Win32/soundlow.c:203-206; absent from CET:game/Win32/soundlow.cpp:15-23), so under CET the `/waveout` switch no longer forces waveOut; only ini `ForceWaveOut` (which skips the hook, CET:PatchSound.cpp:14-17) does. Its failure path assigns `lpPrimaryBuffer = NULL` to CET's pointer-to-global, not to the game global (soundlow.cpp:104) | ORIG:src/Win32/soundlow.c:197-291 (test at :249), fallback :343-350; smixer.c:211-322 |
| `ret` at `fqEqualize` (`DisableEqualizer`) | 0x00558F20 | Rare loud distortion on some systems from the spectral equalizer; disabling skips EQ entirely (EQ curves then have no effect) | GoK:src/SDL/fqeffect.h:83; ORIG `obj/fqeffect.obj` |

### 11.3 PatchLOD (CET:PatchLOD.cpp) — only with `HighDetailMode`

| Patch | Address | Effect |
|---|---|---|
| Replace `lodLevelGet` with: `currentLOD = 0`; recompute `cameraDistanceVector/Squared`; `return 0` | 0x004A4D70 | Every object renders at its top LOD. Note that the Cataclysm function returns an **int level**, while HW1 returns a `lod*` (GoK:src/Game/LOD.c:268-339). The "bug" here is design: the HW1 hysteresis thresholds `bOn/bOff × lodScaleFactor` were tuned for 1999 hardware |

### 11.4 PatchETG (CET:PatchETG.cpp) — only with `HighDetailMode`

| Patch | Address | Effect / original bug |
|---|---|---|
| Replace `etgNParticleBlocksSet` (the `particleBlocks(n)` keyword parser) with `nParticleBlocks = n << UpdateRateShift` | 0x0046E110 | At 32/64 Hz, effects that spawn sprites per tick overflow their particle-block array (default `ETG_NewParticleLength` 8). The parse-time size is scaled instead. The comment calls it "more of a hack than a proper fix" (CET:PatchETG.cpp:49-66; HW1 GoK:src/Game/ETG.c:5295-5309, ETG.h:181) |
| Replace `etgFrequencyExceeded` → `FALSE` | 0x00470BD0 | Removes the per-effect spawn-rate throttle (history ring × `etgHistoryScalar`), the "active effects cap" (HW1 GoK:src/Game/ETG.c:7609-7647) |
| Debug-only (compiled out with `#if 0`) hooks on `etgFunctionCall` 0x0046E7B0 and `etgEffectDelete` 0x0046A6F0 log every native ETG call with its parameter types | | Confirms the `etgfunctioncall` layout in Cataclysm (CET:PatchETG.cpp:9-45, 78-81) |

Observation (Inferred). `ApplyETGPatches` installs **both** hooks only when `HighDetailMode` is true (CET:PatchETG.cpp:90-95). With
`UpdateRateShift > 0` and `HighDetailMode = false`, the particle-block scaling is therefore not applied.

### 11.5 Other presentation-relevant CET fixes (GameHooks / PatchUniverse / PatchSaveGame)

| Fix | Address | Original bug | HW1 anchor |
|---|---|---|---|
| Skip code in `partRenderBillSystem` (jmp 0x004DD43E → 0x004DD468) | 0x004DD43E | Billboard particle rendering ends with `partFilter(FALSE)`, which sets `GL_NEAREST` min/mag filtering on **whatever texture is still bound**. Ship textures then randomly turn pixelated | ORIG:src/Game/particle.c:925 (`partFilter`, :540-555); GoK dropped the call: `partFilter` still exists at GoK:src/Game/Particle.c:571, but `partRenderBillSystem` (:703-965) never calls `partFilter(FALSE)` |
| `je 0x00562943` at `WindowProc_JumpIfF12KeyUp` 0x00562401 | 0x00562401 | The `WM_KEYUP VK_F12` debug case swallows the key-up, so F12 (military parade) works only once | ORIG:src/Win32/main.c:2744-2788 |
| Post-hook `opOptionsAccept` → `rndAspectRatio = w/h` | 0x004D7E30, global 0x00B377A4 | The aspect ratio is computed only in `rndInit`, so it goes stale after a resolution change in Options | ORIG:src/Win32/render.c:1038 |
| `LoadMission_AdjustTextureMemoryLimit` (0x00589AEA → 0x00589B1F), optional limit = INT32_MAX | 0x00589AEA | Mission 17 shrinks the texture pool, forcing low-res ship textures; the fixed VRAM budget triggers `trCram` down-scaling | GoK:src/SDL/texreg.c:59-70, 2392-2560 |
| Trails: `trailSegmentsRead` scales segment count by `1 << shift` (hook installed, PatchUniverse.cpp:338); N-LIPS/trail-scale-cap script callbacks (`shipNLipsRead` 0x008B2B9C, `trailScaleCapRead` 0x008B2C0C/0x008B2C7C/0x008B2CEC/0x008B2D5C) replaced by a reader that stores 0 if `DisableNLips`. A `mistrailUpdate` replacement that would run only every `factor` ticks is **defined but never installed**: no `CreateAndEnableHook` for it in `CreateAndEnableHooks` (:327-350) or anywhere else, so missile trails stay tick-rate dependent under CET | 0x005A7A70, 0x005A4D90 | Trails and missile trails are tick-rate dependent | CET:PatchUniverse.cpp:98-131, 449-453; Cataclysm.h:261-265 |
| NIS / ETG `at()` constants re-based to the new update period | see §8.1, CET:Cataclysm.h:87-94 | NIS and ETG timing hard-codes 1/16 s | CET:PatchUniverse.cpp:352-395 |
| MinHook detour on kernel32 `QueryPerformanceCounter` (process-wide, so the task timer sees it too) re-bases the counter so it starts at about 1 s worth of ticks on the first call | kernel32 | Cataclysm's new QPC-keyed `universe.realtimeelapsed` is a `real32`, so after long PC uptime it stops advancing; CET's comment says the SP mission start/end **fade-in/fade-out then fails to complete** | CET:PatchUniverse.cpp:291-324, 349; CET:game/Game/universe.h:162 |

**Gating.** Everything in `ApplyUniversePatches` (NIS/ETG constant re-basing, trail and N-LIPS changes, the QPC hook) is skipped when `UpdateRateShift == 0` (CET:PatchUniverse.cpp:426-431). At the vanilla 16 Hz, `DisableNLips` has no effect, and neither does the QPC fade fix. Patch order is fixed in CET:Main.cpp:37-46 (startup globals, game hooks, ETG, LOD, RGL, save game, universe, sound, AI).

| `SongNumberLoadGameOffset` redirected to a dummy | 0x00892670 | Loading an SP save zeroes the current music track right after reading it | CET:PatchSaveGame.cpp:6-12 |
| `BorderlessWindow` → `fullScreen = showBorder = FALSE`; `DisableIntroMovies` → `enableAVI = FALSE` | 0x008B8638, 0x008B8628, 0x008B8610 | Command-line equivalents `/window /noBorder /disableAVI` | CET:Main.cpp:20-32 |

---

## 12. Strings and constants to search for in the Cataclysm binary

The "HW1 exe" column is the number of hits from `strings -a` on `SNAP:exe/Homeworld.exe` (`strings -a -el` for UTF-16). A hit there
proves the literal survives into a release build of this engine, so the same literal (or a Cataclysm rename) should be in `cataclysm.exe`.
"rename?" marks HW1-specific names that Cataclysm probably changed. Search by stem in that case.

### 12.1 Strings

| # | Literal | Expect in | HW1 exe | Source anchor | What it finds |
|---|---|---|---|---|---|
| 1 | `rgl.dll` | exe | 9 | ORIG:src/Win32/main.c:218, glcaps.c:486 | `glToSelect` default, `glCapLoadOpenGL`, renderer switch code |
| 2 | `opengl32.dll` | exe | 8 | main.c:657, 1865-1890 | GL selection, 3dfx path search |
| 3 | `rglsw.dll` / `rgld3d.dll` | exe | 1 / 1 | main.c:1919-1920 | `mainFreeLibraries` |
| 4 | `3dfxvgl.dll` | exe | 2 | gldll.c:434 | `glDLLGetProcs` |
| 5 | `FX_GLIDE_NO_SPLASH=1` | exe | 3 | gldll.c:394-403 | `glDLLEnvironment` macro, next to `glDLLGetProcs` |
| 6 | `GL_RGL_rgl_feature` | exe | 1 | glcaps.c:649 | **`glCapStartup`** (CET: 0x00559BE0) |
| 7 | `GL_EXT_compiled_vertex_array` | exe | 1 | glcaps.c:792 | `glCapStartup`, then `glCompiledVertexArrays` (CET: 0x00A72548) |
| 8 | `GL_RGL_lit_texture_palette`, `GL_EXT_paletted_texture`, `GL_EXT_shared_texture_palette`, `GL_EXT_rescale_normal`, `GL_EXT_clip_volume_hint` | exe | 1/3/3/1/1 | glcaps.c:704-794 | glcaps globals |
| 9 | `direct3d` | exe + rgld3d.dll | 1 | glcaps.c:652; RGL:D3D9/d3driver.cpp:2319 | D3Dtype decision |
| 10 | `rglFeature`, `rglCreateWindow`, `rauxInitPosition`, `rglSelectDevice`, `rglMeshRender`, … (29 names) | exe | 5/5/2/… | glcaps.c:217-249; render.c:930, 988 | **`glCapGetRGLAddresses`**: names every rGL function pointer global |
| 11 | `glLockArraysEXT`, `glColorTableEXT`, `glLitColorTableEXT` | exe | 5/7/5 | render.c:1060; texreg.c:213-220 | `rndInit`, `rndSmallInit`, texreg reload |
| 12 | `3Dfx Interactive Inc.`, `GDI Generic`, `Savage` | exe | 1/1/2 | glcaps.c:611-641, 752, 871 | `glCapStartup` |
| 13 | `Default OpenGL`, `3dfx OpenGL`, `Software` (+ `OpenGL Standard`, `Standard-OpenGL`, `OpenGL predeterminado`, `OpenGL di Default`) | exe | 1/2/… | rinit.cpp:1066-1159 | **`rinEnumerateDevices`** |
| 14 | `couldn't initialize default rendering system` | exe | 2 | main.c:2400, 2406, 3150 | `InitWindow` (CET hooks 0x00563563 inside it) and the software-restore path |
| 15 | `devstats.dat` | exe | 2 | ORIG:src/Win32/main.c:1550 (`mainDevStatsInit`) | device-caps loader |
| 16 | `RMF97ba`, `RMF99ba` | exe | 2/2 | GoK:src/Game/Mesh.h:51-52 | **`meshLoad`** |
| 17 | `Willy 7` | exe | 2 | GoK:src/SDL/texreg.h:115 | `trLIFFileLoad` |
| 18 | `Event13`, `textures.ll` | exe | 2/2 | texreg.h:108-110 | `trListFileLoad` |
| 19 | `Mellonn` | exe | 2 | MeshAnim.h:37 | `madFileLoad` |
| 20 | `Crannnberry` | exe | 1 | NIS.h:65 | **`nisLoad`** |
| 21 | `Bananna` | exe | 2 | FEFlow.h:40 | **`feScreensLoad`** |
| 22 | `Orannge` | exe | 1 | font.h:47 | `fontLoad` |
| 23 | `RBF` (full header string `RBF1.23`) | exe | 8 with `strings -n 3` (6 × `RBF1.23` with default `strings`) | BigFile.h:67, 76; BigFile.c:485, 504 | `.big` reader / header check |
| 24 | `Homeworld.big`, `Update.big` (rename?) | exe | 1/1 | GoK:src/Game/BigFile.c:74-84 | big-file open. CET redirects 3 pushes at 0x005ABB21/3E/5C to `Update102.big` (CET:GameHooks.cpp:108-116) |
| 25 | `hsf\%s.hsf`, `hsf\default.hsf`, `.hsf` | exe | 1/2/2 | ORIG:src/Game/nis.c:3241; levelload.c:1463 | lighting loader |
| 26 | `ETG\%s.ebg`, `etg.script`, `general.script` | exe | 1/8/1 | GoK:src/Game/ETG.c:762-771 | **`etgStartup`**, effect loader |
| 27 | `eventStart`, `eachFrame`, `timeIndex`, `particleBlocks`, `createSprites`, `setEffOffsetLOF` | exe | 3/4/5/1/1/1 | ETG.c:258-694 | **`etgParseTable` / `etgFunctionTable`**: 153 named native-function entries |
| 28 | `Front_end.fib`, `Construction_Manager.fib`, `TaskBar.fib`, … | exe | 1/1/… | GoK:src/SDL/utility.c:226-238; ConsMgr.c:43 | `utyFrontEndFiles`, manager startups |
| 29 | `Main_game_screen`, `Sensors_manager`, `Construction_manager`, `Task_Bar`, `HyperspaceRollCall` | exe | 11/3/2/1/9 | utility.c:809; Sensors.h:36; ConsMgr.c:47; TaskBar.h:43; FEFlow.c:1945 | `feScreenStart` call sites |
| 30 | FE callback names `QuitGame`, `CSM_Dock`, `OP_TroubleShoot`, `TA_StartSinglePlayer`… | exe | 4/4/… | GoK:src/SDL/utility.c:431-520 | `fecallback` tables, i.e. names for every UI handler |
| 31 | `fonts\` + `English\`, `hw_eurosecond_11.hff`, `default.hff`, `Arial_12.hff` | exe | 2/…/2 | FontReg.h:35-40; KeyBindings.c:717 | **`frFontRegister`** |
| 32 | `SoundFX\` | exe | 35 | SoundEventPrivate.h:32 | sound loaders |
| 33 | `GunEvents.lut`, `ShipCmnEvents.lut`, `UIEvents.lut`, `Guns.bnk`, `Ships.bnk`, `UI.bnk`, `SpecialEffects.bnk` | exe | 1 each | SoundEvent.c:2680-2885 | **`SEloadbank`** |
| 34 | `Volume.lut`, `Range.lut`, `Frequency.lut` | exe | 2/… | SoundEvent.c:750-772 | `soundEventInit` |
| 35 | `speechsentence_comp.lut` | exe | 1 | SpeechEvent.c:444 | **`speechEventInit`** |
| 36 | `HW_Music.wxh`, `HW_Music.wxd`, `HW_comp.vce` (rename?: search `.wxh`, `.wxd`, `.vce`, `_Music`) | exe | 1/1/1 | SpeechEvent.c:249; utility.c:198-209 | music/speech stream open |
| 37 | `n01r1.nis` (rename?: search `nis\`, `.nis`, `.script`) | exe | 1 | SinglePlayer.c:3076-3110 | NIS name tables |
| 38 | `Movies\sierra.bik` (rename?: search `Movies\`, `.bik`); import `_BinkOpen@8` from `binkw32.dll` | exe | 1 | ORIG:src/Win32/utility.c:4167-4171; bink.c:189 | intro / animatic player |
| 39 | `Tactical.script`, `sensors.script`, `NetTweak.script`, `English.script` | exe | 1 each | Tactical.c:116; Sensors.c:3934; StatScript.c:1883; StringSupport.c:768 | TO, SM, network, localization init |
| 40 | `GAME_PORT`, `AD_PORT`, `LAN_ADVERTISE_USER_TIME`, `DIRSERVER_IPSTRINGS` | exe | 3/… | TitanNet.c:124-160 (`makeEntry` stringizes the variable name) | `NetTweaks[]`, so every network global is named |
| 41 | `routingserv`, `HomeworldPatch.exe` (rename?) | exe | 1/1 | TitanNet.c:92-94 | WON/patch code |
| 42 | UTF-16 `/Homeworld`, `AuthServer`, `TitanRoutingServer`, `TitanFactoryServer` (use `strings -el`) | exe | 1 each | ORIG:src/Win32/TitanInterface.cpp:132-137 | WON Titan client |
| 43 | `BKSPC`, `CAPS `, `TILDE`, `NEXT FORMATION`, `BUILD MANAGER` | exe | 1/…/1 | GoK:src/Game/Options.c:179-330 | key-binding UI (`kbKeyTable*` is next to it) |
| 44 | `/safeGL`, `/device`, `/nopal`, `/noFastFE`, `/dsound`, `/waveout`, `/testNIS`, `/disableAVI`, `/window`, `/noBorder`, `/sw`, `/noSavedMode` | exe | exact-line hits 1 each (`/sw` is only 3 characters, so it needs `strings -n 3`; substring: `/dsound` 2 incl. `/dsoundCoop`, `/testNIS` 2 incl. `/testNISScript`). `/d3d` and `/gl` are **not** options (commented out, main.c:1216-1217); the one `/d3d` substring hit is `/d3dDeviceCRC` (main.c:1203) | ORIG:src/Win32/main.c:1160-1360 | **command-line option table** (`entryVr/entryFn`): gives the address of each option global (CET: `fullScreen` 0x008B8638, `showBorder` 0x008B8628, `enableAVI` 0x008B8610) |
| 45 | `Sierra On-Line`, `CmdLine` (registry; Cataclysm key name UNVERIFIED) | exe | 5/15 | ORIG:src/Win32/regkey.h:9-13; main.c:20-36 | settings load/save |
| 46 | `DefaultShip.lod` | exe | 1 | GoK:src/Game/Universe.c:3085 | LOD table loader |
| 47 | rGL DLL side: `RelicGL rendering library`, `Relic Entertainment, Inc. [1.1a]`, `GL_RGL_rgl_feature GL_EXT_rescale_normal…`, `init_driver`, `rparams.dat` | **rgl.dll / rgld3d.dll** | n/a | RGL:draw.def:2; kgl.c:70-77, 280-290, 2825 | identify the shipped rGL build and its driver interface |

### 12.2 Numeric constants (for byte-pattern search)

| Constant | Value (LE bytes / float hex) | Anchor |
|---|---|---|
| `UNIVERSE_UPDATE_PERIOD` | 0.0625f = `0x3D800000` | ORIG:src/Game/universe.h:57-58 |
| `ETG_UpdateRoundOff` | 0.03125f = `0x3D000000` (Cataclysm global 0x0072A818) | CET:Cataclysm.h:194 |
| `etgEffectorCI` rate constant | −`UNIVERSE_UPDATE_RATE` = −16.0f = `0xC1800000` (Cataclysm `.rdata` 0x0072A82C; the value is Inferred from CET writing `-rate` there) | CET:Cataclysm.h:193; CET:PatchUniverse.cpp:52; HW1 `etgEffectorCI` GoK:src/Game/ETG.c:6564-6581 |
| `UTY_TimerResloutionMax` (task tick) | 240 = `0xF0` int; 240.0f = `0x43700000` (Cataclysm float 0x0072A614) | ORIG:src/Win32/utility.h:39; CET:Cataclysm.h:190 |
| `CAPTAINSERVER_PERIOD` | 0.125f = `0x3E000000` | CommandNetwork.h:126 |
| `LOD_ScaleFactor` | 0.6f = `0x3F19999A` | LOD.h:51 |
| `NIS_FrameRate` | 30.0f = `0x41F00000` | NIS.h:68 |
| `NIS_VersionNumber` | `0x101` | NIS.h:64 |
| `MESH_Version` | `0x00000402` | Mesh.h:54 |
| `FIB_Version` | `0x0212` | FEFlow.h:41 |
| `LIF_FileVersion`, `LL_FileVersion` | `0x104` | texreg.h:111, 116 |
| `MAD_FileVersion` | 0.91f = `0x3F68F5C3` | MeshAnim.h:36 |
| `BTG_FILE_VERSION` | `0x600` | BTG.h:14 |
| `FONT_Version` | `0x100` | font.h:46 |
| Texture pool | `0x00A00000` (10 MB) | texreg.c:65 |
| `FQ_RATE` | 22050 = `0x5622` | fqcodec.h:24 |
| Stream chunk IDs | `0x41544144` "DATA", `0x4F464E49` "INFO" | soundlow.h:91-92 |
| Packet types | `0xCCCC 0x5555 0xE555 0xAAAA 0x0911 0xB1AB 0xCA00 0x10AD 0xE155 0xA1A1 0xA155 0xF11E 0x15ED 0x1A5C 0xCEA1` | CommandNetwork.h:166-193 |
| Ports | 2500 (`0x9C4`), 2501 | TitanNet.c:89-90 |
| Custom GL enums | `GL_SWAPFRIENDLY 0xFFE01`, `GL_COMPILED_ARRAYS_EXT 0xFFE02`, `GL_LIT_TEXTURE_PALETTE_EXT 0x82FB`, `GL_SHARED_TEXTURE_PALETTE_EXT 0x81FB`, `GL_CLIP_VOLUME_CLIPPING_HINT_EXT 0x80F0` | ORIG:src/Win32/glcaps.h:18-19; glext.h:17-25 |
| `rglFeature` codes | 0x1001-0x100E, 0x2000-0x2002, 0x4600/0x4601 (SAVEBUFFER, issued every frame), 0x4640/0x4641 | RGL:kgl.h:622-680 |
| `RGLtype` | SW 0, GLIDE 1, D3D 2, GL 3 | ORIG:src/Win32/glext.h:103-106 |
| `RPE_*` region events | 0x80000 KeyDown, 0x100000 KeyUp, 0x2000 PressLeft, 0x10000 ReleaseLeft | Region.h:66-106 |
| Callback table sizes | `FE_NumberCallbacks 400`, `FE_NumberDrawCallbacks 150`, `FE_NumberScreens 150`, `FE_StackDepth 32` | FEFlow.h:125-128 |

### 12.3 Cataclysm presentation addresses already known (CET, GOG build only)

| Symbol | Address | | Symbol | Address |
|---|---|---|---|---|
| `glCapNT` | 0x00559AC0 | | `etgFunctionCall` | 0x0046E7B0 |
| `glCapStartup` | 0x00559BE0 | | `etgNParticleBlocksSet` | 0x0046E110 |
| `rinDeviceCRC` | 0x0057DCF0 | | `etgFrequencyExceeded` | 0x00470BD0 |
| `rinSortModes` | 0x0057DE00 | | `etgEffectDelete` | 0x0046A6F0 |
| `rinEnumDisplayModes_cb` / `…Primary…_cb` | 0x0057E5C0 / 0x0057E710 | | `lodLevelGet` (returns int) | 0x004A4D70 |
| `mainRescaleMainWindow` | 0x0055FC70 | | `trCramRAMComputeAndScale` | 0x00589A50 |
| `soundStartDSound` | 0x005808E0 | | `fqEqualize` | 0x00558F20 |
| `speechEventQueue` | 0x00522560 | | `feToggleButtonSet` | 0x00475990 |
| `opOptionsAccept` | 0x004D7E30 | | `uicTextEntrySet/Init`, `uicTextBufferResize` | 0x00538980 / 0x00538A30 / 0x005388F0 |
| `trailSegmentsRead` / `mistrailUpdate` | 0x005A7A70 / 0x005A4D90 | | `memAlloc` / `memFree` | 0x006725D0 / 0x00673080 |
| globals `glToSelect` | 0x008B8644 | | `MAIN_WindowWidth/Height/Depth` | 0x008B85E8/EC/F0 |
| `rndAspectRatio` | 0x00B377A4 | | `opDeviceCRC` | 0x00A4CC20 |
| `glCapCompiledVertexArray` | 0x00A72548 | | `etgTotalTimeElapsed` | 0x00C41C74 |

Source: CET:Cataclysm.h:37-266. The surrounding functions fall in clusters, which suggests Cataclysm kept HW1's per-file link order
(e.g. Win32 renderer files around 0x0055xxxx-0x0058xxxx; ETG around 0x0046xxxx-0x00470xxx). That is Inferred and useful for scoping searches.

**Presentation-relevant CET addresses missing from the table above** (all from CET:Cataclysm.h unless noted; `I:` = instruction address inside a function, `G:` = global):

| Symbol (CET name) | Address | Kind / use |
|---|---|---|
| `InitWindow_DeviceCrcCheck` → continue | 0x00563563 → 0x00563645 | I: CRC check in `InitWindow` (:96; PatchRGL.cpp:29) |
| `ActivateMe_ReInitRenderer` → jump target | 0x00562084 → 0x0056208E | I: `rglFeature(RGL_ACTIVATE); mainReinitRenderer = 2` (:98; PatchRGL.cpp:93) |
| frame-limiter instructions | 0x0057B78B, 0x0057B7A1 (8 bytes each) | I: inside the render task (PatchRGL.cpp:98-99) |
| `WindowProc_JumpIfF12KeyUp` / `WindowProc_NormalKeyUpEvent` | 0x00562401 / 0x00562943 | I: `WM_KEYUP` switch in `WindowProc` (:109-110) |
| `partRenderBillSystem_SetPointFiltering` / `_AfterSetPointFiltering` | 0x004DD43E / 0x004DD468 | I: `partFilter(FALSE)` tail of `partRenderBillSystem` (:103-104) |
| `LoadMission_AdjustTextureMemoryLimit` / `_After…` | 0x00589AEA / 0x00589B1F | I: mission-17 texture limit (:106-107) |
| `etgDefineAt_UpdateRate1/2/3` | 0x0046E042 / 0x0046E04F / 0x0046E05B | I: `fcom`/`fsub`/`fld` of `ETG_UpdateRoundOff/2` in the `at()` parser (:87-89; PatchUniverse.cpp:352-362; HW1 GoK:src/Game/ETG.c:5255-5258) |
| `nisStartup_…`, `nisUpdateTask_…` (6 sites) | 0x004CC8F2, 0x004CC8BE, 0x004CC801, 0x004CC634/701/72C/7B8 | I: see §8.1 (:55-60, 91-94) |
| NIS `timeElapsed` static | 0x00A4CAB4 | G: written by CET at 0x004CC8BE (PatchUniverse.cpp:393; Inferred = ORIG nis.c:418) |
| `utyGameSystemsPreInit_UpdateBigFileName1/2/3` | 0x005ABB21 / 0x005ABB3E / 0x005ABB5C | I: pushes of `"Update.big"` (:119-121) |
| `utyGameSystemsInit_UNIVERSE_UPDATE_PERIOD` / `_TimerResolutionMax` | 0x005AC174 / 0x005ABF68 | I: universe-task registration and timer set-up pushes (:52, 71) |
| `ETG_UpdateRoundOff`, `etgEffectorCI_UniverseUpdateRate` | 0x0072A818, 0x0072A82C | G: ETG timing floats (:193-194) |
| `TimerResolutionMax`, `utyTimerDivisor`, `realtimeremainder`, `taskFrequency` | 0x0072A614, 0x00AB7520, 0x00A6D9A0, 0x00BB3260 | G: frame/task timing (:190, 212-218) |
| `universe`, `universe.univUpdateCounter` | 0x00B62A00, 0x00B62BAC | G: (:189, 210); CET also writes 0x00B62ABC (= `universe`+0xBC) in `univUpdateReset` (PatchUniverse.cpp:416) |
| `fullScreen`, `showBorder`, `enableAVI` | 0x008B8638, 0x008B8628, 0x008B8610 | G: command-line option globals (:202-204) |
| `lpDirectSound`, `lpPrimaryBuffer`, `coopDSound`, `bDirectSoundCertified`, `waveFormatEX` | 0x00A75F28, 0x00A75F2C, 0x00A74DC8, 0x00A75F3C, 0x00B06C80 | G: sound back end (:236-240) |
| `SongNumberLoadGameOffset` | 0x00892670 | G: save-game field table entry for the music track (:233) |
| `networkVersion`, `minorBuildVersion`, `tpGameCreated` | 0x008B8850, 0x008B8860, 0x00886028 | G: MP version strings and game-info struct (:199, 243-244) |
| `shipNLipsRead`, `trailScaleCapRead`…`4` | 0x008B2B9C, 0x008B2C0C, 0x008B2C7C, 0x008B2CEC, 0x008B2D5C | G: script-table callback slots (`scriptSetReal32CB` pointers) inside `ShipStaticScriptTable` 0x008B2428 (:224, 261-265) |
| `scriptSet`, `scriptSetReal32CB`, `scriptSetUdwordCB`, `scriptSetFramesCB`, `scriptSetTweakableGlobals` | 0x00526420, 0x005259D0, 0x00525C20, 0x0053AC70, 0x00527820 | F: the `.script` loader used by every presentation tweak file (`Tactical.script`, `sensors.script`, ETG race scripts) (:162-166) |
| `clCommandMessage` | 0x004AC820 | F: on-screen command message, `(const char[], udword flags)`, an extra arg vs HW1 (:131) |
| `mgResourceInjectionInterval`, `mgResourceLumpSumInterval`, `mgGameTypesOtherButtonPressed` | 0x004BDE80, 0x004BE090, 0x004C2590 | F: multiplayer-setup FE callbacks (:149-151); their text-entry globals are 0x00A4C6C4 / 0x00A4C6CC (:196-197) |
| `dbgInt3Enabled` | 0x0087922C | G: debug `int3` on assert; CET clears it unless a debugger is attached (:246; GameHooks.cpp:153-156) |

---

## 13. Open questions / UNVERIFIED (resolve once the GOG offline installer is extracted)

1. Cataclysm's file names for speech, music and movies (`*.vce`, `*_Music.wxd/.wxh`, `Movies\*.bik`), its registry key, its WON directory path (`/Cataclysm`?) and its `.big` names (CET implies `Update.big` → `Update102.big` is redirectable).
2. Whether Cataclysm's `devstats.dat` exists and what it contains; whether `rglsw.dll` still ships.
3. The exact form of Cataclysm's render-task frame limiter (0x0057B78B/0x0057B7A1).
4. The Cataclysm `lodLevelGet` body: it returns an int, so how do callers use it, and are `mOn/mOff` now used?
5. The new particle types `PART_UNK1/UNK2` (CET:game/Game/particle.h:31-40): which ETG `create*` keywords produce them?
6. Whether Cataclysm's `soundStartDSound` still honours a `/dsound` switch. CET's replacement comment suggests the certified-or-`useDSound` test was effectively always false.
7. Which other `glPushAttrib` enums Cataclysm uses beyond `GL_BLEND`/`GL_LIGHTING` (hw_rgl handles only those, RGL:kgl.c:1240-1266).
8. Whether existing tools (Ghidra, llvm-pdbutil, cvdump) can import the embedded CodeView NB11 symbols of `SNAP:exe/Homeworld.exe` (UNVERIFIED). If none can, a small NB11 → Ghidra-script converter would be needed (`docs/hw1-port-lessons.md` lists the subsection counts).


## Verification

An independent skeptical pass on 2026-10-08, made after the author's own resume pass. Sources re-read: GoK HEAD `3af31fb`; ORIG files extracted with `git show ad1bb82:<path>` from a full GoK clone in scratch; hw_rgl `41ff3c3`; CET `19a08c3`; `SNAP:exe/Homeworld.exe` (PE header, NB11 offset and symbol strings parsed in Python, `strings -a` counts redone); the CET Cataclysm data files `assets/feman/multiplayer_game.fib` and `assets/soundfx/shipcmnevents.lut` (re-parsed).

**Claims checked: about 62 claim groups, roughly 200 individual anchors and values.** They cover:

| Area | What was checked |
|---|---|
| ETG tables | ETG table counts (153/150 GoK, 154 ORIG); the parse-table and function-table keyword lists |
| rGL interface | `glCapGetRGLAddresses`; the 103+9 `glDLLGetProcs` set against `draw.def`; `rglFeature` codes; `STR_*` strings; the D3D9 flags; the attrib stack in ORIG and hw_rgl |
| Win32 start-up and input | `WinMain`/`InitWindow` steps 1-11; the command-line table; F12/ESC `WM_KEYUP` |
| Render loop | `setupPixelFormat`, `rndFlush`, `rndClear`; `rndRenderTask` order; the default GL state; the single write of `rndAspectRatio`; the CVA paths; the glcaps policy lines; the `devstats.dat` format |
| CET addresses | every address in §12.3, against CET:Cataclysm.h |
| CET patches | every patch row in §11, against `PatchRGL`/`PatchSound`/`PatchLOD`/`PatchETG`/`PatchUniverse`/`PatchSaveGame`/`GameHooks`/`Main.cpp` |
| Asset formats | `.fib` struct sizes plus a Cataclysm `.fib` parse; the `.lut` arithmetic; struct asserts in CET headers; `.geo`, `.lif`, `.mad` and `.btg` headers |
| Engine tables and constants | NIS keyword and dispatch tables; NetCheck and checksum ranges; packet types and ports; key bindings; region event bits; `mainrgn` anchors; FQ and DirectSound constants |
| Exe strings and hex | about 45 string hit counts in the HW1 exe; every float/hex constant in §12.2 |

**Claims found wrong or imprecise (10), all fixed in place:**

| # | Where | Was | Fix |
|---|---|---|---|
| 1 | §1.2 step 6 | "`/d3d` sets rgl.dll + d3d" | `/d3d` and `/gl` are commented out (ORIG main.c:1216-1217). The live switch is `/device <dev>` (`SelectDevice` :644-653). Run-time OpenGL selection is `mainStartupGL` |
| 2 | §12.1 row 44 | `/d3d` listed with 1 hit | that hit is `/d3dDeviceCRC`. The row now lists `/device`, `/sw`, `/noSavedMode` |
| 3 | §1.2 step 5, §11.1 | CRC mismatch always forces software | only when `mainAutoRenderer` (default TRUE; `/noSavedMode`); `/sw` forces it. The CET hook exists only with `NewRendererSelection` |
| 4 | §3.2 | `setForceVisible` listed as a parse-table keyword | it is commented out (GoK ETG.c:280, ORIG etg.c:265) |
| 5 | §8.1 | `startBlackScreen`, `endBlackScreen`, `cameraBlendOut` listed as NIS keywords | they are commented out (GoK NIS.c:4398-4399, 4413; ORIG nis.c:4345-4346, 4360). There are 66 active keywords |
| 6 | §9.2 | `univGetChecksum` at UnivUpdate.c:5004-5040 | the function is 5004-5274. Derelicts and resources are summed only in non-release builds |
| 7 | §11.2 | CET `soundStartDSound` "identical except certified" | it also drops the `if (useWaveout) return SOUND_ERR` prologue, so `/waveout` is ignored under CET |
| 8 | §11.5 | "`mistrailUpdate` runs only every `factor` ticks" | the replacement is defined but **never installed** (no `CreateAndEnableHook`), so missile trails stay tick-rate dependent |
| 9 | §12.1 row 10 | 28 rGL names | 29 `rwglGetProcAddress` calls (glcaps.c:217-249) |
| 10 | §12.1 row 1 | `"rgl.dll"` at glcaps.c:485 | it is at :486 |

**Added because the scope implies it (all grounded):**

- `RGL_FEATURE_NOLINES/LINES`.
- The `STR_VERSION` MMX/XMM patch made at run time.
- The `.big` header `RBF1.23`.
- How the render task interleaves with 240 Hz task ticks and 16 Hz tasks, plus the Cataclysm timing globals.
- The ORIG texture-pool anchor and the 20 MB no-palette cache.
- Where the mission-17 hook sits (Inferred).
- Evidence that Cataclysm keeps the FE callback signature, and the changed `clCommandMessage` signature.
- An ORIG-line mapping for the 7 NIS patch sites, with global 0x00A4CAB4 (Inferred).
- The `etgEffectorCI` and 240 Hz constants.
- The CET QPC fade-fix row.
- The rule that **every `ApplyUniversePatches` change is skipped at `UpdateRateShift == 0`**.
- A 22-row table of presentation-relevant CET addresses that §12.3 omitted.
- The LUT slot-0 meaning, now grounded at SoundEvent.c:345.

**Remaining issues:**

- Everything about `cataclysm.exe` itself stays UNVERIFIED until the GOG offline installer is extracted. The CET addresses are only as good as CET.
- The NIS instruction-to-source mapping (§8.1) and the location of 0x00589AEA (§2.4) are Inferred from CET opcodes and comments, not from disassembly.
- Not re-checked in this pass: the DOC:*.doc citations, the §5 rows for the Launch and Research managers, Info overlay, Pie plate and Objectives (the `mainrgn`, Tactical, Sensors, TaskBar and ConsMgr anchors were checked), the §6.3 camera anchors, the §7.5 `sstream.c` ranges and the §1.12 GoK `rinit.c`/`main.c` ranges.
- The `RBF` count of 8 needs `strings -n 3`; default `strings` finds 6 `RBF1.23`. Other counts use default `strings -a`, so short literals may be under-counted.
