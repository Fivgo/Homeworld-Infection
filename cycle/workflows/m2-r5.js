export const meta = {
  name: 'homeworld-infection-m2-r5',
  description: 'M2-R5, stage-2 close-out + stage 3 part A of the port: the Ship layout pass + dock purposes, the command layer follow-up + force-field formations + tactics, render/GL, sound and speech, the HUD, each ported into src/ and audited in sequence; then the R5 exit test (stage-2 exit still holds, the dock/command/render/sound/HUD self-tests pass with ASan 0, no CAT-TODO left naming M2-R4) and STATE.md',
  phases: [
    { title: 'Port', detail: 'five src/ producers in sequence (they share src/ and the build), each followed by its auditor', model: 'fable' },
    { title: 'Integrate', detail: 'R5 exit test, summary, STATE.md, checkpoint', model: 'fable' },
  ],
}

// Same two-root layout as M2-R1..R4. REPO is the git worktree this round writes and commits in (branch m2).
// MAIN holds what git never tracks: Ghidra project and exports, original/, reference/.
const MAIN = '/home/evanmedinger/Documents/GameDev/Homeworld-Infection'
const REPO = MAIN + '/.claude/worktrees/m2'
const REF = MAIN + '/reference'
const EXP = MAIN + '/ghidra/exports/cataclysm_gog_r6'
const SCRATCH = MAIN + '/original/scratch-m2'
const HARNESS = SCRATCH + '/m2r2-harness'
const CKPT = `cd ${REPO} && CHECKPOINT_COAUTHOR="Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>" tools/checkpoint.sh`
const PLAN = `${REPO}/docs/m2-r4-summary.md "Plan for M2-R5 and M2-R6"`
const LEFT = `${REPO}/docs/m2-r4-summary.md "What is left of stage 2"`
// The commit the round starts from (M2-R4 complete). The first port's work is everything after it; each later port's
// work is everything after the previous port's audit checkpoint (WIP snapshots included).
const ROUND_BASE = '1434b90'

// RESUME support, as m2-r4.js grew it over four relaunches. A probe agent re-reads docs/audits/m2-r5-*.md, so a relaunch
// in a fresh session skips every audited port. PRIOR holds finished ports' full results copied from an earlier run's
// journal (for the integrate digest; optional). RESUME holds per-port notes when a producer was cut off mid-port.
const PRIOR = {}
const RESUME = {}

