#!/usr/bin/env python3
"""bigfile.py - read Relic "RBF1.23" .big archives (Homeworld 1 / Homeworld: Cataclysm).

Stdlib only.  Format reference (file:line into reference/HomeworldSDL/src):
  Game/BigFile.h:67,76      magic "RBF" + version "1.23" (7 bytes, no NUL)
  Game/BigFile.c:400-418    bigTOCRead: int32 numFiles, int32 flags, then numFiles x bigTOCFileEntry
  Game/BigFile.h:86-102     bigTOCFileEntry (32 bytes, MSVC/x86 natural alignment):
                              u32 nameCRC1, u32 nameCRC2, u16 nameLength, (2 pad),
                              u32 storedLength, u32 realLength, u32 offset, u32 timeStamp,
                              u8 compressionType, (3 pad)
  Game/BigFile.h:104-121    bigTOCFileEntryRemaster (36 bytes: extra u32 before compressionType),
                            selected in BigFile.c:416 only when numFiles == 13887 (HW Remastered)
  Game/BigFile.c:2318-2322  at entry.offset: encrypted name (nameLength bytes + 1 NUL), then data
  Game/BigFile.c:684-692    bigFilenameDecrypt: running XOR, mask starts at 213 (0xD5), next mask
                            is the previous *plaintext* byte
  Game/BigFile.c:551-603    name hash: lowercase, '/'->'\\', CRC32 of the two halves; shipped HW1
                            archives use the "buggy" split (len//2, len//2) - the last char of an
                            odd-length name is ignored; newer tools use (len//2, len - len//2)
  ThirdParty/CRC/CRC32.c    crc32Compute == standard reflected CRC-32 (zlib.crc32)
  ThirdParty/LZSS/LZSS.c:43-52,430-467  Nelson/Gailly LZSS: 4096-byte window, 12-bit index, 4-bit length,
                            BREAK_EVEN 1, flag bit 1 = literal(8 bits), 0 = (index, len+2),
                            index 0 = END_OF_STREAM, window cursor starts at 1, MSB-first bits
                            (ThirdParty/LZSS/BitIO.c:356-392)

Homeworld: Cataclysm's cataclysm.big / update.big use exactly this format (32-byte entries, buggy CRC
split, same LZSS); extraction was verified byte-identical against the HW1 C `biggie -x` for all 16,567
files.  See docs/big-archive-contents.md.

Usage:
  bigfile.py info    <archive>
  bigfile.py list    <archive> [--crc] [--time]
  bigfile.py extract <archive> <outdir> [glob ...] [--jobs N] [--quiet]
  bigfile.py cat     <archive> <name>
"""

import argparse
import datetime
import fnmatch
import os
import signal
import struct
import sys
import zlib

MAGIC = b"RBF1.23"                 # BF_FILE_HEADER BF_VERSION, BigFile.h:67,76
TOC_HEADER = struct.Struct("<ii")  # numFiles, flags                BigFile.c:404-405
ENTRY32 = struct.Struct("<IIH2xIIIIB3x")   # bigTOCFileEntry        BigFile.h:86-102
ENTRY36 = struct.Struct("<IIH2xIIIIIB3x")  # bigTOCFileEntryRemaster BigFile.h:104-121
BF_FLAG_TOC_SORTED = 1             # BigFile.h:79
NAME_MASK0 = 213                   # BigFile.c:670,686

# LZSS parameters, LZSS.c:43-52
INDEX_BIT_COUNT = 12
LENGTH_BIT_COUNT = 4
WINDOW_SIZE = 1 << INDEX_BIT_COUNT
BREAK_EVEN = (1 + INDEX_BIT_COUNT + LENGTH_BIT_COUNT) // 9   # == 1
END_OF_STREAM = 0


class BigError(Exception):
    pass


