# Propose names for binary functions by matching the string literals they reference against
# the string literals used inside functions of the Homeworld 1 source (HomeworldSDL).
#
# Two modes:
#   Ghidra (PyGhidra post-script, program analysed):
#     -postScript MatchHW1Strings.py src=<dir>[,<dir>...] out=<csv> [apply=0|1] [validate=0|1]
#                 [min_score=1.0] [min_margin=0.5] [minlen=4] [min_strings=3]
#   Standalone (plain python3, builds/inspects the source-side index only):
#     python3 MatchHW1Strings.py --index <src-dir> [<src-dir>...] -o hw1_strings.csv
#
# Scoring, per binary function F and candidate HW1 function h:
#   score(F,h) = sum over strings s referenced by F and used in h of
#                w(s) / (|HW1 functions using s| * |binary functions referencing s|)
#   w(s) = specificity(s) in 0..1 (letters left after removing printf specifiers / 12) for an
#          exact match, half that for a whitespace/case-normalised match.
# A proposal needs score >= min_score, score - runner_up >= min_margin, and either one
# fully-specific string unique on both sides (share 1.0) or >= min_strings matching strings;
# it is kept one-to-one
# (a HW1 name proposed for several binary functions goes to the best score; others: "dup").
# apply=1 renames only DEFAULT-named functions (FUN_*), with SourceType.ANALYSIS and tag HW1_STR,
# so CET (IMPORTED) and hand-typed (USER_DEFINED) names always win.
# validate=1 treats the program's existing names as ground truth (use on a symbolised build of
# HW1 itself) and logs precision/coverage.
#
# @category Homeworld-Infection
# @runtime PyGhidra

import csv
import os
import re
import sys

C_EXT = (".c", ".cpp", ".h", ".hpp")
ESC = {"n": "\n", "t": "\t", "r": "\r", "0": "\0", "a": "\a", "b": "\b", "f": "\f",
       "v": "\v", "\\": "\\", "'": "'", '"': '"', "?": "?"}
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "do", "else", "case"}


# ---------------------------------------------------------------- source side
def unescape(body):
    out, i = [], 0
    while i < len(body):
        ch = body[i]
        if ch != "\\" or i + 1 >= len(body):
            out.append(ch)
            i += 1
            continue
        n = body[i + 1]
        if n == "x":
            m = re.match(r"[0-9a-fA-F]{1,2}", body[i + 2:])
            if m:
                out.append(chr(int(m.group(0), 16)))
                i += 2 + len(m.group(0))
                continue
        if n in "01234567":
            m = re.match(r"[0-7]{1,3}", body[i + 1:])
            out.append(chr(int(m.group(0), 8) & 0xff))
            i += 1 + len(m.group(0))
            continue
        if n == "\n":  # line continuation inside a literal
            i += 2
            continue
        out.append(ESC.get(n, n))
        i += 2
    return "".join(out)


TOKEN = re.compile(r'''
    (?P<ws>\s+)
  | (?P<lc>//[^\n]*)
  | (?P<bc>/\*.*?\*/)
  | (?P<str>L?"(?:[^"\\\n]|\\.|\\\n)*")
  | (?P<chr>L?'(?:[^'\\\n]|\\.)*')
  | (?P<id>[A-Za-z_]\w*)
  | (?P<num>\d[\w.]*)
  | (?P<p>.)
''', re.S | re.M | re.X)

PP_LINE = re.compile(r"^[ \t]*#(?:[^\n]*\\\n)*[^\n]*", re.M)
DEFINE_STR = re.compile(r'^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)[ \t]+((?:L?"(?:[^"\\\n]|\\.)*"[ \t]*)+)\s*$',
                        re.M)


def first_arm_only(text):
    """Keep only the first live arm of every #if/#ifdef/#ifndef chain (an '#if 0' arm is
    dead, so its #else arm is kept instead); blank the other arms, preserving line numbers.
    Without this, '#ifdef A  void f(a) {  #else  void f(a, b) {  #endif' unbalances braces."""
    out = []
    stack = []  # per open #if: [this_arm_live, some_arm_taken, parent_live]
    live = True
    for ln in text.split("\n"):
        m = re.match(r"[ \t]*#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b(.*)", ln)
        if m:
            d, rest = m.group(1), m.group(2).strip()
            if d in ("if", "ifdef", "ifndef"):
                arm = not (d == "if" and re.match(r"0\b", rest))
                stack.append([arm, arm, live])
                live = live and arm
            elif d in ("elif", "else") and stack:
                top = stack[-1]
                arm = not top[1] and not (d == "elif" and re.match(r"0\b", rest))
                top[0] = arm
                top[1] = top[1] or arm
                live = top[2] and arm
            elif d == "endif" and stack:
                live = stack.pop()[2]
            out.append("")
            continue
        out.append(ln if live else "")
    return "\n".join(out)