const COMMON = `
PROJECT: Homeworld-Infection: a 1-to-1 recreation of Homeworld: Cataclysm (GOG "Homeworld: Emergence", retail 1.01) for Linux and Windows 11. M2 (reimplementation) follows Option A: the engine is Homeworld 1 (src/ = Gardens of Kadesh 3af31fb) and we port Cataclysm's differences into it. M2-R1 measured the deltas; M2-R2 ported stage 1 (formats + data load); M2-R3 stage 2 part A; M2-R4 stage 2 part B (exit test met: two 6,000-tick headless Mission01 oracle runs byte-identical with ASan 0, sp 1,248 ticks ASan 0, fe Main_game_screen, release SELFTEST PASS, layout32 OK). This round, M2-R5, is the stage-2 close-out plus stage 3 part A (render, sound, HUD) per ${PLAN} (your producer's row in the M2-R5 table) and ${LEFT} (items 1-7, with the owners R5-1 / R5-2 / R5-3 named there).
USER DECISIONS (binding): src/ base = GoK 3af31fb; Cataclysm behaviour wins per subsystem; 16 Hz universe default; FP/optimisation flags pinned (-O2 -fno-strict-aliasing, no fast-math); Relic ad1bb82 Game/ and Ships/ at src/reference/hw1-ad1bb82/ (reference only); the oracle may call the game's own record/playback functions; DLL toolchain clang-18 + lld-link; LAN interop with stock 1.01 is nice-to-have, not required (do not break the wire format gratuitously). The faithfulness defaults in the tree are ACCEPTED (k/tCloakGenerator on DefaultShipHeader where the exe has NULL; crateInit resets 46 entries; the mission-17 texture cut follows the stock exe, not CET; the fq audio code follows the binary), and so are the M2-R4 defaults recorded as decisions 17-22 in ${REPO}/cycle/STATE.md "## M2" (CAT_EXTERNAL_BUILD_DRIVER 0 until R6-4; names chosen from setters for unknown exe names; HWI deviations only where the exe would fault or leak, marked CAT-FIX; CAT_HW1_MISSION_SCRIPTS 0 until stage 6; the LeakSanitizer suppressions). The user will supply the oracle reference recording and two saves LATER: do not block on them; leave a clear hook where they plug in.
PATHS: the working copy is the git worktree ${REPO} (branch m2). Read docs and write EVERY tracked file there; never write into ${MAIN} directly (blocked). Untracked inputs in ${MAIN}: the named Ghidra export ${EXP}/ (cataclysm_gog_r6: functions.csv, symbols.csv, strings.csv, call_graph.csv, decompiled.c, datatypes.h), the game ${MAIN}/original/ (game/, game_extracted/cataclysm/), the reference clones ${REF} (read-only; CET headers in ${REF}/CataclysmEngineTweaks). Scratch for anything derived from game data: ${SCRATCH}/ (gitignored).
READ FIRST: ${REPO}/cycle/STATE.md ("## M2"), ${REPO}/docs/m2-r4-summary.md (your row, "What is left of stage 2", the exit-test tables and harness facts), ${REPO}/docs/CONSTRAINTS.md, then the R4 port reports ${REPO}/docs/m2-r4-*.md and audits ${REPO}/docs/audits/m2-r4-*.md that touch your scope (and older R2/R3 reports where they do), ${REPO}/docs/formats/*.md, ${REPO}/docs/m2-r1-function-deltas.md + ${REPO}/ghidra/delta_map.json (python3 -I tools/m2_stage_counts.py lists your scope's functions; rerun it on the map you start from), ${REPO}/docs/m2-checksum-oracle.md + ${REPO}/docs/m2-r2-oracle.md + ${REPO}/docs/m2-r4-rest-oracle.md (the mission oracle mode), ${REPO}/docs/hw1-engine-gameplay.md.

PORTING CONVENTIONS (every src/ producer): change src/ in place, smallest faithful change; every Cataclysm-specific change carries a comment "CAT: <what> (<exe function> 0x00xxxxxx)"; IDENTICAL functions stay verbatim, but COMPARE EVERY IDENTICAL BODY IN YOUR SCOPE with the exe (R4 found 58 modified) and record each one you find modified as a review entry in ghidra/delta_map.json; new Cataclysm-only files go next to their HW1 siblings with the tree's REULA header style; struct layouts follow docs/formats/*.md and the CET headers, with static asserts on sizes (32-bit layout checks via tools/m2_layout32.sh). Retire the HW1 type aliases in the files you port, and replace every CAT-TODO / CAT-PROVISIONAL that names your producer (R5-1 .. R5-5, or the M2-R4 owner your row inherits) with the port, or re-target it to a later stage with a reason. Keep tools/build.sh --debug and --release green with 0 warnings under -Werror after every subsystem. Never commit (your auditor checkpoints); never put game data, build outputs or logs from game data into git.

TEST HARNESS: ${HARNESS}/ (plain: only what a real install has, no workarounds) and ${REPO}/tools/m2_harness.sh (read its header for usage: modes sp, fe, statics, meshes, textures, oracle, etg, mission; NEVER pass --help or an unknown flag: the script treats the first argument as a run label and starts a run). Runs land in ${HARNESS}/runs/<label>/ with summary.txt. The isolation harness tools/m2_harness_iso.sh is for experiments only; every DONE WHEN is judged on the PLAIN harness. The mission mode (/oracleMission N /oracleDump) is the headless Mission01 oracle run: two runs of 6,000 ticks must give byte-identical .hwo streams (tools/oracle/mission_test.sh and hwo_compare, as in docs/m2-r4-rest-oracle.md; about 22 s each). sp needs --timeout 340 or more (the intro animatic plays in real time); judge sp milestones by univUpdateCounter and ASan. A NEW SELF-TEST (dock, command, render, sound, hud) is an engine switch (/catDockTest etc.) that runs scripted with no input and quits with a "<name>test: done" line, plus a matching --mode in tools/m2_harness.sh following the existing modes (offscreen driver, summary lines, rc), documented in the script header and your report.
SILENT RUNS (docs/CONSTRAINTS.md item 5): every engine run (tests, self-tests, oracle, exit tests) goes through tools/m2_harness.sh, which makes it silent and headless (SDL_AUDIODRIVER=dummy, offscreen video); never bypass it with HWI_AUDIBLE or a direct engine launch. The sound self-test decodes into the dummy device; nothing may reach the speakers. Never run the GOG game; never inject input; no gamescope.

SESSION RULE (the session can restart or hit a usage limit without warning): keep your report doc current after EVERY finished subsystem (a "<!-- done: a, b -->" marker at its top) and keep the tree building. RESUME RULE: if your report doc exists, read it, keep what is done and finish only what is missing. Your work so far is \`git diff <base>\` in ${REPO} (base given in your task), which includes any "WIP (snapshot ...)" commits made after a cut-off; never start over from a WIP snapshot, continue it. HARD RULES: do NOT run Ghidra (no Ghidra stage this round); ground every claim in a file you read or a command you ran; your final text is a return value, not a message.
`

