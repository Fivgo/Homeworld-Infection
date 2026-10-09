# Apply a CET-derived symbol file (ghidra/cet_symbols.json) to the current program.
#
# JSON shape:
#   {"target": {...optional: exe/file/name, sha256, md5, image_base, version...},
#    "symbols": [{"address": "0x...", "name": "...",
#                 "kind": "function|data|vtable|string|patch_site",
#                 "type": "...", "source": "...", "hw1_equivalent": "...", "notes": "..."}]}
#
# Args (key=value, any order; a bare first arg is taken as json=):
#   json=<path>     default: $CET_SYMBOLS, else <scripts dir>/../cet_symbols.json
#   phase=pre|post|all   pre  = seed before auto-analysis (disassemble + create functions +
#                               labels + data types); no prototypes yet (see post)
#                        post = re-assert names/comments after analysis, apply prototypes (so the
#                               analysed calling convention is kept), verify, name patch-site
#                               containers, write report
#                        all  = both in one go (default; use when running on an analysed program)
#   report=<csv>    write a per-symbol status CSV (post/all phases)
#   force=1         apply even if target sha256/md5/image_base disagree with the program
#   tag=CET         function tag / bookmark category (default CET)
#   apply_proposals=1   (post/all) name a still-unnamed function that contains patch sites
#                   whose hw1_equivalent fields all agree; source ANALYSIS, tag CET_container
#
# Tolerant by design: a missing file, empty file, bad JSON, unknown kinds, addresses outside
# the program, and unparseable types are reported and skipped; the script never aborts the
# headless run.
#
# Source type IMPORTED is used for every name, so (a) auto-analysis (DEFAULT/ANALYSIS) never
# overrides it and (b) a name an analyst typed by hand (USER_DEFINED) is never clobbered.
#
# @category Homeworld-Infection
# @runtime PyGhidra

import typing
if typing.TYPE_CHECKING:
    from ghidra.ghidra_builtins import *  # noqa: F401,F403

import csv
import json
import os
import re

from ghidra.app.cmd.function import ApplyFunctionSignatureCmd, CreateFunctionCmd
from ghidra.app.util import NamespaceUtils
from ghidra.app.util.cparser.C import CParserUtils
from ghidra.program.model.address import AddressSet
from ghidra.program.model.data import DataUtilities
from ghidra.program.model.listing import CommentType
from ghidra.program.model.symbol import SourceType, SymbolType, SymbolUtilities
from ghidra.util.data import DataTypeParser

KINDS = ("function", "data", "vtable", "string", "patch_site")
MARK = "[CET]"


def log(msg):
    println("[ImportSymbolsJson] " + msg)


def parse_args():
    args = {}
    raw = list(getScriptArgs())
    for i, a in enumerate(raw):
        a = str(a)
        if "=" in a:
            k, v = a.split("=", 1)
            args[k.strip().lower()] = v.strip()
        elif i == 0:
            args["json"] = a
    return args


def default_json_path():
    env = os.environ.get("CET_SYMBOLS")
    if env:
        return env
    try:
        script_dir = os.path.dirname(str(getSourceFile().getAbsolutePath()))
        return os.path.normpath(os.path.join(script_dir, "..", "cet_symbols.json"))
    except Exception:
        return "cet_symbols.json"


def parse_int(v):
    if v is None:
        return None
    if isinstance(v, int):
        return v
    s = str(v).strip().lower().replace("`", "").replace("_", "")
    if not s:
        return None
    try:
        if s.startswith("0x"):
            return int(s, 16)
        if re.fullmatch(r"[0-9a-f]+h", s):
            return int(s[:-1], 16)
        return int(s, 16)  # address strings are always hex, e.g. "00524841"
    except ValueError:
        return None


