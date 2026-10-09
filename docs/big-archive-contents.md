# Cataclysm `.big` archives: format and contents

Covers `original/game/cataclysm.big` (76,864,402 bytes) and `original/game/update.big` (244,405 bytes,
the 1.01 patch overlay) from the GOG "Homeworld: Emergence" install. Written 2026-10-08.

**Reading this document.** Every claim cites what backs it:
- `BigFile.c:NNN` and other bare paths are in `reference/HomeworldSDL/src/` (the Homeworld 1 SDL port) unless
  another prefix is given.
- `CET:` means `reference/CataclysmEngineTweaks/`.
- `[data]` means the extracted archive data, measured with `tools/bigfile.py` or a short script over
  `original/game_extracted/`.
- `[exe 0xADDR]` means an `objdump -d` / `strings` reading of `original/game/cataclysm.exe`. Function
  identities there are inferred from strings and call shapes; nothing has been symbolized yet.
- Anything else is labelled **UNVERIFIED**.

Extracted copies, byte-identical to the reference C extractor:
- `original/game_extracted/cataclysm/` holds 16,528 files. Paths keep the archive's spelling, which is all lowercase.
- `original/game_extracted/update/` holds 39 files with mixed-case paths, such as `RACE_SECT/sCarrier.shp`.

To get the file the game actually sees, read `update/` first (case-insensitively) and fall back to
`cataclysm/`. Section 1.7 has the full precedence rules.

---

## 1. The RBF1.23 archive format

### 1.1 Layout

| offset | size | field | source |
|---|---|---|---|
| 0 | 7 | ASCII `RBF1.23` with no NUL. It is `BF_FILE_HEADER "RBF"` + `BF_VERSION "1.23"` | BigFile.h:67,76; written by `bigHeaderWrite` BigFile.c:481-488; checked by `bigHeaderVerify` BigFile.c:499-511 |
| 7 | 4 | `int numFiles` (LE) | `bigTOCRead` BigFile.c:404 |
| 11 | 4 | `int flags`. Bit 0 is `BF_FLAG_TOC_SORTED` | BigFile.c:405, BigFile.h:79 |
| 15 | 32 x numFiles | TOC, an array of `bigTOCFileEntry` | BigFile.c:418, BigFile.h:86-102 |
| ... | ... | one blob per file: encrypted name, `\0`, then stored data | BigFile.c:1557-1617 (fast-create), 2318-2322 (extract) |

`bigTOCFileEntry` uses MSVC/x86 natural alignment and is 32 bytes. In both archives every pad byte holds the
value shown below [data]:

| off | type | field | notes |
|---|---|---|---|
| 0 | u32 | `nameCRC1` | CRC-32 of the first half of the normalized name (see 1.3) |
| 4 | u32 | `nameCRC2` | CRC-32 of the second half |
| 8 | u16 | `nameLength` | name length without the NUL. Followed by 2 pad bytes, `00 00` in both archives |
| 12 | u32 | `storedLength` | bytes in the archive |
| 16 | u32 | `realLength` | bytes after expansion |
| 20 | u32 | `offset` | absolute offset of the name blob. Data starts at `offset + nameLength + 1` (BigFile.c:2595) |
| 24 | u32 | `timeStamp` | Unix `st_mtime` of the source file (BigFile.c:1554). Used for the disk-override rule (1.7) |
| 28 | u8 | `compressionType` | 0 means raw, 1 means LZSS. Followed by 3 pad bytes, `C9 CA CB` in every entry of both archives |

Errata in the bundled community spec
(`reference/HomeworldSDL/documentation/File Formats/BIG Specification.txt`):
- `name_size` is a u16 plus 2 pad bytes, not a `ulong`.
- `header_unknown`, which that spec says "is always 1", is the TOC-sorted flag.

The Relic addendum in the same file confirms that `toc_unknown[1..3]` is compiler padding.

A Homeworld Remastered variant also exists. `bigTOCFileEntryRemaster` (BigFile.h:104-121) adds a `u32` before
`compressionType`, which makes 36-byte entries. `bigTOCRead` only switches to it when `numFiles == 13887`
(BigFile.c:416-441). `tools/bigfile.py` detects the layout structurally instead: the smallest `offset` must
equal `15 + numFiles*entrySize`.

### 1.2 Name encryption

The name blob is a running XOR (`bigFilenameDecrypt`, BigFile.c:684-692; encrypt at 668-678):
- The mask starts at 213 (`0xD5`).
- Each output byte is `cipher ^ mask`.
- The next mask is that plaintext byte.
- The trailing NUL is not encrypted.

### 1.3 Name hash (the CRC64)