const PROD_SCHEMA = { type: 'object', properties: {
  report: { type: 'string' }, files_changed: { type: 'array', items: { type: 'string' } }, summary: { type: 'string' },
  done_when_met: { type: 'boolean' }, harness_milestone: { type: 'string' }, first_stop: { type: 'string' },
  counts: { type: 'object' }, open_questions: { type: 'array', items: { type: 'string' } } },
  required: ['report', 'files_changed', 'summary', 'done_when_met', 'harness_milestone', 'first_stop', 'open_questions'] }
const AUDIT_SCHEMA = { type: 'object', properties: {
  audit_path: { type: 'string' }, sampled: { type: 'number' }, wrong: { type: 'number' }, precision: { type: 'number' },
  fixed_in_place: { type: 'number' }, build_ok: { type: 'boolean' }, verdict: { type: 'string' }, commit: { type: 'string' },
  notes: { type: 'array', items: { type: 'string' } } },
  required: ['audit_path', 'sampled', 'wrong', 'precision', 'build_ok', 'verdict', 'notes'] }

// Order follows docs/m2-r4-summary.md: the two stage-2 close-out producers first (the Ship layout pin is systemic, so
// it is the very first item), then render, sound and the HUD (which draws through the renderer).
const PORTS = [
  { key: 'layout-dock', label: 'port:layout-dock',
    scope: `Layout pass + dock purposes + flight/math compares (M2-R5 producer 1, row 1 of ${PLAN}; ${LEFT} items 1, 3, 4, 5 and the item-6 leftovers marked R5-1). FIRST the systemic header: Ship pinned at the exe's 0x63C with CET's 40 offsets (health at 0x170), the +0x18 word as one field (retire the CAT_OBJ_FLAGS18 bridge), SpaceObj.attributes as the exe's dword at +0x38 (the blast bits 0x10000/0x20000, the crystal game option), the Bullet/Missile layouts (the Missile +0x254 float), with ILP32 asserts in tools/m2_layout32.sh; keep the oracle's checksum terms by name so the mission stream stays comparable. THEN Dock.c and LaunchMgr.c: the purposes 0..10 in place of HW1's dock bitmask, cat_dockGetDockClass 0x00463d70, cat_dockShipDocksAtShip 0x00465860 and its cone helpers (cat_dockFlyOutOfCone, cat_dockFindNearestDockPoint, cat_dockFindLaunchPoint, cat_dockFinishedDocking, cat_dockCanDockWithBusyHost, cat_dockInitShipDockInfo, cat_dockHasFreeSlotForShip, cat_dockFlyInFrontOfHost), ThisShipCanDockWith's 4th argument and per-class counts, clDock 0x00466c70, the REWRITTEN dockReserveDockPoint / RemoveShipFromDocking / RemoveShipFromLaunching, processDockToDo's swarm part, dockPutShipOutside (modules, FOW, rotations), clLaunchShip's 3-argument InitializeDockVars, cat_clHarvestNearestResource 0x00449790, the SPECIAL_EXITING_DOOR readers, cat_univClampResourceTimer; replace the CAT-PROVISIONAL hive / slipgate / Leech / Worker / Processor dock stand-ins, the hive's purpose 2 and the carrier door; delete the HW1-only dock paths (95 of Dock.c's 123 functions are HW1-only). The experience level: cat_shipUpdateExperienceLevel 0x005147d0 and its 9 upgrade helpers, the level in cat_shipGetSpeedMultiplier / cat_shipGetDamageMultiplier, kills to experience in ApplyDamageToTarget, clCreateShip's per-class stats. The R5-1 leftovers (cat_gunPickMothershipModuleTarget, cat_collSentinelFieldExempt, the four dissolve helpers, the three variant StaticInits, cat_shipHasRamUpgrade). Compare the 97 IDENTICAL bodies of FlightMan.c, Vector.c, Matrix.c, Volume.c, fastmath.c, Twiddle.c, Teams.c, Undo.c, Debug.c (108 / 20 / 3, 8,123 M+R B; cat-new 14 / 7,629 B).`,
    done: `tools/m2_layout32.sh holds Ship at 0x63C (health 0x170) and the +0x38 dword; a new scripted self-test /catDockTest (harness mode dock: a scripted start with a carrier, a processor, workers and fighters; orders through clDock / clLaunchShip / clHarvest; no input) ends with every ship where the exe's state machine puts it, ASan 0; no HW1 dock path, purpose bitmask or CAT-PROVISIONAL dock stand-in is left; two 6,000-tick mission streams still byte-identical; sp as before with ASan 0`,
    audit: `Check 20 Ship/SpaceObj/Bullet/Missile offsets against the exe and CET and run tools/m2_layout32.sh; compare 25 bodies (dock state machine and helpers, clDock, launch, harvest, experience, 5 of the compared FlightMan/math IDENTICALs) with the exe in ${EXP}; grep for any HW1 dock bitmask reader or dock CAT-PROVISIONAL left; run the dock self-test, two 6,000-tick mission runs (compare the streams) and sp (--timeout 340).` },
  { key: 'command-tactics', label: 'port:command-tactics',
    scope: `Command layer follow-up + force-field formations + tactics (producer 2, row 2 of ${PLAN}; ${LEFT} item 2 and the item-6 leftover marked R5-2). CommandLayer.c (47 / 49 / 1 at the end of R4, 10 bodies ported then): RemoveShipsFromDoingStuff with its reason codes, FreeCommandToDoContents / FreeLastOrder, the attack, move and hyperspace order families, clSpecial, clHalt*, clProtect, clSetMilitaryParade, clHoldingPattern, clProcess, clPostProcess, GroupShip, clRUTransfer; the orders 11-16 (cat_clLink, cat_clUnlink, cat_processLinkToDo, cat_processUnlinkToDo, cat_clMimic, cat_clResearch, cat_clPointOrder, cat_clUpgradeShips, cat_clSpecialTarget, cat_clCalculateMoveToOffsets, cat_clAddAttachedModulesToTargets, cat_clFindLinkPartner), the link flight (cat_linkFlyIntoPosition, cat_linkComputeTargetCoordSys); COMMAND_CAT_POINT for the big gun; CommandWrap.c's cat_clWrapSetModuleSlot / SetLeechAction / Mimic / Unleech and the Beast-system MP path. Formation.c (18 / 15, 12,524 B): clFormation's 4th argument, FormationCommand +0x288, the force-field wall and sphere (SetFormationOffsetsForForceFieldWall, cat_formationSphereSentinelLinks, formationSpecificsSphere, createSphereStaticInfo, setFormationToDo, processFormationToDo, GetShipsTravelVel, processMilitaryParadeToDo, setMilitaryParade, paradeSetTweakables). Tactics.c: the Cataclysm TacticsInfo layout, the 7 HW1 tactics callers, cat_tacticsRetaliate* (12 cat-new), cat_univLimitProtectedShipSpeed; cat_beastGetAttackStaticInfo. Compare the 75 IDENTICAL bodies of the files not yet compared (80 / 79 / 2, 53,636 M+R B; cat-new 40 / 15,044 B).`,
    done: `every MODIFIED body of CommandLayer.c, CommandWrap.c, Formation.c and Tactics.c is ported or listed in the report with an owner outside stage 2; a new scripted /catCommandTest (harness mode command: link, unlink, point, mimic and research orders and a force-field wall and sphere of Sentinels over N ticks) completes with the exe's end states and ASan 0; sp as before with ASan 0; the two 6,000-tick mission streams still byte-identical`,
    audit: `Compare 25 CommandLayer/CommandWrap/Formation/Tactics bodies (at least 5 of the orders 11-16 and 3 force-field functions) with the exe in ${EXP}; list every MODIFIED body of the four files from the delta map and check each is ported or owned; run the command self-test, sp (--timeout 340) and two 6,000-tick mission runs.` },
  { key: 'render', label: 'port:render',
    scope: `Render/GL (producer 3, row 3 of ${PLAN}; the stage-3 renderers of ${LEFT} "Outside stage 2" and the item-6 leftovers marked R5-3). render.c (cat_rndRenderObjectList 9,890 B: the object list with the per-vertex glow state, fade, dissolve and mimic flags, depth sort), the lens flares (cat_rndLensFlaresRender, the .script callbacks), the fog layers (cat_rndFogLayer*, cat_scriptSetFogCB), the billboards (cat_partRenderUnk1System), Shader.c, Light.c, Clipper.c, prim2d.c / prim3d.c, Color.c, glcompat.c, glcaps.c, rinit.c / .cpp, Star3d.c, BTG.c, NavLights.c, Trails.c, Ping.c, screenshot.c, debugwnd.c, Transformer.c; the renderer halves the ship ports left (Sentinel field segments, Dreadnought charge, Nomad beam, blast lightning, the hyperspace gate rectangle hsStaticGateRender, the repair-droid proxies, cat_meshLineIntersect for the ramming latch and cutting beam, the damage points cat_dmgShipLoadDamagePoints / Free), the ETG real32 parameters and madAnimationStart's third argument (218 / 36 / 0, 27,544 M+R B; cat-new 71 / 36,696 B).`,
    done: `a new /catRenderTest (harness mode render) renders N frames of Mission01 offscreen through the Cataclysm object-list renderer and logs its object, lens-flare and fog-layer counts: counts > 0, ASan 0, and the 6,000-tick mission stream unchanged (render-side randoms stay on their streams); fe unchanged; sp as before`,
    audit: `Compare 25 render/GL bodies (the object list, lens flares, fog layers, billboards, the ship renderer halves, cat_meshLineIntersect) with the exe in ${EXP}; run the render self-test, fe, sp (--timeout 340) and two 6,000-tick mission runs; confirm the counts > 0 come from the Cataclysm paths (not a stub).` },
  { key: 'sound', label: 'port:sound',
    scope: `Sound and speech (producer 4, row 4 of ${PLAN}). SoundEvent.c (13,629 MODIFIED B), SoundEventPlay.c / SoundEventStop.c, SpeechEvent.c (the alerts, the race voice, the Easter egg), smixer.c, soundlow.c, sstream.c, mixfft.c, the fq codec (fqcodec, fqeffect, fquant: follow the binary), dct.c, Subtitle.c, Battle.c (battle chatter); every sound/speech CAT-TODO of the R3/R4 ports becomes a call (the dock speech ids, cat_soundEventBlast, the asteroid break and harvest sounds, speechEventUnderAttack's merged branch, cat_clSpecialTargetSpeech, cat_ioMessageAdd's speech side); fix the fquant.c misaligned loads and the PATCH misaligned accesses (SoundEvent.c / soundlow.c UBSan kinds) the faithful way (122 / 44 / 1, 35,711 M+R B; cat-new 17 / 2,771 B).`,
    done: `a new /catSoundTest (harness mode sound) opens Music4Cat.wxd, SectSpeech.vce and BeastSpeech.vce, decodes one stream of each through the fq codec into the DUMMY audio device and looks up the 88 battle-chatter events: completes with ASan 0 and 0 UBSan reports in fquant.c / fqcodec.c (and no PATCH misaligned report in any run); sp and fe unchanged; nothing audible (the harness forces SDL_AUDIODRIVER=dummy)`,
    audit: `Compare 20 sound/speech/codec bodies with the exe in ${EXP}; confirm the R3/R4 sound CAT-TODOs are calls now (grep); run the sound self-test, fe and sp (--timeout 340) through tools/m2_harness.sh only and count the UBSan kinds in each summary; confirm the runs used the dummy audio driver.` },
  { key: 'hud', label: 'port:hud',
    scope: `HUD: status bars and picking, ship info panel, overlays, pie plate, game-stats screen (producer 5, row 5 of ${PLAN}). The 25 cat_shipPrint* / cat_selStatus* / cat_selHealth* functions (the ship info panel 7,471 B and the status bars 2,896 B) with selStatusDraw0/1/2 and the picking API at the exe bodies; ShipView.c; InfoOverlay.c (cat_ioClick, the overlay buttons and print options); Tactical.c (the cat_mo* main overlay: construction, research, tooltips, game messages, player names, support units, objectives); PiePlate.c (cat_pieCompassDraw); TaskBar.c; Scroller.c; the message scroller (cat_ioMessageAdd, cat_ioUpdateHover); GamePick.c and the game-stats screen (cat_gameStats*); HorseRace.c; the ship names (cat_shipNames*); the overlay icons; the 21 UI cat-new the R4 tool moved to stage 3 (40 / 22 / 2, 15,938 M+R B; cat-new 100 / 43,905 B).`,
    done: `a new /catHudTest (harness mode hud) selects Mission01's ships through the selection API and draws the status bars, the info panel, the overlays and the pie plate for N frames offscreen: ASan 0; the 21 UI cat-new are ported; fe and sp unchanged; the mission stream unchanged`,
    audit: `Compare 20 HUD bodies (status bars, info panel, main overlay, pie plate, game stats, picking) with the exe in ${EXP}; run the hud self-test, fe, sp (--timeout 340) and two 6,000-tick mission runs; confirm the 21 UI cat-new are in the tree.` },
]

