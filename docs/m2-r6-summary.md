<!-- done: exit-test, ports, deltamap, motion, oracle-a, left, r7-plan, decisions -->
# M2-R6 summary: stage 3 part B (main region + popup menu + game stats, front end, start-up + keys + console, managers + external build, NIS + FOW + camera + droids) + motion interpolation + oracle mode A

Status: **COMPLETE, 2026-10-18.** Branch `m2`, worktree `.claude/worktrees/m2`, workflow `cycle/workflows/m2-r6.js`, run
from the user's terminal session 38027cce (Fable), not from the orchestrator (cycle/STATE.md "M2-R6: IN FLIGHT"). 15
agents: seven producers in sequence (the five `src/` ports of docs/m2-r5-summary.md "Plan for M2-R6", then the two
additions: the user's first approved feature, render-time motion interpolation with an in-game toggle, and oracle mode A,
the user's reference recording `oracle01` replayed in hwi-engine), seven auditors (each checkpointed its port) and this
integrate step. The startup-keys port was cut off twice by the usage limit (WIP snapshots `57ae3b4`, `6645763`) and
resumed from its report. User decisions: cycle/STATE.md "## M2" (the faithfulness defaults and decisions 17-33 applied;
the reference recording `oracle01` of 2026-10-17 is the oracle-mode-a input; the two M0 saves still open, nothing blocks
on them; motion interpolation approved on top of the 1-to-1 recreation, docs/PLAN.md "Later", with the hard requirement of
a switch for A/B comparisons).

**R6 exit test (the stage-3 exit): met.** From a clean build on the plain harness (silent, `tools/m2_harness.sh` only):
(a) the earlier exit tests hold: two 6,000-tick headless Mission01 runs of the debug `hwi-engine` (ASan + UBSan) are
byte-identical with ASan 0 and so are two release runs, both pairs with the perturbed run and the flipped copy caught
(`MISSION TEST PASS` twice), on the new baseline **`49240abcf9f0ed97`** (this step's build), which differs from R5's
`645f94c69d68319e` in `rng6` and `rng_all` from tick 1 and in nothing else (the managers port's exe-faithful
computer-player timer); sp reaches the running universe with the Cataclysm HUD and renderers drawing (3,376 ticks at
340 s, ASan 0); fe `Main_game_screen`; statics, meshes, textures, oracle and etg give R5's numbers; the release oracle
self-test says `SELFTEST PASS`; every quitting run returns rc 0; `tools/m2_layout32.sh` OK; (b) the twelve self-tests
dock, command, render, sound, hud, ui, fescreens (0 `fescreen` UBSan reports), strings (1,705 / 1,705), keys, console,
build (with `CAT_EXTERNAL_BUILD_DRIVER 1`, two runs byte-identical) and nisparse complete with `result=PASS` and ASan 0,
and the `fescreen` UBSan kind is gone from every run; (c) no `CAT-TODO` / `CAT-PROVISIONAL` in `src/` names an M2-R5 or
M2-R6 owner, and the last six "stage 3" tags are settled by this step (three small ports, two re-targets, one closure);
(d) the motion-interpolation guard-rails hold: the two 6,000-tick pairs with `/noInterp` and `/noNISInterp` are
byte-identical to (a)'s pair, the `/noInterp` dumps of the motion and nis modes are byte-identical to the pre-feature
dumps (`b2af0d6d…`, `3f04c837…`), and with the flags on the crossing object and the tracked NIS object have 0 repeated
frames; (e) oracle mode A: the replay of `oracle01` from the clean build reproduces the audited result, first divergence
from the GOG recording at **tick 136, `ships_health`** (ship 47, `cat_clExternalBuildUpdate` 0x0044dfc0's x87
single rounding), IDENTICAL in every simulation field through tick 135. Stage 3 is closed except for the items under
"What is left of stage 3" (every one with a later owner); stage 4 (AI) is next.

## Exit test (this step, reproduced from a clean build at 9e20b5b plus this step's integrate edits)

**The integrate edits** (uncommitted when the builds ran; committed by this step's checkpoint; every one tagged
`[integrate: M2-R6]`): the six `CAT-TODO` tags that still named "stage 3" after the seven ports. Three are small ports
grounded in the r6 decompile and the disassembly: `cat_taskTimeCompressionPrint` 0x0052a3a0's `%s` is
`strGetString(strTimesTimeCompression)` (the exe pushes `ds:0x00bb4ddc` = the string table 0x00bb3620 + 4 * 1519;
Task.c); `univKillPlayer` 0x0054ce20 takes the exe's third argument, the player credited with the kill, whose
`PlayerStats +0x7D0` (`catStat7D0`, the stats screen's first row) goes up by one, closes the current player's research,
construction and launch managers first (`rmCloseIfOpen` / `cmCloseIfOpen` / `lmCloseIfOpen`, now in the tree since the
managers port), queues the exe's speech id 0x200102d (HW1's `STAT_F_RelicPlayerDies` was muted, decision 26) and writes
no game-stats file; its three callers pass what the exe pushes at each site (`univKillOtherPlayersIfDead` the ship's
`catKilledByPlayer` +0x60C when it names another player 0..7, `clProcessSyncPacket` 0x00450e47 and
`singlePlayerMissionFailedCB` 0x0051be8d -1); `univKillOtherPlayersIfDead` 0x00548c70 is the exe's body (the index > 7
guard, the manager closes for a player who plays on without a mothership, the `MG_Hyperspace` look into
`singlePlayerGameInfo.ShipsInHyperspace` for a live ship, the kill credit). Two are re-targeted with their reason:
`DeclarePlayerWin` 0x005487f0 (the multiplayer win message from the player names and
`cat_DeclarePlayerWinReportResults` = `cat_titanContestRecordResult(0x12, ...)`, the WON contest API; the function
returns at once in single player) to `CAT-TODO(stage 5 LAN)`, and `TurnHarvestEffectOn` 0x004f1ac0's real32 effect
parameters (one change in the ETG's variable-parameter reading for every `etgEffectCreate` caller, a stage-2 file) to
`CAT-TODO(M2-R7 ETG)`. One is closed: `regRegionDelete` 0x004f0e10's extra dirtying runs only while
`feShouldSaveMouseCursor()` is set, the Win32 front-buffer cursor save GoK removed (decision 28). None of the three
ports is reached by the Mission01 oracle run or the `oracle01` replay (no player dies, the time-compression key is
never pressed): the streams below are byte-identical to the runs made from the unedited 9e20b5b build earlier in this
step (the same nine sha256 values). The delta map got the five functions' review notes (`[integrate: M2-R6]`,
`univKillPlayer` and `univKillOtherPlayersIfDead` now confidence high) and, found on the way, the review file
`ghidra/delta_map_review.json` was brought back in step with the map: 16 entries the oracle-mode-a producer had written
into `delta_map.json` directly (its 32 `[review: M2-R6 oracle-mode-a]` notes) were reverted by this step's first
`delta_map.py merge` and restored from the map, and the 107 reviewed map entries that had no review-file row (the
startup-keys and oracle-mode-a producers wrote into the map) were added to the review file, so the next merge cannot
revert them; `delta_map.py merge --done m2r6:integrate` then changes nothing in the map but this step's five entries.

**Builds.** `tools/build.sh --debug --clean` and `--release --clean` on the final tree: both rc 0, `[315/315] Linking
target hwi-engine` (R5: 308; the seven new files), 0 `warning:` and 0 `error:` lines under `-Werror` (the only WARNING
line is meson's sanitizer notice), 8.1 s and 7.5 s of wall time.

| build | size | sha256 |
|---|---:|---|
| debug (ASan + UBSan) | 87,136,752 B | `af0d8cd89236075159…` |
| release (`-O2`, no sanitizers) | 5,362,248 B | `8bafc9dd76e0856567…` |

`tools/m2_layout32.sh`: OK. ILP32 sizes as R5 (`Ship` 1596 = 0x63C, `Player` 25904, `Universe` 252348, ...) plus this
round's pins: `CatPopupItem` 60 (0x3C), `CatTargetActions` 164 (0x29 dwords), `CatKbCommandBindings` 52 (0x34), the
`.fib` screen 20 / link 12 / atom 0x4C, `nisevent` 0x28 / `nisheader` 0x84 / `nisplaying` 0x54, `CatFOWGrid` 0x20,
`Camera` 0x5C / `CameraStackEntry` 0x78 / `CameraCommand` 0x94 (`ccMode` +0x8C), the madanim offsets; the specifics
asserts of all 45 ship files hold; `python3 -I tools/m2_save32_gen.py --check`: current.

**Harness.** `tools/m2_harness.sh` on `original/scratch-m2/m2r2-harness/data` (the GOG install's own files, no CET, no
workarounds), offscreen video, dummy audio (docs/CONSTRAINTS.md 5), no input; the recording `oracle01` and its `.pkts`
in the data dir's `SavedGames/RecordedGames` (copied there by the replay mode, never written in `original/game`). The
two binaries were copied to `original/scratch-m2/m2r6-integrate/` first so that no rebuild could touch a running test.
Logs are in `original/scratch-m2/m2r2-harness/runs/r6-integrate-*/`. The runs were made from three parallel chains on an
otherwise idle machine (load average 0.4): the mission tests and the replay; sp then fe; the self-tests and the motion
dumps.

| run | mode | rc / wall | milestone | first stop | sanitizers and checks |
|---|---|---|---|---|---|
| `r6-integrate-mission-a`, `-b` (`tools/oracle/mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000`) | mission, 6,000 ticks, debug | **0** / 19.1 s and 18.6 s | **oracle mission run done** (`ticks=6000 records=6003 ships=154`) | none (rc 0) | **ASan 0** in both, no unsuppressed leak; 3,783 file loads each; `cmp` equal, **1,476,370 B, sha256 `49240abcf9f0ed97`**; `hwo_compare.py`: `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000)`. Against the oracle-mode-a audit's `aud-r6a-mission-a`, the producer's `r6a-fin-mission-a` and the motion-interp `mi-fin3-on-mission-a`: IDENTICAL over all 6,001 `T` records (only the header's build identity differs). **Against R5's `r5-integrate-mission-a` (`645f94c69d68319e`): `FIRST DIVERGENT TICK 1`, 2 fields, `rng6` (CAND 10 draws ahead) and `rng_all`**; with every other field selected (`--fields rng10,rng0,ships_n,ships_misc,ships_pos,ships_vel,ships_rot,ships_health,missiles,bullets,derelicts,resources,statcheck,univcheck,players,dt,total,real,fpcw`): IDENTICAL over 6,001 ticks: the managers port's move ("Decisions" 44) |
| `r6-integrate-mission-perturb` | mission, 6,000, `--perturb 3000` | 0 / 19 s | oracle mission run done | none | ASan 0; `FIRST DIVERGENT TICK 3000`, 2 fields, likely origin `rng10`, `CAND is 1 draw(s) ahead`; events `game-new, perturb, quit` |
| copy of A with bit 0 of `ships_health` flipped at tick 4000 | | | | | `FIRST DIVERGENT TICK 4000`, 1 field, origin `ships_health`. **MISSION TEST PASS** |
| `r6-integrate-mission-rel-a`, `-b`, `-rel-perturb` (`mission_test.sh` with the release binary) | mission, 6,000 ticks, **release** | 0 / 4.8 s each | oracle mission run done | none | `cmp` equal, sha256 `cdc60256f37f6961`; vs run A: `IDENTICAL: 6001 ticks` (header build flags 1fbfa32f vs 1afeb7ac): the -O2 build gives the same stream as the ASan build; the release perturbed run caught at 3000 / `rng10`, the flipped copy at 4000 / `ships_health`: **MISSION TEST PASS** |
| `r6-integrate-mission-noi-a`, `-b` (`-- /noInterp`) and `-nonis-a`, `-b` (`-- /noNISInterp`) | mission, 6,000, debug | 0 / 18.5-18.7 s | oracle mission run done | none | ASan 0; all four `cmp` equal to run A (sha256 `49240abcf9f0ed97`): the motion-interpolation guard-rail (d) |
| `r6-integrate-replay` (`--mode replay`, the default recording `oracle01`) | **oracle mode A**, debug | **0** / 34.5 s | **oracle replay done** (`ticks=9601 packets=4800 ships=81 randSyncErr=4037 univSyncErr=4703 blobSyncErr=4746 pktSyncErr=0 records=14405`; `savecat32: loaded ... 1842 chunks converted, 47 ships, 0 size problems`; `replay started oracle01 players=2 ships=47 tick=1 multiplayer=0`) | none (the replay quits by itself at the end of the packets) | ASan 0, no leak; 5,015 file loads; 2,630,923 B, sha256 `c6adbb94c1f57aa2`; against the producer's `r6a-final2-replay-a` (`c55fd4b2d2e9afd0`): `IDENTICAL: 9601 ticks and 4800 packets compared (ticks 1-9601), no difference` (build identity only). The comparison with the GOG stream is in "Oracle mode A" |
| `r6-integrate-sp` | sp, `--timeout 340` | 0 / 340 s (timeout) | **universe running**: last `debugScreenshots univUpdateCounter` line **3,376** (R5: 1,248 on a loaded machine; the animatics now play at their 30 fps and the machine was idle) | none: still running at the timeout. 3,784 file loads (R5's 3,767 plus the cursors, the popup fonts, `FEMan\console.fib`, the options file) | **ASan 0**; no unsuppressed leak; **0 `fescreen` UBSan reports**; UBSan kinds: the ETG opcode kinds (`etgdecision` 21, `etgfunctioncall` 10, `etgconditional` 8, `etgbranch` 8), `BTG.c:388` (2) and `BTG.c:864` (1), `stb_image_write.h:1263` (1) |
| `r6-integrate-fe` | fe, 120 s | 0 / 121 s (timeout) | **`Main_game_screen`** | none: still running at the timeout | ASan 0; no unsuppressed leak; 300 file loads (R5's 284 plus the cursors, the popup fonts and `Cataclysm.cfg`); **0 UBSan reports of any kind** (R5: 92 `fescreen`) |
| `r6-integrate-statics` | statics | **0** / 11 s | ship statics self-test done | none | ASan 0, no leak. **143 of 145** ship types loaded, 2 not found (the disabled cloak-generator `.shp`), 27 other `.shp`; 3,011 file loads |
| `r6-integrate-statics-init` | statics + `/catShipStaticsStaticInit` | **0** / 11 s | ship statics self-test done | none | ASan 0, no leak. 143 of 145; **108 StaticInits ran** (R4 / R5: 108) |
| `r6-integrate-meshes` | meshes | **0** / 11 s | mesh self-test done | none | ASan 0, no leak. 1,758 listed, 1,729 `.geo/.peo` + 29 `.mad` loaded, 0 with bad objects |
| `r6-integrate-textures` | textures | **0** / 14 s | texture self-test done | none | ASan 0, no leak. 12,403 / 12,403 listing lookups, 10,661 registered in 3 batches and loaded, 0 not loaded, 1,742 rows with no file; 9,248 scaled down (R5: 6,289: the start-up now applies the exe's texture-detail default 50 through `cat_trTextureMemorySet`, M2-R6 frontend) |
| `r6-integrate-oracle` | oracle (debug), 512 ticks | **0** / 10 s | oracle self-test done | none | ASan 0, no leak. `randSyncErr=0 univSyncErr=0 blobSyncErr=0`, 772 records, 140,723 B |
| `r6-integrate-etg` | etg, 128 ticks | **0** / 10 s | etg effect self-test done | none | ASan 0, no leak. 9 of 9 scripts parsed, 197 effects loaded, 0 referenced but not loaded; 618 created, 3,197 deleted while running, 1,093 left, peak 2,515 effects / 3,012 particle blocks (R3, R4 and R5's numbers) |
| `tools/oracle/selftest.sh --label r6-integrate-oracle-rel` | oracle (release), 512 ticks | 0 each | oracle self-test done | none | runs a and b `cmp` equal (140,723 B, sha256 `b82281edf8b017b1`) and `IDENTICAL: 513 ticks and 256 packets`; the perturbed run reported at tick 300 / `rng10` / 1 draw ahead; the flipped copy at tick 400 / `ships_pos`; `rngs=27` in every `T` record. **`SELFTEST PASS`** |
| `r6-integrate-dock` | dock, 5,000 ticks | **0** / 19 s | **dock self-test done** | none | ASan 0, no leak. `orders tick=16 fighters=6 corvettes=2 workers=3 RUs=750`; purpose 0: 8 of 8 docking; purpose 3: 1 worker depositing 250 RUs; the carrier leg at sCarrier#173 (2 of 2) and the processor leg at sProcessor#161; `A inside=6 of 6 fighters; RUs 750 -> 1250`; `done fighters=6 outside=6 corvettes=2 carrierleg=2 processorleg=1 RUs=1850 jackPausedTick=1024 result=PASS`, 0 FAIL lines (the managers and nis-fow-camera numbers) |
| `r6-integrate-command` | command, 3,000 ticks | **0** / 16 s | **command self-test done** | none | ASan 0, no leak. `link ordered: 2 acolytes, 2 mimics; point ordered at tick 18; link done at tick 252: sACV#178; unlink done at tick 285; done tick=2998 link=252 mcv=1 unlink=285 research=1/1 mimic=1 point=1/1 result=PASS`, 0 FAIL lines; `fow grid=set stealthUpdates=387` |
| `r6-integrate-render` | render, 600 frames | **0** / 15 s | **render self-test done** | none | ASan 0, no leak. `frames=583 listed=1823 sorted=1241 ships=582 effects=1241 trails=582; lensflares=583 lensflareElements=9328 foglayers=1166; droids=2500 droidUpdates=583; result=PASS` (the nis-fow-camera audit's numbers) |
| `r6-integrate-sound` | sound | **0** / 17 s | **sound self-test done** | none | ASan 0, no leak. `audio driver dummy`; `Music4Cat.wxd: 62 streams, track 0 decoded 43 blocks; SectSpeech.vce: event 0 decoded 124 blocks; BeastSpeech.vce: event 26 decoded 281 blocks; chatter: 88 events, 88 named, 84 with an id (4 have 0, as in the exe), 64 with a Somtaaw sentence, 57 with a Beast sentence; 403 audible output blocks` (into the dummy device); `result=PASS`; **UBSan reports in `fquant.c` / `fqcodec.c`: 0; `struct PATCH`: 0** |
| `r6-integrate-hud` | hud, 600 frames | **0** / 15 s | **hud self-test done** | none | ASan 0, no leak. `selected tick=64 band-boxed=19 player ships=154 selected=154 selecting=154 piemode=2; sensors open tick=480 active=1 hotkeygroup=154; sensors scenes=119 rangecircles=14364 shippoints=8791 meshes=236 icons=236 hotkeys=5969 resources=0; counts frames=536 statusbars=7923 bars=7923 icons=813 infopanels=536 infolines=9112; selectedlines=8040 selectinglines=8040 messagelines=202 supportunits=536 pie=536 compass=536; result=PASS` (the last 120 frames are the sensors manager's since M2-R6 managers) |
| `r6-integrate-ui` | ui | **0** / 13 s | **ui self-test done** | none | ASan 0, no leak. `selected tick=16 ships=154 player=1 mrDisabled=0; popup open tick=24 open=1 rows=10; popup drawn frames=16; commands tick=40 run=26 target=resource; popup closed tick=40 closed=1 target rows=1; game stats begin tick=48 active=1 paused=1 players=2; done selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS; game stats ended` |
| `r6-integrate-fescreens` | fescreens | **0** / 10 s | **fescreens self-test done** | none | ASan 0, no leak. `fescreentest: done files=22 loaded=22 screens=195 pushed=195 popped=195 wrong=0 skipped=0 listchanged=0 userregionstandins=84 result=PASS` (84: the two Smooth-motion atoms of the motion-interp port join their screens); `optionstest: done options=34 startdefaults=34 readback=34 result=PASS`; **0 `fescreen` UBSan reports** |
| `r6-integrate-strings` | strings | **0** / 9 s | **strings self-test done** | none | ASan 0, no leak. `done ids=1705 mapback=1705 resolved=1681 nostring=24 wrongtext=0 wrongnostring=0 result=PASS` (the 24 ids the install's English.script has no row for, as the exe); ship names 145 of 145 |
| `r6-integrate-keys` | keys | **0** / 10 s | **keys self-test done** | none | ASan 0, no leak. `loaded rows=197 known=149 unknown=48 commands=147 bindings=149 mismatches=0; saved rows=149 default=149 version=84 same=1; reload same=1 reset same=1; new key kept=1 reset=1 (F1); conflict 'M' is already bound to 'Move'; names named=149 found=149 groups=150; done rows=197 known=149 unknown=48 commands=147 bindings=149 startsave=1 saved=1 reload=1 reset=1 newkey=1 conflicts=1 shares=1 failures=0 result=PASS` |
| `r6-integrate-console` | console | **0** / 13 s | **console self-test done** | none | ASan 0, no leak. `open tick=24 open=1 screen=1 single=1 paused=1; scrolled in frame=4 pos=300; config file 240 bytes, rammingtime after loadConfig=12345 (the exe's reader does not read its writer's byte order); table rows=31 shown=31 set=24 commands=8 failures=0; keys typed=32 history=1; done selected=154 frames=4 shown=31 set=24 commands=8 keys=32 closed=1 failures=0 result=PASS` (31 / 24: the exe's 29 / 22 plus the two `objInterpolate` / `nisInterpolate` rows) |
| `r6-integrate-build`, `-build-b` | build, 7,000 ticks (`CAT_EXTERNAL_BUILD_DRIVER 1`) | **0** / 20 s | **build self-test done** | none | ASan 0, no leak. The docking bay (external) appears at tick 17 at health 0 clamped and finishes at 1457, the sDestroyer (external) appears at 1457 and finishes at 4177, the sAcolyte (inside) appears at 208; the three managers open and draw; `done tick=4205 driver=1 SU=40/100 RUs=25925 result=PASS`; `run.hwo` 1,722,370 B, sha256 `85969556400f4a83` both runs, `cmp` equal (`r6-integrate-build-b` run after the batch, 20 s, PASS, 3,857 file loads each) |
| `r6-integrate-nisparse` | nisparse | **0** / 9 s | **nisparse self-test done** | none | ASan 0, no leak. `done nis=14 nisScripts=14 animatics=18 objects=593 events=1488 cataclysmEvents=31 unknownKeywordLines=47 failed=0 result=PASS`; ignored keywords (not in the exe's table): `motionCueScale` x12, `minimumLODSet` x32, `stretch`, `closeslipgate`, `widescreenOut` |
| `r6-integrate-motion-noi`, `-nis-noi` | motion 224 / nis n06:26:36, `-- /noInterp` | **0** / 32 s and 35 s | motion self-test done | none | ASan 0, no leak. Dumps: `motion.dump` 1,431 frames, **sha256 `b2af0d6decdf6474`**; `nis.dump` 1,440 frames, **sha256 `3f04c837e605ba23`**: byte-identical to the pre-feature dumps the motion-interp report and its audit recorded (`b2af0d6decdf647431d6c2adf701798cd8e98d4a614d7b69390e6df02452d1dd`, `3f04c837e605ba23d6c74c639bba4ff10a95c139f33d462bca79427d4a985029`) |
| `r6-integrate-motion-on`, `-nis-on` | motion 224 / nis n06:26:36, flags on | **0** / 32 s and 35 s | motion self-test done | none | ASan 0, no leak. Dumps `d9a8661fcce6f4b3` (1,431 frames) and `074a70f17e316cd2` (1,440 frames): the producer's and the audit's hashes; the `tools/nis_smoothness.py` numbers are in "Motion interpolation" |
| `r6-integrate-toggle` | toggle | **0** / 9 s | toggle self-test done | none | ASan 0, no leak. `start objInterpolate=1 nisInterpolate=1; cancel click to 0 ... cancel restored 1 / 1; accept-off file rows=2 read back 0 / 0; accept-on ... read back 1 / 1; ... result=PASS` |

So every row of the R6 exit test holds:

| condition (docs/m2-r5-summary.md "R6 integrate (stage-3 exit)" and this round's additions) | result |
|---|---|
| (a) two 6,000-tick Mission01 streams byte-identical with ASan 0, the perturbed run and the flipped copy caught, also in release; the new baseline's hash and the moved fields stated against R5's `645f94c69d68319e` | **met**: debug `49240abcf9f0ed97` (A = B), release `cdc60256f37f6961` (A = B, IDENTICAL to debug), both `MISSION TEST PASS` (perturb 3000 / `rng10`, flip 4000 / `ships_health`); against R5: `rng6` and `rng_all` from tick 1, every other field IDENTICAL |
| (a) sp universe running (`--timeout 340`, `univUpdateCounter` at least 1,248, ASan 0) with the Cataclysm HUD and renderers drawing | **met**: 3,376 ticks at the 340 s timeout, ASan 0, no leak; the HUD and the Cataclysm object-list renderer are what the sp frames draw (the hud and render self-tests exercise the same paths with their counts) |
| (a) fe `Main_game_screen` | **met**, ASan 0, 0 UBSan reports of any kind |
| (a) statics, meshes, textures, oracle, etg unchanged or better | **met**: 143/145 (108 StaticInits), 1,758/1,758, 10,661 / 0 not loaded, 0 sync errors, 618 effects: R5's numbers |
| (a) release oracle self-test `SELFTEST PASS` | **met**, 27 RNG streams |
| (a) every quitting run returns rc 0 | **met**: 38 of 38 quitting runs (the 7 debug and 3 release mission runs, the replay, statics, statics-init, meshes, textures, oracle, etg, the 3 release self-test runs, dock, command, render, sound, hud, ui, fescreens, strings, keys, console, build x2, nisparse, the 4 motion / nis runs, toggle); sp and fe stop at their timeouts by design |
| (a) `tools/m2_layout32.sh` OK; clean builds 0 warnings | **met** (315/315 debug and release, 0 warnings) |
| (b) dock, command, render, sound, hud, ui, fescreens (0 `fescreen` UBSan), strings (1,705 / 1,705), keys, console, build (driver 1), nisparse `result=PASS` with ASan 0; the `fescreen` kind gone from every run | **met**: all twelve `result=PASS`, rc 0, ASan 0; `fescreen UBSan reports: 0` in every summary of this step (the harness counts them in every mode); the UBSan kinds left are listed below |
| (c) no `CAT-TODO` / `CAT-PROVISIONAL` in `src/` names an M2-R5 or M2-R6 owner; every "stage 3" tag re-targeted with a reason | **met**: `grep -rn -E 'CAT-(TODO\|PROVISIONAL)\([^)]*M2-R[56]' src/` is empty; `grep -rn -E 'CAT-(TODO\|PROVISIONAL)\([^)]*stage 3' src/` is empty after this step's six edits (3 ported, 2 re-targeted: `stage 5 LAN`, `M2-R7 ETG`; 1 closed: decision 28); the tags left: 96 `CAT-TODO(<owner>)` tags on 105 lines (stage 4 AI 22, stage 5 25, stage 6 44, M2-R7 4, `Memory.c` 1) and 8 `CAT-PROVISIONAL` (the stage-6 switch `CAT_HW1_MISSION_SCRIPTS`, the kas2c / NIS-first notes, the M2-R2 appends, four medium-confidence readings) |
| (d) two 6,000-tick mission runs each with `/noInterp` and `/noNISInterp` byte-identical to (a)'s pair; the motion and nis dumps under `/noInterp` byte-identical to the pre-feature dumps; the smoothness numbers with the flags on restated | **met**: `49240abcf9f0ed97` in all six debug streams; dumps `b2af0d6decdf6474` / `3f04c837e605ba23` = the pre-feature hashes; B (crossing) 0 / 724 and O9 (tracked NIS) 0 / 1389 repeated frames with the flags on ("Motion interpolation") |
| (e) the replay of `oracle01` from the clean build compared with `oracle01-rec.hwo`: the first divergent tick and field with the owning function, matching docs/m2-r6-oracle-mode-a.md and its audit | **met**: tick 136 `ships_health`, last equal tick 135, `cat_clExternalBuildUpdate` 0x0044dfc0; every per-field first tick equals the report's (below) |

**UBSan kinds left** (all non-fatal; none is `fescreen`, `PATCH` or `fquant.c` any more): the ETG opcode kinds of GoK's packed effect bytecode in every Mission01 run (`etgdecision` 21-25, `etgfunctioncall` 10-16, `etgconditional` 8-32, `etgbranch` 8-10, `etgvariablecopy` 4) and `struct particle` (131-166 in the etg, render and replay runs); `BTG.c:388` (2 misaligned `short` loads) and `BTG.c:864` (1 misaligned `udword` load) in every Mission01 run; `stb_image_write.h:1263` (1, sp only); `Select.c:1166` (2, the replay only); `Subtitle.c:1144` `index 112 out of bounds for subtheme [16]` (1, the nis mode only, present in the motion-interp producer's and audit's nis runs: n06's subtitle theme index); `AIShip.c:1467` `member access within null pointer of type struct Ship` (1, the dock mode only, present in every dock run since the managers port: a stage-4 file, M2-R7 producer 1). The ETG /
particle opcode kinds and `BTG.c:388` are the stage-1/2 LP64 artefacts of GoK's packed structs listed since R5 (not
owned by a stage-3 producer); `BTG.c:864` (one misaligned `udword` load of the star-filename length in `btgLoad`) and
`stb_image_write.h:1263` (a signed left shift in GoK's JPEG writer of the `/debugScreenshots` shot) appeared with the
frontend port (its audit W1; the same loader kind as `BTG.c:388`, and GoK's screenshot writer); `Select.c:1166`
(`selCircleCompute`, `384 - INT_MIN` for a far-off ship's screen y) is reported twice per replay (oracle-mode-a's
finding, render-only). Gone since R5: `fescreen` (M2-R6 frontend: the `.fib` images converted at their 64-bit offsets).

Harness facts for the next rounds, in addition to R3-R5's:
- **The sp counter is 2.5-3x R5's**: the 18 animatics play at their 30 fps (M2-R6 nis-fow-camera: GoK's avi player
  assumed HW1's 15), so the intro ends sooner and the universe gets more of the 340 s; 3,376 on an idle machine
  this step, 3,712 in the producers' 360 s runs. The milestone and ASan are the gate, not the count.
- **The options file is `Cataclysm.cfg`** (M2-R6 startup-keys): read from `$HOME/.homeworld/Cataclysm.cfg`, written to
  the run's `/settingspath`; the harness's home template holds it (`TutorialNeeded 0`, `PlayIntros 0`) next to the
  older `Homeworld.cfg`. The motion-interpolation rows `objInterpolate` / `nisInterpolate` live there too.
- **The mission stream baseline moved once this round** (managers, `rng6` / `rng_all` from tick 1): compare with the
  nearest audited baseline through `hwo_compare.py`, and expect the stage-4 AI port to move it again (player 1 of
  Mission01 has a computer player; `rng6` is its stream).
- **The replay mode** (`--mode replay [--recording NAME]`) is the oracle's mode A: about 35 s under ASan for the 9,601
  ticks of `oracle01`; `/oracleReplay` turns sound and speech off (the mixer thread's ambience ends drew stream 11 on
  the wall clock). Judge it against the GOG stream with the field list of "Oracle mode A" (not the full record).
- **The motion and nis modes** need `--fps` (144 by default) and the nis mode `/noSpeech` (set by the harness); the
  dumps are deterministic and their sha256 is the guard-rail; `tools/nis_smoothness.py DUMP [DUMP2]` reads them.
- **The review file is the merge's source**: a producer that writes review notes into `ghidra/delta_map.json` directly
  loses them at the next `delta_map.py merge` of anyone else (this step restored 16 and added 107 rows); write reviews
  into `ghidra/delta_map_review.json` and merge.

## The seven ports

| # | item | what landed | DONE WHEN | audit | commit |
|---|---|---|---|---|---|
| 1 | main region, mouse, the popup menu, the game-stats screen ([report](m2-r6-mainrgn-popup.md), [audit](audits/m2-r6-mainrgn-popup.md)) | **New** `Game/PopupMenu.c/.h`: the 32 exe functions of the right-click popup menu block 0x004eaa60..0x004ef860 with the exe's item tables (49 rows of 0x3C bytes, 13 submenus), rows < 0x18 through the key command table, the rest through the `cat_ui*` commands to the wraps. **mainrgn.c**: the 11 unread bodies (`mrStartup`'s 256 key children, `mrObjectClick`, `mrDockingOrders`' slipgate order, `mrSelectHold`, `mrRetire`, `mrScuttle`, `mrFormationShips`, `toMoveLineDraw`, ...), the band box (`cat_mrBandBoxRelease`, `cat_mrReleaseRight` / `Left`, `cat_mrSelectRectDraw`), tactics / formation cycling, `cat_mrMimicTarget`, the 26 `cat_ui*` commands (special target with its eight wrappers, swarm, siege cannon, cloak, waypoint mode, halt, link / unlink, unmimic, force-field sphere, Dreadnought repulse, Leech), the waypoint UI; 7 IDENTICAL found modified (`mrBuildShips`, `mrLaunch`, `mrResearch`, `mrNextMothershipPtr`'s eight mothership types, the pulsed / radial / move-to draws). **mouse.c**: `cat_mouseGetContextAction` 0x0056f8f0 with both jump tables, the 31-cursor system, `mouseSetCursorSetting` at the exe body. **Game stats** (`GamePick.c`): `cat_gameStats*` x8 at the exe's layout data. Also the command table's callbacks and `cat_kbCommandExecute`, `cat_selComputeTargetActions`, the mimic menu tables, `cat_bigGunFireCommand` / `IsCharged`, `cat_ccShipIsLegalToFocusOn`, `cat_spPlayerCanHyperspace`, the three prim draws, `psLinkDraw`. `/catUiTest` + mode `ui` | met: ui self-test PASS ASan 0 (popup open for Mission01's fleet, 26 commands through the wraps, the stats screen drawn 8 frames); the context-action table checked entry by entry against both jump tables (18 of 18); sp, fe and the mission stream unchanged (IDENTICAL to R5; +15 file loads: 13 cursors, 2 fonts) | PASS-WITH-FIXES, **0.92** (47/51). Fixes: the inverted `cat_popupMenuFilterTargets` (the repair row kept enemies), the inverted and case-sensitive `TerryStats` music test of `cat_gameStatsBegin`, a split CAT-TODO owner, the ui harness header | `60bfd21` |
| 2 | front-end flow, screens, controls, fonts, strings, options, subtitles ([report](m2-r6-frontend.md), [audit](audits/m2-r6-frontend.md)) | **The `fescreen` UBSan kind fixed the faithful way**: `FIB_ScreensOffset` (the 12-byte header on ILP32, the next 8-byte boundary in GoK's converted 64-bit image), the converter sized from the image, static asserts on the `.fib` layouts. `feCurrentScreenDelete`, `nextLog2` (CAT-FIX above 256), `uicToggleProcess` / `uicRadioButtonProcess` with Cataclysm's UI events, `strLoadLanguage` (the 24 `*NO_STRING*` ids reported as the exe), `strSetStringCB` (`http:` kept), `fontPrintN`'s fourth argument `bAdditive` through `glfontDisplayString`, `cat_fontPrintSubtitle`; **Options.c**: the 26 Cataclysm callbacks (`cat_op*`: LOD detail, small fonts, taskbar, experience messages, CSA, waypoints, the ten sensors toggles, the two presets, texture detail with `cat_trTextureMemorySet`, speech / SFX tests, `cat_opSetAllDefaults`, `cat_opCataclysmFramerate`) and 24 HW1 bodies at the exe's (init, update, save / restore through one table, the effects presets with CPV / lens flares / repair droids, the frame-rate slider, the advanced audio page, no voice preview), the options-file section; **subtitles**: `subTitlesDraw`'s rest (the additive strip, `cat_tutButtonRowSet`, `cat_ioSubtitleNotify`), `subStringsChop`, the Bink term as `aviIsPlaying`; **ScenPick.c** at the exe's (every map its own scenario, the description list), `universeReset`'s leech words, Demo / Titan / ColPick compares; 230 IDENTICAL compared, 15 found modified, 1 the other way. `/catFeScreensTest` + mode `fescreens` (with the options round trip), `/catStringsTest` + mode `strings` | met: fescreens PASS 22 files / 195 screens pushed and popped, **0 `fescreen` UBSan**; strings PASS 1,705 / 1,705 (1,681 with the install's text, 24 `*NO_STRING*` as the exe); options 34 / 34 round-tripped at the exe's defaults; fe `Main_game_screen` with 0 UBSan reports; sp unchanged (1,248 on the shared machine); mission IDENTICAL to R5 | PASS-WITH-FIXES, **0.912** (62/68). Fixes: a three-line CAT-TODO, a missing (`nisTextCardListDraw`) and a stale (`fontPrint`) review, two comments, the report's sp UBSan claim (two new non-fatal kinds `BTG.c:864`, `stb_image_write.h:1263` recorded) | `142a8e2` |
| 3 | start-up, main, utility, key bindings, console ([report](m2-r6-startup-keys.md), [audit](audits/m2-r6-startup-keys.md)) | **Options file** `Cataclysm.cfg`: `utyOptionsList` is the exe's 94-row table 0x008c1478 row for row (no sections, `"%s    %d"`), the registry rows and GoK's own after it; HW1's key rows gone. **Keys**: HW1's 19-command key table deleted; the exe's binding table `catKbBindings` (0x34 per command), `cat_kbLoadKeyBindings` (`default_keys.cfg` from the install, `keys.cfg` made in the settings path), `cat_kbReload` / `Save`, the parse / to-string / conflict / share / set-new-key helpers, `kbCommandKeyIsHit`, the options screen's 150-command list, every row of the command table 0x008b6310 with the exe's callbacks (hot-key groups, the Pause flag `catUnivFrameStep`, time keys, screenshot, scrolling, gamma), `mrRegionProcess`'s key events as the exe (HW1's 1,100-line `mrKeyPress` / `Release` gone), `cat_keyNameStringsInit`, the Win32 virtual-key / SDL scancode conversion. **New** `Game/Console.c/.h`: the developer console (17 functions: open / close / key press / tab completion / run / show, the 29-row command table with the exe's variables, the config file with the exe's byte-order quirk, `experienceships`, `upgradeships`, `alltech`, add resources, warp). **Start-up**: `main` at the exe's order (`ProcessCommandLine` stops on a bad switch, the 64-row table compared, `/prepath`, `/CDpath`, `/SaveMPGames`, `/LoadMPGame`, `/aiplayerLog`; `/NoBind` gone), the window title "Emergence", `networkVersion` "Emergence 1", every key down to the console; `gameStart` read in full and ported (races, managers closed, AI level, resources x5 on flag bit 5, `universeSwitchToPlayer`, the tech-set pass, the upgrade pass; the start-position shuffle re-targeted), `gameEnd`, `utyNewGameStart`, `utyGameQuit`, `utyGameSystemsPreInit` (no `bigCRC`), `versionNumDraw` "Emergence 1.01", `cat_utyGameQuitToMainOrStats`, the video toggles, the debug-key overlay `cat_moDebugKeysDraw`, the screenshot message; 60 IDENTICAL compared (3 found modified). `/catKeysTest` + mode `keys`, `/catConsoleTest` + mode `console` | met: keys PASS (the written `keys.cfg` equals the GOG game's own line for line); console PASS; start-up as the exe's `main` with the R2 workaround list empty; fe and sp unchanged but for the options-file name and `console.fib`; mission IDENTICAL to R5 (debug and release) | PASS-WITH-FIXES, **0.90** (36/40). Fixes: two table row counts (94 options rows, 64 command-line rows), two "identical" credits-track reviews (the exe plays track 0x29), two missing render.c review entries | `c3e7c1f` |
| 4 | construction, research, trade, launch and sensors managers; the external-build driver ([report](m2-r6-managers.md), [audit](audits/m2-r6-managers.md)) | **`CAT_EXTERNAL_BUILD_DRIVER` is 1** (decision 17 retired): `cmBuildTaskFunction` 0x00454940 at the exe's body (the external ship created at a free slot after the docking bay, progress through `cat_clWrapExternalBuildUpdate` mode 2 as the exe's double, `cat_cmBuildJobFinished`), `cmDeterministicBuildStart` / `Cancel` / `Process` (the recorded / multiplayer list, 0x48-byte nodes), `cat_shipUpdateExternalBuildAndAI` 0x00543f10 (the computer player's timer, auto-upgrade, special abilities and attack chance from stream 6), the support units `cat_su*`, `cat_cmExternalBuildShipDied` / `Attach`, `cat_cmCancelJobsOfDeadFactory`, `univCreateShip`'s creator argument and factory block, `shipinprogress` at 0x91 entries; **ConsMgr.c** at the exe's (the nine construction lists from the exe's data, kind 2 / stat 4 rows, three carrier slots, the SU bar, the factory name, every callback incl. the pointer-only ones from the disassembly; HW1's class caps deleted); **ResearchGUI.c rewritten** on the exe's (the Sect 37-row and Beast 189-row print lists from the exe's data, `rmUpdateTechList`'s state rules, `cat_rmAcquireShipTechs`, the Beast animation, the lab draws); **LaunchMgr.c rewritten** (two carrier slots, the Beast screen, `cat_lmLaunchOne`, the seven module slots, the Beast systems, `cat_mothershipModuleSlotSet`; `cat_lmFillInModuleSlots`' swapped slots fixed); **Sensors.c**: `cat_smViewportRenderScene` 0x0050b4b0 and the `cat_sm*` draws (range circles, meshes, blips, hot-key numbers, resources, bounds, mines), `smShipTypeRenderFlags` from the exe's data, the viewport as the whole screen, `catSmReopenAfterManager`, `REG_RenderEventsDefault` 1024; **Collision.c**: the exe's `SOF_CatDocking` exemption in the three ship-collision tests (found by the dock test's processor leg); `TradeMgr.c`; the dock self-test's sCarrier / sProcessor legs and the ACV judgement; the HUD test's sensors frames. `/catBuildTest` + mode `build` | met: build PASS (external ships at health 0, finished at full health; the three managers open and draw) and two runs byte-identical; dock PASS with both host legs and both ACVs judged (the corvette-cone orbit does not persist with the exemption); hud's sensors frames through `cat_sm*`; the research lists equal the exe's data; **the mission stream moved**: `rng6` (10 draws ahead at tick 1) and `rng_all` only, the exe's per-ship timer for Mission01's 154 player-1 ships, nothing else | PASS-WITH-FIXES, **0.86** (44/51). Fixes: the research gate tested `TECH_INFECT` for the exe's 0x22 = `TECH_CRYSTAL_HARVEST`, `cat_univConvertShip`'s new-technology test read the new owner's rows (the exe: the old owner's), two split CAT-TODOs, `clWrapCreateShip` reclassified MODIFIED, four HW1 aliases, a wrong tag reference | `a6e72c9` |
| 5 | NIS, animatic, mesh animation + repair droids, fog of war, camera ([report](m2-r6-nis-fow-camera.md), [audit](audits/m2-r6-nis-fow-camera.md)) | **NIS.c**: the Cataclysm opcode enum (75 entries, nine appended, `NEO_Repair` 0x05 ...), the 0x28-byte event with six parameters, the module copies, all 16 unread bodies (`nisUpdateTask`, `nisNewObjectCreate` with the mothership's hyperspace modules, `nisStart` / `nisStop`, `nisCamPathUpdate`, `nisUpdate` through `cat_univSetShipPosition` and `cat_clampUpdateAttachedObjects`, ...), the 27 `cat_nis*` script callbacks (fog, lights, mimic, repair, slipgates, subtitles, clip planes, e-bolts, the Beast effect) with the exe's 81-row script table; 9 IDENTICAL found modified (`nisCameraFOVSet` read stdin in GoK's tree). **Animatic.c / avi.c**: the frames converted by the movie's rate: **Cataclysm's 18 animatics are 30 fps** (GoK played them at half speed). **Camera**: the exe's `Camera` 0x5C (`catSnapToFocus`, `catScriptedDistance`), `CameraCommand` 0x94 with `ccMode` at +0x8C (HW1's `dontUpdateRenderList` and `catSensorsHoldRenderList` are one field), the 7 unread bodies (`NewSetFocusPoint`'s single precision, `CameraChase`'s pilot view through `cat_ccChaseCamFromShip` and the scripted snap, ...), the `cat_cc*` scripted calls, **the motion cues** `cat_mc*` (the exe's 0x00c49400 state, the GL calls identified from the glcompat pointer table), HW1's pilot offsets retired. **New** `Game/FOW.c/.h`: the fog of war (grid, cubes, boundary renderer, `cat_fowCanPlayerSeeShip`, `cat_shipAffectsFOW`), `cat_fowCreatePlayerGrids` at `levelInit` (**the local player only**, as the exe's `curPlayer` compare), the universe box +5,000 per axis, `cat_univShipFOW*`, the stealth detection, `univFindNearestResource`'s fourth argument, the waiting sites (ACV / Hive / Ramming, `processAttackToDo`'s prelude, the focus test), the picking helpers' grid, `pingListDraw`, the FOW saves. **The beam's sixth argument** (the Sentinel-field triangle) and the exe's squared length test. **Mesh animation**: the exe's madanim (mode word, animation numbers, previous animation, deferred binding swap, the per-type sound, pause-at-end), `madAnimationStart`'s third argument at all 25 callers from the pushes, 8 `madLinkIn*` found modified (sounds 0x405 / 0x406). **Repair droids** (`RepairDroid.c`): `droidCreateArray` / `UpdateArray` / `cat_droidFreeArray` and helpers, the seven states, wired at `univCreateShip`, `FixShip`, the self-repair and the renderer. `/catNisParseTest` + mode `nisparse` | met: nisparse PASS (14 NIS, 14 scripts, 18 animatics, 1,488 events); `Player.catFOWGrid` set for the local player (the exe makes no other), `stealthUpdates=387` in the command test; the camera bodies at the exe's; the Worker holds its paused jack animation (`jackPausedTick=1024`); droids update and render (2,500 drawn); **the stream did not move**: the pre-droid pair and the post-droid pair are both `24253cbae125485f` (no damaged Sect ship in play; the main view not drawn), IDENTICAL to the managers' stream and to R5 but for `rng6` / `rng_all` | PASS-WITH-FIXES, **0.89** (50/56). Fixes: 8 unmarked CAT-FIX guards in FOW.c, a wrong mimic-resource comment, `cameraInit`'s undocumented deviation, a missing KAS tag, two stale review notes, a report count | `3aa4cce` |
| 6 | **motion interpolation** ([report](m2-r6-motion-interp.md), [audit](audits/m2-r6-motion-interp.md), spec docs/features/motion-interpolation.md) | Not in the exe: every code line carries `HWI-FEATURE(motion-interp)` (433 lines), no `CAT:` comment and no delta-map review but `memFree`'s one-line hook. **Measurement first** (`Game/MotionTest.c`, modes `motion` and `nis`, `tools/nis_smoothness.py`, the pre-feature dumps). **Gameplay** (`Game/MotionInterp.c`): a malloc'd side table of every object's previous position / coordinate system / collision and engine positions captured at the start of each universe tick; `alpha` from the task scheduler's leftover ticks and the clock's sub-tick remainder (`taskPeriodFraction`, time compression honoured); `miRenderBegin` / `miRenderEnd` around `regFunctionsDraw` write `lerp(prev, cur, alpha)` into the objects for the draw and restore them bit for bit, so the camera's focus centre, the selection circles, picking, the HUD and the sensors manager all use the drawn positions; a jump over 2,000 units or a paused universe draws `cur`; missile trails' newest point hidden for the frame. **NIS**: `bsCurveEvalAt` / `bsCurveEvalAtBeforeJump` (non-advancing; `bsCurveUpdate` and `nisUpdate` untouched), `nisRender*At` at `tRender = curve time + alpha * period * nisPlayFactor`, never blended across a camera cut. **Toggle**: the "Smooth motion" checkbox added in code to `Video_Options` and the in-game `Video` screen, `Cataclysm.cfg` rows `objInterpolate` / `nisInterpolate`, `/noInterp` / `/noNISInterp`, two console rows. `/catToggleTest` + mode `toggle` | met: B (crossing) 0 / 724 repeated frames (off: 640 / 719), O9 (the tracked sRecon of n06) 0 / 1389 (off: 1231 / 1384); the `/noInterp` dumps byte-identical to the pre-feature dumps; six 6,000-tick streams identical with the flags on, off and NIS-off and IDENTICAL to the base's; the toggle persists through the options file; layout32 OK; fe and sp ASan 0 | PASS-WITH-FIXES, **0.884** (38/43). Fixes: a stale-capture blend after re-enabling the flags (the generation now advances every tick), the switch-persistence claim (a switch's 0 is written to `Cataclysm.cfg` by the next `opOptionsSaveSettings`: a follow-up), a `memFree` review entry, three report details | `af50963` |
| 7 | **oracle mode A** ([report](m2-r6-oracle-mode-a.md), [audit](audits/m2-r6-oracle-mode-a.md)) | hwi-engine loads the user's reference recording `oracle01` (a cataclysm.exe 0x10ad0027 multiplayer save: 2 players, 47 ships, 1,532 asteroids, 59 blobs) through a new converter `Game/SaveCat32.c` with 120 generated layout tables (`tools/m2_save32_gen.py`: the tree's headers compiled `-m32` and native, 39 roots and 45 ship specifics, `SaveCat32Layouts.c`), the exe's `PreLoadGame` / `LoadGame` / `SaveGame` / `LoadUniverse` / `SaveUniverse` / `FixShip` order, the misc chunks (Mimic.c), the ship-record tables, the counters, the blast list, the state block, `LoadSinglePlayerGame`; `LoadRecordedGame` 0x00487cf0 as the front end's; the packet reader `clProcessSyncPacket` at the exe's offsets for all 27 types (13 occur in `oracle01`, 10,881 `extbuildupdate` among them); `/oracleReplay` + mode `replay` (sound and speech off: the mixer's ambience ends drew stream 11 on the wall clock); the AI chunks walked (the players restart fresh: the stock playback never runs them); 3 CAT-FIXes (`sizeship` rounded to 8, `univAddAsteroid`'s `catMimicShip` NULL where the exe left allocator garbage, a 64-bit region id); docs/formats/savegame.md corrected in 13 places from the real files; 25 recorder / save CAT-TODOs closed | met: the replay runs to tick 9601 / packet 4800 with ASan 0 and two runs byte-identical (`c55fd4b2d2e9afd0`); against `oracle01-rec.hwo` the first divergent tick, field, packet and the owning function with evidence stated (tick 136 `ships_health`; packet 46 is the stock game's own record / playback difference); mission IDENTICAL to motion-interp's; fe / sp ASan 0 | PASS-WITH-FIXES, **0.93** (41/44). Fixes: two report statements (13, not 15, savegame.md corrections; player 1 of `oracle01` is "Computer 1", not a second human), two missing review entries (`gcStartup`, `nebLoad_Nebula`) | `9e20b5b` |

**Round size** (`git diff a57394b`, the R5 integrate commit to this step's tree): 212 tracked files changed, +51,207 /
−20,333 lines at 9e20b5b (plus this step's 7 `src/` files, +118 / −62, and the two map files); `src/` alone: 173
files, +32,786 / −13,306, with **2,833 added lines carrying `CAT`**; 13 new files (`Game/PopupMenu.c/.h`,
`Game/Console.c/.h`, `Game/FOW.c/.h`, `Game/MotionInterp.c/.h`, `Game/MotionTest.c/.h`, `Game/SaveCat32.c/.h`,
`Game/SaveCat32Layouts.c`), 0 deleted (`KeyBindings.c`, `ResearchGUI.c`, `LaunchMgr.c` and `mainrgn.c` lost their HW1
bodies in place). The tree now carries 3,533 `CAT:` lines, 105 lines carrying `CAT-TODO` (R5: 188; 96 owner tags), 8 `CAT-PROVISIONAL`
(22), 15 `CAT-DORMANT`, **121 `CAT-FIX`** (40), 22 `HWI-ORACLE`, 433 `HWI-FEATURE(motion-interp)`. Tools:
`tools/m2_harness.sh` (12 new modes: ui, fescreens, strings, keys, console, build, nisparse, motion, nis, toggle,
replay, plus the harvest / FOW / droid lines of the older ones), `tools/m2_layout32.sh` (the UI, key, NIS, FOW and
camera layouts), `tools/nis_smoothness.py`, `tools/m2_save32_gen.py`, `tools/oracle/mission_test.sh` unchanged.
`runs.txt` logs 411 harness runs this round (lines 663-1073), 79 of them this step's (the exit
test was run twice: once from the unedited 9e20b5b build, once from the final tree).

## Delta map after the round

The ports recorded their comparisons as reviews in `ghidra/delta_map_review.json`, merged with `tools/delta_map.py
merge` into `ghidra/delta_map.json` (the startup-keys and oracle-mode-a producers wrote into the map directly; this
step synced the review file, above). Notes tagged `[review: M2-R6 <port>]`: mainrgn-popup 50, frontend 272 (+1 audit),
startup-keys 89, managers 108 (incl. its audit's one), nis-fow-camera 186, oracle-mode-a 32 (+2 audit), motion-interp
2 (`memFree`, `utyTasksDispatch`: HWI hooks, not deltas), this step 5.

| class | M2-R5 end | M2-R6 end | M2-R6 bytes |
|---|---:|---:|---:|
| IDENTICAL | 3,009 | **2,942** | 544,730 |
| MODIFIED | 1,334 | **1,394** | 678,202 |
| REWRITTEN | 77 | **84** | 15,915 |
| hand-reviewed | 2,162 | **2,816** | |

Per stage (`python3 -I tools/m2_stage_counts.py --md` on the map after this step): HW1-matched IDENTICAL / MODIFIED /
REWRITTEN functions (MODIFIED bytes), then cat-new by the tool's rules.

| stage | M2-R5 end | M2-R6 end | cat-new |
|---|---|---|---|
| 1 | 211 / 104 / 0 (54,697 B) | 201 / 114 / 0 (57,114 B) | 76 / 27,383 B |
| 2 | 765 / 607 / 30 (253,494 B) | 758 / 614 / 30 (255,297 B) | 808 / 180,868 B |
| **3** | **896 / 295 / 7 (187,650 B)** | **847 / 337 / 14 (199,754 + 8,558 B)** | 450 / 139,196 B |
| 4 | 386 / 91 / 2 (50,741 B) | 386 / 91 / 2 (50,741 B) | 23 / 14,404 B |
| 5 | 406 / 114 / 12 (74,848 B) | 406 / 114 / 12 (74,848 B) | 67 / 17,483 B |
| 6 | 345 / 123 / 26 (40,059 B) | 344 / 124 / 26 (40,448 B) | 2,365 / 656,918 B (2,085 kas2c) |

What moved: **68 functions the map called IDENTICAL were found modified** by the ports and their audits (nis-fow-camera
23, managers 19 + 1 in its audit, frontend 15, mainrgn-popup 10; per stage 50 in stage 3, 10 in stage 1 (the mesh
animation and the `.fib` loader), 7 in stage 2, 1 in stage 6), 1 went the other way (`ferDrawBitmapButton`) and 7
MODIFIED became REWRITTEN (the managers' rewritten GUI bodies). The startup-keys port's three found modified
(`utySinglePlayerOptions`, `utyShowCredits`, `utyGameQuitToMain`, its audit's finding 3) are recorded in their notes
without a class change. Every MODIFIED / REWRITTEN function of the R6 files has a review (0 unread in mainrgn.c,
mouse.c, the front-end files, main.c, utility.c, KeyBindings.c, ConsMgr.c, ResearchGUI.c, LaunchMgr.c, Sensors.c,
NIS.c, Animatic.c, MeshAnim.c, MadLinkIn.c, Camera.c, CameraCommand.c). `docs/m2-r1-function-deltas.md` was
regenerated by this step (`delta_map.py report`; it had been stale since the frontend audit).

**Cat-new in the tree** (this step's count by the R5 method: a name is "defined" when a line of `src/` starts its
definition; the kas2c mission region left out): stage 1: **70 of 76 defined** (23,693 of 27,383 B; R5: 63); stage 2:
**760 of 808** (173,939 of 180,868 B; R5: 650), 15 mentioned only, 33 absent (3,488 B: the developer fight statistics
`cat_stats*`, the dissolve helpers the tree holds as statics, the `cat_trans*` / `cat_kniTrans*` GoK-backend paths, the
ten `cat_kbSelectHotKeyGroupN` thunks the tree folds into one function); stage 3: **428 of 450** (135,538 of 139,196 B;
R5: 223), 5 mentioned only, 17 absent (3,369 B: the Win32 DirectDraw debug window `cat_dd*`, the DirectSound / Glide
layers, `cat_mainRGLFeaturesSet`, `cat_rinEnumD3DDevices_cb`: decision 28 and 33, plus `cat_cpValueGradientCreateSized`
432 B and `cat_pingListGet` 6 B); stage 4: 2 of 23 (the `cat_ais*` team code is M2-R7's); stage 5: 18 of 67; stage 6
(outside the kas2c region): 37 of 280.

## Motion interpolation

**What landed** (the user's first approved feature, docs/features/motion-interpolation.md, on top of the 1-to-1
recreation; not in the exe, every line tagged `HWI-FEATURE(motion-interp)`, never `CAT:`): render-time interpolation of
gameplay motion and of NIS curves, with the toggle the user asked for. The simulation is untouched: the side table of
previous transforms is malloc'd outside the game's pools, `miRenderBegin` / `miRenderEnd` bracket only the region draw
of `rndRenderTask` and restore every object bit for bit, `nisUpdate` / `bsCurveUpdate` / the KAS runtime have no
changed line, and the exe-layout structs are unchanged (`tools/m2_layout32.sh` OK).

**How the user toggles it:**
- **The checkbox "Smooth motion"** on Options → Video (front end) and on the in-game Video screen (Escape menu), added
  in code to `Video_Options` / `Video` (a copy of the screen's "full detail" checkbox and label, moved into free space;
  no `.fib` edited, the plain harness stays plain). It drives both flags; Accept persists them, Cancel undoes a click.
- **The options file** `Cataclysm.cfg` (read from `$HOME/.homeworld/Cataclysm.cfg`, written to the user settings path):
  rows `objInterpolate 0|1` and `nisInterpolate 0|1`, default 1 and 1.
- **The command line**: `/noInterp` (both off) and `/noNISInterp` (the NIS flag off). They set the same variables the
  options rows point at, so the next `opOptionsSaveSettings` (an options screen opened, Accept, the end of a game)
  writes them into the file: harmless for harness runs (a fresh HOME and settings dir per run), a follow-up for the
  user's own install (a run-only override; "Decisions" 46).
- **The developer console** (the tilde key): `objInterpolate 0|1`, `nisInterpolate 0|1` (shown with the bare name).

**Guard-rail results** (this step, final tree; the motion-interp audit reproduced the same from independent clean
builds): the two 6,000-tick Mission01 pairs with `/noInterp` and `/noNISInterp` are byte-identical to the pair with the
flags on (all six `49240abcf9f0ed97`, 1,476,370 B), and the release pair too (`cdc60256f37f6961`); the motion and nis
dumps under `/noInterp` are byte-identical to the pre-feature dumps recorded by the motion-interp report and audit
(`motion.dump` `b2af0d6decdf647431d6c2adf701798cd8e98d4a614d7b69390e6df02452d1dd`, 1,431 frames; `nis.dump`
`3f04c837e605ba23d6c74c639bba4ff10a95c139f33d462bca79427d4a985029`, 1,440 frames), frame CRCs included; with the flags
on the dumps are `d9a8661fcce6f4b3` and `074a70f17e316cd2` (the producer's and the audit's); `/catToggleTest` PASS.

**Measured** (`tools/nis_smoothness.py`, this step's dumps, 144 fps virtual = 9 frames a universe tick, 1024x768; a
repeated frame = the object moved less than 0.05 px (motion) / 0.01 px (nis) on screen; off = `/noInterp`):

| object | off: max / p95 / mean px, repeated | on: max / p95 / mean px, repeated |
|---|---|---|
| motion A (the camera's focus, 300 u/s) | 0.003 / 0.001 / 0.001, 1430 / 1430 (locked to the camera) | 0.002 / 0.001 / 0.001, 1430 / 1430 (locked) |
| **motion B (crossing the view)** | 10.152 / 10.108 / 1.114, **640 / 719** (8.01 of 9 a tick) | 1.149 / 1.139 / 1.126, **0 / 724** |
| motion C (receding along the line of sight) | 3.310 / 1.355 / 0.166, 1272 / 1430 | 0.380 / 0.334 / 0.166, 0 / 1430 |
| motion D (at rest, carried across the view by the camera) | 36.339 / 2.886 / 0.621, 1272 / 1430 | 4.162 / 2.516 / 0.615, 0 / 1430 |
| motion camera (eye + look movement per frame, world units) | 37.5 / 37.5, 1272 / 1430 repeated poses | 4.168 / 4.168, 0 / 1430 |
| **nis O9 (n06's sRecon, tracked, 26-36 s)** | 120.534 / 24.118 / 2.724, **1231 / 1384** (8.01 of 9) | 39.525 / 6.200 / 2.808, **0 / 1389** |
| nis camera | 13,589.9 (the cut) / 139.4, 1290 / 1439 repeated poses, 573 repeated frame CRCs | 13,589.9 (the cut) / 178.2, 180 / 1439 (n06's static shots), 0 repeated CRCs |

The means stay (the same path is covered); the per-frame steps shrink to the per-frame share of the motion: the user's
observation (the focus locked, everything else moving on one frame in nine) is gone with the flags on. Design calls
made by the producer (the user may want to rule, "Decisions" 46): a paused universe draws the simulation state (no
blend) rather than the previous tick; a camera cut is never blended across and still shows at its tick, the frames
before it continue the pre-cut key's motion; the NIS objects and camera are evaluated ahead of their last tick while
objects without curves are drawn between their last two ticks (up to one tick apart in time); the render transforms are
written into the simulation's own fields for the draw and restored (the spec asked for separate render-only fields;
the identical dumps and streams show the restore holds). Left for a later owner: the repair droids are drawn at their
tick positions while their host is blended; objects clamped to a non-NIS host blend on their own transforms; the
camera-derived render values (`univUpdateRenderList`'s distances) follow the blended focus in a played game (the
oracle run and the replay are unaffected: the mission run draws the sensors manager, the replay's streams are identical
with and without the flags as far as compared).

## Oracle mode A

**The comparison result.** The replay of the user's reference recording `oracle01` (2026-10-17, 2 players: the user as
Somtaaw against "Computer 1", 47 ships at the save, 4,800 packets = 9,600 universe ticks of the recording) in hwi-engine
from the clean build (`r6-integrate-replay`: `ticks=9601 packets=4800 ships=81`, ASan 0, rc 0, 34.5 s, two runs
byte-identical) compared with the GOG game's checksum stream of the same session (`oracle01-rec.hwo`, GOG DLL in record
mode, 12,282 `T` records, `record-stop` at tick 9600):

- **First divergence: tick 136, `ships_health`** (last equal tick 135), one ship: ID 47, the Somtaaw destroyer player 0
  builds externally (its `extbuildupdate` commands start at packet 62). Every other simulation field is IDENTICAL
  through tick 135 and most are identical far longer: per field the first divergent tick is `ships_health` 136, `rng6`
  161, `ships_vel` 177, `ships_pos` 178, `univcheck` 179, `resources` 243, `ships_misc` 871, `rng10` 1376, `bullets`
  1917, `ships_n` 2048 (this step's run; the same ten numbers as the report and its audit).
- **Owning function: `cat_clExternalBuildUpdate` 0x0044dfc0** (tree: `clExternalBuildAdvance`, CommandLayer.c). The
  exe computes `health += (progress - oldProgress) * maxhealth` on the x87 at 53-bit precision (`fld; fsub; fmul; fadd;
  fst`, one rounding at the store; the GOG header's control word 0x027f) and the tree rounds every float step; the
  tick-136 health hash recomputed with ship 47's x87 result 0x43160001 in place of the tree's 0x43160000 equals the GOG
  hash `b3990c7b2addc54d` exactly (the audit unwound the FNV-1a hash and confirmed it). **The proposed fix is the first
  item of M2-R7** (evaluate the increment in double and round once, after checking the exe's instruction sequence for
  which operands are loaded as float).
- **Excluded from the comparison, with the reason**: `ships_rot` (the GOG DLL hashes `Ship+0x80`, `hook_sites.h
  CAT_SHIP_coordsys 0x80`, where the exe's coordsys is at `Ship+0x98`: a tool defect, our tick-1 rotation hash equals the
  one computed from the save's own matrices; not comparable until a recording is made with a fixed DLL), `rng_all` (GOG
  splits stream 0 and the sound streams follow its mixer), `fpcw` (not filled by this engine), and `players` (tick 93:
  player 0's RUs 2499 GOG vs 2500 ours, the stock game's own record / playback difference: `cmBuildTaskFunction` returns
  while `playPackets` is set, so no playback charges the local player's build; the GOG **playback** stream of the same
  recording leaves the recording at tick 93 in the same way, and its `blobSyncErr` 4746 from frame 109 equals ours).
  Checked this step: against the GOG playback stream `oracle01-play-a.hwo` the recording itself differs only in
  `players` (93) and `rng6` (161: the stock playback skips the computer players, `aiplayerUpdateAll`), so after the
  health fix **the oracle's next rung is `ships_vel` at tick 177**, and our `rng6` agrees with the GOG playback's
  through tick 6406.
- The engine's own sync check against the recorded checksums: `randSyncErr 4037` (first frame 1393), `univSyncErr 4703`
  (first 195), `blobSyncErr 4746` (first 109, equal to GOG's own playback), `pktSyncErr 0`; first divergent packet 46
  (tick 93, `blobcheck`), as the report.

**What else landed** (the report): the Cataclysm save loaded by field-by-field conversion of the exe's ILP32 structs
(1,842 chunks, 0 size problems; `tools/m2_save32_gen.py --check` keeps the tables current), the exe's load / save /
fix order, the 27 packet types at the exe's offsets (13 occur in `oracle01`; the send sides of move / hyperspace /
formation still build HW1 bodies: stage 5 recorder), `LoadRecordedGame` as the front end's, the AI chunks walked and
the players restarted at the saved difficulty, three CAT-FIXes, 15 open points of docs/formats/savegame.md closed
from the two real files (`oracle01` and the user's Mission 1 save), 25 recorder / save CAT-TODOs closed. Release vs
debug replay: every simulation field IDENTICAL over 9,601 ticks and 4,800 packets; `rng_all` differs from tick 1952
(stream 8, battle chatter, a speech path compiled in one build only).

## What is left of stage 3

Nothing blocks the stage-3 exit test (met above); each item has an owner:

1. **The oracle's first divergence** (`cat_clExternalBuildUpdate` 0x0044dfc0's single x87 rounding): M2-R7, first item.
2. **`rndShamelessPlug` 0x00579ff0 / `rndLoadShamelessPlug` 0x00579fa0** (plug.tga while the console or the game-stats
   screen is up, or during a load; its GL quad to be read from the disassembly): `CAT-TODO(M2-R7 render)` x2.
3. **The waypoint draw 0x004e6e70** (it draws jitter from `ranRandomFn(0)` at draw time, consuming stream-0 randoms
   while the waypoint UI is active): `CAT-TODO(M2-R7 waypoint draw)`; the mainrgn-popup producer's question 1.
4. **`TurnHarvestEffectOn`'s real32 effect parameters**: `CAT-TODO(M2-R7 ETG)` (this step's re-target: one change in
   the ETG's variable-parameter reading for every `etgEffectCreate` caller).
5. **`selShipUnderMouse` 0x00506550** is still HW1's body: the exe's enemy-cursor test calls
   `cat_fowCanPlayerSeeShip(curPlayer, target, 0)` behind 0x00a5b1c8 (nis-fow-camera's question 4): M2-R7 (with 3).
6. **The motion-interpolation follow-ups**: the switches' persistence through `opOptionsSaveSettings` (a run-only
   override), the repair droids and clamped objects drawn against a blended host, the camera-derived render values:
   M2-R7 or the high-refresh-4k feature (docs/features/high-refresh-4k.md).
7. **`DeclarePlayerWin` 0x005487f0** (the multiplayer win message, `cat_titanContestRecordResult`): stage 5 LAN (this
   step's re-target); the rest of the single-player defeat flow (the stats screen after `univKillPlayer`, the KAS
   end-of-mission) is stage 6's.
8. **Stage 5 and the saves**: the send bodies of move / hyperspace / formation and the `CAT-TODO(stage 5 ...)` sites
   (29: the recorder's record side, the colour checksum 0x00ab7658, `catExtra`, the remaining 0x48 `CaptainGameInfo`
   bytes, the multiplayer options word, the Titan / WON interface); the user's two saves for `savegame_probe.py` (the
   mode-A loader now reads a real multiplayer save, so what stays unverified is a single-player save's mission tail).
9. **Stage 6**: the kas2c missions and the NIS-first start (`CAT_HW1_MISSION_SCRIPTS 0` stays), the start-position
   shuffle of `gameStart` (40 stream-0 draws per new game, re-targeted by startup-keys: moves the mission stream's
   `rng0` when it lands), the task bar's removal (`tbStartup` / `tbShutdown` and their callers), the tutorial buttons
   around the managers (12 sites), the letterbox callers, the KAS-hashed selection names, the console's KAS command,
   `cat_spGameDifficulty` / `DifficultySetting`'s readers, `cat_trPrecacheShipTextures`' caller, `nebLoad_Nebula`'s
   chunks, the mission-number mapping (44 `CAT-TODO(stage 6 ...)` sites).
10. **Not applicable to the SDL build** (decisions 28 and 33, recorded in the map): the Win32 DirectDraw debug window
    `cat_dd*`, the DirectSound / waveOut and Glide layers, rGL / glcompat, the registry, Bink pause / stop, the
    `CataclysmPatch.exe` launch, the front-buffer cursor save (and with it `regRegionDelete`'s dirtying, closed here).
11. **Cleanup the auditors left to a later owner**: HW1's blob render (`smBlobsDraw` and helpers) and the derelict mesh
    table uncalled in Sensors.c; `collCheckShipShipColl`'s other exe-only tests (the clamped-host comparison) not
    compared beyond the docking exemption (the physics owner); the `CAT-TODO(Memory.c)` tag on `memStartup`'s fill
    patterns (an owner name, not a stage: the next Memory.c reader); the `mrHoldRight` / `mrHoldLeft` stand-ins next to
    Cataclysm's state words (InfoOverlay, Select, PiePlate, TaskBar and Sensors still test them).
12. **Readings kept as they are**: decision 29 (the 0.325 pixel-centre offset: GoK's, kept by producers 1 and 2); the
    local-player-only FOW grid (the exe's); the `fescreens` test's reading of "every screen" (the in-game screens run
    with no-op stand-in callbacks, their own need a running game); the start-up's effects preset 0 turning the dynamic
    CPV, lens flares and repair droids on by default (`.bss` 0 in the exe, the preset applied at start-up as the exe).

## Plan for M2-R7: stage 4 (AI), with the oracle's first divergence first

Stage 4 at the end of R6 is **386 / 91 / 2 HW1-matched functions (50,741 + 169 MODIFIED / REWRITTEN bytes; 60 of the
93 bodies unread, 35,494 B; 386 IDENTICAL bodies, 72,962 B, to compare) + 23 cat-new (14,404 B), of which 2 are defined
in the tree** (`python3 -I tools/m2_stage_counts.py` and this step's grouping of `ghidra/delta_map.json` by HW1 file:
AIOrders.c 3 / 19 / 0 (16 unread, 10,533 B), AIUtilities.c 63 / 22 / 0 (14 unread, 6,065 B), AIMoves.c 154 / 9 / 0 (7
unread, 4,707 B), AIPlayer.c 20 / 8 / 0 (5 unread, 1,793 B), AIFleetMan.c 5 / 6 / 0 (3 unread, 3,708 B), AIResourceMan.c
7 / 6 / 0 (3 unread, 3,270 B), AIHandler.c 15 / 5 / 0 (3 unread, 1,855 B), AIShip.c 15 / 5 / 0 (3 unread, 1,023 B),
AITeam.c 39 / 3 / 2 (3 unread, 737 B), AIDefenseMan.c 10 / 3 / 0 (2 unread), AIEvents.c 13 / 2 / 0 (1 unread),
AIAttackMan.c 10 / 3 / 0, AITrack.c 19 / 0 / 0, AIVar.c 13 / 0 / 0), plus the 22 `CAT-TODO(stage 4 ...)` stand-ins in
the tree (the 14 HW1-type stand-ins of M2-R4 ships-b in AIOrders.c / AIHandler.c / AIResourceMan.c / AIFleetMan.c, the
computer player's dock order after a conversion, the AI resource counters in Dock.c and Salvage.c, `cat_aisReset` and
`aiplayerStartup`'s arguments in utility.c, the AI save chunks, `aiuFindCaptureTarget`'s targetable test, the rest of
`aimProcessDock`'s body). About 140 KB of exe code to read against the tree (R6 read about 190 KB with five
producers), so M2-R7 is **four producers + four auditors + integrate** (9 agents), in sequence. The porting conventions,
the harness and the session rules stay (every producer compares each IDENTICAL body in its scope, runs the engine only
through `tools/m2_harness.sh`, writes its reviews into `ghidra/delta_map_review.json` and merges, writes `CAT-TODO`s on
one line owner first, and records the mission-stream hash before and after: **every AI port is expected to move the
Mission01 stream**, since player 1 of Mission01 has a computer player whose updates draw from stream 6).

| # | producer | scope | counts | done when |
|---|---|---|---|---|
| 1 | **the oracle's first divergence**, then the AI ship and utility code | `cat_clExternalBuildUpdate` 0x0044dfc0: the exe's x87 sequence (`fld [progress]; fsub [ship+0x3e8]; fmul [static+0xa4]; fadd [ship+0x170]; fst [ship+0x170]`, one rounding at the store) reproduced by evaluating the increment in double and rounding once (check which operands the exe loads as float first), the replay rerun and the next divergence localised; `AIShip.c` (5 MODIFIED, 3 unread: `aishipGuideMissile`, `aishipFlyMineToTarget`, `aishipmineaquiretarget`; `aishipFlyToPointAvoidingObjsFunc` 0x00413e40's two remaining differences: the small-target pass skipped below static +0x48 500, the Ship+0x608 / mission-type exceptions), `AIUtilities.c` (22 MODIFIED, 14 unread: the `aiuWrap*` on `cat_shipIsTargetableBy`, `aiuFindCaptureTarget`, `aiuFindArmadaTarget`, `aiuFillInArrays`, `aiuFindDecloakedShipInSphereOfInfluence`, ...; `cat_aiuWrapCapture`), `AITrack.c` / `AIVar.c` (32 IDENTICAL to compare), `aimProcessDock`'s rest | 27 / 17 unread (7,088 B); 1 cat-new | the replay of `oracle01` IDENTICAL to `oracle01-rec.hwo` in every simulation field (the "Oracle mode A" list) through tick 176 at least, the health divergence gone, the next divergence (velocities at 177 or later) owned with evidence; the AI ship / utility bodies at the exe's; the mission stream hash before and after stated |
| 2 | AI orders, moves, handlers, events | `AIOrders.c` (19 MODIFIED, 16 unread: every `aioCreate*` with Cataclysm's ship types in place of the 14 HW1-type stand-ins), `AIMoves.c` (9 MODIFIED, 7 unread: the flank / advanced / harass attacks, defend mothership, active recon / capture, `aimCreateGetShips`), `AIHandler.c` (5, 3 unread: the numbers-low handlers), `AIEvents.c` (`aieExecute`), `AIAttackMan.c`, `AIDefenseMan.c` (`aidDefenseManager`, `aidAddAllShips`); 195 IDENTICAL to compare | 41 / 29 unread (19,942 B) | the 14 type stand-ins gone; a scripted `/catAiTest` (mode `ai`): a headless Mission01 run where the computer player (player 1) gets a scripted factory and RUs (as `/catBuildTest`) and must form teams and issue attack, defence and recon moves within N ticks, ASan 0, two runs byte-identical |
| 3 | AI player, fleet and resource managers, teams, the Cataclysm skirmish AI | `AIPlayer.c` (8 MODIFIED, 5 unread: `aiplayerStartup` with the exe's arguments (utility.c's stand-in), `aiplayerClose`, `aiplayerUpdateAll`, `aiplayerPlayerDied`, `aiplayerSave`; the AI save chunks converted for the mode-A loader), `AIFleetMan.c` (6, 3 unread: `BuildShip`, `aifBuildRequestedShips`, `aifProcessShipBuildRequests`; `cat_aifRequestSupportModule`), `AIResourceMan.c` (6, 3 unread; the resource counters of Dock.c / Salvage.c), `AITeam.c` (3 + 2 REWRITTEN: `aitMsgSend` / `Received`, `aitTeamIsInMothershipRange`; `cat_aitMsgQueueRemoveAt` / `Flush`); **the 17 `cat_ais*` functions** (the Cataclysm-only skirmish AI, 11.9 KB: `cat_aisUpdateTeams` 4,691 B, `cat_aisScoutsUpdate`, `cat_aisTeamAttack`, `cat_aisLeechesUpdate`, `cat_aisPlay`, `cat_aisMothershipUpdate`, `cat_aisTeamAddShip`, `cat_aisSetupAlliances`, `cat_aisResearchUpdate`, `cat_aisSentinelsUpdate`, `cat_aisWorkersUpdate`, `cat_aisCarrierUpdate`, `cat_aisProcessorUpdate`, `cat_aisTeamFindNearestSelection`, `cat_aisTeamGetShip`, `cat_aisShipIsBusy`, `cat_aisReset`), the computer player's dock order after a conversion | 25 / 14 unread (9,508 B); 20 cat-new (14,230 B) | the computer player builds, researches and harvests in the ai self-test (the build and resource requests reach `cmBuildJobsAdd` and the Workers); the `cat_ais*` code at the exe's with its 8 play timers; no `CAT-TODO(stage 4 ...)` left in `src/` |
| 4 | the R6 leftovers (render / UI, no stream impact) | `rndShamelessPlug` / `rndLoadShamelessPlug` from the disassembly, the waypoint draw 0x004e6e70 (its draw-time stream-0 jitter stated and judged), the ETG real32 variable parameters for every `etgEffectCreate` caller, `selShipUnderMouse`'s FOW test, the motion-interpolation follow-ups (a run-only override for the switches, the droids and clamped objects against a blended host), `cat_cpValueGradientCreateSized`, `cat_pingListGet`, HW1's blob render cleanup in Sensors.c, the `mrHoldRight` / `mrHoldLeft` stand-ins, the `Memory.c` tag | 4 `CAT-TODO(M2-R7 ...)` + the audits' leftovers | no `CAT-TODO` names M2-R7; the motion and nis dumps under `/noInterp` still equal the pre-feature hashes; the mission stream unchanged by this producer |

**M2-R7 integrate (stage-4 exit):** from a clean build on the plain harness: the R6 gates hold (two 6,000-tick Mission01
streams byte-identical in debug and release on the re-baselined stream, with the hash and the moved fields stated
against this step's `49240abcf9f0ed97`; sp 1,248+ ticks at 340 s with ASan 0; fe `Main_game_screen`; the self-tests
statics, meshes, textures, oracle, etg, dock, command, render, sound, hud, ui, fescreens (0 `fescreen`), strings
(1,705), keys, console, build, nisparse, toggle `result=PASS` with ASan 0; the release oracle `SELFTEST PASS`; every quit
rc 0; `m2_layout32.sh` OK; the `/noInterp` dumps still equal the pre-feature hashes); the new `ai` self-test PASS with
ASan 0 and two runs byte-identical; **the oracle's next rung**: the replay of `oracle01` IDENTICAL to `oracle01-rec.hwo`
in every simulation field of the "Oracle mode A" list (`rng10`, `rng6`, `ships_n`, `ships_misc`, `ships_pos`,
`ships_vel`, `ships_health`, `missiles`, `bullets`, `derelicts`, `resources`, `univcheck`, `dt`, `total`, `real`; the
stock differences `players` at 93 and `rng6` at 161 excluded as GOG's own playback shows them) **through tick 176 at
least**, i.e. the health divergence at 136 gone, with the next divergence localised to its owning function and its fix
proposed as this round did; no `CAT-TODO` / `CAT-PROVISIONAL` naming an M2-R6 or M2-R7 owner; the `stage 4` tags gone
or re-targeted with a reason. Then stage 5 (LAN, nice-to-have: the recorder's record side and the packets' send sides,
29 sites) and stage 6 (missions: the kas2c code, the NIS-first start, the start-position shuffle, the task bar, the
letterbox callers, the tutorial buttons, 44 sites; `CAT_HW1_MISSION_SCRIPTS` retired), each sized by
`tools/m2_stage_counts.py` when its turn comes.

## Decisions

**Applied this round:**
- The faithfulness defaults and decisions 17-33 of cycle/STATE.md: `CAT_EXTERNAL_BUILD_DRIVER` is now 1 (17 retired by
  the managers port); the chosen names (18, 24) extended by this round's (below); the HWI deviations marked `CAT-FIX`
  (now 121); `CAT_HW1_MISSION_SCRIPTS 0` and the LeakSanitizer suppressions stand (34); `fsqrt` stays `sqrtf` (23: not
  switched, the mode-A divergence is not a square root); GoK's GL backend and SDL's audio layer stay (28; the Win32
  layers recorded as not applicable, 33); the 0.325 pixel-centre offset stays GoK's (29, kept by producers 1 and 2);
  the dock self-test's coverage gap closed (33, by the managers port).
- The reference recording `oracle01` (decision 6) is recorded and replayed (10: the DLL loads under Proton, the H6
  trigger works, `vqpc` gives 1/16 s per tick: all three verified by the user's sessions of 2026-10-17); it is the
  oracle-mode-a input and the M2-R7 rung.
- Motion interpolation lands as an addition on top of the recreation with the toggle the user required.

**Still open (the user's; none blocks M2-R7):**
- **8.** Two saves from the M0 play-test (one mission, one skirmish): the mode-A loader now reads a real multiplayer
  save (`oracle01`) and probes the user's Mission 1 save, so what stays code-derived is a single-player save's mission
  tail (KAS / objectives / timers / pings: stage 6).
- **The M0 play-test sign-off** (docs/PLAN.md).
- **A second recording with the fixed oracle DLL** (`CAT_SHIP_coordsys` 0x98: "Decisions" 48) when convenient, so
  `ships_rot` becomes comparable; not needed for the M2-R7 rung.

**Surfaced by M2-R6.** Each has a default in the tree; say if you want otherwise.
35. **The waypoint draw 0x004e6e70** draws its jitter from `ranRandomFn(0)` at draw time, so it would consume stream-0
    randoms while the waypoint UI is active; not ported (M2-R7, "What is left" 3). Default: port it as the exe does and
    state the stream effect in the report.
36. **The `mrHoldRight` / `mrHoldLeft` stand-ins** stay next to Cataclysm's state words (0x00a75284 / 0x00a75288) while
    other modules test them; the `CameraCommand` layout question of the mainrgn-popup port is closed by nis-fow-camera
    (`CameraCommand` 0x94, `ccMode` +0x8C).
37. **The `fescreens` self-test's reading**: the front-end files run the game's own callbacks; the in-game files (the
    managers, the in-game menus, the loading screen, chat, console) run with no-op stand-ins, since their callbacks need
    a running game and fault out of context, as in the exe. Default: that is "pushes and pops every screen".
38. **The options file** is `Cataclysm.cfg` (the exe's name), read from `$HOME/.homeworld/Cataclysm.cfg` (GoK's config
    dir, not `/settingspath`) and written to the user settings path; the harness's home template holds it. Default:
    keep the GoK read location.
39. **The start-up applies effects preset 0** (the exe's `opUpdateSettings` path), which turns the dynamic CPV, lens
    flares and repair droids on by default (their `.bss` value is 0 in the exe). The Mission01 stream is unchanged by it.
40. **The start-position shuffle** of `gameStart` (20 swaps of a table with two `ranRandomFn(0)` picks each in every
    non-tutorial new game, Mission01 included) is re-targeted to stage 6 level load with its readers
    (`scriptSetMissionSphereCB` / `scriptPreMissionSphereCB`); it will move the mission stream's `rng0` when it lands.
41. **Cataclysm has no task bar** (`gameStart` has no `tbStartup`), but HW1's stays until stage 6 removes its callers
    (Objectives.c, SinglePlayer.c, KASFunc.c).
42. **HW1's blob render** (`smBlobsDraw` and helpers) and the derelict mesh table stay in Sensors.c uncalled (the exe has
    no blob render); cleanup by a later owner.
43. **The FOW grid belongs to the local player only** (`cat_fowCreatePlayerGrids` 0x004a38c0 compares with `curPlayer`):
    computer players have none and see every ship that is not fully cloaked and undetected; while no cell is visited the
    sensors scene hides every grid-tested point (resources, mines, crates), as the exe.
44. **The mission-stream baseline moved in managers**: `rng6` (10 draws ahead at tick 1) and `rng_all` only, from
    `cat_shipUpdateExternalBuildAndAI` 0x00543f10's per-ship draws for Mission01's 154 player-1 ships (the exe's; its
    call site verified). Default: the new baseline (this step: `49240abcf9f0ed97`) is accepted; the droid and
    motion-cue draws of nis-fow-camera did not appear in the headless run (no damaged Sect ship in play, the main view
    not drawn), so the plan's "post-droid re-baseline" was empty.
45. **Cataclysm's 18 animatics are 30 fps** (their Bink headers); GoK's avi player assumed 15, so every animatic played
    at half speed until nis-fow-camera; sp now reaches the universe sooner.
46. **Motion interpolation defaults and design calls**: `objInterpolate` / `nisInterpolate` both 1 (`Cataclysm.cfg`
    rows, `/noInterp`, `/noNISInterp`, console rows, the Smooth motion checkbox added in code); a paused universe draws
    the simulation state (no blend); a camera cut is never blended across, the frames before it continue the pre-cut
    key; NIS objects ahead of their last tick, non-curve objects between their last two; the render transforms written
    into the simulation's fields and restored bit for bit (the spec asked for separate fields); the switches persist
    into `Cataclysm.cfg` through the next `opOptionsSaveSettings` (a run-only override is the M2-R7 follow-up). The
    `tools/nis_smoothness.py` thresholds: 0.05 px (motion) / 0.01 px (nis).
47. **Oracle mode A's exclusions**: `ships_rot` (DLL offset defect), `rng_all` (GOG splits stream 0; sound streams),
    `fpcw` (not filled), `players` (the stock record / playback difference at tick 93); `rng6`'s stock difference at
    161 (the playback skips the AI). Default: compare the "Oracle mode A" field list, ticks 1..9600.
48. **The oracle DLL's `CAT_SHIP_coordsys` should be 0x98, not 0x80** (tools/oracle/dll/hook_sites.h); the DLL was not
    run or rebuilt this round (the GOG side of R6 is a recorded stream). The fix is a one-line edit for the user's next
    recording session.
49. **The x87 single rounding of `cat_clExternalBuildUpdate`** is a simulation fix proposed, not made (M2-R7's first
    item): the recreation keeps float-stepping until then.
50. **Release vs debug replay** differ in RNG stream 8 (battle chatter) from tick 1952 of the replay, on a speech path
    compiled in one build only; simulation fields identical. Default: the oracle compares simulation fields only.
51. **Latent 64-bit union aliasing** in `MCVSpec` and `P1StandardCorvetteSpec` (listed by `m2_save32_gen.py`): no owner
    yet; the ships' reader.
52. **New non-fatal UBSan kinds**: `BTG.c:864` and `stb_image_write.h:1263` (frontend audit W1, GoK loader / writer
    artefacts), `Select.c:1166` (`selCircleCompute`, oracle-mode-a) in the replay. Listed, not required.
53. **This step's three small ports** (the kill credit, the manager closes and the exe's speech id in
    `univKillPlayer`; `univKillOtherPlayersIfDead`'s ether look and kill credit; the time-compression string) are
    grounded in the decompile and the disassembly and unaudited; the M2-R7 audits may re-check them (as 32 did).
54. **Chosen names** where the exe's are unknown: `catKillerIndex` (univKillPlayer's third argument), `catSnapToFocus` /
    `catScriptedDistance` (Camera +0x55 / +0x58), `CAT_SHIPCMN_ANIM_OPEN` / `CLOSE` (sounds 0x405 / 0x406), the FOW
    structs `CatFOWGrid` / `CatFOWCell`, the madanim words `catPreviousAnim` / `catBindingSwapPending`, the droid entry
    fields, `catExternalShipAttached`, `cmDetermProgress_t`'s `catAttached` / `catShipStarted` / `catShip`,
    `SS2_CatConsole`, `catKeyNameStrings`, `catConsole*`, the NIS opcode names `NEO_*` for the nine appended codes.
55. **The review file is the merge's source** (above): a producer's reviews go into `ghidra/delta_map_review.json`.