To hash a name, the game first lowercases it, converts `/` to `\`, and collapses runs of `\`
(`filenameSlashMassage`, BigFile.c:104-148, applied at 569-573). It then splits the name at `h = len/2`.

The two halves are hashed with `crc32Compute` (ThirdParty/CRC/CRC32.c:91-104). That function is the standard
reflected CRC-32 with init `0xFFFFFFFF` and a final NOT, so it equals `zlib.crc32`.

There are two split rules:
- **HW1 "buggy" split** (BigFile.c:579-581): `crc2 = CRC(name[h : h+h])`. For odd-length names the last
  character is ignored. The comment at BigFile.c:575-595 says shipped HW1 archives depend on this.
- **Fixed split** (BigFile.c:596-600, also used by the fast-create path at 1480-1484): `crc2 = CRC(name[h:])`.
  It is tried second.

**Cataclysm uses the buggy split** [data]:
- In `cataclysm.big`, all 16,528 entries match it. 7,635 of those have odd-length names and match only the
  buggy split. The other 8,893 match both rules.
- In `update.big`, all 39 entries match it (16 buggy-only, 23 both).
- Neither archive has duplicate CRC pairs or duplicate case-insensitive names.

### 1.4 TOC ordering and lookup

Both archives have `flags = 1`, and their TOCs really are sorted ascending by `(crc1, crc2)` [data]. With that
flag set, `bigTOCFileExistsByCRC` binary-searches on the CRC pair alone (BigFile.c:616-631). An unsorted TOC
gets a linear scan that also compares `nameLength` (BigFile.c:637-650).

Fast-create writes the data in file-list order and sorts only the TOC afterwards (BigFile.c:1398-1630 and 1699).
That matches Cataclysm: `bigcontentsculled.txt` in the archive root has 16,528 lines. After normalizing case and
slashes it is exactly the archive's file list in data-offset order [data]. It fits Relic's
pipeline of a contents list, culled by `bigproc.pl`, then fed to `biggie -f Homeworld.big @BigContents.txt`
(`reference/gardens-of-kadesh/zarchive/original-release-snapshot/tools/batch/BigMake.btm:85`, `bigproc.pl:1-15`).

### 1.5 Compression (LZSS)

The compressor is Nelson and Gailly's *Data Compression Book* LZSS (ThirdParty/LZSS/LZSS.c header comment,
constants at :43-52):

- The window is 4,096 bytes (`INDEX_BIT_COUNT 12`). `LENGTH_BIT_COUNT 4`, and `BREAK_EVEN = (1+12+4)/9 = 1`.
- The bit stream is MSB-first with no byte alignment (BitIO.c:356-392 for buffers, 198-235 for files).
- Each token starts with one flag bit:
  - **1** means a literal: the next 8 bits are the byte.
  - **0** means a match: a 12-bit window index, then a 4-bit length.
- An index of 0 is `END_OF_STREAM`.
- A match copies `len4 + BREAK_EVEN + 1` bytes, which is 2..17 (`for (i = 0; i <= match_length; i++)`,
  LZSS.c:455-456). It reads from `window[(index+i) & 4095]` and writes each byte back at the cursor, so
  overlapping copies repeat bytes.
- The window cursor starts at **1** (LZSS.c:442).
- `window[]` is a never-cleared global (LZSS.c:65). Every stream in both archives decodes identically from a zero
  window, so no stream depends on leftover window contents. This follows from the byte-identical biggie
  comparison in section 2.

The game checks two things after expanding:
- the expanded size equals `realLength`;
- the bytes consumed in bit mode equal `storedLength` (`dbgAssertOrIgnore`, BigFile.c:2621-2622 and 2663-2664).

`tools/bigfile.py` checks both. **Every compressed entry in both archives passes both checks** [data].

Compression is per file. If a file's compressed/real ratio exceeds `MINIMUM_COMPRESSION_RATIO 0.95`
(BigFile.c:44, 1581-1587), fast-create stores it raw instead. That matches the data:
- `cataclysm.big`: 16,081 of 16,528 entries are compressed, and the worst compressed ratio is 0.9496.
- The 447 raw entries are mostly `.mex` (280), `.jpg` (76) and `.lif` (61).
- Every raw entry has `stored == real`.
- `update.big`: all 39 entries are compressed [data].

### 1.6 Cataclysm vs HW1: same format

- **Header and version are the same.** `cataclysm.exe` holds `"RBF"` and `"1.23"` and formats them with `"%s%s"` in
  its header check [exe 0x425330-0x425344, strings at 0x7514a4/0x7514a8/0x75149c].
- **TOC entries are 32 bytes.** The Cataclysm open routine sizes the TOC read as `numFiles << 5`
  [exe 0x425418 `shl eax,0x5`]. It reads `numFiles` and `flags` as two 4-byte values first
  [exe 0x4253ee-0x425407].
- **The archive pair is opened as main plus update.** The game calls `0x425260("Cataclysm.big", "Update.big")`
  [exe 0x5abb21-0x5abb32]. This is a two-argument open, not HW1-SDL's `bigFilePrecedence[]` array
  (BigFile.c:72-91). It matches the strings `"Compared %d filesystem files to main & update bigfiles."` and
  `"%d files found newer than main bigfile in filesystem."` / `"...newer than update bigfile..."` in the exe, which suggests the original
  Relic `bigOpen(main, update)` API (inferred).
- **`Extra.big` appears to be unused.** The exe has the string `Extra.big` right after `Update.big`
  [strings 0x4c0f44], but no absolute reference to it exists anywhere in the image. Its use is **UNVERIFIED**
  and probably dead. No `Extra.big` ships.
- **Name hash:** the buggy split (1.3).
- **Compression:** the same LZSS. `tools/bigfile.py` output is byte-identical to the HW1 C `biggie` for
  every file (section 2).

No version or hash difference needed special handling. The only variant the tool supports beyond HW1 is
the 36-byte Remaster layout.

### 1.7 Precedence: update archive, main archive, and loose files on disk

This is how HW1-SDL does it:

1. **Startup.** `bigOpenAllBigFiles` (BigFile.c:2505-2573):
   - Opens `Update.big` (optional) and then `Homeworld.big` (required), in that precedence (BigFile.c:72-91).
   - Verifies the header and reads the TOC.
   - Zeroes a per-entry `localFileRelativeAge[]`.
   - Calls `bigFilesystemCompare(fileOverrideBigPath, "")` (2570).
2. **The newer-file-on-disk check.** `bigFilesystemCompare` (BigFile.c:2703-2867):
   - Does nothing unless `CompareBigfiles && !IgnoreBigfiles` (2721-2724).
   - Otherwise it recursively walks the override directory. For each loose file whose relative path is in a TOC,
     it marks `LOCAL_FILE_IS_NEWER` when `toc.timeStamp < st_mtime`, and `LOCAL_FILE_IS_OLDER` otherwise
     (2828-2832).
3. **Lookup.** `bigFindFile` (BigFile.c:2869-2891) walks the archives in precedence order. It returns the first
   archive that contains the name and whose local copy is **not** newer (2882). If no archive qualifies, it
   returns FALSE and the caller loads from the filesystem.
4. **Callers.**
   - `fileLoadAlloc` and `fileLoad` try `bigFindFile` first and fall back to the filesystem
     (File.c:837-857 and 918-946).
   - `fileOpen` in the SDL port checks for an existing local file first and only then consults the archives
     (File.c:1186-1194). That path ignores timestamps.
5. **Defaults.**
   - `CompareBigfiles` is FALSE in `HW_BUILD_FOR_DISTRIBUTION` builds and TRUE otherwise (BigFile.c:54-60). Its
     `/compareBigfiles` switch is commented out (SDL/main.c:822).
   - `/ignoreBigfiles` (main.c:643) and `/bigoverride <path>` (main.c:633) exist.
   - The override path defaults to the data directory (SDL/utility.c:3586-3606).
6. **Multiplayer CRC.** Each opened archive contributes `crc32(TOC entries)` to `HomeworldCRC`
   (`bigTOCCRC` BigFile.c:467-470, `bigCRC` 2916-2935, SDL/utility.c:3825-3833). As an inference, changing
   either archive's TOC therefore changes the network version check.

For Cataclysm, the strings `"Scanning for newer files in %s"`, `"Compared %d filesystem files to main & update bigfiles."`
and `"%d files found newer than main bigfile in filesystem."` are present, but nothing in the image references
them directly [exe search]. They may be compiled-out debug messages.

**Whether retail Cataclysm runs the timestamp compare, and whether its `fileOpen` prefers loose files, is
UNVERIFIED.** It needs disassembly of the file layer. `cataclysm.exe` has none of the HW1-SDL option strings
`/ignoreBigfiles`, `/bigoverride` or `/compareBigfiles` [strings]. A global at `0x8df9fc` short-circuits the
open routine and returns success [exe 0x4252cb-0x4252e7]. It is probably the "ignore bigfiles" flag, but that is
**UNVERIFIED**.

Why this matters: the GOG install ships four **loose files that shadow archive entries**. All are dated
2017-06-23 and all differ from the archive copies, which are dated July 2000 [data]:
- `feman/texdecorative/a_main_title_{left,mid,right}.LiF` are valid `Willy 7` LiFs of the same sizes as the
  archive copies.
- `credits/english/01_title.jpg` is 42,227 bytes, against 26,918 bytes in the archive.

These are presumably the "Emergence" retitle art (**UNVERIFIED**, not rendered). They can only take effect
through one of the loose-file paths above.

---

## 2. `tools/bigfile.py`

The tool is stdlib-only Python 3. Each part of it cites the C it ports.

```
tools/bigfile.py info    <archive>                       # header, flags, entry size, sort check, hash-split census
tools/bigfile.py list    <archive> [--crc] [--time] [--by-offset]   # TSV: offset stored real comp [crc64] [time] name
tools/bigfile.py extract <archive> <outdir> [glob ...] [--jobs N] [--quiet]
tools/bigfile.py cat     <archive> <name>               # lookup by CRC exactly like the game
```

- Globs use `fnmatch`, are case-insensitive, and match against both the `\` and `/` spellings.
- Extraction is parallel and restores each file's mtime from the TOC timestamp. It refuses `..` paths.
- Extraction runs the two LZSS checks from 1.5 and exits non-zero on any failure.

**Verification:**

1. **Integrity.**
   - `extract` on `cataclysm.big`: 16,528 of 16,528 files, 0 problems. It runs in about 2 s on 24 cores.
   - `update.big`: 39 of 39 files, 0 problems.
   - Both pass `expanded == realLength` and `consumed == storedLength` for every compressed entry.
2. **Comparison with the reference C.**
   - I ran the prebuilt HW1 `biggie -x` (`reference/build-hwsdl/tools/biggie/biggie`, built from the same
     `BigFile.c`, `LZSS.c` and `BitIO.c`) on symlinks of both archives in a scratch directory.
   - `diff -r` against `original/game_extracted/{cataclysm,update}` reports **no differences in any of the
     16,567 files**.
   - biggie's CRC column also matches the tool's (`%08x%08x` of crc1, crc2).
3. **Text check.**
   - `english.script` begins `; English language script file`.
   - `race_sect/scarrier.shp` begins `[sCarrier]`, followed by `LODFile sCarrier.lod` and physics keys.
   - `tweak.script` begins `[Tweak]`.
   - All are readable CRLF text.
4. **Magic and version check.** Extracted headers were compared with what the HW1 loaders expect:

| ext | count | header observed [data] | HW1 loader expects | result |
|---|---:|---|---|---|
| `.lif` | 12,096 | `"Willy 7\0"`, version 0x104 | `LIF_FileIdentifier "Willy 7"`, `LIF_FileVersion 0x104` (SDL/texreg.h:115-116; check texreg.c:1571-1578) | match. Cataclysm checks the same ident and version [exe 0x587df9-0x587e13 `cmp ...,0x104`] |
| `textures.ll` | 1 | `"Event13\0"`, 0x104 | `LL_FileIdentifier`/`LL_FileVersion` (texreg.h:110-111; texreg.c:2702-2706) | match |
| `.geo` | 967 | `"RMF97ba\0"`; version **0x500** for 966 files, 0x402 for 1 | `MESH_FileID "RMF97ba"` / `MESH_FileID2 "RMF99ba"`, **`MESH_Version 0x402`** (Mesh.h:51-54; fatal on mismatch at Mesh.c:915-918 when `MESH_ERROR_CHECKING`) | **ident matches, version differs** |
| `.peo` | 762 | `"RMF99ba\0"` with version 0x500 for 761 files; 1 file is `"RMF97ba\0"`/0x402 | same constants. `RMF99ba` sets `newFormat` (Mesh.c:921-928) | **version differs**. Cataclysm's mesh loader reads a 0x44-byte header and only `strncmp`s `RMF99ba` to choose the format, with no version check [exe 0x4aef44-0x4aef86] |
| `.mad` | 29 | `"Mellonn\0"`, 0.91f | `MAD_FileIdentifier`/`MAD_FileVersion 0.91f` (MeshAnim.h:36-37) | match |
| `.mex` | 967 | `"mannngo\0"`, u16 version 0x106 | ident `"mannngo"` (MEX.c:30); header `{char[8], u16 version, u16 nChunks}` (MEX.h) | match |
| `.fib` | 21 | `"Bananna\0"`, u16 0x212 | `FIB_Identify`, `FIB_Version 0x212` (FEFlow.h:40-41; header FEFlow.h:150-156) | match |
| `.hff` | 102 | `"Orannge\0"`, 0x100 | `FONT_Identification`, `FONT_Version 0x100` (SDL/font.h:46-47) | match |
| `.nis` | 14 | `"Crannnberry\0"`, 0x101 | `NIS_Identifier`, `NIS_VersionNumber 0x101` (NIS.h:64-65) | match |
| `.btg` | 46 | first u32 = 0x600 | `BTG_FILE_VERSION 0x600` (BTG.h:14) | match |
| `.hsf` | 34 | `"rrasberry"`, version 0x100 | `HSF_FILE_ID "rrasberry"` (Light.h:54) | match |
| `.dem` | 1 | `"BaNng"` | `DEM_HeaderString` (Demo.h:50) | match |

The only format change found against HW1 is mesh **version 0x500**. The ident and the 68-byte header size are
unchanged, but what else changed inside a v0x500 mesh is **UNVERIFIED**.

---

## 3. Archive summaries

| | `cataclysm.big` | `update.big` |
|---|---|---|
| numFiles | 16,528 | 39 |
| flags | 1 (CRC-sorted; verified sorted) | 1 (verified sorted) |
| entry size | 32 | 32 |
| compressed | 16,081 | 39 |
| stored total | 75,559,532 | 241,947 |
| real total | 128,441,214 | 710,998 |
| TOC timestamps (UTC) | 2000-06-22 05:15 to 2000-08-25 07:04 | 2000-11-23 00:00 to 2000-12-21 20:11 |
| name case | 100% lowercase | mixed (`RACE_SECT\sHiveFrigate.shp`) |
| longest name | 73 chars (limit `BF_MAX_FILENAME_LENGTH 128`, BigFile.h:78) | |
| trailing bytes after last blob | 0 | 0 |

Several things in the root are neither scripts nor game data:
- `bigcontentsculled.txt` is the build list (1.4).
- `kver.kp` is the WON verifier key (`VERIFIER_KEY_FILE_NAME "kver.kp"`, SDL/TitanInterface.cpp:162).
- `statfile.bin` is the fight-stats table (Stats.c:1106-1132).
- `textures.ll` is the LiF listing (texreg.h:109).
- `plug.tga` is also present.
- `demos\homeworld.bat` is a leftover HW1 E3 demo launcher that runs `\Homeworld\EXE\Homeworld.exe ... /demoplay=demos\E3DEMO.dem`.
- `fonts\fontusage.doc` is also a leftover.

### 3.1 Files by top-level directory (`cataclysm.big`)

| top-level dir | files | real bytes | stored bytes |
|---|---:|---:|---:|
| `race_beast\` | 2711 | 16,916,512 | 9,129,476 |
| `race_sect\` | 2540 | 16,402,255 | 8,284,583 |
| `race_taidan\` | 2348 | 9,336,696 | 5,722,224 |
| `race_derelict\` | 2293 | 13,553,000 | 7,797,543 |
| `race_kushan\` | 1702 | 7,530,118 | 4,298,929 |
| `race_traders\` | 1291 | 7,753,293 | 4,682,225 |
| `etg\` | 822 | 5,815,322 | 2,429,382 |
| `feman\` | 595 | 7,628,928 | 3,419,998 |
| `race_p1\` | 525 | 2,969,610 | 1,744,649 |
| `multiplayer\` | 297 | 581,792 | 200,922 |
| `singleplayer\` | 254 | 6,095,161 | 5,839,987 |
| `resources\` | 248 | 1,124,861 | 916,911 |
| `researchgui\` | 107 | 5,933,072 | 2,511,628 |
| `fonts\` | 104 | 2,786,128 | 551,814 |
| `credits\` | 97 | 1,575,520 | 1,546,968 |
| `derelicts\` | 95 | 1,687,739 | 1,176,796 |
| `btg\` | 87 | 2,845,449 | 978,743 |
| `(root)` | 72 | 2,604,812 | 640,498 |
| `speechicons\` | 54 | 952,864 | 711,834 |
| `overlayicons\` | 50 | 217,867 | 108,483 |
| `misc\` | 46 | 113,467 | 51,641 |
| `cursorbitmaps\` | 34 | 71,264 | 27,872 |
| `hsf\` | 34 | 5,124 | 3,378 |
| `nis\` | 28 | 540,537 | 145,969 |
| `screenshots\` | 24 | 3,518,629 | 3,443,048 |
| `soundfx\` | 22 | 9,220,894 | 8,831,463 |
| `movies\` | 19 | 49,896 | 10,112 |
| `tutorials\` | 17 | 218,552 | 212,230 |
| `defaultship\` | 4 | 4,261 | 1,527 |
| `demos\` | 3 | 52,796 | 18,132 |
| `lensflares\` | 3 | 266,091 | 93,693 |
| `colorpicker\` | 2 | 68,704 | 26,874 |
| **total** | **16528** | **128,441,214** | **75,559,532** |

`update.big` contributes these files: `RACE_SECT\` 13, `RACE_BEAST\` 12, `multiplayer\` 11, `feman\` 1, and
2 in the root (`english.script`, `tweak.script`).

### 3.2 Files by extension (`cataclysm.big`)

| ext | files | real bytes | what it is (source) |
|---|---:|---:|---|
| `.lif` | 12096 | 74,460,964 | paletted texture (texreg.h:259-274) |
| `.geo` | 967 | 10,138,214 | mesh, `RMF97ba` v0x500 (2) |
| `.mex` | 967 | 1,129,144 | mesh extras (guns, engines, collision) (MEX.h) |
| `.peo` | 762 | 8,793,524 | mesh, `RMF99ba` v0x500. Not referenced in HW1 source; `.lod` files point at it |
| `.dist` | 322 | 52,361 | resource/asteroid distribution referenced from `.missphere` |
| `.ebg` | 255 | 1,066,987 | preprocessed (`#line 1 ...`) ETG effect script, loaded by `etg\*.script` `LoadEffect` lines |
| `.shp` | 173 | 1,630,309 | ship/object static stats (Universe.c:1577ff `strcat(shipname,".shp")`) |
| `.lod` | 172 | 210,291 | LOD script (`pMeshFileN`, `bOnN`...) (LOD.c:37-71 script table) |
| `.missphere` | 128 | 666,157 | MissionMan sphere contents (`Ships`, `AIPoint`, `Resources`...) (LevelLoad.c:109-120) |
| `.script` | 115 | 1,030,238 | tweak, text and table scripts (section 4) |
| `.hff` | 102 | 2,754,683 | font (font.h:46-47) |
| `.jpg` | 85 | 10,965,550 | loading screens, credits, screenshots |
| `.plug` | 60 | 14,195 | plug-screen (credits/ad) layout (`psScreenStart`, SDL/utility.c:2183, 3041) |
| `.btg` | 46 | 2,771,517 | background (BTG.c, BTG.h:14) |
| `.tga` | 42 | 82,148 | images |
| `(none)` | 36 | 152,091 | extensionless MissionSphere files inside `singleplayer\missionNN\` (5) |
| `.hsf` | 34 | 5,124 | lighting set (Light.h:54-61) |
| `.mad` | 29 | 136,428 | mesh animation (MeshAnim.h:36-37) |
| `.mission` | 29 | 69,415 | single-player/tutorial level script (SinglePlayer.c:400-422, Tutor.c:515) |
| `.fib` | 21 | 566,188 | front-end screen layout (FEFlow.h:40-41) |
| `.mif` | 17 | 5,568 | `texturedone.mif`, text starting `#\r\nTEXTU...`. Not in HW1 source. **UNVERIFIED** (probably a tool artifact) |
| `.lut` | 15 | 315,204 | sound and speech lookup tables (SoundEvent.c, SpeechEvent.c) |
| `.nis` | 14 | 422,867 | in-engine cinematic (NIS.h:64-65) |
| `.txt` | 14 | 839,007 | 12 MP `description.txt`, `fonts\english.txt`, `bigcontentsculled.txt` |
| `.level` | 12 | 19,097 | multiplayer map (ScenPick.c:270-281) |
| `.bnk` | 4 | 8,904,404 | sound banks (SoundEvent.c) |
| `.cred` | 2 | 482 | credits plug screens (SDL/utility.c:2183, 3089) |
| `.bat` `.doc` | 1+1 | | HW1 leftovers (above) |
| `.bin` | 1 | 192,892 | `statfile.bin` (Stats.c:1112-1132) |
| `.dem` | 1 | 52,668 | `demos\abc640x480.dem` auto-demo (SDL/utility.c:2452-2466) |
| `.kp` | 1 | 816 | `kver.kp` |
| `.list` | 1 | 11 | `demos\autodemos.list` (SDL/utility.c:2452) |
| `.ll` | 1 | 960,117 | `textures.ll` |
| `.lst` | 1 | 448 | `movies\animatics.lst` (Animatic.c:623-648) |
| `.wxh` | 1 | 756 | `soundfx\music4cat.wxh`, `WXH1` header for the on-disk `Music4Cat.wxd` (SpeechEvent.c) |