function portPrompt(p, base) {
  return `${COMMON}
TASK (M2-R5 port, key ${p.key}; your base commit is ${base}: your work so far is git diff ${base} in ${REPO}): ${p.scope}${RESUME[p.key] ? '\n' + RESUME[p.key].note : ''}
DONE WHEN: ${p.done}.
Report: ${REPO}/docs/m2-r5-${p.key}.md (what changed and why, file by file; each Cataclysm delta with its exe function and address; the build results; the plain-harness runs with label, milestone and first stop; the new self-test and harness mode; delta-map review entries you added; the CAT-TODOs you closed or re-targeted; what is left and its owner). Subsystems: split your scope into 3-6 subsystems and record them in the done marker.
Return via StructuredOutput: report, files_changed, summary, done_when_met, harness_milestone, first_stop, counts, open_questions.`
}

function auditPrompt(p, r, base) {
  return `${COMMON}
TASK: ADVERSARIAL AUDITOR for M2-R5 port "${p.key}" (producer result: ${JSON.stringify(r || {}).slice(0, 2500)}). RESUME RULE: if ${REPO}/docs/audits/m2-r5-${p.key}.md exists and ends with a "Verdict:" line, return its numbers immediately.
1. Build from clean in a separate dir: rm -rf ${SCRATCH}/build-audit-r5-${p.key} && meson setup / compile the debug and release configurations there exactly as tools/build.sh does (read it), 0 warnings under -Werror; record sizes.
2. ${p.audit}
3. Read the full diff of this port: git diff ${base} in ${REPO} (the commits after ${base}, WIP snapshots included, plus the working tree; commits whose subjects show they are the user's or the orchestrator's, such as harness rules or workflow relaunches, are not port work). Every change outside the producer's scope, every edited function the delta map calls IDENTICAL without a review entry, every Cataclysm change without a "CAT:" comment is a finding.
4. Re-check the producer's DONE WHEN claim (${p.done}) yourself on the plain harness (silent, through tools/m2_harness.sh only).
Fix factual errors you prove directly (code or doc; mark doc fixes "[audit: ...]"), keep the build green, and count the fixes. Write ${REPO}/docs/audits/m2-r5-${p.key}.md: what you checked, each finding with evidence, precision = correct/(correct+wrong) over the sampled checks, fixes, and a last line "Verdict: PASS|PASS-WITH-FIXES|FAIL - <reason>".
Then CHECKPOINT this port (even on FAIL, so the next port starts from a recorded state): ${CKPT} "M2-R5 ${p.key}: <verdict word> (<one-line summary>)" (it refuses forbidden paths; on an index.lock, wait 30 s and retry). Return the commit hash in "commit".`
}