def check_target(target, force):
    """Return (ok, problems). Hash/image-base mismatch blocks unless force."""
    problems, blocking = [], False
    if not isinstance(target, dict):
        return True, problems
    sha = (target.get("sha256") or "").lower()
    md5 = (target.get("md5") or "").lower()
    prog_sha = (currentProgram.getExecutableSHA256() or "").lower()
    prog_md5 = (currentProgram.getExecutableMD5() or "").lower()
    if sha and prog_sha and sha != prog_sha:
        problems.append("sha256 mismatch: json=%s program=%s" % (sha, prog_sha))
        blocking = True
    if md5 and prog_md5 and md5 != prog_md5:
        problems.append("md5 mismatch: json=%s program=%s" % (md5, prog_md5))
        blocking = True
    ib = parse_int(target.get("image_base") or target.get("imagebase") or target.get("base"))
    if ib is not None:
        prog_ib = currentProgram.getImageBase().getOffset()
        if ib != prog_ib:
            problems.append("image_base mismatch: json=0x%x program=0x%x" % (ib, prog_ib))
            blocking = True
    fname = target.get("exe") or target.get("file") or target.get("filename") or target.get("name")
    if fname and str(fname).lower() != str(currentProgram.getName()).lower():
        problems.append("file name differs (informational): json=%s program=%s"
                        % (fname, currentProgram.getName()))
    return (not blocking) or force, problems


def clean_name(name):
    name = str(name).strip()
    name = re.sub(r"\s+", "_", name)
    return SymbolUtilities.replaceInvalidChars(name, True)


def split_namespace(name):
    if "::" in name:
        ns, base = name.rsplit("::", 1)
        try:
            return NamespaceUtils.createNamespaceHierarchy(ns, None, currentProgram,
                                                           SourceType.IMPORTED), base
        except Exception:
            return None, name.replace("::", "__")
    return None, name


def add_comment(addr, ctype, text):
    listing = currentProgram.getListing()
    line = "%s %s" % (MARK, text)
    old = listing.getComment(ctype, addr)
    if old and line in old:
        return
    listing.setComment(addr, ctype, (old + "\n" + line) if old else line)


def describe(sym):
    parts = []
    for k in ("type", "hw1_equivalent", "source", "notes"):
        v = sym.get(k)
        if v not in (None, "", []):
            parts.append("%s=%s" % (k, v))
    return "; ".join(parts)


# HW1/CET primitive typedefs (CET game/Game/TYPES.h:11-24) -> Ghidra built-ins
HW1_TYPEDEFS = {
    "sbyte": "char", "ubyte": "uchar", "sword": "short", "uword": "ushort",
    "sdword": "int", "udword": "uint", "sqword": "longlong", "uqword": "ulonglong",
    "real32": "float", "real64": "double", "bool8": "char", "bool16": "short",
    "bool32": "int", "size_t": "uint",
}
CC_WORDS = ("__cdecl", "__stdcall", "__fastcall", "__thiscall", "WINAPI", "CALLBACK")


def known_type(base):
    try:
        dtm = currentProgram.getDataTypeManager()
        DataTypeParser(dtm, dtm, None, DataTypeParser.AllowedDataTypes.ALL).parse(base)
        return True
    except Exception:
        return False


def degrade_type(tokens, stars):
    """Map a C type to something Ghidra already knows: HW1 typedefs -> built-ins, unknown
    pointers -> void *, unknown by-value types (enums, handles) -> undefined4."""
    words = [HW1_TYPEDEFS.get(w, w) for w in tokens if w not in ("const", "struct", "enum",
                                                                  "union", "volatile")]
    base = " ".join(words) or "int"
    if not known_type(base):
        return ("void" + " *" * stars) if stars else "undefined4"
    return base + (" " + "*" * stars if stars else "")