class Entry:
    __slots__ = ("index", "crc1", "crc2", "name_len", "stored", "real", "offset",
                 "timestamp", "comp", "extra", "name", "name_raw", "hash_variant")

    @property
    def data_offset(self):
        return self.offset + self.name_len + 1   # BigFile.c:2595

    @property
    def unix_name(self):
        return self.name.replace("\\", "/")


def decrypt_name(raw):
    """bigFilenameDecrypt (BigFile.c:684-692)."""
    out = bytearray(raw)
    mask = NAME_MASK0
    for i in range(len(out)):
        out[i] ^= mask
        mask = out[i]
    return bytes(out)


def normalize_name(name_bytes):
    """tolower + filenameSlashMassage(..., FALSE): '/'->'\\', runs of '\\' collapsed (BigFile.c:104-148, 569-573)."""
    s = name_bytes.lower().replace(b"/", b"\\")
    while b"\\\\" in s:
        s = s.replace(b"\\\\", b"\\")
    return s


def name_crcs(name_bytes):
    """Return {variant: (crc1, crc2)} for the two split rules in bigTOCFileExists (BigFile.c:579-602)."""
    s = normalize_name(name_bytes)
    n = len(s)
    h = n // 2
    return {
        "hw1-buggy": (zlib.crc32(s[:h]), zlib.crc32(s[h:h + h])),   # BigFile.c:579-581
        "fixed": (zlib.crc32(s[:h]), zlib.crc32(s[h:])),           # BigFile.c:596-600, 1480-1484
    }


def lzss_expand(src, real_len=None):
    """Exact port of lzssExpandBuffer (LZSS.c:430-467) + bit reader (BitIO.c:356-392).

    The C keeps a 4096-byte ring `window`, cursor starting at 1, and writes each output byte
    to window[cursor].  Equivalently the window is the last 4096 output bytes, so we decode
    straight into the output buffer, pre-filled with 4096 zero bytes (= a zero window; the
    encoder can only reference positions written earlier in the same stream, see AddString).
    A match at window index p while the cursor is at c starts d = (c - p) mod 4096 bytes back
    (d == 0 means 4096), copying len = 4-bit value + BREAK_EVEN + 1 bytes (2..17), byte by
    byte so overlapping copies replicate.

    Returns (data, bytes_consumed).  bytes_consumed mirrors BitIO's index (number of input
    bytes touched), which the game asserts == storedLength (BigFile.c:2621-2622).
    """
    out = bytearray(WINDOW_SIZE)
    n_src = len(src)
    ip = 0
    acc = 0
    nbits = 0
    cur = 1                      # current_position = 1  (LZSS.c:442)
    append = out.append
    while True:
        if nbits < 17:
            while nbits <= 24 and ip < n_src:
                acc = (acc << 8) | src[ip]
                ip += 1
                nbits += 8
        if nbits < 1:
            raise BigError("LZSS: input exhausted (no END_OF_STREAM)")
        nbits -= 1
        if (acc >> nbits) & 1:
            if nbits < 8:
                raise BigError("LZSS: input exhausted in literal")
            nbits -= 8
            append((acc >> nbits) & 0xFF)
            cur = (cur + 1) & 0xFFF
        else:
            if nbits < INDEX_BIT_COUNT:
                raise BigError("LZSS: input exhausted in match index")
            nbits -= INDEX_BIT_COUNT
            pos = (acc >> nbits) & 0xFFF
            if pos == END_OF_STREAM:
                break
            if nbits < LENGTH_BIT_COUNT:
                raise BigError("LZSS: input exhausted in match length")
            nbits -= LENGTH_BIT_COUNT
            length = ((acc >> nbits) & 0xF) + BREAK_EVEN + 1   # loop i = 0..match_length inclusive
            d = (cur - pos) & 0xFFF or WINDOW_SIZE
            s = len(out) - d
            if d >= length:
                out += out[s:s + length]
            else:
                for i in range(length):
                    append(out[s + i])
            cur = (cur + length) & 0xFFF
        acc &= (1 << nbits) - 1
        if real_len is not None and len(out) - WINDOW_SIZE > real_len:
            raise BigError("LZSS: output overruns realLength %d" % real_len)
    consumed = (ip * 8 - nbits + 7) // 8
    return bytes(out[WINDOW_SIZE:]), consumed


