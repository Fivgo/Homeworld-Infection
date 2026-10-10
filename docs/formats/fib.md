<!-- done: fib -->
# Front-end screens (`.fib`, FEMan)

M2-R1 item 2. Sources: Cataclysm `feScreensLoad` 0x004744e0, `feStartup` 0x00474130 and the `.data` limits at
0x0087be0c (decompiled.c of `ghidra/exports/cataclysm_gog_r5`, `objdump -s`), the `.fib` path strings in
`strings.csv`; HW1 `ORIG@ad1bb82:src/Game/FeFlow.h:30-240` (`FIB_Identify`, `FIB_Version`, `fibfileheader`,
`fescreen`, `felink`, `featom`, `FA_*`, `FAF_*`) and `FeFlow.c` (`feScreensLoad`, `feScreenEntryAdd`, `feStartup`,
limits at :63-73). Checked against all 22 `.fib` files (21 in `original/game_extracted/cataclysm/feman/`, one newer
`multiplayer_game.fib` in `update/feman/`) with `tools/formats/fib_probe.py`.

## Summary

* **The `.fib` format is unchanged.** Ident `"Bananna"`, version 0x212, 12-byte header, 20-byte screens, 12-byte links,
  76-byte (0x4C) atoms, all pointers stored as file offsets. Cataclysm's `feScreensLoad` walks exactly these strides
  (`iVar5 += 0xc` per link, `+= 0x4c` per atom, screen pointer `+= 20`) and does the same fix-ups as HW1. Neither build
  checks the ident or version in release (`FEF_ERROR_CHECKING` 0, FeFlow.h:30).
* **The loader logic is HW1's, inlined.** Same 2-point-to-width/height conversion (with the
  `LW_BarWidth + LW_WindowXBarSpace` = 18 correction for `FA_ListWindow`), same "menu items present / HyperspaceRollCall"
  test that disables re-centring, same 640x480 on-screen test and centring by `(MAIN_WindowWidth-640)/2`,
  `(MAIN_WindowHeight-480)/2`, same pData/attribs rules (radio buttons keep raw pData, text-bearing non-bitmap atoms
  become `FA_StaticText`, static text attribs -> `frFontRegister`, bitmap buttons -> offset fix-up), same de-duplicating
  `feScreenEntryAdd` (inlined). Only addition: three `Sleep(0)` calls around the load.
* **Same limits:** 400 callbacks, 150 draw callbacks, 150 screens (`.data` 0x0087be0c = 0x190, 0x96, 0x96; HW1
  `FE_NumberCallbacks`/`FE_NumberDrawCallbacks`/`FE_NumberScreens`).
* **What changed is content, not format:** new screen files for the Beast (`Beast_Construction_Manager.fib`,
  `Beast_Launch_Manager.fib`, `Beast_Research_Manager.fib`), the console (`console.fib`), and Cataclysm's own versions of
  the HW1 screens; the callbacks they name are Cataclysm functions (the game registers its own callback tables).
* **Atom type 0 occurs in the files** (3,354 of 6,605 atoms in the 22 files): 542-of-765 in `front_end.fib` + `in_game_esc_menu.fib`
  carry pData and a font (text, turned into `FA_StaticText` at load), the rest are plain regions. HW1's enum starts at
  `FA_UserRegion = 1`, so 0 is an editor default, not a new type. Types seen: 0..19, nothing beyond HW1's 21.

## On-disk layout (unchanged from HW1)

```c
typedef struct fibfileheader {      /* 12 bytes */
/*00*/ char  identify[8];           /* "Bananna\0" */
/*08*/ uword version;               /* 0x212 */
/*0A*/ uword nScreens;
} fibfileheader;                    /* followed by fescreen[nScreens] */

typedef struct fescreen {           /* 20 bytes */
/*00*/ udword oName;                /* file offset -> char* */
/*04*/ udword flags;
/*08*/ uword  nLinks;
/*0A*/ uword  nAtoms;
/*0C*/ udword oLinks;               /* -> felink[nLinks], 4-byte aligned */
/*10*/ udword oAtoms;               /* -> featom[nAtoms], 4-byte aligned */
} fescreen;

typedef struct felink {             /* 12 bytes; names fixed only when flags & FL_Enabled */
/*00*/ udword oName; /*04*/ udword flags; /*08*/ udword oLinkToName;
} felink;

typedef struct featom {             /* 76 bytes */
/*00*/ udword oName;                /* callback / control name, 0 = none */
/*04*/ udword flags;                /* FAF_*, FAM_* (justification, drop shadow) */
/*08*/ udword status;
/*0C*/ ubyte  type;                 /* FA_* (0 = text/plain in the files) */
/*0D*/ ubyte  borderWidth;
/*0E*/ uword  tabstop;
/*10*/ color  borderColor;
/*14*/ color  contentColor;
/*18*/ sword  x, loadedX;           /* file: x1; loadedX overwritten at load */
/*1C*/ sword  y, loadedY;           /* file: y1 */
/*20*/ sword  width, loadedWidth;   /* file: x2 (width = x2 - x1 at load) */
/*24*/ sword  height, loadedHeight; /* file: y2 */
/*28*/ udword oData;                /* text or type-specific data (radio buttons: not an offset) */
/*2C*/ udword oAttribs;             /* font name (static text) or bitmap name (bitmap button) */
/*30*/ char   hotKeyModifiers;
/*31*/ char   hotKey[5];            /* FE_NumberLanguages */
/*36*/ char   pad2[2];
/*38*/ udword drawstyle[2];
/*40*/ void  *region;               /* set to NULL at load */
/*44*/ udword pad[2];
} featom;
```