def degraded_prototype(ret, args, name):
    """'Player* player, ShipType t, udword n' -> 'void * player, undefined4 t, uint n'.
    Keeps the parameter count and pointer-ness, which is what the decompiler needs most."""
    cc = [w for w in CC_WORDS if re.search(r"\b%s\b" % w, ret)]
    cc = ["__stdcall" if c in ("WINAPI", "CALLBACK") else c for c in cc]
    ret = re.sub(r"\b(%s)\b" % "|".join(CC_WORDS), "", ret).strip()
    # 'void allianceFormWith' -> 'void' ; 'ShipStaticInfo* GetShipStaticInfo' -> 'ShipStaticInfo*'
    ret = re.sub(r"(?<=[\w*\s])\s*\b%s\s*$" % re.escape(name), "", ret).strip()
    m2 = re.match(r"^(.*?[\w*])\s+([A-Za-z_]\w*)$", ret)
    if m2 and re.search(r"[A-Za-z_]", m2.group(1)) and not known_type(m2.group(2)):
        ret = m2.group(1)
    rtoks = re.findall(r"[A-Za-z_]\w*", ret)
    rret = degrade_type(rtoks, ret.count("*")) if rtoks and rtoks != ["void"] or "*" in ret \
        else "void"
    out = []
    inner = args.strip()[1:-1].strip()
    parts, depth, cur = [], 0, ""
    for ch in inner:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur)
    for i, a in enumerate(parts):
        a = a.strip()
        if a in ("", "void"):
            continue
        if a == "...":
            out.append("...")
            continue
        stars = a.count("*") + (1 if "[" in a else 0)   # T x[N] -> T *x
        a = re.sub(r"\[.*?\]", "", a).replace("*", " ")
        toks = re.findall(r"[A-Za-z_]\w*", a)
        pname = "p%d" % (i + 1)
        if len(toks) >= 2:
            pname = toks[-1]
            toks = toks[:-1]
        out.append("%s %s" % (degrade_type(toks, stars), pname))
    return "%s %s%s(%s)" % (rret, (cc[0] + " ") if cc else "", name, ", ".join(out) or "void")


def try_signature(func, sym, name):
    """Best effort: apply a C prototype. Returns a status string."""
    t = sym.get("type")
    if not t or not isinstance(t, str) or "(" not in t:
        return "no-proto"
    t = t.strip().rstrip(";")
    t = re.sub(r"\b(WINAPI|CALLBACK)\b", "__stdcall", t)
    candidates = [t]
    # "ret (args)" / "ret (*)(args)" -> "ret name(args)"
    m = re.match(r"^(.*?)\(\s*\*?\s*\)\s*(\(.*\))$", t)
    if m:
        candidates.append("%s %s%s" % (m.group(1).strip(), name, m.group(2)))
    m = re.match(r"^([^()]*?)\s*(\(.*\))$", t)
    if m and not re.search(r"\b%s\s*\(" % re.escape(name), t):
        candidates.append("%s %s%s" % (m.group(1).strip() or "void", name, m.group(2)))
    degraded = None
    m = re.match(r"^([^()]*?)\s*(?:\(\s*\*?\s*\))?\s*(\(.*\))$", t)
    if m:
        try:
            degraded = degraded_prototype(m.group(1) or "void", m.group(2), name)
            candidates.append(degraded)
        except Exception:
            degraded = None
    for c in candidates:
        try:
            fdef = CParserUtils.parseSignature(None, currentProgram, c, False)
        except Exception:
            fdef = None
        if fdef is None:
            continue
        fdef.setName(func.getName())
        cmd = ApplyFunctionSignatureCmd(func.getEntryPoint(), fdef, SourceType.IMPORTED)
        if cmd.applyTo(currentProgram, monitor):
            return "proto-applied-degraded" if c == degraded and c != candidates[0] \
                else "proto-applied"
    return "proto-unparsed"


def clean_data_type(t):
    """'real32 (constant 240.0f)' -> 'float'; 'Ship* (pointer variable)' -> 'Ship *';
    'udword[TOTAL_STD_SHIPS=60]' -> 'uint[60]'; 'char[512?]' / 'end of X' -> None."""
    t = re.sub(r"\s*\(.*\)\s*$", "", t.strip())
    if not t or "(" in t or "?" in t or re.match(r"(end|start) of\b", t):
        return None
    m = re.match(r"^(.*?)\s*\[\s*(?:[A-Za-z_]\w*\s*=\s*)?(\d*)\s*\]$", t)
    count = None
    if m:
        t, count = m.group(1), m.group(2)
        if not count:
            return None  # T[] : unknown length
    words = re.findall(r"[A-Za-z_]\w*|\*", t)
    words = [HW1_TYPEDEFS.get(w, w) for w in words if w not in ("const", "struct", "volatile")]
    base = " ".join(w for w in words if w != "*")
    stars = words.count("*")
    out = base + (" " + "*" * stars if stars else "")
    if not known_type(base):
        if not stars:
            return None
        out = "void " + "*" * stars
    return out + ("[%s]" % count if count else "")