class BigArchive:
    def __init__(self, path):
        self.path = path
        self.size = os.path.getsize(path)
        self.fp = open(path, "rb")
        hdr = self.fp.read(len(MAGIC))
        if hdr != MAGIC:   # bigHeaderVerify, BigFile.c:499-511
            raise BigError("%s: bad header %r (expected %r)" % (path, hdr, MAGIC))
        self.num_files, self.flags = TOC_HEADER.unpack(self.fp.read(TOC_HEADER.size))
        if self.num_files < 0 or self.num_files > 10_000_000:
            raise BigError("%s: implausible numFiles %d" % (path, self.num_files))
        self.toc_start = len(MAGIC) + TOC_HEADER.size
        self.entry_struct = self._detect_entry_layout()
        self.entries = self._read_toc()

    # -- TOC -------------------------------------------------------------------------------
    def _detect_entry_layout(self):
        """HW1/Cataclysm use 32-byte entries; HW Remastered's classic archive uses 36.

        BigFile.c:416 keys this off numFiles == 13887.  We detect it structurally instead:
        every archive written by biggie has its first name/data blob immediately after the
        TOC, so min(offset) must equal toc_start + numFiles * entry_size.
        """
        if self.num_files == 0:
            return ENTRY32
        for st, off_field in ((ENTRY32, 5), (ENTRY36, 5)):
            self.fp.seek(self.toc_start)
            raw = self.fp.read(st.size * self.num_files)
            if len(raw) != st.size * self.num_files:
                continue
            offs = [st.unpack_from(raw, i * st.size)[off_field] for i in range(self.num_files)]
            if min(offs) == self.toc_start + st.size * self.num_files:
                return st
        raise BigError("%s: TOC layout matches neither 32- nor 36-byte entries" % self.path)

    def _read_toc(self):
        st = self.entry_struct
        self.fp.seek(self.toc_start)
        raw = self.fp.read(st.size * self.num_files)
        entries = []
        for i in range(self.num_files):
            f = st.unpack_from(raw, i * st.size)
            e = Entry()
            e.index = i
            if st is ENTRY32:
                (e.crc1, e.crc2, e.name_len, e.stored, e.real, e.offset, e.timestamp,
                 e.comp) = f
                e.extra = None
            else:
                (e.crc1, e.crc2, e.name_len, e.stored, e.real, e.offset, e.timestamp,
                 e.extra, e.comp) = f
            if e.offset + e.name_len + 1 + e.stored > self.size:
                raise BigError("entry %d runs past end of file" % i)
            self.fp.seek(e.offset)
            blob = self.fp.read(e.name_len + 1)
            e.name_raw = decrypt_name(blob[:e.name_len])     # BigFile.c:2321-2322
            if blob[e.name_len:e.name_len + 1] != b"\0":
                raise BigError("entry %d: name not NUL-terminated" % i)
            e.name = e.name_raw.decode("latin-1")
            crcs = name_crcs(e.name_raw)
            e.hash_variant = None
            for k in ("hw1-buggy", "fixed"):
                if crcs[k] == (e.crc1, e.crc2):
                    e.hash_variant = k if e.hash_variant is None else e.hash_variant + "+" + k
            entries.append(e)
        return entries

    def is_sorted(self):
        keys = [(e.crc1, e.crc2) for e in self.entries]
        return all(a <= b for a, b in zip(keys, keys[1:]))

    def hash_summary(self):
        out = {}
        for e in self.entries:
            k = e.hash_variant or "NO MATCH"
            out[k] = out.get(k, 0) + 1
        return out

    # -- data ------------------------------------------------------------------------------
    def read_stored(self, e):
        self.fp.seek(e.data_offset)
        return self.fp.read(e.stored)

    def read(self, e):
        """Return (data, problems[])  - mirrors bigFileLoad (BigFile.c:2638-2673)."""
        stored = self.read_stored(e)
        problems = []
        if e.comp:                                      # compressionType != 0
            data, consumed = lzss_expand(stored, e.real)
            if len(data) != e.real:
                problems.append("expanded %d != realLength %d" % (len(data), e.real))
            if consumed != e.stored:
                problems.append("consumed %d != storedLength %d" % (consumed, e.stored))
        else:
            data = stored
            if e.stored != e.real:
                problems.append("uncompressed but stored %d != real %d" % (e.stored, e.real))
        return data, problems

    def find(self, name):
        """Lookup the way the game does: by 64-bit name CRC, buggy split first (BigFile.c:551-603)."""
        crcs = name_crcs(name.encode("latin-1"))
        by_crc = {(e.crc1, e.crc2): e for e in self.entries}
        for k in ("hw1-buggy", "fixed"):
            e = by_crc.get(crcs[k])
            if e is not None:
                return e
        return None

    def close(self):
        self.fp.close()