---

## 4. `.script` files (115)

The "HW1 loader" column cites where the HW1 source loads that exact name. "exe" means the name appears as a
literal in `cataclysm.exe` strings. The **configures** text comes from the file's own header or keys [data].

### 4.1 Root (67)

| file | HW1 loader / table | configures |
|---|---|---|
| `aiplayer.script` | StatScript.c:1882 `AIPlayerTweaks` | `[AIPlayer]` AI update rates per difficulty |
| `airesourceman.script` | StatScript.c:1884 `AIResourceManTweaks` | AI resource manager update rates |
| `aiship.script` | StatScript.c:1865 `AIShipTweaks` | ship navigation to points (`VELOCITY_SCALE_FACTOR`...) |
| `aitrack.script` | StatScript.c:1867 `AITrackTweaks` | velocity tracking and stabilization |
| `autolod.script` | StatScript.c:1888 `AutoLODTweaks` | automatic LOD scale adjustment |
| `battlechatter.script` | Battle.c:730 `battleTweaks` | battle chatter (speech) tweakables |
| `camera.script` | StatScript.c:1868 `CameraTweaks` | camera rotation and zoom rates |
| `cameracommand.script` | StatScript.c:1869 `CameraCommandTweaks` | camera stack and default distance |
| `cloud.script` | StatScript.c:1879 `CloudTweaks` | dust/gas cloud tweakables |
| `colpick.script` | ColPick.c:1101 `colPickScriptTable` | hue-specific team colour scalars |
| `commandlayer.script` | StatScript.c:1870 `CommandLayerTweaks` | command layer (carrier refuel/heal...) |
| `countries.script` | none in HW1; exe `countries.script` | list of country names. Its use (probably WON/online registration) is **UNVERIFIED** |
| `crates.script` | Crates.c:774 `cratesScriptTable` | crate spawn timing |
| `damage.script` | StatScript.c:1886 `DamageTweaks` | damage tweakables |
| `demomissions.script` | ScenPick.c:402 (demo builds) | demo map list. All entries are commented out. No exe literal |
| `demoresearchdepend.script` | ResearchAPI.c:996,1143 (demo builds) | demo research tree (`PG - CATACLYSM research dependancies`). No exe literal |
| `english.script`, `french.script`, `german.script` | StringSupport.c:768/773/778 `LanguageStrings` | all UI and game text. The exe also names `Italian.script` and `Spanish.script`, which are not shipped |
| `fecolour.script` | StatScript.c:1890 `FrontEndColourTweaks` | front-end colours |
| `flightman.script` | StatScript.c:1874 `FlightmanTweaks` | flight manoeuvres (flip turns...) |
| `formation.script` | StatScript.c:1871 `FormationTweaks` | formation spacing and behaviour |
| `galaxy.script` | StatScript.c:1872 `StarTweaks`; UnivUpdate.c:6301 `GalaxyTweaks` | star field (`NumStars 800`...) |
| `gametypes.script` | StatScript.c:1937 `fileOpen` | multiplayer game-type presets (`MG_*` flag bits) |
| `gatherstatsfor.script` | StatScript.c:1889 `GatherStatsScriptTable` | which races feed `statfile.bin`. Columns are `SECT BEAST R1 R2 P1` |
| `light.script` | Light.c:144 `lightScriptTable` | default light position and colour |
| `mad.script` | none; no exe literal | header comment only ("mad animations + their relation to the game"), 67 bytes |
| `mainoverlay.script` | none in HW1; exe `MainOverlay.script` | in-game overlay layout per SP/MP mode and view |
| `multiplayermissions.script` | ScenPick.c:405 | MP map list (7.3) |
| `nebulae.script` | StatScript.c:1881 `NebulaeTweaks` | nebula tweakables |
| `nettweak.script` | StatScript.c:1883 `NetTweaks` | `[NetTweak]` Titan/WON timings |
| `parade_bcarrier`, `parade_bheavycruiser`, `parade_bmothership`, `parade_bprocessor`, `parade_dprisonship`, `parade_p1mothership`, `parade_r1carrier`, `parade_r2carrier`, `parade_scarrier`, `parade_slipgate`, `parade_smothership`, `parade_sprocessor` (12 files, `.script`) | HW1 loads only 6 `Parade_*` via `ParadeInfoScriptTable` (Formation.c:2756-2761). The Cataclysm exe names all 12 | `SlotPositionInfo[SLOT_*]` military-parade slot offsets around a host ship |
| `ping.script` | Ping.c:195 `pingTweaks` | sensor ping tweakables |
| `researchdepend.script` | ResearchAPI.c:998,1145 `TechnologyDependancies` | Cataclysm research tree |
| `resource.script` | StatScript.c:1878 `ResCollectTweaks` | resource collection distances and rates |
| `sensors.script` | Sensors.c:3932 `smTweaks` | sensors manager |
| `shipnames.script` | none in HW1; exe `shipnames.script` | name pools per ship type (`ShipName sMothership Kuun-Lan`...) |
| `shipview.script` | StatScript.c:1887 `ShipViewTweaks` | ship-view camera |
| `singleplayer.script` | StatScript.c:1880 `SinglePlayerTweaks` | `[SinglePlayer]` hyperspace effect parameters |
| `soundevent.script` | StatScript.c:1876 `SoundeventTweaks` | SFX volume and voice limits |
| `subtitle.script` | Subtitle.h:73, Subtitle.c:579 `subScriptTable` | subtitle themes per actor |
| `tactical.script` | Tactical.c:116 `toIconTweaks` | tactical overlay icon shapes per class |
| `tactics.script` | Tactics.c:516 `TacticsInfotable` | aggressive/neutral/evasive tactics modifiers |
| `teamcolors.script` | Teams.h:27, Teams.c:161 `teColorTweaks` | team colour schemes |
| `trail.script` | StatScript.c:1877 `TrailTweaks` | engine trails |
| `tweak.script` | StatScript.c:1873 `Tweaks` | `[Tweak]` general gameplay tweakables |
| `unitlimitcaps.script` + `unitlimitcaps1`..`8.script` | StatScript.c:1885 and Universe.c:4426 (`"UnitLimitCaps%i.script"` by player count) `ShipStaticCapLimits` | unit caps (`ShipTotalCaps 297` + per-type caps) |
| `warp.script` | SinglePlayer.c:88 `WARP_FILENAME` | debug level warp (`WarpEnabled 0`) |