def try_data_type(addr, sym):
    t = sym.get("type")
    if not t or not isinstance(t, str):
        return "no-type"
    block = currentProgram.getMemory().getBlock(addr)
    if block is None or block.isExecute():
        return "type-skipped-exec-block"
    ct = clean_data_type(t)
    if ct is None:
        return "type-unknown"
    try:
        dtm = currentProgram.getDataTypeManager()
        parser = DataTypeParser(dtm, dtm, None, DataTypeParser.AllowedDataTypes.FIXED_LENGTH)
        dt = parser.parse(ct)
    except Exception:
        return "type-unparsed:%s" % ct
    cur = currentProgram.getListing().getDefinedDataAt(addr)
    if cur is not None and cur.getDataType().isEquivalent(dt):
        return "type-same"
    try:
        # CET types are authoritative over analysis guesses: clear conflicting data
        # (never code: executable blocks were excluded above)
        DataUtilities.createData(currentProgram, addr, dt, -1,
                                 DataUtilities.ClearDataMode.CLEAR_ALL_CONFLICT_DATA)
        return "type-applied"
    except Exception as e:  # conflicts with instructions etc.
        return "type-conflict:%s" % (str(e).splitlines()[0][-160:])


def set_function_name(func, ns, base):
    cur = func.getSymbol()
    if cur.getSource() == SourceType.USER_DEFINED and func.getName() != base:
        # never clobber a hand-typed name; keep ours as a secondary label
        currentProgram.getSymbolTable().createLabel(func.getEntryPoint(), base, ns,
                                                    SourceType.IMPORTED)
        return "kept-user-name"
    if func.getName() != base or (ns is not None and func.getParentNamespace() != ns):
        # an analyzer (e.g. the demangler) may have taken over the function name and left ours
        # behind as a secondary label at the entry: drop that label so the rename can succeed
        st = currentProgram.getSymbolTable()
        for other in list(st.getSymbols(func.getEntryPoint())):
            if other.getName() == base and other.getSymbolType() != SymbolType.FUNCTION:
                other.delete()
        if ns is not None:
            func.getSymbol().setNameAndNamespace(base, ns, SourceType.IMPORTED)
        else:
            func.setName(base, SourceType.IMPORTED)
        return "renamed"
    return "same-name"


def apply_function(addr, sym, name, phase, tag):
    ns, base = split_namespace(name)
    listing = currentProgram.getListing()
    func = getFunctionAt(addr)
    detail = []
    outer = getFunctionContaining(addr) if func is None else None
    if outer is not None:
        # never carve a new function out of the middle of an existing one: a CET "function"
        # address that lands mid-body means the JSON (or the binary version) is wrong
        detail.append("inside:%s@%s" % (outer.getName(True), outer.getEntryPoint()))
    elif func is None and phase in ("pre", "all"):
        if listing.getInstructionAt(addr) is None and listing.getDefinedDataAt(addr) is None:
            disassemble(addr)
        cmd = CreateFunctionCmd(base, addr, None, SourceType.IMPORTED)
        if cmd.applyTo(currentProgram, monitor):
            func = getFunctionAt(addr)
            detail.append("created")
        else:
            detail.append("create-failed:%s" % cmd.getStatusMsg())
    if func is None:
        container = getFunctionContaining(addr)
        lbl = currentProgram.getSymbolTable().createLabel(addr, base, ns, SourceType.IMPORTED)
        lbl.setPrimary()
        add_comment(addr, CommentType.PRE, "function %s: %s" % (name, describe(sym)))
        return "label-only", container, ";".join(detail)
    detail.append(set_function_name(func, ns, base))
    func.addTag(tag)
    for other in currentProgram.getFunctionManager().getFunctionsOverlapping(AddressSet(addr, addr)):
        if other.getEntryPoint() != addr:
            detail.append("overlaps:%s@%s" % (other.getName(True), other.getEntryPoint()))
    add_comment(addr, CommentType.PLATE, "%s  %s" % (name, describe(sym)))
    if phase in ("post", "all"):
        # only after analysis: a prototype without a convention keyword then keeps the
        # analysed convention/stack purge (applied pre-analysis it left "unknown")
        detail.append(try_signature(func, sym, base))
    return "function", func, ";".join(detail)


