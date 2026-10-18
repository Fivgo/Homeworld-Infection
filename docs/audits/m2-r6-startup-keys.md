# Audit: M2-R6 startup-keys (adversarial)

Port under audit: producer 3 of M2-R6, report `docs/m2-r6-startup-keys.md`, work = `git diff 142a8e2` (the WIP
snapshots `57ae3b4` and `6645763`; the working tree was clean at the start). Not port work and not judged:
`404e74e` (docs/PLAN.md, a features note) and `60fdec7` (docs/assets-export.md, tools/assets/assemble_modular.py), both
from other agents. Every engine run went through `tools/m2_harness.sh` (silent, headless, plain harness); no Ghidra,
no GOG game, no input.

## 1. Clean build

`/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/build-audit-r6-startup-keys`, removed
first, set up and compiled with tools/build.sh's arguments (debug: `--buildtype=debug -Db_sanitize=address,undefined`;
release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`), -Werror in the build files:

| config | rc | warnings | hwi-engine size |
|---|---|---|---|
| debug | 0 | 0 | 84,191,992 B |
| release | 0 | 0 | 5,179,768 B |

`tools/m2_layout32.sh`: `m2_layout32: OK` (prints `CatKbCommandBindings` 52 = 0x34).

## 2. Plain-harness runs (audit binaries above)

| label | mode | result | ASan |
|---|---|---|---|
| aud-r6sk-keys | keys | `keystest: done rows=197 known=149 unknown=48 commands=147 bindings=149 startsave=1 saved=1 reload=1 reset=1 newkey=1 conflicts=1 shares=1 failures=0 result=PASS`, rc 0 | 0 |
| aud-r6sk-console | console | `consoletest: done selected=154 frames=4 shown=29 set=22 commands=8 keys=32 closed=1 failures=0 result=PASS`, rc 0 | 0 |
| aud-r6sk-fe | fe, 90 s | Main_game_screen, still running at the timeout; 299 loads, the base `r6sk-base-fe` list with Cataclysm.cfg (twice) for Homeworld.cfg | 0 |
| aud-r6sk-sp | sp, 340 s | universe running, univUpdateCounter 1,360, 3,783 loads (= the producer's r6sk-fin-sp; base 3,782 + `FEMan\console.fib`); UBSan kinds identical to `r6sk-base-sp` | 0 |
| aud-r6sk-mission-a / -b | mission_test.sh --ticks 6000 --no-perturb-run | MISSION TEST PASS: byte-identical (1,476,370 B, sha256 `11d23b2613622fa9`), hwo_compare IDENTICAL, flipped copy found at tick 2000 (ships_health) | 0 / 0 |
| aud-r6sk-mission-rel | mission, release, 6000 | oracle mission run done | n/a |

R5 baseline `r5-integrate-mission-a/run.hwo` (sha256 `645f94c69d68319e` confirmed): `hwo_compare.py` against
aud-r6sk-mission-a and against aud-r6sk-mission-rel: `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no
difference`, header build identity only (02e8e4da vs 66457631). The sp counter (1,360 vs the producer's 1,248) is the
wall clock of the real-time animatic (sp is judged by milestone and ASan); not a change.

keys.cfg: the file the keys run leaves (`runs/aud-r6sk-keys/settings/keys.cfg`, 152 lines) equals the GOG game's own
`original/game/keys.cfg` line for line (CR/LF aside); against the install's `default_keys.cfg` (200 lines) it lacks
exactly the 48 developer rows (`Play_NIS` .. `Toggle_Draw_KAS_Vars`, names the exe's command table does not know).

Options file: the home template's `.homeworld/Cataclysm.cfg` is `TutorialNeeded    0` / `PlayIntros    0`; the fe
run read it (FileLoads lines 281 / 299) and wrote `settings/Cataclysm.cfg` (107 rows, exe format) with both still 0.
The tutorial and the intros stay off.

## 3. Bodies compared with the exe (r6 export, tools/m2_fdec.py / m2_fdis.py / m2_exedata.py, objdump for 3 bodies
Ghidra did not list)

| # | body | verdict |
|---|---|---|
| 1 | ProcessCommandLine 0x0055f880 | correct: missing parameter / failing function -> DebugHelpDefault(token) and -1, unknown switch -> help and -1 |
| 2 | WinMain 0x005631f0 (order) | correct: options file, opKeyboardLoad, command line, PreInit, window, Init, loop |
| 3 | WindowProc 0x00561f10 (key part) vs HandleEvent | correct: every WM_KEYDOWN / repeat to cat_consoleKeyPress; Escape key-up ends its case |
| 4 | mainResetRender 0x00560420 (inlined copies in WindowProc) | correct: frReset, ferReset, mouseReset, feReset, icons, lens flare, fog unloads |
| 5 | utyGameSystemsPreInit 0x005ab810 | correct: no bigCRC call in the body; fmathInit present (decision 23); minorBuildVersion |
| 6 | gameStart 0x005a9290 | correct: support units, /LoadMPGame (0x008dfa20 is ""), races, managers closed, sensorLevel 0, AI level byte, researchOff word, x5 resources on flag bit 5, universeSwitchToPlayer, tech pass (researchinfo +0x2A4 = HasTechnology), console start-up, bobResetProperties, track 0x12, upgrades + updateClampedObject (clampInfo +0x34) |
| 7 | gameEnd 0x005a9f40 | correct: no writeGameStatsToFile, opOptionsSaveSettings and 0x00a6ed74 cleared at the end |
| 8 | utyGameQuit 0x005ab570 | correct |
| 9 | utyOptionsFileRead / Write 0x005a83a0 / 0x005a84f0 + table 0x008c1478 | correct: 94 rows, names, order and every callback equal; "%s    %d"; Cataclysm.cfg |
| 10 | cat_keyNameStringsInit 0x0049b1b0 | correct: all 95 table entries equal (scripted compare against StringsOnly.h) |
| 11 | cat_kbKeySetCB 0x0054d460 | correct |
| 12 | cat_kbSaveKeyBindings 0x0054e2d0 | correct (strings "v", header, "%s%s%s%d", "key  %s  %s") |
| 13 | cat_kbSetNewKey 0x0054e410 | correct: the inlined share test equals the tree's symmetric cat_kbCommandsCanShareKey pair |
| 14 | cat_kbCommandsCanShareKey 0x0054d6a0 | correct |
| 15 | cat_kbLoadKeyBindings 0x0054da10 | correct (151 rows cleared, version compare, copy when missing / older) |
| 16 | cat_consoleExperienceShips 0x0054d1c0 | correct: Ship +0x5E0 = catKillsByClass (+0x5C8) [CLASS_Fighter 6] |
| 17 | cat_consoleUpgradeShips 0x0054d0e0 | correct |
| 18 | cat_consoleShowVariable 0x0045f110 | correct (dword read for every integer type, strings) |
| 19 | cat_consoleConfigFileSave / Load 0x0045fec0 / 0x0045faf0 | correct, incl. the claim: fwrite little-endian vs a big-endian reader, so loadConfig leaves the values |
| 20 | console command table 0x008783c8 | correct: 29 rows, types, ids, names, callbacks and initial values equal |
| 21 | cat_utyGameQuitToMainOrStats 0x005ab5b0 + its univUpdate call site | correct |
| 22 | utySinglePlayerOptions 0x005aa580 | correct |
| 23 | bobInitProperties 0x00425ca0 | correct: a 5-byte jmp 0x425c90 |
| 24 | mrRegionProcess 0x00567930 key events | correct (0x3b Focus, 0x5a Escape, 0x6b Skip_Sensors) |
| 25 | cameraControl / smViewportRender key ids | correct (0x45 / 0x46, 0x41..0x44) |
| 26 | 0x004aca90 / 0x004acad0 / 0x004acb90 (objdump) | correct |
| 27 | utyShowCredits 0x005aa160 / utyGameQuitToMain 0x005ab6b0 review entries | WRONG (finding 3) |

## 4. Findings

1. Report: "utyOptionsList is the exe's table 0x008c1478 .. 0x008c18e0 row for row (95 rows". Wrong count: 0x008c18e0 is
   the terminator; the table has 94 rows ((0x008c18e0 - 0x008c1478) / 12; a dump gives 94 names). Fixed in the report.
2. Report and ProcessCommandLine's delta-map note: "the exe's 63-row table 0x008b8870". Wrong count: the dump gives 64
   rows (57 switches, 7 section rows; the NULL parameter of the terminator is at 0x008b8e74, row 64). Fixed in both.
3. delta_map.json: `utyShowCredits` and `utyGameQuitToMain` (IDENTICAL) carry "[review: M2-R6 startup-keys] compared:
   identical", yet the exe plays track 0x29 in both (`soundEventPlayMusic(0x29)`) where HW1 plays `RollCredits` (28,
   SoundMusic.h), and this port changed both bodies to `CAT_MUSIC_CREDITS`. They are modified IDENTICALs. Notes
   corrected; the report's "one found modified" now says three.
4. Edited IDENTICAL functions without a startup-keys review entry: `setupPixelFormat` 0x00574550 (the window title
   "Emergence", render.c) and `rndBackgroundRender` 0x00574b90 (btgRender now also needs `catRndDrawBTG`; its R5 note
   still said the byte 0x0087816e is "never cleared", which the console's BTG row now can do). Entries appended;
   counts.reviewed unchanged (both were already reviewed).

No other finding: every Cataclysm change in the diff carries a "CAT:" comment (Task.c's accessor is marked HWI); every
added CAT-TODO is on one line, owner first (12 new / re-targeted); no CAT-TODO / CAT-PROVISIONAL naming the M2-R6 key
bindings / console / start-up / HUD-messages owners is left in src/; no src/ change falls outside the producer's scope;
the R2 workaround list stays empty. Note (not a finding): the start-position shuffle of gameStart is deferred with a
reason (it moves stream 0 in Mission01 and its readers are the level loader's), as a one-line CAT-TODO(stage 6 level
load).

## 5. DONE WHEN, re-checked

| item | verdict |
|---|---|
| default_keys.cfg loads and saves round-trip through the Cataclysm key-binding code (/catKeysTest, mode keys) | met (PASS, ASan 0; output equals the GOG game's keys.cfg) |
| /catConsoleTest runs the console's command table headless, ASan 0 | met |
| start-up as the exe's main, R2 workaround list empty | met (plain harness only) |
| options file Cataclysm.cfg, tutorial and intros off | met |
| fe and sp unchanged | met (fe: same loads but the file name; sp: milestone, ASan 0, same UBSan kinds, +console.fib) |
| Mission01 stream IDENTICAL to R5's 645f94c69d68319e | met (debug and release) |

## 6. Precision and fixes

Sampled checks: the 27 bodies / tables above, plus the report's claims checked by runs or data: the build (0
warnings), layout32, keys PASS, console PASS, fe, sp, mission IDENTICAL (debug, release), keys.cfg vs GOG, the options
file, the CAT-TODO format, the options-table row count, the command-line table row count, the two render.c review
entries = 40 checks; wrong: 4 (findings 1-4). Precision = 36 / 40 = 0.90.

Fixes in place: 4 (docs/m2-r6-startup-keys.md three "[audit: ...]" edits plus a note on the added entries;
ghidra/delta_map.json five review notes: ProcessCommandLine, utyShowCredits, utyGameQuitToMain, setupPixelFormat,
rndBackgroundRender). No src/ change; the build stays green.

Verdict: PASS-WITH-FIXES - every DONE WHEN item met on the plain harness (keys and console PASS with ASan 0, Mission01 IDENTICAL to R5, fe/sp unchanged, keys.cfg equal to the GOG game's); four factual errors in the report and the delta map's review notes fixed
