# Load the curated Cataclysm C header set into the program's data type manager, check the
# resulting layouts against CET's static_asserts, then type the known globals with the structs.
#
# Input is produced outside Ghidra by tools/prep_ghidra_headers.py (clang preprocess + C++->C
# fixups; see that file and docs/ghidra-round2-prep.md "A2" for every change it makes):
#   <decls.json> = {"pack": 4, "headers": [{"header", "origin", "decls": [...]}],
#                   "asserts": [{"kind": "offset|size", "struct", "field", "value", "where"}],
#                   "fixups": [...]}
#
# Args (key=value):
#   decls=<path>     default <scripts>/../exports/headers_r2b/decls.json
#   symbols=<a,b,c>  symbol JSON files (cet_symbols.json schema) that give global addresses;
#                    default <scripts>/../cet_symbols.json,symbols_round1.json,symbols_round1_data.json
#   report=<path>    JSON report (default next to decls.json: import_headers_report.json)
#   category=/Cataclysm  root category for the parsed types (one sub-category per header)
#   apply=1          0 = parse + check only, do not type any global
#
# Best effort by design: each top-level declaration is parsed on its own; a declaration that
# fails is recorded and skipped, a header whose declarations all fail is reported as skipped,
# and nothing aborts the headless run.
#
# Which globals get a type:
#   1. CET anchors (cet_symbols.json kind=data) whose "type" names a parsed struct, pointer to
#      struct or array of them (e.g. universe: Universe, aisTeams: AISTeamEntry[34]); these
#      clear conflicting data (CET is authoritative).
#   2. Named data globals (any symbol file) with a declared type in decls.json "globals": the
#      CET header extern (including blocks CET disabled with #if 0), else the HW1 .c definition
#      named by the symbol's hw1_equivalent. Struct-shaped and scalar types alike; a scalar never
#      replaces a differently sized item already defined there.
#   Guards: the address must be in a non-executable block; anything larger than a pointer must
#   fit in its block, must match any byte size stated in the CET notes, and must not cover another
#   named global (except the parent's own derived "<name>_<field>" labels); a global inside an
#   already typed parent is not typed separately.
#
# @category Homeworld-Infection
# @runtime PyGhidra

import json
import os
import re
import traceback

try:
    from ghidra.app.util.cparser.C import CParser
    from ghidra.program.model.data import (Array, CategoryPath, DataUtilities, Pointer,
                                           Structure, TypeDef, Union, DataTypeConflictHandler)
    from ghidra.util.data import DataTypeParser
except Exception:  # imported outside Ghidra (syntax check)
    pass

LOG_PREFIX = "[ImportHeaders] "


def log(msg):
    try:
        println(LOG_PREFIX + msg)
    except NameError:
        print(LOG_PREFIX + msg)


# ---------------------------------------------------------------- parsing

EXTERN_VAR_RE = re.compile(r"^extern\s+(?!.*\)\s*;\s*$)(.*?)([A-Za-z_]\w*)\s*(\[[^\]]*\])?\s*;\s*$", re.S)


def parse_headers(dtm, doc, category):
    """Parse every declaration; return (header_stats, extern_types, parser)."""
    parser = CParser(dtm, True, None)
    try:
        parser.setMonitor(monitor)
    except Exception:
        pass
    pack = doc.get("pack")
    if pack:
        parser.parse("#pragma pack(push, %d)\n" % int(pack))
    stats = {}
    externs = {}
    for grp in doc.get("headers", []):
        h = grp.get("header") or "unknown"
        st = stats.setdefault(h, {"header": h, "origin": grp.get("origin", ""), "ok": 0,
                                  "failed": 0, "failures": []})
        for decl in grp.get("decls", []):
            text = decl if decl.endswith("\n") else decl + "\n"
            try:
                dt = parser.parse(text)
                ok = parser.didParseSucceed()
            except Exception as e:  # ParseException, TokenMgrError, runtime errors
                dt, ok = None, False
                msg = str(e).splitlines()[0][:160] if str(e) else repr(e)[:160]
                st["failures"].append({"decl": " ".join(decl.split())[:140], "error": msg})
            if ok:
                st["ok"] += 1
                m = EXTERN_VAR_RE.match(decl.strip())
                if m and dt is not None and "(" not in m.group(1):
                    externs[m.group(2)] = (dt, h)
            else:
                st["failed"] += 1
                if not st["failures"] or st["failures"][-1]["decl"] != " ".join(decl.split())[:140]:
                    st["failures"].append({"decl": " ".join(decl.split())[:140],
                                           "error": (parser.getParseMessages() or "")[-160:]})
    for st in stats.values():
        st["status"] = ("parsed" if st["failed"] == 0 else
                        "skipped" if st["ok"] == 0 else "partial")
        st["failures"] = st["failures"][:12]
    return stats, externs, parser