def apply_label(addr, sym, name, kind):
    ns, base = split_namespace(name)
    st = currentProgram.getSymbolTable()
    existing = [s for s in st.getSymbols(addr) if s.getName() == base]
    if existing:
        sym_obj = existing[0]
    else:
        sym_obj = st.createLabel(addr, base, ns, SourceType.IMPORTED)
    prim = st.getPrimarySymbol(addr)
    if prim is None or prim.getSource() in (SourceType.DEFAULT, SourceType.ANALYSIS):
        sym_obj.setPrimary()
    detail = try_data_type(addr, sym) if kind == "data" else ""
    add_comment(addr, CommentType.PRE, "%s %s: %s" % (kind, name, describe(sym)))
    return detail


def apply_patch_site(addr, sym, name, tag):
    container = getFunctionContaining(addr)
    st = currentProgram.getSymbolTable()
    base = name
    ns = container if container is not None else None
    have = [s for s in st.getSymbols(addr) if s.getName() == base]
    if not have:
        st.createLabel(addr, base, ns, SourceType.IMPORTED)
    elif ns is not None and have[0].getParentNamespace() != ns:
        # created as a global before analysis existed; move it under its function now
        have[0].setNamespace(ns)
    add_comment(addr, CommentType.PRE, "patch site %s: %s" % (name, describe(sym)))
    currentProgram.getBookmarkManager().setBookmark(addr, "Note", tag,
                                                    "patch site %s" % name)
    return container


def hw1_symbol(eq):
    """'HomeworldSDL/src/Game/ETG.c:etgAtDefine' -> 'etgAtDefine'; 'univUpdate()' -> 'univUpdate'."""
    if not eq:
        return ""
    eq = str(eq).strip()
    if ":" in eq:
        eq = eq.rsplit(":", 1)[1]
    eq = eq.split("(")[0].strip()
    m = re.search(r"[A-Za-z_]\w*$", eq)
    return m.group(0) if m else ""


def proposed_container_name(sym, name):
    eq = hw1_symbol(sym.get("hw1_equivalent"))
    if eq:
        return eq
    m = re.match(r"^([a-z][A-Za-z0-9]+|[A-Z][a-z]+[A-Za-z0-9]*)_", name)
    return m.group(1) if m else ""


