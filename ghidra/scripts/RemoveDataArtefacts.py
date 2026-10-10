# Remove function objects that are not code: the 42 one-byte FUN_* "functions" in .data at
# 0x00891a1c-0x00892114 (docs/binary-map.md R4.6 item 6, docs/ghidra-round5-prep.md D1).
#
# Where they come from (D1 inspection of the file bytes, 2026-10-09): .idata (0xc91000) holds the
# .cms_t stub's own import table, which the PE loader never binds (the loader's import directory
# is 0x749950 in .rdata). Its unbound IAT slots (0xc915b4...) still hold the RVAs of their
# KERNEL32 hint/name entries, e.g. 0x00891a1c -> "GetVersionExA" at 0xc91a1c. .cms_t calls through
# those slots (`CALL dword ptr [slot]`), and Ghidra took each slot value as an absolute VA, so it
# created a function at the RVA read as a VA, which lands in .data.
#
# Per function whose entry lies in range=, the script checks:
#   - the memory block is not executable;
#   - no reference into the entry is a direct flow: every CALL/JUMP reference must come from an
#     indirect `CALL/JMP [slot]` whose slot holds the entry's offset as a 32-bit value, or from a
#     `CALL reg` whose register was last loaded by `MOV reg, [slot]` from such a slot (at most
#     MAX_BACK instructions back, linear); both are the pointer-derived references described above;
# and, when both hold, removes the function object only. Data, labels, references and any code
# unit at the address are left as they are (clear=1 also clears a stray instruction at the entry
# that lies in a non-executable block).
#
# Args (key=value):
#   range=<lo>-<hi>  default 0x00891a1c-0x00892114
#   report=<csv>     per-function CSV (default <cwd>/ghidra/remove_data_artefacts_r5.report.csv)
#   dry=1|0          report only, remove nothing (default 0)
#   clear=1|0        also clear an instruction code unit at a removed entry (default 0)
#
# Tolerant: per-function errors are reported; the script never aborts the headless run.
#
# @category Homeworld-Infection
# @runtime PyGhidra

import typing
if typing.TYPE_CHECKING:
    from ghidra.ghidra_builtins import *  # noqa: F401,F403

import csv
import os
import struct
import time

T0 = time.time()
MAX_BACK = 64  # instructions walked back from a `CALL reg` to the load of reg
FIELDS = ["address", "name", "size", "block", "perm", "code_unit", "refs", "flow_refs",
          "via_pointer", "rva_target", "status", "detail"]


def log(msg):
    println("[RemoveDataArtefacts +%.1fs] %s" % (time.time() - T0, msg))


def parse_args():
    args = {}
    for a in list(getScriptArgs()):
        a = str(a)
        if "=" in a:
            k, v = a.split("=", 1)
            args[k.strip().lower()] = v.strip()
    return args


def parse_int(v):
    s = str(v or "").strip().lower().replace("_", "")
    if not s:
        return None
    try:
        return int(s[2:] if s.startswith("0x") else s, 16)
    except ValueError:
        return None


def first_line(e):
    s = str(e)
    return (s.splitlines()[0] if s else repr(e))[:200]


def raw(addr, n):
    try:
        return bytes((x & 0xFF) for x in getBytes(addr, n))
    except Exception:
        return b""


def perm(block):
    return "%s%s%s" % ("r" if block.isRead() else "-", "w" if block.isWrite() else "-",
                       "x" if block.isExecute() else "-")


def rva_target(off):
    """The entry offset read as an RVA: block and, for a hint/name entry, its import name."""
    mem = currentProgram.getMemory()
    base = currentProgram.getImageBase().getOffset()
    try:
        a = toAddr(off + base)
    except Exception:
        return ""
    if not mem.contains(a):
        return ""
    b = mem.getBlock(a)
    s = raw(a, 66)
    name = ""
    if len(s) > 3:
        z = s.find(b"\0", 2)
        cand = s[2:z if z > 2 else len(s)]
        if cand and all(32 < c < 127 for c in cand):
            name = "hint=%d:%s" % (struct.unpack_from("<H", s, 0)[0], cand.decode("ascii"))
    return "%s@%s%s" % (b.getName(), a, (":" + name) if name else "")


def classify_ref(ref, off):
    """('data'|'via-pointer'|'direct-flow', detail)."""
    rt = ref.getReferenceType()
    src = ref.getFromAddress()
    if not (rt.isCall() or rt.isJump() or rt.isFlow()):
        return "data", "%s<-%s" % (rt, src)
    ins = currentProgram.getListing().getInstructionAt(src)
    if ins is None:
        return "direct-flow", "%s<-%s(no-instruction)" % (rt, src)
    for r in ins.getReferencesFrom():
        t = r.getReferenceType()
        if r.getToAddress().isMemoryAddress() and (t.isRead() or t.isIndirect() or t.isData()):
            slot = r.getToAddress()
            b = raw(slot, 4)
            if len(b) == 4 and struct.unpack("<I", b)[0] == off:
                return "via-pointer", "%s<-%s'%s'[slot %s]" % (rt, src, ins, slot)
    # raw check for FF 15 / FF 25 abs32 when Ghidra kept no READ reference
    b = raw(src, 6)
    if len(b) == 6 and b[0] == 0xFF and b[1] in (0x15, 0x25):
        slot_off = struct.unpack_from("<I", b, 2)[0]
        try:
            v = raw(toAddr(slot_off), 4)
            if len(v) == 4 and struct.unpack("<I", v)[0] == off:
                return "via-pointer", "%s<-%s'%s'[slot 0x%08x raw]" % (rt, src, ins, slot_off)
        except Exception:
            pass
    # `MOV reg, dword ptr [slot]` ... `CALL reg` (COMPUTED_CALL found by register propagation):
    # walk back from the call to the instruction that last loaded that register
    reg = None
    try:
        objs = ins.getOpObjects(0)
        if ins.getNumOperands() == 1 and len(objs) == 1 and \
                objs[0].getClass().getSimpleName() == "Register":
            reg = objs[0]
    except Exception:
        reg = None
    if reg is not None:
        listing = currentProgram.getListing()
        p = ins
        for _ in range(MAX_BACK):
            p = listing.getInstructionBefore(p.getAddress())
            if p is None:
                break
            try:
                res = [o for o in p.getResultObjects() if o.getClass().getSimpleName() == "Register"]
            except Exception:
                res = []
            if not any(r == reg or r.contains(reg) for r in res):
                continue
            if p.getMnemonicString().upper() == "MOV":
                for r in p.getReferencesFrom():
                    t = r.getReferenceType()
                    if r.getToAddress().isMemoryAddress() and (t.isRead() or t.isData()):
                        b = raw(r.getToAddress(), 4)
                        if len(b) == 4 and struct.unpack("<I", b)[0] == off:
                            return "via-pointer", "%s<-%s'%s' loaded by %s'%s'[slot %s]" % (
                                rt, src, ins, p.getAddress(), p, r.getToAddress())
            break  # the register was last written by something else
    return "direct-flow", "%s<-%s'%s'" % (rt, src, ins)