def scan_file(path, macro_strings, unbalanced=None):
    """Yield (string, owner, kind, line) for every string literal; owner = enclosing function
    (kind 'function') or the global being initialised (kind 'data')."""
    try:
        text = open(path, "r", encoding="latin-1").read()
    except OSError:
        return
    text = first_arm_only(text)
    # blank out preprocessor lines (keeping line numbers)
    text = PP_LINE.sub(lambda m: "\n" * m.group(0).count("\n"), text)
    depth = 0
    owner, kind = None, None
    stmt = []            # tokens since last ';' / '}' at depth 0
    pending = []         # adjacent literal pieces (concatenation)
    pending_line = 0
    line = 1
    for m in TOKEN.finditer(text):
        typ = m.lastgroup
        val = m.group(typ)
        tok_line = line
        line += val.count("\n")
        if typ in ("ws", "lc", "bc"):
            continue
        if typ == "str":
            if not pending:
                pending_line = tok_line
            body = val[2:-1] if val.startswith("L") else val[1:-1]
            pending.append(unescape(body))
            continue
        if pending:
            s = "".join(pending)
            pending = []
            if depth > 0 and owner:
                yield s, owner, kind, pending_line
            elif depth > 0:
                yield s, "<anon>", "data", pending_line
        if typ == "id" and depth > 0 and owner and val in macro_strings:
            for s in macro_strings[val]:
                yield s, owner, kind, tok_line
        if typ == "p" and val == "{":
            if depth == 0:
                owner, kind = classify(stmt)
            depth += 1
            continue
        if typ == "p" and val == "}":
            depth = max(0, depth - 1)
            if depth == 0:
                owner, kind = None, None
                stmt = []
            continue
        if depth == 0:
            if typ == "p" and val == ";":
                stmt = []
            else:
                stmt.append(val)
    if depth != 0 and unbalanced is not None:
        unbalanced.append((path, depth))


def classify(stmt):
    """Decide what a top-level '{' opens."""
    if "=" in stmt:  # initialiser: owner is the identifier before '=' (skip [..])
        i = stmt.index("=") - 1
        while i >= 0 and not re.match(r"[A-Za-z_]\w*$", stmt[i]):
            i -= 1
        return (stmt[i] if i >= 0 else "<data>"), "data"
    if stmt and stmt[-1] == ")":
        # walk back over the parameter list to its '(' and take the identifier before it
        d, i = 0, len(stmt) - 1
        while i >= 0:
            if stmt[i] == ")":
                d += 1
            elif stmt[i] == "(":
                d -= 1
                if d == 0:
                    break
            i -= 1
        j = i - 1
        if j >= 0 and stmt[j] == ")":  # function returning a function pointer: rare, skip
            return None, None
        if j >= 0 and re.match(r"[A-Za-z_]\w*$", stmt[j]) and stmt[j] not in KEYWORDS:
            inner = stmt[i + 1:-1]
            # function-defining macro, e.g. HW1 Task.h:149  DEFINE_TASK(name) -> void name(...)
            if re.match(r"[A-Z][A-Z0-9_]*$", stmt[j]) and len(inner) == 1 and \
                    re.match(r"[A-Za-z_]\w*$", inner[0]):
                return inner[0], "function"
            return stmt[j], "function"
    if stmt and stmt[0] in ("struct", "union", "enum", "typedef", "extern"):
        return None, None
    return None, None