def main():
    args = parse_args()
    path = args.get("json") or default_json_path()
    phase = args.get("phase", "all").lower()
    report = args.get("report")
    force = args.get("force", "0") in ("1", "true", "yes")
    tag = args.get("tag", "CET")
    if phase not in ("pre", "post", "all"):
        log("bad phase=%s, using all" % phase)
        phase = "all"

    if not os.path.isfile(path):
        log("symbol file not found, skipping: %s" % path)
        return
    try:
        with open(path, "r", encoding="utf-8") as fh:
            text = fh.read()
        doc = json.loads(text) if text.strip() else {}
    except Exception as e:
        log("could not parse %s (%s), skipping" % (path, e))
        return
    if isinstance(doc, list):
        doc = {"symbols": doc}
    symbols = doc.get("symbols") or []
    ok, problems = check_target(doc.get("target") or {}, force)
    for p in problems:
        log("target check: " + p)
    if not ok:
        log("target does not match this program; not applying (pass force=1 to override)")
        return

    mem = currentProgram.getMemory()
    stats = {}
    rows = []
    proposals = {}  # container entry -> {name: [patch sites]}
    apply_props = args.get("apply_proposals", "1") in ("1", "true", "yes")

    # functions first, ascending, so an enclosing function exists before any address inside it
    def order(item):
        i, sy = item
        if not isinstance(sy, dict):
            return (2, 0, i)
        off = parse_int(sy.get("address"))
        return (0 if str(sy.get("kind") or "").lower() == "function" else 1,
                off if off is not None else 0, i)
    for idx, sym in sorted(enumerate(symbols), key=order):
        if not isinstance(sym, dict):
            stats["bad-entry"] = stats.get("bad-entry", 0) + 1
            continue
        raw_name = sym.get("name") or sym.get("hw1_equivalent") or ""
        kind = str(sym.get("kind") or "data").lower()
        off = parse_int(sym.get("address"))
        row = {"address": sym.get("address"), "name": raw_name, "kind": kind, "status": "",
               "function_at": "", "containing_function": "", "container_entry": "",
               "proposed_container_name": "", "detail": ""}
        rows.append(row)
        if off is None or not raw_name:
            row["status"] = "bad-address-or-name"
        elif kind not in KINDS:
            row["status"] = "unknown-kind"
        else:
            try:
                addr = toAddr(off)
            except Exception:
                addr = None
            if addr is None or not mem.contains(addr):
                row["status"] = "not-in-memory"
            else:
                row["address"] = "0x%08x" % off
                name = clean_name(raw_name)
                try:
                    if kind == "function":
                        st, f, detail = apply_function(addr, sym, name, phase, tag)
                        row["status"] = st
                        row["detail"] = detail
                        if st == "function":
                            row["function_at"] = f.getName(True)
                        elif f is not None:
                            row["containing_function"] = f.getName(True)
                            row["container_entry"] = str(f.getEntryPoint())
                    elif kind == "patch_site":
                        c = apply_patch_site(addr, sym, name, tag)
                        row["status"] = "patch-site"
                        if c is not None:
                            row["containing_function"] = c.getName(True)
                            row["container_entry"] = str(c.getEntryPoint())
                            if c.getSymbol().getSource() in (SourceType.DEFAULT, SourceType.ANALYSIS):
                                prop = proposed_container_name(sym, name)
                                row["proposed_container_name"] = prop
                                if prop and hw1_symbol(sym.get("hw1_equivalent")):
                                    proposals.setdefault(c.getEntryPoint().getOffset(), {}) \
                                             .setdefault(prop, []).append(name)
                        else:
                            row["detail"] = "no-containing-function" + (
                                "-yet(pre-analysis)" if phase == "pre" else "")
                    else:
                        row["detail"] = apply_label(addr, sym, name, kind)
                        row["status"] = "label"
                except Exception as e:
                    row["status"] = "error"
                    row["detail"] = str(e).splitlines()[0][:120] if str(e) else repr(e)
        stats[row["status"]] = stats.get(row["status"], 0) + 1

    # containers of patch sites whose hw1_equivalent agrees -> name them (ANALYSIS source,
    # so any IMPORTED/USER_DEFINED evidence later wins); disagreements are only reported
    if apply_props and phase in ("post", "all"):
        named = conflicts = 0
        for entry, cands in sorted(proposals.items()):
            f = getFunctionAt(toAddr(entry))
            if f is None:
                continue
            if len(cands) != 1:
                conflicts += 1
                add_comment(f.getEntryPoint(), CommentType.PLATE,
                            "conflicting container names from patch sites: %s" % cands)
                continue
            prop = clean_name(list(cands)[0])
            taken = [g for g in getGlobalFunctions(prop) if g.getEntryPoint() != f.getEntryPoint()]
            if taken:
                log("container %s: name %s already used at %s; not renaming"
                    % (f.getEntryPoint(), prop, taken[0].getEntryPoint()))
                conflicts += 1
                continue
            if f.getSymbol().getSource() in (SourceType.DEFAULT, SourceType.ANALYSIS) \
                    and f.getName() != prop:
                try:
                    f.setName(prop, SourceType.ANALYSIS)
                except Exception as e:
                    log("could not name container %s as %s: %s" % (f.getEntryPoint(), prop, e))
                    continue
                f.addTag(tag + "_container")
                add_comment(f.getEntryPoint(), CommentType.PLATE,
                            "named from patch site(s) %s" % ", ".join(list(cands.values())[0]))
                named += 1
        log("container names applied=%d conflicts=%d" % (named, conflicts))

    log("phase=%s file=%s symbols=%d results=%s"
        % (phase, path, len(symbols), ", ".join("%s:%d" % kv for kv in sorted(stats.items()))))

    if report and phase in ("post", "all"):
        d = os.path.dirname(report)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(report, "w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()) if rows else ["address"])
            w.writeheader()
            for r in rows:
                w.writerow(r)
        log("report: %s" % report)


main()
