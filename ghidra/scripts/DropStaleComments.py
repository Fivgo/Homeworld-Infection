# Remove stale "[CET] <old name> ..." lines that ImportSymbolsJson.py left in plate/pre comments
# after a rename (ImportSymbolsJson.add_comment only appends, so the old name's note stays).
#
# Args (key=value):
#   drop=<addr>:<oldname>[,<addr>:<oldname>...]   e.g. drop=0x00419e80:aiuGenerateRandomVector
#
# For each item, every comment line (PLATE and PRE) at <addr> that starts with "[CET] <oldname> "
# or "[CET] function <oldname>:" is removed; the other lines are kept. Tolerant: bad items are
# printed and skipped.
#
# @category Homeworld-Infection
# @runtime PyGhidra

import typing
if typing.TYPE_CHECKING:
    from ghidra.ghidra_builtins import *  # noqa: F401,F403

from ghidra.program.model.listing import CommentType


def main():
    items = []
    for a in getScriptArgs():
        if a.startswith("drop="):
            for it in a[5:].split(","):
                if ":" in it:
                    addr, name = it.split(":", 1)
                    items.append((addr.strip(), name.strip()))
    listing = currentProgram.getListing()
    for addr_s, name in items:
        addr = toAddr(addr_s)
        if addr is None:
            print("[DropStaleComments] bad address %s" % addr_s)
            continue
        prefixes = ("[CET] %s " % name, "[CET] function %s:" % name)
        for ctype in (CommentType.PLATE, CommentType.PRE):
            old = listing.getComment(ctype, addr)
            if not old:
                continue
            lines = old.split("\n")
            keep = [l for l in lines if not l.startswith(prefixes)]
            if len(keep) != len(lines):
                listing.setComment(addr, ctype, "\n".join(keep) if keep else None)
                print("[DropStaleComments] %s %s: removed %d line(s) for %s"
                      % (addr_s, ctype, len(lines) - len(keep), name))


main()