const AUDITED_SCHEMA = { type: 'object', properties: { ports: { type: 'array', items: { type: 'object', properties: {
  key: { type: 'string' }, verdict: { type: 'string' }, commit: { type: 'string' } }, required: ['key', 'verdict', 'commit'] } } },
  required: ['ports'] }

async function portChain() {
  const out = []
  const probe = await agent(`In ${REPO}, for each key in [${PORTS.map(p => p.key).join(', ')}]: if docs/audits/m2-r5-<key>.md exists and its last non-empty line starts with "Verdict:", report {key, verdict: the verdict word (PASS, PASS-WITH-FIXES or FAIL), commit: the short hash of the newest commit whose subject starts with "M2-R5 <key>:" (git log --format='%h %s' | grep), or "" if none}. Omit keys whose audit doc is missing or has no Verdict line. Read only; change nothing.`,
    { label: 'probe:audited', phase: 'Port', schema: AUDITED_SCHEMA, model: 'fable', effort: 'high' })
  const audited = {}
  for (const a of (probe && probe.ports) || []) if (a.verdict && a.commit) audited[a.key] = a
  let base = ROUND_BASE
  for (const p of PORTS) {
    if (audited[p.key]) {
      const prior = PRIOR[p.key] || { produced: { report: `${REPO}/docs/m2-r5-${p.key}.md` }, audit: { verdict: audited[p.key].verdict, commit: audited[p.key].commit } }
      out.push({ key: p.key, produced: prior.produced, audit: prior.audit })
      base = audited[p.key].commit
      log(`${p.key}: already audited (${audited[p.key].verdict}, ${audited[p.key].commit}); skipped`)
      continue
    }
    const r = await agent(portPrompt(p, base), { label: p.label, phase: 'Port', schema: PROD_SCHEMA, model: 'fable', effort: 'high' })
    const v = await agent(auditPrompt(p, r, base), { label: 'audit:' + p.key, phase: 'Port', schema: AUDIT_SCHEMA, model: 'fable', effort: 'high' })
    out.push({ key: p.key, produced: r, audit: v })
    if (v && v.commit) base = v.commit
    log(`${p.key}: done_when=${r && r.done_when_met} milestone=${r && r.harness_milestone} verdict=${v && v.verdict}`)
  }
  return out
}