def build_index(src_dirs):
    files = []
    for d in src_dirs:
        for root, _dirs, names in os.walk(d):
            if "ThirdParty" in root.split(os.sep):
                continue
            for n in names:
                if n.lower().endswith(C_EXT):
                    files.append(os.path.join(root, n))
    macro_strings = {}
    for f in files:
        try:
            text = open(f, "r", encoding="latin-1").read()
        except OSError:
            continue
        for m in DEFINE_STR.finditer(text):
            parts = re.findall(r'L?"((?:[^"\\\n]|\\.)*)"', m.group(2))
            macro_strings.setdefault(m.group(1), set()).add(unescape("".join(parts)))
    rows = []
    unbalanced = []
    for f in sorted(files):
        rel = f
        for d in src_dirs:
            if f.startswith(d):
                rel = os.path.relpath(f, os.path.dirname(d.rstrip(os.sep)))
        for s, owner, kind, ln in scan_file(f, macro_strings, unbalanced):
            rows.append((s, owner, kind, rel, ln))
    for path, depth in unbalanced:
        print("[MatchHW1Strings] WARNING unbalanced braces (depth %d at EOF, #if arms?): %s"
              % (depth, path))
    return rows


FMT = re.compile(r"%[-+ #0]*(\d+|\*)?(\.(\d+|\*))?[hlLqjzt]*[diouxXeEfFgGaAcspn%]")


def specificity(s):
    """0..1: how much a string identifies code. Format specifiers, punctuation and short words
    count little ('%.2f' -> 0, 'Free' -> 0.33, 'Error - Attack Type Not Found' -> 1)."""
    letters = len(re.findall(r"[A-Za-z]", FMT.sub("", s)))
    if letters < 3:
        return 0.0
    return min(1.0, letters / 12.0)


def norm(s):
    return re.sub(r"\s+", " ", s).strip().lower()


def index_maps(rows, minlen):
    exact, loose = {}, {}
    for s, owner, kind, rel, ln in rows:
        if kind != "function" or len(s) < minlen or not re.search(r"[A-Za-z]", s):
            continue
        exact.setdefault(s, {}).setdefault(owner, "%s:%d" % (rel, ln))
        loose.setdefault(norm(s), {}).setdefault(owner, "%s:%d" % (rel, ln))
    return exact, loose


def standalone(argv):
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--index", nargs="+", required=True)
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--minlen", type=int, default=4)
    a = ap.parse_args(argv)
    dirs = [os.path.abspath(d) for d in a.index]
    rows = build_index(dirs)
    with open(a.out, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["string", "owner", "kind", "file", "line"])
        for s, owner, kind, rel, ln in rows:
            w.writerow([s.encode("unicode_escape").decode("ascii"), owner, kind, rel, ln])
    exact, _ = index_maps(rows, a.minlen)
    funcs = {o for s, o, k, _, _ in rows if k == "function"}
    unique = sum(1 for s, owners in exact.items() if len(owners) == 1)
    owners_with_unique = {list(o)[0] for s, o in exact.items() if len(o) == 1}
    print("literal occurrences=%d  functions-with-literals=%d  distinct strings(len>=%d)=%d  "
          "unique-to-one-function=%d  functions owning >=1 unique string=%d"
          % (len(rows), len(funcs), a.minlen, len(exact), unique, len(owners_with_unique)))