def import_types(pdtm, doc, category):
    """Parse into a scratch data type manager with the program's data organization (so nothing
    collides with types the program already has), move every parsed type under <category>, then
    copy them into the program. Returns (stats, externs{name: (program type, header)}, copied)."""
    from ghidra.program.model.data import (StandAloneDataTypeManager, BuiltInDataType, Composite,
                                           Enum, FunctionDefinition)
    tmp = StandAloneDataTypeManager("cataclysm_headers", pdtm.getDataOrganization())
    tx = tmp.startTransaction("parse headers")
    try:
        stats, externs, _ = parse_headers(tmp, doc, category)
        cat = CategoryPath(category)
        fcat = CategoryPath(category.rstrip("/") + "/functions")  # typedef'd fn ptrs share names
        movable = []
        it = tmp.getAllDataTypes()
        while it.hasNext():
            d = it.next()
            if isinstance(d, BuiltInDataType):
                continue
            if isinstance(d, (Composite, TypeDef, Enum, FunctionDefinition)):
                movable.append(d)
        for d in movable:
            if not str(d.getCategoryPath().getPath()).startswith(category):
                try:
                    d.setCategoryPath(fcat if isinstance(d, FunctionDefinition) else cat)
                except Exception as e:
                    log("could not move %s: %s" % (d.getName(), e))
        copied = 0
        for d in movable:
            try:
                pdtm.addDataType(d, DataTypeConflictHandler.REPLACE_HANDLER)
                copied += 1
            except Exception as e:
                log("could not copy %s: %s" % (d.getName(), e))
        ext = {}
        for n, (dt, h) in externs.items():
            try:
                ext[n] = (pdtm.resolve(dt, DataTypeConflictHandler.KEEP_HANDLER), h)
            except Exception:
                pass
    finally:
        tmp.endTransaction(tx, True)
        tmp.close()
    return stats, ext, copied


def find_type(dtm, name, category):
    """A parsed type by name, preferring our category tree."""
    name = name.strip()
    hits = []
    it = dtm.getAllDataTypes()
    while it.hasNext():
        d = it.next()
        if d.getName() == name:
            hits.append(d)
    if not hits:
        return None
    ours = [d for d in hits if str(d.getCategoryPath().getPath()).startswith(category)]
    return (ours or hits)[0]


def base_type(dt):
    while isinstance(dt, TypeDef):
        dt = dt.getBaseDataType()
    return dt


def enum_value(dtm, name):
    from ghidra.program.model.data import Enum
    it = dtm.getAllDataTypes()
    while it.hasNext():
        d = it.next()
        if isinstance(d, Enum):
            try:
                return d.getValue(name)
            except Exception:
                continue
    return None


def field_offset(dtm, comp, path):
    """offsetof(comp, 'a.b[IDX]') using the Ghidra layout."""
    off = 0
    cur = base_type(comp)
    for part in path.split("."):
        m = re.match(r"^(\w+)(?:\[(\w+)\])?$", part)
        if not m or not isinstance(cur, (Structure, Union)):
            return None
        fld = None
        for c in cur.getDefinedComponents():
            if c.getFieldName() == m.group(1):
                fld = c
                break
        if fld is None:
            return None
        off += fld.getOffset()
        cur = base_type(fld.getDataType())
        if m.group(2) is not None:
            idx = int(m.group(2), 0) if re.match(r"^(0x[0-9a-fA-F]+|\d+)$", m.group(2)) \
                else enum_value(dtm, m.group(2))
            if idx is None or not isinstance(cur, Array):
                return None
            off += idx * cur.getElementLength()
            cur = base_type(cur.getDataType())
    return off