def fmt_time(ts):
    return datetime.datetime.fromtimestamp(ts, datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S")


def match_globs(e, globs):
    if not globs:
        return True
    cands = (e.name.lower(), e.unix_name.lower())
    for g in globs:
        gl = g.lower()
        for c in cands:
            if fnmatch.fnmatchcase(c, gl):
                return True
    return False


def safe_out_path(outdir, e):
    parts = [p for p in e.unix_name.split("/") if p not in ("", ".")]
    if any(p == ".." for p in parts):
        raise BigError("refusing path traversal in %r" % e.name)
    return os.path.join(outdir, *parts)


# -- commands --------------------------------------------------------------------------------
def cmd_info(args):
    a = BigArchive(args.archive)
    tot_s = sum(e.stored for e in a.entries)
    tot_r = sum(e.real for e in a.entries)
    ncomp = sum(1 for e in a.entries if e.comp)
    comp_vals = sorted(set(e.comp for e in a.entries))
    print("archive        : %s (%d bytes)" % (a.path, a.size))
    print("magic          : %s" % MAGIC.decode())
    print("numFiles       : %d" % a.num_files)
    print("flags          : 0x%x (%s)" % (a.flags, "CRC-sorted TOC" if a.flags & BF_FLAG_TOC_SORTED else "unsorted TOC"))
    print("entry size     : %d bytes (%s)" % (a.entry_struct.size, "HW1/Cataclysm bigTOCFileEntry" if a.entry_struct is ENTRY32 else "Remaster bigTOCFileEntryRemaster"))
    print("TOC really sorted by (crc1,crc2): %s" % a.is_sorted())
    print("compressed     : %d / %d (compressionType values: %s)" % (ncomp, a.num_files, comp_vals))
    print("stored total   : %d" % tot_s)
    print("real total     : %d" % tot_r)
    print("name-hash split: %s" % a.hash_summary())
    if a.entries:
        ts = [e.timestamp for e in a.entries]
        print("timestamps     : %s .. %s UTC" % (fmt_time(min(ts)), fmt_time(max(ts))))
        end = max(e.data_offset + e.stored for e in a.entries)
        print("last blob ends : %d (file size %d, trailing %d)" % (end, a.size, a.size - end))
    a.close()


def cmd_list(args):
    a = BigArchive(args.archive)
    hdr = ["offset", "stored", "real", "comp"]
    if args.crc:
        hdr.append("crc64")
    if args.time:
        hdr.append("timestamp_utc")
    hdr.append("name")
    print("\t".join(hdr))
    for e in sorted(a.entries, key=lambda e: e.offset) if args.by_offset else a.entries:
        row = [str(e.offset), str(e.stored), str(e.real), str(e.comp)]
        if args.crc:
            row.append("%08x%08x" % (e.crc1, e.crc2))
        if args.time:
            row.append(fmt_time(e.timestamp))
        row.append(e.name)
        print("\t".join(row))
    a.close()


def _extract_worker(job):
    path, outdir, indices = job
    a = BigArchive(path)
    results = []
    for i in indices:
        e = a.entries[i]
        try:
            data, problems = a.read(e)
            dest = safe_out_path(outdir, e)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, "wb") as f:
                f.write(data)
            try:
                os.utime(dest, (e.timestamp, e.timestamp))
            except OSError:
                pass
            results.append((i, problems))
        except Exception as ex:   # report and carry on, like biggie (BigFile.c:2368-2376)
            results.append((i, ["EXCEPTION: %s" % ex]))
    a.close()
    return results


