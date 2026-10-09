export const meta = {
  name: 'homeworld-infection-dissect-public',
  description: 'Opus agents map the Homeworld 1 engine source, Cataclysm Engine Tweaks, port lessons, and build the Ghidra + Proton tooling for the Homeworld: Emergence recreation project',
  phases: [
    { title: 'Dissect', detail: 'seven parallel Opus readers/builders, each producing one doc or tool set', model: 'opus' },
    { title: 'Verify', detail: 'one skeptic per deliverable spot-checks claims against the source and fixes errors', model: 'opus' },
    { title: 'Synthesize', detail: 'write PLAN.md + README.md, checkpoint commit', model: 'opus' },
  ],
}

const ROOT = '/home/evanmedinger/Documents/GameDev/Homeworld-Infection'
const REF = ROOT + '/reference'

const COMMON = `
PROJECT CONTEXT (read fully before starting):
You are working on "Homeworld-Infection" at ${ROOT}: a project to reverse engineer Homeworld: Cataclysm (re-released by GOG in 2017 as "Homeworld: Emergence") into a 1-to-1 recreation that runs natively on Windows 11 and Linux. Cataclysm's source was lost. It is a fork of the Homeworld 1 engine, whose source Relic released in 2003 (now the HomeworldSDL / Gardens of Kadesh port). A fan DLL-hook project, Cataclysm Engine Tweaks (CET), has already reversed parts of the GOG executable.

The user has NOT yet supplied the real game: the file they downloaded (original/GOG_Galaxy_Homeworld_Emergence.exe) is only GOG's Galaxy download stub. So in this round nothing runs or disassembles the game binary. Everything you produce must be grounded in the PUBLIC reference material and be ready for the moment the real GOG offline installer (setup_homeworld_emergence_*.exe) lands in ${ROOT}/original/.

Reference clones (read-only except where your task says to build in a separate build dir):
- ${REF}/HomeworldSDL        : Homeworld 1 source port mirror (Relic 2003 source + SDL port). src/Game (~300 files), src/Ships, src/SDL, documentation/, Linux/BUILD.md
- ${REF}/gardens-of-kadesh   : the active continuation of HomeworldSDL (may be missing if the clone failed; then use HomeworldSDL)
- ${REF}/CataclysmEngineTweaks : CET source (C++, Detours-style hooks). Patch*.cpp files, game/Game + game/rgl + game/Win32 reconstructed headers, loader/
- ${REF}/hw_rgl              : CET's replacement Direct3D9 renderer implementing the rGL DLL interface (if missing, clone it: git clone --depth 1 https://github.com/ncblakely/hw_rgl.git ${REF}/hw_rgl)

Machine facts: Pop!_OS Linux, COSMIC desktop on Wayland (WAYLAND_DISPLAY=wayland-1, DISPLAY=:1 via Xwayland), NVIDIA RTX 5090 (driver 580.x open), 24 CPUs, Java 21 installed, innoextract 1.9 installed, gamescope 3.14.x at /usr/local/bin/gamescope, no system wine; Proton builds at ~/.local/share/Steam/compatibilitytools.d/GE-Proton10-34 and "~/.local/share/Steam/steamapps/common/Proton - Experimental". cosmic-screenshot exists; grim does not. Passwordless sudo works for apt-get (use ONLY for development packages you genuinely need, e.g. libsdl2-dev, meson, grim; never remove packages).

HARD RULES:
- Never fake mouse/keyboard input to any program (no xdotool/XTEST/ydotool). Never launch a full-screen game. GUI smoke tests, if any, must be tiny and windowed, and must be closed again.
- Never commit anything under original/ or reference/. Do NOT run git commit/add at all; the final synthesis agent commits.
- Do not modify files inside the reference clones. Build in a separate directory if you build.
- Write only to the files your task names (plus scratch under /tmp/claude-1000/ if needed). Other agents write other files in parallel.
- Ground every claim in a file you actually read. Cite file paths (repo-relative) and, for important facts, line numbers. If you did not verify something, label it "UNVERIFIED".
- Docs are Markdown, written for an engineer who will do the decompilation and port; be concrete and dense, prefer tables and file:line references over prose. No fluff.
- Your final text is a return value to a script, not a message to a person.
`