def main():
    args = parse_args()
    lo, hi = 0x00891a1c, 0x00892114
    rg = args.get("range", "")
    if rg and "-" in rg:
        a, b = rg.split("-", 1)
        if parse_int(a) is not None and parse_int(b) is not None:
            lo, hi = parse_int(a), parse_int(b)
    report = args.get("report") or os.path.join(os.getcwd(), "ghidra",
                                                "remove_data_artefacts_r5.report.csv")
    dry = args.get("dry", "0") in ("1", "true", "yes")
    clear = args.get("clear", "0") in ("1", "true", "yes")
    fm = currentProgram.getFunctionManager()
    listing = currentProgram.getListing()
    mem = currentProgram.getMemory()
    rm = currentProgram.getReferenceManager()
    before = fm.getFunctionCount()
    cands = []
    for fn in fm.getFunctions(toAddr(lo), True):
        if fn.getEntryPoint().getOffset() > hi:
            break
        if not fn.isExternal():
            cands.append(fn)
    log("functions-before=%d; %d function entries in 0x%08x-0x%08x (dry=%s clear=%s)"
        % (before, len(cands), lo, hi, dry, clear))
    rows = []
    stats = {"removed": 0, "kept": 0, "failed": 0, "insn_cleared": 0}
    for fn in cands:
        a = fn.getEntryPoint()
        off = a.getOffset()
        row = {k: "" for k in FIELDS}
        rows.append(row)
        try:
            row["address"] = "0x%08x" % off
            row["name"] = fn.getName()
            row["size"] = str(fn.getBody().getNumAddresses())
            blk = mem.getBlock(a)
            row["block"] = blk.getName() if blk else "?"
            row["perm"] = perm(blk) if blk else "?"
            cu = listing.getCodeUnitAt(a)
            ins = listing.getInstructionAt(a)
            if ins is not None:
                row["code_unit"] = "instruction:'%s'" % ins
            elif cu is not None:
                d = listing.getDataContaining(a)
                row["code_unit"] = ("data:%s@%s" % (d.getDataType().getName(), d.getAddress())
                                    if d is not None and d.isDefined() else "undefined")
            else:
                row["code_unit"] = "none"
            kinds = {"data": [], "via-pointer": [], "direct-flow": []}
            for ref in rm.getReferencesTo(a):
                k, det = classify_ref(ref, off)
                kinds[k].append(det)
            row["refs"] = str(sum(len(v) for v in kinds.values()))
            row["flow_refs"] = str(len(kinds["direct-flow"]))
            row["via_pointer"] = str(len(kinds["via-pointer"]))
            row["rva_target"] = rva_target(off)
            why = []
            if blk is None or blk.isExecute():
                why.append("executable-block")
            if kinds["direct-flow"]:
                why.append("direct-flow:" + "|".join(kinds["direct-flow"][:3]))
            if why:
                row["status"] = "kept"
                row["detail"] = ";".join(why)
                stats["kept"] += 1
                continue
            row["detail"] = ("example " + kinds["via-pointer"][0]) if kinds["via-pointer"] else ""
            if dry:
                row["status"] = "would-remove"
                continue
            if not fm.removeFunction(a):
                row["status"] = "failed"
                row["detail"] += ";removeFunction returned false"
                stats["failed"] += 1
                continue
            row["status"] = "removed"
            stats["removed"] += 1
            if clear and ins is not None:
                listing.clearCodeUnits(a, ins.getMaxAddress(), False)
                row["detail"] += ";instruction-cleared"
                stats["insn_cleared"] += 1
        except Exception as e:
            row["status"] = "failed"
            row["detail"] += ";" + first_line(e)
            stats["failed"] += 1
    log("results: %s; functions before=%d after=%d"
        % (", ".join("%s=%d" % kv for kv in sorted(stats.items())), before, fm.getFunctionCount()))
    for r in rows:
        if r["status"] in ("kept", "failed"):
            log("NOT REMOVED %s %s: %s %s" % (r["address"], r["name"], r["status"], r["detail"]))
    try:
        d = os.path.dirname(report)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(report, "w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=FIELDS)
            w.writeheader()
            for r in rows:
                w.writerow(r)
        log("report: %s" % report)
    except Exception as e:
        log("could not write report %s: %s" % (report, first_line(e)))


main()