def check_asserts(dtm, asserts, category):
    res = {"pass": 0, "fail": [], "unresolved": []}
    cache = {}
    for a in asserts:
        s = a["struct"]
        if s not in cache:
            cache[s] = find_type(dtm, s, category)
        dt = cache[s]
        if dt is None:
            res["unresolved"].append("%s %s.%s (%s): type missing" % (a["kind"], s, a["field"], a["where"]))
            continue
        got = dt.getLength() if a["kind"] == "size" else field_offset(dtm, dt, a["field"])
        if got is None:
            res["unresolved"].append("%s %s.%s (%s): field missing" % (a["kind"], s, a["field"], a["where"]))
        elif got == a["value"]:
            res["pass"] += 1
        else:
            res["fail"].append("%s %s%s expected 0x%x got 0x%x (%s)" % (
                a["kind"], s, ("." + a["field"]) if a["field"] else "", a["value"], got, a["where"]))
    return res


# ---------------------------------------------------------------- applying to globals

def struct_shaped(dt):
    """True for struct/union, pointer(s) to one, or arrays of those."""
    d = base_type(dt)
    while isinstance(d, (Array, Pointer)):
        nxt = d.getDataType()
        if nxt is None:
            return False
        d = base_type(nxt)
    return isinstance(d, (Structure, Union))


def parse_type_text(dtm, text, category):
    """CET 'type' text -> DataType: 'Universe', 'AIPlayer* (pointer variable)',
    'AISTeamEntry[34] (0x9D10 each)', 'Player[9] (...)'. None if not struct-shaped/known."""
    t = re.sub(r"\s*\(.*$", "", (text or "").strip())
    m = re.match(r"^(?:struct\s+)?([A-Za-z_]\w*)\s*(\**)\s*(?:\[\s*(?:\w+\s*=\s*)?(\d+)\s*\])?$", t)
    if not m:
        return None
    base = find_type(dtm, m.group(1), category)
    if base is None:
        return None
    dt = base
    from ghidra.program.model.data import PointerDataType, ArrayDataType
    for _ in m.group(2):
        dt = PointerDataType(dt, dtm)
    if m.group(3):
        dt = ArrayDataType(dt, int(m.group(3)), dt.getLength(), dtm)
    return dt if struct_shaped(dt) else None


def stated_size(text):
    """'(688 bytes in Cataclysm...)' -> 688 ; '(0x9D10 each)' -> element size."""
    m = re.search(r"\((\d+) bytes", text or "")
    if m:
        return ("total", int(m.group(1)))
    m = re.search(r"\((0x[0-9A-Fa-f]+) each", text or "")
    if m:
        return ("each", int(m.group(1), 16))
    return None


def parse_addr(v):
    try:
        s = str(v).strip().lower()
        return int(s, 16)
    except Exception:
        return None


def load_symbols(paths):
    """name -> (address, kind, type text, source file); later files do not override earlier."""
    out = {}
    for p in paths:
        if not os.path.isfile(p):
            log("symbol file missing, skipped: %s" % p)
            continue
        try:
            doc = json.load(open(p, encoding="utf-8"))
        except Exception as e:
            log("bad symbol file %s: %s" % (p, e))
            continue
        for s in (doc.get("symbols") if isinstance(doc, dict) else doc) or []:
            if not isinstance(s, dict):
                continue
            a = parse_addr(s.get("address"))
            n = s.get("name")
            if a is None or not n or n in out:
                continue
            out[n] = (a, str(s.get("kind") or "data").lower(), s.get("type") or "",
                      os.path.basename(p))
    return out


def build_type(dtm, g, category):
    """{"base","stars","dims"} from decls.json globals -> DataType (None if unknown)."""
    from ghidra.program.model.data import PointerDataType, ArrayDataType
    base = {"bool": "sdword"}.get(g["base"], g["base"])
    if g.get("dims") is None:
        return None
    if base == "void" and not g["stars"]:
        return None
    dt = find_type(dtm, base, category)
    if dt is None:
        try:
            dt = DataTypeParser(dtm, dtm, None, DataTypeParser.AllowedDataTypes.ALL).parse(base)
        except Exception:
            return None
    for _ in range(int(g["stars"])):
        dt = PointerDataType(dt, dtm)
    for n in reversed(g["dims"]):
        if n <= 0:
            return None
        dt = ArrayDataType(dt, int(n), dt.getLength(), dtm)
    if dt.getLength() <= 0:
        return None
    from ghidra.program.model.data import FunctionDefinition
    if isinstance(base_type(dt), FunctionDefinition):
        return None
    return dt