def cmd_extract(args):
    a = BigArchive(args.archive)
    sel = [e.index for e in a.entries if match_globs(e, args.globs)]
    # duplicate-name guard (two TOC entries extracting to the same path)
    seen = {}
    for i in sel:
        k = a.entries[i].unix_name.lower()
        if k in seen:
            print("WARNING: %r and %r map to the same path" % (a.entries[seen[k]].name, a.entries[i].name), file=sys.stderr)
        seen[k] = i
    a.close()
    os.makedirs(args.outdir, exist_ok=True)
    jobs = max(1, args.jobs)
    chunks = [sel[k::jobs] for k in range(jobs)]
    work = [(args.archive, args.outdir, c) for c in chunks if c]
    if jobs > 1 and len(sel) > 1:
        from concurrent.futures import ProcessPoolExecutor
        with ProcessPoolExecutor(max_workers=jobs) as ex:
            res = [r for part in ex.map(_extract_worker, work) for r in part]
    else:
        res = [r for w in work for r in _extract_worker(w)]
    a = BigArchive(args.archive)
    bad = 0
    for i, problems in sorted(res):
        e = a.entries[i]
        if problems:
            bad += 1
            print("FAIL %s: %s" % (e.name, "; ".join(problems)), file=sys.stderr)
        elif not args.quiet:
            print(e.name)
    print("extracted %d/%d files from %s to %s; %d with problems"
          % (len(res) - bad, len(sel), args.archive, args.outdir, bad), file=sys.stderr)
    a.close()
    return 1 if bad else 0


def cmd_cat(args):
    a = BigArchive(args.archive)
    e = a.find(args.name)
    if e is None:
        print("not found: %s" % args.name, file=sys.stderr)
        return 1
    data, problems = a.read(e)
    for p in problems:
        print("WARNING: %s" % p, file=sys.stderr)
    sys.stdout.buffer.write(data)
    return 0


def main(argv=None):
    if hasattr(signal, "SIGPIPE"):          # `bigfile.py list x.big | head` should not traceback
        signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    p = argparse.ArgumentParser(description="Relic RBF1.23 .big archive tool (Homeworld 1 / Cataclysm)")
    sp = p.add_subparsers(dest="cmd", required=True)
    s = sp.add_parser("info", help="header / TOC summary")
    s.add_argument("archive")
    s = sp.add_parser("list", help="list TOC: offset, stored size, real size, compressed flag, name")
    s.add_argument("archive")
    s.add_argument("--crc", action="store_true", help="also print the 64-bit name CRC")
    s.add_argument("--time", action="store_true", help="also print the stored timestamp (UTC)")
    s.add_argument("--by-offset", action="store_true", help="order by data offset instead of TOC order")
    s = sp.add_parser("extract", help="extract (optionally glob-filtered, case-insensitive; / or \\ separators)")
    s.add_argument("archive")
    s.add_argument("outdir")
    s.add_argument("globs", nargs="*")
    s.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    s.add_argument("--quiet", action="store_true")
    s = sp.add_parser("cat", help="write one file to stdout (looked up by name CRC like the game)")
    s.add_argument("archive")
    s.add_argument("name")
    args = p.parse_args(argv)
    try:
        return {"info": cmd_info, "list": cmd_list, "extract": cmd_extract, "cat": cmd_cat}[args.cmd](args) or 0
    except BigError as ex:
        print("ERROR: %s" % ex, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