const DOC_SCHEMA = {
  type: 'object',
  properties: {
    doc_path: { type: 'string' },
    files_written: { type: 'array', items: { type: 'string' } },
    summary: { type: 'string', description: 'at most 150 words' },
    key_facts: { type: 'array', items: { type: 'string' }, description: '8-20 one-line facts most useful to the decompilation/port' },
    open_questions: { type: 'array', items: { type: 'string' } },
    blocked_on_game_binary: { type: 'array', items: { type: 'string' }, description: 'things that need the real GOG installer' },
  },
  required: ['doc_path', 'files_written', 'summary', 'key_facts', 'open_questions', 'blocked_on_game_binary'],
}

const VERIFY_SCHEMA = {
  type: 'object',
  properties: {
    doc_path: { type: 'string' },
    claims_checked: { type: 'number' },
    claims_wrong: { type: 'number' },
    fixes_applied: { type: 'array', items: { type: 'string' } },
    remaining_issues: { type: 'array', items: { type: 'string' } },
    confidence: { type: 'string', enum: ['high', 'medium', 'low'] },
  },
  required: ['doc_path', 'claims_checked', 'claims_wrong', 'fixes_applied', 'remaining_issues', 'confidence'],
}

const TASKS = [
  {
    key: 'hw1-core',
    label: 'map:hw1-core',
    out: ROOT + '/docs/hw1-engine-core.md',
    prompt: `TASK: Map the CORE/PLATFORM layer of the Homeworld 1 engine from ${REF}/HomeworldSDL/src (prefer gardens-of-kadesh if present and newer). Write ${ROOT}/docs/hw1-engine-core.md.
Cover, with file:line anchors: program entry and startup sequence (main/WinMain path in src/SDL and src/Game, command-line options, registry/config), the main loop and frame timing (how the universe update rate is fixed at 16 Hz: find the constant, the accumulator, and every place that assumes it), the Task scheduler (Task.c: how tasks are registered, their frequencies, the task table), memory manager (Memory.c: pools, mem handles), file I/O and the .big archive format (BigFile.c, File.c, bigfile tools in the repo: header layout, compression, CRC, how overriding files on disk works), the .script config parser (Scripting.c/Tweak: how scriptSet tables bind names to globals), string tables and localization, save/load game serialization (SaveGame.c: what structures are serialized and how pointers are fixed up), demo record/playback, the DebugWindow/logging, the Key/Option tables, and the module dependency graph at the directory level (which .c files include which). Finish with a section "What Cataclysm most likely changed here" based on CET evidence you can see in ${REF}/CataclysmEngineTweaks (e.g. PatchUniverse.cpp universe update rate hooks) and a section "Strings and constants to search for in the Cataclysm binary" listing 40+ literal strings from these files that are likely to survive verbatim in the executable (error messages, script variable names, file names).`,
  },
  {
    key: 'hw1-gameplay',
    label: 'map:hw1-gameplay',
    out: ROOT + '/docs/hw1-engine-gameplay.md',
    prompt: `TASK: Map the GAMEPLAY/SIMULATION layer of the Homeworld 1 engine from ${REF}/HomeworldSDL/src (prefer gardens-of-kadesh if present). Write ${ROOT}/docs/hw1-engine-gameplay.md.
Cover, with file:line anchors: the Universe and object model (Universe.c/Universe.h, SpaceObj.h: the object type hierarchy Ship/Bullet/Asteroid/etc., the per-ship StaticInfo vs instance data, linked-list containers), the update pipeline (UnivUpdate.c: order of operations each tick), ship behaviour modules in src/Ships (how a ship type is a struct of function pointers: list every ship and its special abilities), the command layer (CommandLayer.c: command types, attack/move/dock/formation state machines), tactics, formations, docking, construction/research trees (ResearchAPI, ShipDefs, .script data), damage/collision (Collision.c, Physics.c: fixed-timestep physics and how determinism for multiplayer lockstep is kept), AI (AIPlayer/AIFleetMan/AITeam/AIMoves: the AI architecture), SinglePlayer.c and the mission system: explain exactly how missions are authored (KAS language, kas2c compiler under tools/ or similar, generated C in src/Missions or Game/*.c) and why that means Cataclysm missions live as compiled code in its executable, multiplayer game rules, trade/salvage. Finish with sections "Cataclysm gameplay additions to expect in the binary" (from public knowledge of the game: Beast infection, ship upgrades, modules, time compression, fog of war, waypoints, Siege Cannon, ship-type list for the Somtaaw and Beast races, as far as CET's assets/RACE_* and .script files reveal them; label anything not grounded in a file as UNVERIFIED) and "Strings and constants to search for in the Cataclysm binary" (40+ literal strings).`,
  },
  {
    key: 'hw1-presentation',
    label: 'map:hw1-presentation',
    out: ROOT + '/docs/hw1-engine-presentation.md',
    prompt: `TASK: Map the PRESENTATION layer of the Homeworld 1 engine from ${REF}/HomeworldSDL/src (prefer gardens-of-kadesh if present), cross-referenced with ${REF}/hw_rgl and ${REF}/CataclysmEngineTweaks/game/rgl. Write ${ROOT}/docs/hw1-engine-presentation.md.
Cover, with file:line anchors: the renderer abstraction (the rGL interface: what rgl.dll/rgld3d.dll are, the function table the engine expects, glcaps/rinit/render.c, how the native OpenGL path and the rGL path coexist, which GL features/extensions are used, compiled vertex arrays), mesh/geometry formats (.geo, .mex, LOD system, texture .lif format and texture memory pools, mesh animation .mad), effects (ETG: the effect template language, its compiler, how effects are executed), the front end (FEFlow/feman screens, .fib files, how screens and callbacks are bound, fonts and .hsf), the in-game UI (tactical overlay, sensors manager, task bar, construction manager), camera and input handling (mouse/keyboard routing, the key binding table), sound (fqcodec, soundlow, SoundEvent, DirectSound vs SDL paths, music streaming .wxd, speech .vce), cutscenes (NIS files and their player), networking (TitanNet/WON, LAN, the lockstep sync and checksum), and the differences between the Win32 original paths and the SDL port paths (so we know what Cataclysm's binary still does the Win32 way). Finish with "What CET changes in presentation" (summarize PatchRGL, PatchSound, PatchLOD, PatchETG with the original bug each fixes) and "Strings and constants to search for in the Cataclysm binary" (40+ literal strings).`,
  },
  {
    key: 'cet-knowledge',
    label: 'map:cet-knowledge',
    out: ROOT + '/docs/cet-knowledge.md',
    prompt: `TASK: Extract EVERYTHING Cataclysm Engine Tweaks already knows about the GOG Homeworld: Emergence executable. Read all of ${REF}/CataclysmEngineTweaks (every .cpp/.h, the loader, the .ini, assets/, vcpkg.json) and ${REF}/hw_rgl. Write ${ROOT}/docs/cet-knowledge.md AND a machine-readable ${ROOT}/ghidra/cet_symbols.json.
cet-knowledge.md must contain: (1) which executable CET targets (exe name, any version/hash/size/timestamp checks, image base, how the loader injects the DLL, whether it rebases), (2) the hooking mechanism (Assembler.cpp, detours, inline patches: list every hook/patch site with address, what instruction bytes it expects, and what it replaces), (3) a complete table of every absolute address CET references: address, symbol name CET uses, kind (function/global/vtable/string), inferred type/signature, source file:line, (4) every reconstructed struct/class in game/Game, game/rgl, game/Win32 with field offsets and the engine concept it corresponds to in HomeworldSDL (name the HW1 file/struct), (5) per Patch*.cpp: the original engine bug or limitation, the fix, and which HW1 source function it corresponds to, (6) the rGL DLL ABI as hw_rgl implements it (exported functions, the dispatch table, D3D9 specifics), (7) the config/.ini options and what they toggle, (8) the loader's start-up sequence, (9) known issues and the 'universe update rate' caveats, (10) a section "Address anchors for Ghidra" and (11) "Gaps: what CET does not know".
cet_symbols.json schema (exactly): {"target": {"exe": "...", "image_base": "0x...", "notes": "..."}, "symbols": [{"address": "0x00xxxxxx", "name": "snake_or_camel_name", "kind": "function|data|vtable|string|patch_site", "type": "C signature or type if known, else \\"\\"", "source": "CataclysmEngineTweaks/File.cpp:LINE", "hw1_equivalent": "HomeworldSDL/src/Game/File.c:function or \\"\\"", "notes": "short"}]}. Include every address you find; precision matters more than prose. Validate the JSON parses (python3 -c 'import json;json.load(open(...))').`,
  },
  {
    key: 'port-lessons',
    label: 'build:hw1-port',
    out: ROOT + '/docs/hw1-port-lessons.md',
    prompt: `TASK: Learn how the community ported Relic's Homeworld 1 source to modern Linux, and attempt a native Linux build of it on this machine as a toolchain proof. Write ${ROOT}/docs/hw1-port-lessons.md.
Part A (reading): from ${REF}/HomeworldSDL and ${REF}/gardens-of-kadesh (if present) document: the build system (meson.build, Linux/BUILD.md, flake.nix: dependencies and options), the SDL layer (src/SDL: what Win32 API surface it replaces: window, input, timing, threads, file paths, registry emulation), endianness and 64-bit fixes (grep for 'LITTLE_ENDIAN', 'udword', pointer-size assumptions, 'HW_ENABLE_*' defines), floating-point determinism concerns, asset path/case-sensitivity handling, the list of enhancements vs Relic's original (documentation/HWSDL_enhancements.txt, CHANGELOG), what was removed (Titan/WON networking? Glide? rGL?), the licence (LICENSE.txt: summarize terms, especially the non-commercial clause), and the original Relic tools shipped in the repo (kas2c, bigfile tools, etc.).
Part B (doing): try to build gardens-of-kadesh (fallback HomeworldSDL) natively into ${REF}/build-hwsdl (out of tree; do not modify the clones). Install missing dev packages with 'sudo -n apt-get install -y ...' (meson, ninja, libsdl2-dev, libgl-dev, libopenal-dev, libvorbis-dev, etc. as the build demands). Record every command and every error verbatim in the doc. Report honestly whether the binary was produced (path, size, 'file' output). Do NOT run the game (there are no assets). If the build cannot be made to work within ~25 minutes of effort, stop and document where it failed.
Part C: "Lessons for the Cataclysm recreation": a concrete list of port tasks the HW1 port had to do that a Cataclysm decompilation will have to repeat, plus which HomeworldSDL files could be reused wholesale as the platform layer for a Cataclysm rebuild.`,
  },
  {
    key: 'runbook',
    label: 'tools:proton-runbook',
    out: ROOT + '/docs/RUNBOOK.md',
    prompt: `TASK: Build the "get the GOG game running on this Linux machine" toolkit, ready to execute the moment the real installer arrives. Write ${ROOT}/docs/RUNBOOK.md and executable scripts under ${ROOT}/tools/: extract_installer.sh, setup_prefix.sh, install_cet.sh, run_game.sh, screenshot.sh (all bash, 'set -euo pipefail', PROJECT_ROOT derived from the script location, logging to ${ROOT}/logs/ which you may create).
Requirements:
- extract_installer.sh: find setup_homeworld_emergence*.exe (+ any .bin parts) in ${ROOT}/original/, run innoextract (--gog, into ${ROOT}/original/game/), then write original/game/MANIFEST.sha256 (every file with size and sha256) and print the game exe(s) found with 'file' output, PE timestamp and size. Must refuse to run on the Galaxy stub (original/GOG_Galaxy_Homeworld_Emergence.exe) with a clear message.
- setup_prefix.sh: create a dedicated Wine prefix at ${ROOT}/prefix using GE-Proton10-34 (~/.local/share/Steam/compatibilitytools.d/GE-Proton10-34) WITHOUT Steam having to be running. Research and implement the most reliable way: either umu-launcher (check if 'pip install --user umu-launcher' or a GitHub release zipapp works on this machine; install it under ~/.local if it does) or direct 'proton run' with STEAM_COMPAT_CLIENT_INSTALL_PATH / STEAM_COMPAT_DATA_PATH. ACTUALLY TEST prefix creation now (it does not need the game): run something headless like 'cmd /c ver' or 'wineboot -u' inside it and show the result in the doc. Decide DXVK on/off defaults for a 2000-era 32-bit D3D7/OpenGL game and for CET's D3D9 renderer (DXVK on) and document how to flip it.
- install_cet.sh: download the latest Cataclysm Engine Tweaks release zip with 'gh release download -R ncblakely/CataclysmEngineTweaks' into ${ROOT}/original/cet/ and unpack it into original/game/ (backing up rgl.dll / rgld3d.dll first, per the CET README). Write it, do not run the install part (no game yet), but DO run the download part now and list what the release zip contains in the doc.
- run_game.sh: launch cataclysm_loader.exe (CET) or the original exe (flag) inside the prefix, in a WINDOWED nested gamescope session at a configurable size (default 1920x1080; the user's displays are 4K; keep it a window, never fullscreen). Per the user's standing notes gamescope on this box needs 'SDL_VIDEODRIVER=x11' and '--prefer-vk-device 10de:2b85'. Also provide a '--no-gamescope' path. Log stdout/stderr with timestamps; print the log path.
- screenshot.sh: capture the game window for later visual verification without any input injection. Investigate what works on COSMIC/Wayland here: cosmic-screenshot (does it have a non-interactive mode?), grim (installable via apt; does it work on COSMIC's compositor?), or gamescope's own capture options. Test on the current desktop (a desktop screenshot to ${ROOT}/logs/ is fine) and document what actually worked.
- RUNBOOK.md: step-by-step from "installer downloaded" to "game window on screen", the exact GOG download instructions (GOG.com -> Games -> Homeworld: Emergence -> 'Download offline backup game installers'; expected setup_*.exe size roughly a few hundred MB), what each script does, how to verify, troubleshooting, and a "Windows 11 side" section (CET install steps) for completeness.
Do not launch any game. You may launch tiny windowed test programs and must close them. Test every script's syntax with 'bash -n' and its help/dry-run path.`,
  },
  {
    key: 'ghidra',
    label: 'tools:ghidra-pipeline',
    out: ROOT + '/docs/ghidra-pipeline.md',
    prompt: `TASK: Install Ghidra and build a headless analysis pipeline for the Cataclysm executable, tested now on a stand-in PE. Write ${ROOT}/docs/ghidra-pipeline.md, ${ROOT}/tools/ghidra_headless.sh, and Ghidra scripts under ${ROOT}/ghidra/scripts/.
Step 1: Ghidra 12.1.4 is being downloaded in the background to ~/Applications/ghidra-dl/ghidra_12.1.4_PUBLIC_*.zip; a file ~/Applications/ghidra-dl/DONE appears when complete (FAILED on error). Poll with 'sleep 20' loops up to 20 minutes. If FAILED or absent after that, download it yourself: 'gh release download Ghidra_12.1.4_build -R NationalSecurityAgency/ghidra --pattern "ghidra_12.1.4_PUBLIC_*.zip" -D ~/Applications/ghidra-dl'. Unzip into ~/Applications/ (yields ~/Applications/ghidra_12.1.4_PUBLIC). Verify 'support/analyzeHeadless' runs with Java 21.
Step 2: write ${ROOT}/tools/ghidra_headless.sh <pe-file> [project-name]: creates/reuses a Ghidra project under ${ROOT}/ghidra/projects/, imports the PE with the x86:LE:32 default language, runs auto-analysis with sensible options for an MSVC6-era 32-bit game (aggressive instruction finder, decompiler parameter ID, function start patterns), then runs post-scripts and exports to ${ROOT}/ghidra/exports/<name>/: functions.csv (address,name,size,calls_in,calls_out), strings.csv (address,string,xref_count,first_xref_function), imports.txt, a decompiled C dump of all functions (one file per function or a single concatenated .c, your choice; document it), and a symbol summary. Scripts: ${ROOT}/ghidra/scripts/ImportSymbolsJson.py (Jython or PyGhidra, whichever works headless in 12.1.x: reads ${ROOT}/ghidra/cet_symbols.json of the form {"target":{...},"symbols":[{"address":"0x...","name":"...","kind":"function|data|vtable|string|patch_site","type":"...","source":"...","hw1_equivalent":"...","notes":"..."}]} and applies labels/functions/comments; it must tolerate the file being absent since another agent is producing it right now) and ExportAnalysis.py (the exports above).
Step 3: smoke-test the whole pipeline on a stand-in 32-bit PE from this machine, e.g. a small DLL under ~/.local/share/Steam/compatibilitytools.d/GE-Proton10-34/files/lib/wine/i386-windows/ (pick something ~100-500 KB). Record timings and the export file sizes/heads in the doc. Also write a tiny valid cet_symbols.json test fixture in /tmp/claude-1000/ to prove the importer applies labels.
Step 4: in the doc, write the plan for the real binary: how we will combine (a) CET's address anchors, (b) string-xref matching against the HomeworldSDL source (propose a concrete script: for each string literal in the HW1 source, find the function(s) using it; for each string in the Cataclysm binary, find the function(s) referencing it; propose names), (c) optional BinDiff/Diaphora function matching (check whether Diaphora can be installed: 'pip install --user diaphora' or a git clone; document, do not force it), and (d) how to export to a format the decompilation effort can consume. Be explicit about Ghidra version-specific gotchas you hit.`,
  },
]