## Loader control flow (`feScreensLoad` 0x004744e0)

1. `fileLoadAlloc` the whole file (with `Sleep(0)` before and after).
2. For each screen (20-byte stride): fix `name`, `links`, `atoms`; for each enabled link fix `name` and `linkToName`.
3. Scan atoms for `FA_MenuItem` (type 11) or a screen named `HyperspaceRollCall`; if found, the screen is not re-centred.
4. For each atom (0x4C stride): `region = NULL`; convert x2/y2 to width/height (ListWindow minus 18); copy to the
   `loaded*` fields; unless menu items: background atoms (`FAF_Background` 0x40000) -> `feResRescaleBackground`, others
   -> mark `FAF_Hidden` (0x8000) when off the 640x480 screen, then add the centring offset; fix `name`; fix `pData`
   unless radio button and turn text-bearing non-bitmap atoms into `FA_StaticText`; register static-text fonts, fix
   bitmap-button attribs.
5. Append the screen to the screen list if not already there (`feScreenEntryAdd`, inlined); return the header.

## Diff vs HW1

| Field / key / branch | HW1 (`ad1bb82`) | Cataclysm | Evidence |
|---|---|---|---|
| Ident / version | "Bananna" / 0x212, checked only with `FEF_ERROR_CHECKING` | same, never checked | FeFlow.h:40-41; feScreensLoad 0x004744e0; probe |
| Header / screen / link / atom size | 12 / 20 / 12 / 76 | same | feScreensLoad strides; probe 22/22 OK |
| Rectangle conversion, ListWindow -18 | yes | yes | `-0x12 - x` for type 0x10 |
| Menu-item / HyperspaceRollCall no-recentre | yes | yes | `*pcVar6 == '\v'`, `strcmp(name, "HyperspaceRollCall")` |
| On-screen test + centring | `feAtomOnScreen`, `feResRepositionX/Y` | inlined: 0..639 x 0..479 corners, `+(MAIN_WindowWidth-640)/2`, `+(MAIN_WindowHeight-480)/2` | feScreensLoad |
| pData / attribs fix-ups | yes | same | type 12 radio, 2 static text, 0x11 bitmap button |
| Screen registration | `feScreenEntryAdd` (dedupe) | inlined, same | list at 0x00a3e150, count 0x00c40498 |
| `Sleep(0)` | no | 3 calls | feScreensLoad |
| Limits | 400 / 150 / 150 | 400 / 150 / 150 | `.data` 0x0087be0c |
| Screen files | Construction/Launch/Research Manager, Game Chat, Horse Race, MP Game, LAN Game, Trader Interface, TaskBar, Hyperspace Roll Call, front end | + `Beast_Construction_Manager`, `Beast_Launch_Manager`, `Beast_Research_Manager`, `console`, `Choose_Server`, `single_player_objective`, `CSM-All`, `In_game_ESC_menu`, `Sensors_manager` named in the exe | strings.csv; ORIG `#define *_FIBFile` |

## Probe

`tools/formats/fib_probe.py [-v] FILE.fib...`:

```
OK   screens=26 links=52 atoms=899 fonts=2 non2point=0  ./cataclysm/feman/front_end.fib
OK   screens=41 links=39 atoms=1869 fonts=2 non2point=0  ./cataclysm/feman/multiplayer_game.fib
OK   screens=1 links=0 atoms=54 fonts=2 non2point=0  ./cataclysm/feman/beast_construction_manager.fib
OK   screens=41 links=39 atoms=1941 fonts=2 non2point=0  ./update/feman/multiplayer_game.fib
summary: 22 files, 22 ok, 0 fail; atom types {0: 3354, DecorativeRegion: 1314, Button: 705, RadioButton: 335,
UserRegion: 270, CheckBox: 206, TextEntry: 111, ListWindow: 76, CutoutRegion: 74, HorizSlider: 52, BitmapButton: 33, ...}
```

## Open points

* The callback tables Cataclysm registers (`feCallbackAddMultiple`/`feDrawCallbackAddMultiple` callers) were not
  diffed against HW1's; a screen atom whose name has no registered callback is silently inert, so that diff matters for
  the UI round, not for the file format.
* `front_end_demo.fib` is not named in the exe strings found here; whether it is loaded at all is UNVERIFIED.