def apply_globals(prog, dtm, symbols, globals_decl, externs, category, do_apply):
    mem = prog.getMemory()
    listing = prog.getListing()
    data_addrs = sorted((a, n) for n, (a, k, t, f) in symbols.items() if k in ("data", "vtable", "string"))
    ptr_size = prog.getDefaultPointerSize()
    cands = []  # (priority, address, name, dt, why, stated)
    seen = set()
    for n, (a, k, t, f) in symbols.items():
        if k != "data" or f != "cet_symbols.json":
            continue
        dt = parse_type_text(dtm, t, category)
        if dt is not None:
            cands.append((0, a, n, dt, "cet-anchor type '%s'" % t[:60], stated_size(t)))
            seen.add(n)
    for n, g in sorted(globals_decl.items()):
        if n in seen or n not in symbols:
            continue
        a, k, t, f = symbols[n]
        if k != "data":
            continue
        dt = build_type(dtm, g, category)
        if dt is None:
            continue
        cands.append((1, a, n, dt, "%s (%s)" % (g.get("source", ""), f), None))
        seen.add(n)
    for n, (dt, h) in externs.items():
        if n in seen or n not in symbols or symbols[n][1] != "data":
            continue
        cands.append((1, symbols[n][0], n, dt, "extern in %s" % h, None))
    # anchors first, then big by-value containers, so derived sub-labels are seen as inside
    cands.sort(key=lambda c: (c[0], -c[3].getLength(), c[1]))
    typed_ranges = []
    results = []
    for prio, a, n, dt, why, stated in cands:
        shaped = struct_shaped(dt)
        row = {"address": "0x%08x" % a, "name": n, "type": dt.getDisplayName(),
               "length": dt.getLength(), "struct_shaped": bool(shaped), "why": why, "status": ""}
        results.append(row)
        addr = toAddr(a)
        blk = mem.getBlock(addr)
        if blk is None:
            row["status"] = "skipped:not-in-memory"
            continue
        if blk.isExecute():
            row["status"] = "skipped:exec-block"
            continue
        inside = [r for r in typed_ranges if r[0] <= a < r[1]]
        if inside:
            row["status"] = "skipped:inside-typed-%s" % inside[0][2]
            continue
        ln = dt.getLength()
        if stated:
            kind, sz = stated
            el = ln if kind == "total" else (
                base_type(dt).getElementLength() if isinstance(base_type(dt), Array) else ln)
            if el != sz:
                row["status"] = "skipped:size-mismatch(header 0x%x, CET note 0x%x)" % (el, sz)
                continue
        end = a + ln
        if end - 1 > blk.getEnd().getOffset():
            row["status"] = "skipped:past-block-end(%s)" % blk.getName()
            continue
        if ln > ptr_size:
            others = [nm for (ad, nm) in data_addrs if a < ad < end and not nm.startswith(n + "_")]
            if others:
                row["status"] = "skipped:covers-%d-named-globals(%s)" % (len(others), ",".join(others[:3]))
                continue
        cur = listing.getDefinedDataAt(addr)
        if cur is not None and cur.getDataType().isEquivalent(dt):
            row["status"] = "same"
            typed_ranges.append((a, end, n))
            continue
        if prio > 0 and not shaped:
            # scalars/strings from HW1 or header declarations: never clobber a differently sized
            # item the analysis (or round 1) already defined there
            cont = listing.getDefinedDataContaining(addr)
            if cont is not None and not cont.getMinAddress().equals(addr) \
                    and not str(cont.getDataType().getName()).startswith("undefined"):
                row["status"] = "skipped:inside-existing-%s@%s" % (cont.getDataType().getName(), cont.getMinAddress())
                continue
            if cur is not None and cur.getLength() != ln \
                    and not str(cur.getDataType().getName()).startswith("undefined"):
                row["status"] = "skipped:conflicts-existing-%s(len %d)" % (cur.getDataType().getName(), cur.getLength())
                continue
        if not do_apply:
            row["status"] = "would-apply"
            continue
        try:
            DataUtilities.createData(prog, addr, dt, -1,
                                     DataUtilities.ClearDataMode.CLEAR_ALL_CONFLICT_DATA)
            row["status"] = "applied"
            typed_ranges.append((a, end, n))
        except Exception as e:
            row["status"] = "error:%s" % (str(e).splitlines()[0][-120:] if str(e) else repr(e))
    return results