function verifyPrompt(t, r) {
  const written = (r && r.files_written || []).join(', ')
  return `${COMMON}
TASK: You are a skeptical verifier. Another agent just produced ${t.out} (and possibly: ${written}). Its summary: ${JSON.stringify(r && r.summary || '')}.
Verify it against reality, then FIX it in place:
1. Pick at least 15 concrete, falsifiable claims from the document (file:line references, constants, struct offsets, addresses, function names, command outputs, "X calls Y", "this script does Z"). Prefer the claims that matter most to a decompilation/port. Check each by reading the cited source file, grepping the reference clones, or running the script's safe path (bash -n, --help, dry-run). For claims about script behaviour, read the script.
2. For every wrong or unverifiable claim: correct it in the document (or script) if you can establish the truth; otherwise mark it clearly as UNVERIFIED with a note. Never delete correct content.
3. Look for what is MISSING that the task title implies (e.g. an engine map that never mentions the 16 Hz constant, a symbols JSON with addresses the CET code references but the table omits, a runbook script with no error handling). Add it if you can ground it in the sources within reason.
4. Append a short "## Verification" section at the end of the doc: claims checked, claims wrong and how fixed, remaining issues.
Rules: same hard rules as above (no git commit, no fullscreen anything, don't modify reference clones). Keep the document's structure; do not rewrite it wholesale.`
}