HW1 loads `Dock.script` (StatScript.c:1875), but it is **not in Cataclysm**, and the Cataclysm exe has no
literal for it.

### 4.2 Subdirectories (48)

| files | HW1 loader | configures |
|---|---|---|
| `etg\general.script` | ETG.c:762-764 | global effect `LoadEffect` list |
| `etg\race_beast`, `race_derelict`, `race_kushan`, `race_p1`, `race_sect`, `race_taidan`, `race_traders` `.script` (7) | HW1 per-race list was `R1`, `R2`, `P1`, `P2`, `P3`, `Traders.script` (ETG.c:765-770). Exe: `RACE_*.script` | per-race effect loads |
| `etg\derelict.script` | none; no exe literal | 50-byte stub (`//***DERELICT Loads*** //deltas only`) |
| `lensflares\ps-105mm-prime.script`, `ps-50-300mm-zoom.script` | none in HW1. Exe has `LensFlares\` and `enableLensFlare` | lens-flare texture atlas and fade (new in Cataclysm) |
| `movies\animatic01`..`animatic18.script` (18) | Animatic.c:721-727 (`Movies\%s.script`, names from `Movies\animatics.lst`, Animatic.c:637-648) | NIS-format event tracks for the Bink animatics. The `.bik` files are loose on disk in `original/game/Movies/` |
| `nis\n01`..`n11.script`, `nis\nislet01`..`03.script` (14) | HW1 lists race-specific NIS (`n01r1`/`n01r2`..., SinglePlayer.c:3081-3116) and builds nislet names (3272-3309). Exe: `nis\n01.script`..`nis\n11.script`, `nis\nislet01..03` | cinematic event scripts, preprocessed from `c:\cataclysm\DataSrc\nis\...` per their `#line` headers |
| `overlayicons\overlayicons.script` | none in HW1; exe `OverlayIcons.script` | popup-menu and overlay icon atlas coordinates |
| `race_beast\beastweapon.script` | none in HW1; exe `beastweapon.script` | Beast infection tweakables (`beastAttackCascadePoints`, `stunnedSpinRate`...) |
| `screenshots\shotlist.script` | HorseRace.c:602-626 | loading-screen screenshot list |
| `soundfx\scripts\eq_fleet.script`, `eq_intel.script` | SpeechEvent.c:350-351 `streamEQScriptTable` | speech EQ for Fleet Command and Fleet Intel |