# ---------------------------------------------------------------- main

def parse_args():
    args = {}
    for a in list(getScriptArgs()):
        a = str(a)
        if "=" in a:
            k, v = a.split("=", 1)
            args[k.strip().lower()] = v.strip()
    return args


def main():
    args = parse_args()
    here = os.path.dirname(str(getSourceFile().getAbsolutePath()))
    gdir = os.path.normpath(os.path.join(here, ".."))
    decls_path = args.get("decls") or os.path.join(gdir, "exports", "headers_r2b", "decls.json")
    sym_paths = args.get("symbols") or ",".join(os.path.join(gdir, f) for f in (
        "cet_symbols.json", "symbols_round1.json", "symbols_round1_data.json"))
    report = args.get("report") or os.path.join(os.path.dirname(decls_path), "import_headers_report.json")
    category = args.get("category", "/Cataclysm")
    do_apply = args.get("apply", "1") in ("1", "true", "yes")
    if not os.path.isfile(decls_path):
        log("decls file not found, nothing to do: %s (run tools/prep_ghidra_headers.py)" % decls_path)
        return
    doc = json.load(open(decls_path, encoding="utf-8"))
    prog = currentProgram
    dtm = prog.getDataTypeManager()
    stats, externs, copied = import_types(dtm, doc, category)
    log("copied %d parsed types into %s" % (copied, category))
    n_ok = sum(s["ok"] for s in stats.values())
    n_bad = sum(s["failed"] for s in stats.values())
    log("declarations parsed ok=%d failed=%d; headers parsed=%d partial=%d skipped=%d" % (
        n_ok, n_bad, sum(1 for s in stats.values() if s["status"] == "parsed"),
        sum(1 for s in stats.values() if s["status"] == "partial"),
        sum(1 for s in stats.values() if s["status"] == "skipped")))
    chk = check_asserts(dtm, doc.get("asserts", []), category)
    log("layout asserts pass=%d fail=%d unresolved=%d" % (chk["pass"], len(chk["fail"]), len(chk["unresolved"])))
    for f in chk["fail"][:20]:
        log("  assert FAIL " + f)
    symbols = load_symbols([p for p in sym_paths.split(",") if p])
    try:
        applied = apply_globals(prog, dtm, symbols, doc.get("globals") or {}, externs, category, do_apply)
    except Exception:
        log("apply_globals crashed:\n" + traceback.format_exc())
        applied = []
    by = {}
    for r in applied:
        k = r["status"].split(":")[0]
        by[k] = by.get(k, 0) + 1
    log("globals: %s" % ", ".join("%s=%d" % kv for kv in sorted(by.items())))
    shaped_ok = [r for r in applied if r["status"] in ("applied", "same") and r["struct_shaped"]]
    log("struct-shaped globals typed: %d; scalar globals typed: %d" % (
        len(shaped_ok), sum(1 for r in applied if r["status"] in ("applied", "same") and not r["struct_shaped"])))
    for r in shaped_ok:
        log("  %s %s : %s (%s)" % (r["address"], r["name"], r["type"], r["status"]))
    out = {"decls": decls_path, "category": category, "headers": sorted(stats.values(), key=lambda s: s["header"]),
           "extern_declarations": len(externs), "asserts": chk, "globals": applied,
           "fixups": doc.get("fixups", [])}
    try:
        with open(report, "w", encoding="utf-8") as fh:
            json.dump(out, fh, indent=1)
        log("report: %s" % report)
    except Exception as e:
        log("could not write report %s: %s" % (report, e))


def _in_ghidra():
    try:
        return currentProgram is not None  # PyGhidra injects it; absent when imported for tests
    except NameError:
        return False


if _in_ghidra():
    try:
        main()
    except Exception:
        log("unexpected error (run continues):\n" + traceback.format_exc())