phase('Dissect')
log(`Dissecting public material with ${TASKS.length} Opus agents, each followed by a skeptic verifier`)

const results = await pipeline(
  TASKS,
  t => agent(COMMON + '\n' + t.prompt + `\nReturn via the StructuredOutput tool; doc_path must be ${t.out}.`,
             { label: t.label, phase: 'Dissect', schema: DOC_SCHEMA, model: 'opus' }),
  (r, t) => agent(verifyPrompt(t, r),
             { label: 'verify:' + t.key, phase: 'Verify', schema: VERIFY_SCHEMA, model: 'opus' })
           .then(v => ({ task: t.key, doc: t.out, produced: r, verified: v })),
)

const ok = results.filter(Boolean)
log(`${ok.length}/${TASKS.length} deliverables produced and verified; synthesizing plan`)

phase('Synthesize')
const digest = ok.map(x => ({
  task: x.task, doc: x.doc,
  summary: x.produced && x.produced.summary, key_facts: x.produced && x.produced.key_facts,
  open_questions: x.produced && x.produced.open_questions,
  blocked_on_game_binary: x.produced && x.produced.blocked_on_game_binary,
  verification: x.verified,
}))

const plan = await agent(`${COMMON}
TASK: You are the lead. Read every document under ${ROOT}/docs/ (all were just written and verified) plus ${ROOT}/ghidra/cet_symbols.json and the scripts in ${ROOT}/tools/, then write:
(1) ${ROOT}/docs/PLAN.md: the project plan for a 1-to-1 recreation of Homeworld: Cataclysm/Emergence on Windows 11 and Linux, in milestones. M0 "Stable run on this machine" (GOG build under Proton + CET, verification by screenshot and the user's own play-test; list the exact commands from RUNBOOK.md). M1 "Binary map": Ghidra import, CET anchors, string-xref naming against the HW1 source, module-by-module coverage targets with the HW1 directory map as the checklist (percent of functions named). M2 "Reimplementation strategy": decide and justify the approach (recommended: a HomeworldSDL-style tree where HW1 code is reused where the binary matches and Cataclysm-specific functions are decompiled and cleaned one module at a time, verified by behaviour comparison against the running GOG build; discuss the alternative of a matching decompilation and why/when), the order of modules (platform → file formats → universe → ships → UI → missions last, or justify another order), the role of CET's hooks as a test harness (hook the real game to dump state for comparison), and how mission code (compiled KAS) will be recovered. M3 "Native Linux + Windows 11 builds". Then "Later: additional features" as a placeholder (the user has features in mind; leave a clearly marked section). Include a risk table (lost source, legal/licence (Relic RDN non-commercial, Gearbox IP), SafeDisc if a disc copy is ever used, floating-point determinism, mission code volume), and an explicit "Blocked until the real installer arrives" list merged from all docs. Be concrete: every milestone gets entry criteria, exit criteria, and the tool/script that proves it.
(2) ${ROOT}/README.md: short project README (what this is, legal stance in 3 sentences, layout of the repo, how to get started = RUNBOOK.md, docs index with one line per doc).
(3) Then stage and commit everything in ${ROOT} EXCEPT original/ and reference/ (they are gitignored; double-check with 'git status --porcelain' that no game binary or clone is staged) with the message "Public-material dissection: HW1 engine maps, CET knowledge base, Ghidra + Proton tooling" using 'git -c user.name=evanmedinger -c user.email=thefiveegos@gmail.com commit'. Append this trailer line to the commit message: "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>".
Agent digest for orientation (the docs themselves are the source of truth): ${JSON.stringify(digest).slice(0, 20000)}
Return a JSON object via StructuredOutput: {commit: "<short hash or 'none'>", plan_path, readme_path, milestones: [string], top_risks: [string], blocked_until_installer: [string]}`,
  { label: 'synthesize:plan', phase: 'Synthesize', model: 'opus', effort: 'high',
    schema: { type: 'object', properties: {
      commit: { type: 'string' }, plan_path: { type: 'string' }, readme_path: { type: 'string' },
      milestones: { type: 'array', items: { type: 'string' } },
      top_risks: { type: 'array', items: { type: 'string' } },
      blocked_until_installer: { type: 'array', items: { type: 'string' } },
    }, required: ['commit', 'plan_path', 'readme_path', 'milestones', 'top_risks', 'blocked_until_installer'] } })

return { deliverables: digest, plan }