The preprocessed scripts' `#line` directives name the developers' source tree: `c:\cataclysm\DataSrc\...`,
`c:\cataclysm\Src\game\speechevent.h`, and `c:\newcat\Src\game\speechevent.h`. They match the exe's
`C:\Cataclysm\src\game\btg.c` string.

---

## 5. Missions, levels and cinematics

How missions are built:
- **HW1:** mission logic is KAS. `src/Missions/SinglePlayer/Mission01..16.kas` (+ `Mission05_OEM.kas`) is
  preprocessed by `cc -E` (Missions/SinglePlayer/meson.build) and compiled into the exe. The FSM table is chosen
  per mission (SinglePlayer.c:2968-2973). The data side only holds level layouts:
  `SinglePlayer\MissionNN\MissionNN.mission` (SinglePlayer.c:400-422).
- **Cataclysm:** the archive holds **no `.kas`/`.kp`/compiled mission code**. The exe holds
  `kasfMission13DeleteEngineeringModuleHack` and `G_StopShipsFiringOnMission15`, which fits KAS still being
  compiled into the exe (inferred).
- **Cataclysm path formats** [exe strings]:
  - `SinglePlayer\Mission%02d\` + `Mission%02d.mission`
  - `SinglePlayer\Mission%02d_NIS%02d\` + `Mission%02d_NIS%02d.mission`
  - `SinglePlayer\mission%02d\loading.jpg` and `SinglePlayer\mission%02d_Nis%02d\loading.jpg`
  - `SinglePlayer\mission%02d\loading.script`. No `loading.script` ships. **UNVERIFIED** whether it is optional.

### 5.1 Single player: `singleplayer\` (28 dirs, 254 files)

Contents of each directory:
- `missionNN.mission`. It is MissionMan output: `MissionSphere` lines, `Lighting`, `Background`, `sm*`
  sensors-manager bounds, and `ExcludeShips`/`IncludeShips` per `RACE_*` (LevelLoad.c:122-191).
- `*.missphere` sphere files, `*.dist` distributions, and `loading.jpg`.
- Some missions also have extensionless MissionSphere files named in `.mission`, for example `General`,
  `Crystals`, `CaalShto`, `Taiidan` and `Points` in mission06. Mission06 also has an extensionless `taidan`
  that its `.mission` does not reference.

Titles come from each file's `; Info:` line [data]:

| dir | Info | dir | Info |
|---|---|---|---|
| mission01 | Taidan Contras Mount Attack On Higaran Space | mission01_nis01 | There Is Life After Homeworld |
| mission02 | Rescue Kith Destroyer And Find Pod | | |
| mission03 | Rescue Ecclesiam From Pirate Blockade | | |
| mission04 | (Empty) | mission04_nis02 | Genetics Module Gets Infected And Jettisoned |
| mission05 | Beast Attacks Bentusi, Station Scuttled | mission05_nis03 | Bentusi Harassed By The Beast |
| mission06 | Escourt Callisto Into Nebulae | mission06_nis04 | Into The Nebula |
| mission07 | Rescue Convoy | mission07_nis05 | Callisto Leaves |
| mission08 | Rendezvous With Spy | | |
| mission09 | Retrieve Info From Taidan Research Planet | | |
| mission10 | (Empty) | mission10_nis06 | Big Gun Derelict Found |
| mission11 | The Factorem Attacks | | |
| mission12 | Negotiations With Pirates And Help From Contras | mission12_nis07 | Pirate Outpost |
| mission13 | Knocked From Hyperspace | | |
| mission14 | (blank) | mission14_nis08 | Alien Vessel Shown With Taidan Contras Already Analyzing It |
| mission15 | Convince Bentusi To Help | mission15_nis09 | Bentusi Evacuating |
| mission16 | (Empty) | mission16_nis10 | Nomad Moon |
| mission17 | (Empty) | mission17_nis11 | Nomad Moon |

That makes 17 playable missions and 11 `_nisNN` staging levels, each with 3 files: `.mission`, one
`resourcesphere_1.missphere`, and `loading.jpg`. The NIS staging levels pair with `nis\n01..n11.nis` (inferred
from the numbering). HW1 has 16 missions.

### 5.2 Tutorial: `tutorials\tutorial1\`

It holds 17 files: `tutorial1.mission`, `tutorialsphere.missphere`, 5 more `.missphere`, 9 `.dist`, and
`loading.jpg`. HW1 loads `<name>.mission` the same way (Tutor.c:515).

### 5.3 Multiplayer: `multiplayer\` (12 maps, 297 files)

Each map directory has:
- `<name>.level`, loaded as `MultiPlayer\<name>\<name>.level` (ScenPick.c:270-281);
- `description.txt` with `Description`, `DescriptionFrench` and `DescriptionGerman` keys;
- `mothership_N.missphere`, one per start position;
- `resource.missphere` (`resourcesphere_0.missphere` for kristalzupacken4);
- 15 `.dist` files.

`multiplayermissions.script` enables 10 maps:
- Coldwar2, TheGates3, TrigsBones3, Kristalzupacken4, FerrousFaucets4, TheAnvil6, KiithKnox6, Metro8, Armed8,
  ArmedDeux8.
- `LiveFireExercise2` and 12 `Demo*2` test maps are commented out.
- `ferrousfaucets2` ships but is not listed.

| dir | `; Info:` |
|---|---|
| armed8 | Armed for Burial (8) |
| armeddeux8 | Close Quarters (8) |
| coldwar2 | Coldwar (2) |
| ferrousfaucets2 | Ferrous Faucets (2) (not in the list) |
| ferrousfaucets4 | Ferrous Faucets (4) |
| kiithknox6 | Kiith Knox (6) |
| kristalzupacken4 | Kristalzupacken [4] |
| livefireexercise2 | Live Fire Exercise (2) (commented out) |
| metro8 | Metro (8) |
| theanvil6 | The Anvil (6) |
| thegates3 | The Gates (3) |
| trigsbones3 | Trig's Bones (3) |

The original `.level` files place `RACE_SECT` `sMothership` at every start, for example coldwar2. Race
selection presumably overrides that at runtime (**UNVERIFIED**).

### 5.4 Cinematics

- `nis\`: `n01`..`n11` `.nis` + `.script`, and `nislet01`..`03` `.nis` + `.script`. All are `Crannnberry`/0x101.
- `movies\`: `animatics.lst`, which maps levels 0..17 to Animatic01..18, plus 18 animatic `.script` files.
  The videos are loose: `original/game/Movies/Animatic01..18.bik`, `sierra.bik` and `bdogintro.bik`.
- `hsf\ez01..`: lighting sets. `btg\ez01..` and `default.btg`: backgrounds. These match the `Lighting MissionNN`
  and `Background MissionNN` keys (LevelLoad.c:141-142).

---

## 6. Ships per race

The ship directories are `race_<name>\<ship>\rl0\lod0..4\` and hold `.geo`, `.peo`, `.mex` and `.lif` files.
Each ship also has `race_<name>\<ship>.shp` (stats) and `<ship>.lod` (LOD script) at the race root.

Race ids are from CET:game/Game/RaceDefs.h: Sect 0, Beast 1, Kushan 2, Taiidan 3, P1 4, Traders 5,
Derelict 6. Ship types are from CET:game/Game/ShipDefs.h:1-150, which has 145 types.

**Every one of the 145 `ShipDefs.h` types has a directory** [data]. The directories below are the full list.
Extras are flagged.

| race dir (id) | ship directories |
|---|---|
| `race_sect` (0), 35 dirs | sacolyte, sacv, scarrier, scarriersupport, sdestroyer, sdreadnought, sdreadnoughtcannon, shivefrigate, sleech, smcv, smimic, smothership, smothershiparmour, smothershipbiggun, smothershipcommand, smothershipdockingbay, smothershipengineering, smothershipengineslower, smothershipenginesupper, smothershiplowerspine, smothershipmicro, smothershiporecanarmour, smothershipspecial, smothershipsupport, smothershipweapons, smultibeamfrigate, sprocessor, srammingfrigate, srecon, srepairdroid, ssentinel, sswarmer, sworker, missile. **Extra:** `smothershipgenetics` (meshes but no `.shp`; probably the genetics module jettisoned in mission04_nis02, inferred from that level's title). The root also has `generic_ship.shp` |
| `race_beast` (1), 30 dirs | bacolyte, bacv, battackbomber, bcarrier, bcloakedfighter, bcruisemissile, bdfgfrigate, bheavycorvette, bheavycruiser, bhivefrigate, binterceptor, bionarrayfrigate, bmissilecorvette, bmothership, bmothershipdockingbay, bmothershipengineslower, bmothershipsupport, bmultibeamfrigate, bmultiguncorvette, bprocessor, brammingfrigate, brecon, brepairdroid, bsectmoship, bsentinel, bswarmer, bworker, mine, missile. **Extra:** `bmissiledestroyer` (`.peo` + textures; no `.shp`/`.lod`). Probably cut content, **UNVERIFIED** |
| `race_kushan` (2), 16 dirs | kadvancesupportfrigate, kattackbomber, kcarrier, kcloakgenerator, kgravwellgenerator, kheavycorvette, kheavycruiser, kheavydefender, kheavyinterceptor, kioncannonfrigate, kmultiguncorvette, kproximitysensor, ksalcapcorvette, kstandarddestroyer, mine, missile |
| `race_taidan` (3), 20 dirs | tadvancesupportfrigate, tattackbomber, tcarrier, tcloakgenerator, tdfgfrigate, tgravwellgenerator, theavycruiser, theavydefender, theavyinterceptor, tioncannonfrigate, tminelayercorvette, tmissiledestroyer, tmultiguncorvette, tproximitysensor, tresourcecollector, tsalcapcorvette, tstandarddestroyer, tstandardfrigate, mine, missile |
| `race_p1` (4), 7 dirs | p1fighter, p1ionarrayfrigate, p1missilecorvette, p1mothership, p1outpost, p1standardcorvette, missile |
| `race_traders` (5), 11 dirs | trcargobarge, trfloatingcity, trghostship, trjunk_lgun, trjunk_sgun, trjunkyarddawg, trjunkyardhq, trminingbase, trresearchstation, trresearchstationbridge, trresearchstationtower. `trresearchstationbridge` and `trresearchstationtower` have no `.shp` at the race root |
| `race_derelict` (6), 39 dirs | dacolyte, dbeastdockingbay, dbeastmothership, dbiggun, dbiggununit, dbuoy, dcrate, dcrystal0, dcrystal1, ddeadplanet, decclesiam, decclesiaminfected, dfirstplanet, dionarrayfrigate, dioncannonfrigate, djunk1parta, djunk1shell, djunk1strut, djunk2shipwreck, djunk3boiler, djunk3boilercasing, dkushancarrier, dmessagepod, dnisbuoy, dnomadfieldemitter, dnomadmoon, dprisonship, dprisonshipinfected, drko, dslipgatelarge, dslipgatemedium, dslipgatenode, dslipgatesmall, mine, nislensflaresource1, nislightsource1, nislightsource2. **Extras:** `dplanetoforigin` (no `.shp`), `dplasmacloud` (has `.shp`/`.lod` but no `ShipDefs.h` entry) |

Non-race object trees:
- `derelicts\`: crate, galaxy, homeworld, lavaball, mattworld, planetoforigin, planetoforigin_scarred. Each has
  `.shp` and `.lod`. This is HW1's `Derelicts\` path (Universe.c:1804).
- `resources\asteroids\asteroid0-4`, `resources\dustclouds\dustcloud0-3`, `resources\nebulae\nebula0`. These are
  HW1's `Resources\*\` paths (Universe.c:1569-1717). There is no `gasclouds\`.

---

## 7. What `update.big` (1.01) overrides

All 39 entries replace files that already exist in `cataclysm.big`. The CRC pairs are identical, and **no file is
new**. Every replacement differs from the original [data].

| file | change (old -> new) |
|---|---|
| `english.script` | `strR1TECH_MIMICinfo`: Mimics SU2 -> SU3, MCVs SU4 -> SU6. Mimic, MCV and bHeavyCorvette info text updated. Adds key bindings for ship group 10. `strCheatDetect` drops `Frame %d`. Adds `strPAUSED_TEXT`, support/web URLs (`homeworld.sierra.com/cataclysm/support/`, `www.homeworld.net`, `won.net/re/hwcatcontest.html`) and launch-manager class names. It also introduces typos: `strNIS01` becomes "Kiith Somtaaw**http** Mining Vessel", and `strNIS10` becomes "Dark Matter **F**Cluster" |
| `tweak.script` | adds `g_bNeedMoreSpaceForOtherLanguage FALSE` and `g_bNeedReverseWordsForOtherLanguage FALSE`. `tpGameCreated.aiplayerDifficultyLevel` changes from 50 (scale 0..100) to 1 (scale "0-2"). `tpGameCreated.aiplayerBigotry` changes from 50 to 2 |
| `feman\multiplayer_game.fib` | binary front-end screen, 221,684 -> 229,460 bytes |
| `multiplayer\*\description.txt` (11; all MP maps except ferrousfaucets2) | removes the trailing "Settings: default." lines. coldwar2 also fixes typos ("damgage") |
| `RACE_BEAST\bCarrier.shp` | buildCost 3400->3500; buildTime 280->300; TimeShockedByEMP 0.75->1.5; maxhealth 45000->40000 |
| `RACE_BEAST\bHeavyCorvette.shp` | maxvelocity 550->500; buildCost 250->350; buildTime 35->40; supportUnits 4->5; TimeShockedByEMP 1.25->2.0 |
| `RACE_BEAST\bWorker.shp` | maxhealth 3700->3800; resourcesAtOneTime and maxresources 500->470 |
| `RACE_BEAST\` bCruiseMissile, bDFGFrigate, bHiveFrigate, bIonArrayFrigate, bMultiBeamFrigate | TimeShockedByEMP 1.5->2.0 |
| `RACE_BEAST\` bHeavyCruiser, bProcessor | TimeShockedByEMP 0.75->1.5 |
| `RACE_BEAST\bMissileCorvette.shp` | TimeShockedByEMP 1.25->1.5 |
| `RACE_BEAST\bMultiGunCorvette.shp` | TimeShockedByEMP 1.25->2.0 |
| `RACE_SECT\sCarrierSupport.shp` | buildCost 500->300; buildTime 50->30; maxhealth 5000->6000 |
| `RACE_SECT\sMothershipDockingBay.shp` | buildCost 1250->1000; buildTime 125->90; providesSU 20->30 |
| `RACE_SECT\sMothershipSpecial.shp` | buildCost 1750->1350; buildTime 175->135 |
| `RACE_SECT\sMothershipSupport.shp` | buildCost 500->300; buildTime 50->30 |
| `RACE_SECT\sMothership.shp` | FireTime2 1.2->1.0 (6 guns) |
| `RACE_SECT\sMCV.shp` / `sMimic.shp` | supportUnits 4->6 / 2->3 |
| `RACE_SECT\` sCarrier, sDreadnought, sProcessor | TimeShockedByEMP 0.75->1.5 |
| `RACE_SECT\sDestroyer.shp` | TimeShockedByEMP 1->1.5 |
| `RACE_SECT\` sHiveFrigate, sMultiBeamFrigate | TimeShockedByEMP 1.5->2.0 |

In short, the 1.01 patch is a balance pass. EMP stun times are longer across both factions, Somtaaw mothership
modules and support are cheaper, and the Beast heavy corvette and carrier are nerfed. It also changes the
skirmish AI difficulty scale and fixes text.

`CET:assets/` mirrors this same set of files (RACE_SECT/RACE_BEAST `.shp`, the MP `description.txt` files,
`feman/multiplayer_game.fib`, `english.script`, `tweak.script`). It adds `aiskirmish.script`, swarmer and
hive-frigate `.mex` overrides, and `soundfx/shipcmnevents.lut`. That mod builds on top of the 1.01 overlay.
The CET assets were not compared value-by-value.

---

## 8. Data-side differences vs Homeworld 1

1. **Archive names.**
   - HW1 uses `Homeworld.big`, required, plus an optional `Update.big` (BigFile.c:72-91).
   - Cataclysm opens `Cataclysm.big` plus `Update.big` [exe 0x5abb21]. An unreferenced `Extra.big` string is also
     present.
   - Speech and music stay outside the archive in `SectSpeech.vce`, `BeastSpeech.vce` and `Music4Cat.wxd`
     [exe strings at 0x4c0f08-0x4c0f28, files in `original/game/`]. HW1's build excluded its own `HW_MUSIC.WXD`
     and `HW_COMP.VCE` the same way (BigMake.btm:47).
2. **Container format is unchanged.** It is RBF1.23 with 32-byte entries, XOR names, the buggy CRC split and
   LZSS. HW1's own `biggie` extracts Cataclysm byte-for-byte.
3. **Stored names are all lowercase** in `cataclysm.big`. `update.big` keeps mixed case. Lookups lowercase
   names, so this only matters for a case-sensitive host filesystem.
4. **Race directory scheme.**
   - HW1 keys ship paths by `ShipRaceToStr`, which yields `R1`, `R2`, `P1`, `P2`, `P3` and `Traders`
     (ObjTypes.c:125-134, used at Universe.c:1874-1879).
   - Cataclysm uses `race_sect`, `race_beast`, `race_kushan`, `race_taidan`, `race_p1`, `race_traders` and a new
     `race_derelict`. Note the spelling `taidan`.
   - There are no P2 or P3 dirs. The HW1-style `derelicts\` and `resources\` trees remain, without `gasclouds`.
5. **Mesh version 0x500** replaces HW1's 0x402, for 966/967 `.geo` and 761/762 `.peo` files.
   - A new `.peo` mesh flavour uses the old `RMF99ba` "newFormat" ident.
   - `.lod` scripts reference both formats: 438 `pMeshFile` entries point at `.geo` and 351 at `.peo`.
   - This is the one place an HW1 mesh loader cannot be reused unmodified. The layout change is **UNVERIFIED**.
   - All other binary formats keep HW1's ident and version (table in section 2).
6. **ETG** is per `RACE_*` instead of R1/R2/P1/P2/P3/Traders (ETG.c:762-772).
7. **NIS** is a single set, `n01`..`n11` + `nislet01`..`03`. HW1 had per-race variants (SinglePlayer.c:3081-3116).
8. **Missions:**
   - Cataclysm has 17 missions plus 11 `MissionNN_NISmm` staging levels and one tutorial. HW1 has 16 missions
     (+ OEM 5B).
   - Mission logic is still in the exe. The data has only `.mission`, `.missphere`, extensionless sphere files,
     `.dist` and `loading.jpg`.
   - Multiplayer has 12 maps, of which 10 are listed.
9. **Scripts:**
   - New in Cataclysm: per-ship parade scripts (12 vs HW1's 6), plus `mainoverlay`, `overlayicons`, `shipnames`,
     `countries`, `race_beast\beastweapon` and `lensflares\*`.
   - `Dock.script` is gone.
   - `Italian` and `Spanish` language scripts are referenced but not shipped.
   - Research is `researchdepend.script` ("PG - CATACLYSM research dependancies").
   - `gatherstatsfor.script` columns are `SECT BEAST R1 R2 P1`.
10. **Leftovers:** `demos\homeworld.bat` (an HW1 E3 demo loop), `demos\abc640x480.dem`, `fonts\fontusage.doc` and
    `bigcontentsculled.txt` (the build manifest).

---

## 9. Open questions

- **Override rule at runtime.**
  - Does retail Cataclysm run the timestamp filesystem compare, and does its `fileOpen` prefer loose files?
  - That decides whether GOG's loose 2017 title LiFs and `credits/english/01_title.jpg` are used.
  - The compare messages have no code references [exe search], and the global at `0x8df9fc` that gates the open
    routine is unidentified.
  - Next step: symbolize `0x425260` (open), `0x425190`, and `bigFindFile`'s Cataclysm equivalent in Ghidra.
- **Mesh v0x500.** What changed versus the 0x402 layout? The header is still 68 bytes, and the loader branches
  only on `RMF99ba`. When does the game load `.geo` versus `.peo`? Both exist for most LODs, while `.lod` files
  name one or the other.
- **Extension meanings.** `.mif` (`texturedone.mif`), extensionless `taidan` versus `taiidan` in mission06,
  `mad.script`, and `etg\derelict.script` are present but have no exe literal. Are they loaded via format
  strings, or dead?
- **`Extra.big`.** It is in the string table but unreferenced. Was it planned DLC or an add-on path?
- **Cut content.** `bmissiledestroyer`, `smothershipgenetics` (the NIS-only module?) and `dplanetoforigin` have
  meshes but no stats in the ship enum.
- **Multiplayer CRC.** `HomeworldCRC` is fed by the TOC CRCs in HW1 (SDL/utility.c:3830-3833). Does Cataclysm
  do the same? If so, any repacked archive breaks multiplayer compatibility with stock 1.01 clients
  (inference, **UNVERIFIED**).