phase('Port')
log('M2-R5: 5 src/ ports in sequence (layout-dock, command-tactics, render, sound, hud)')
const all = (await portChain()).filter(Boolean)  // skips ports whose audit doc already has a Verdict line

phase('Integrate')
const digest = JSON.stringify(all.map(d => ({ key: d.key, done: d.produced && d.produced.done_when_met, milestone: d.produced && d.produced.harness_milestone, first_stop: d.produced && d.produced.first_stop, counts: d.produced && d.produced.counts, questions: d.produced && d.produced.open_questions, audit: d.audit && { precision: d.audit.precision, verdict: d.audit.verdict, commit: d.audit.commit } }))).slice(0, 20000)
const integrated = await agent(`${COMMON}
TASK: INTEGRATE M2-R5. Digest: ${digest}
RESUME RULE: if ${REPO}/docs/m2-r5-summary.md exists, read it and finish only what is missing.
1. R5 EXIT TEST (${PLAN} "R5 integrate"), from a clean build on the PLAIN harness (silent, tools/m2_harness.sh only): (a) the stage-2 exit test still holds: two 6,000-tick headless Mission01 runs (mission mode) byte-identical with ASan 0, also in release; the perturbed run and a flipped copy caught; sp universe running (--timeout 340, univUpdateCounter at least 1,248, ASan 0); fe Main_game_screen; the self-tests statics, meshes, textures, oracle, etg unchanged or better; the release oracle self-test SELFTEST PASS; quitting runs rc 0; tools/m2_layout32.sh OK with the Ship 0x63C pin; (b) the new self-tests dock, command, render, sound and hud complete with ASan 0 (sound: 0 UBSan reports in fquant.c / fqcodec.c); (c) grep -rn "M2-R4" src/ finds no CAT-TODO or CAT-PROVISIONAL that still names an M2-R4 owner (each is done or re-targeted to a stage with a reason). Run them yourself and report each with its run label and the UBSan kinds left. Also tools/build.sh --debug and --release from clean, 0 warnings.
2. Write ${REPO}/docs/m2-r5-summary.md in the shape of docs/m2-r4-summary.md: per port (what landed, done-when met or not, audit precision/verdict, commit), the delta map after the round (tools/m2_stage_counts.py), the exit-test table, what is left of stage 2 (only the two switches, if all went to plan) and of stage 3 part A, with owners, and the plan for M2-R6 (stage 3 part B, from ${PLAN}'s M2-R6 table, revised with this round's numbers and anything part A left), with counts, done-whens and the stage-3 exit test.
3. Update ${REPO}/cycle/STATE.md "## M2": an "M2-R5" subsection (COMPLETE with date and numbers, links), the M2-R6 plan in one table, and the M2 decisions (keep the open ones: the oracle reference recording, the two saves, the M0 sign-off, the first live oracle session; add anything this round surfaced, with the default the tree uses). Keep the rest of the file; the orchestrator updates the workflow table row.
4. git status: nothing from original/, reference/, build/ or game data staged or untracked outside .gitignore. Then: ${CKPT} "M2-R5: stage-2 close-out + stage 3 part A (layout + dock purposes, command layer + formations + tactics, render/GL, sound and speech, HUD)"
Return via StructuredOutput.`,
  { label: 'integrate:m2-r5', phase: 'Integrate', model: 'fable', effort: 'max',
    schema: { type: 'object', properties: {
      commit: { type: 'string' }, r5_exit_met: { type: 'boolean' }, unmet: { type: 'array', items: { type: 'string' } },
      milestone: { type: 'string' }, first_stop: { type: 'string' }, build_ok: { type: 'boolean' },
      m2r6_plan: { type: 'array', items: { type: 'string' } }, user_decisions: { type: 'array', items: { type: 'string' } }, summary: { type: 'string' } },
      required: ['commit', 'r5_exit_met', 'unmet', 'milestone', 'first_stop', 'build_ok', 'm2r6_plan', 'user_decisions', 'summary'] } })

return { items: all.map(d => ({ key: d.key, done: d.produced && d.produced.done_when_met, milestone: d.produced && d.produced.harness_milestone, verdict: d.audit && d.audit.verdict, commit: d.audit && d.audit.commit })), integrated }