# ---------------------------------------------------------------- Ghidra side
def ghidra_main():
    from ghidra.program.model.symbol import SourceType

    args = {}
    for a in list(getScriptArgs()):
        a = str(a)
        if "=" in a:
            k, v = a.split("=", 1)
            args[k.strip().lower()] = v.strip()
    here = os.path.dirname(str(getSourceFile().getAbsolutePath()))
    root = os.path.normpath(os.path.join(here, "..", ".."))
    src = args.get("src") or os.path.join(root, "reference", "HomeworldSDL", "src")
    out = args.get("out") or os.path.join(root, "ghidra", "exports", currentProgram.getName(),
                                          "hw1_string_matches.csv")
    apply = args.get("apply", "0") in ("1", "true", "yes")
    validate = args.get("validate", "0") in ("1", "true", "yes")
    min_score = float(args.get("min_score", "1.0"))
    min_margin = float(args.get("min_margin", "0.5"))
    minlen = int(args.get("minlen", "4"))
    min_strings = int(args.get("min_strings", "3"))

    dirs = [os.path.abspath(d) for d in src.split(",") if d]
    missing = [d for d in dirs if not os.path.isdir(d)]
    if missing:
        println("[MatchHW1Strings] source dir(s) missing: %s; skipping" % ", ".join(missing))
        return
    rows = build_index(dirs)
    exact, loose = index_maps(rows, minlen)
    println("[MatchHW1Strings] index: %d literal occurrences, %d distinct strings" %
            (len(rows), len(exact)))

    # binary side: string -> referencing functions
    listing = currentProgram.getListing()
    s2f = {}
    it = listing.getDefinedData(True)
    while it.hasNext():
        d = it.next()
        if not d.hasStringValue():
            continue
        v = d.getValue()
        s = str(v) if v is not None else ""
        if len(s) < minlen:
            continue
        fs = s2f.setdefault(s, {})
        for ref in getReferencesTo(d.getAddress()):
            f = getFunctionContaining(ref.getFromAddress())
            if f is not None:
                fs[f.getEntryPoint().getOffset()] = f
    scores = {}     # entry -> {hw1: [score, nstr, loc, evidence]}
    funcs = {}
    for s, fs in s2f.items():
        if not fs:
            continue
        spec = specificity(s)
        if spec == 0.0:
            continue
        cand, w = exact.get(s), spec
        if cand is None:
            cand, w = loose.get(norm(s)), 0.5 * spec
        if not cand:
            continue
        share = w / (len(cand) * len(fs))
        for off, f in fs.items():
            funcs[off] = f
            sc = scores.setdefault(off, {})
            for h, loc in cand.items():
                e = sc.setdefault(h, [0.0, 0, loc, [], 0.0])
                e[0] += share
                e[1] += 1
                e[4] = max(e[4], share)
                if len(e[3]) < 3:
                    e[3].append(s[:60])

    results = []
    for off, sc in scores.items():
        ranked = sorted(sc.items(), key=lambda kv: -kv[1][0])
        best, bs = ranked[0]
        ru, rs = (ranked[1][0], ranked[1][1][0]) if len(ranked) > 1 else ("", 0.0)
        # one strong unique string (share 1.0), or >= min_strings weaker ones
        ok = bs[0] >= min_score and bs[0] - rs >= min_margin and \
            (bs[4] >= 0.999 or bs[1] >= min_strings)
        results.append([off, funcs[off], best, bs, ru, rs, "proposed" if ok else "weak"])
    # one-to-one
    best_for = {}
    for r in results:
        if r[6] == "proposed":
            cur = best_for.get(r[2])
            if cur is None or r[3][0] > cur[3][0]:
                best_for[r[2]] = r
    for r in results:
        if r[6] == "proposed" and best_for.get(r[2]) is not r:
            r[6] = "dup"

    applied = 0
    tp = fp = 0
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["address", "current_name", "current_source", "proposed", "score", "n_strings",
                    "hw1_location", "runner_up", "runner_up_score", "status", "evidence"])
        for off, f, best, bs, ru, rs, status in sorted(results, key=lambda r: r[0]):
            src_t = f.getSymbol().getSource()
            if validate and status == "proposed":
                if f.getName() == best:
                    tp += 1
                else:
                    fp += 1
            if apply and status == "proposed" and src_t == SourceType.DEFAULT:
                try:
                    f.setName(best, SourceType.ANALYSIS)
                    f.addTag("HW1_STR")
                    status = "applied"
                    applied += 1
                except Exception as e:
                    status = "apply-failed:%s" % str(e).splitlines()[0][:60]
            w.writerow(["0x%08x" % off, f.getName(), str(src_t), best, "%.3f" % bs[0], bs[1],
                        bs[2], ru, "%.3f" % rs, status,
                        " | ".join(x.encode("unicode_escape").decode("ascii") for x in bs[3])])
    n_prop = sum(1 for r in results if r[6] == "proposed")
    println("[MatchHW1Strings] binary strings=%d  functions with matches=%d  proposed=%d  "
            "applied=%d  out=%s" % (len(s2f), len(results), n_prop, applied, out))
    if validate:
        println("[MatchHW1Strings] validate: proposed names equal existing names: %d / %d "
                "(precision %.1f%%)" % (tp, tp + fp, 100.0 * tp / max(1, tp + fp)))


try:
    currentProgram  # noqa: B018  (defined only inside Ghidra)
    _IN_GHIDRA = True
except NameError:
    _IN_GHIDRA = False

if _IN_GHIDRA:
    ghidra_main()
elif __name__ == "__main__":
    standalone(sys.argv[1:])
