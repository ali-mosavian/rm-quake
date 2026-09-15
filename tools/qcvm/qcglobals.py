"""qcglobals.py -- what progs.dat's globals are, by who writes them.

    python3 qcglobals.py PAK0.PAK

reserved: OFS_NULL, OFS_RETURN and the eight parm slots. system: up to end_sys_globals, shared with the
engine. locals: the words dfunction.locals names (parameters first). temps: written, unnamed and past a
function's locals -- qcc's expression temporaries. named: written and named by a global def. constants:
nothing writes them (literals, function and field references, initialised globals).
"""

import struct
import sys
from collections import defaultdict

import mdlview
from qcpack import RESERVED_OFS, parse

OP_STORE_F, OP_STORE_FNC = 31, 36
OP_STOREP_F, OP_STOREP_FNC = 37, 42
OP_RETURN, OP_IF, OP_IFNOT, OP_GOTO = 43, 49, 50, 61
OP_CALL0, OP_CALL8, OP_STATE = 51, 59, 60


def writers(p: bytes, pr) -> dict[int, set[int]]:
    st_ofs = struct.unpack_from("<16i", p, 0)[2]
    out: dict[int, set[int]] = defaultdict(set)
    for i, rng in pr.body.items():
        for j in rng:
            op, a, b, c = struct.unpack_from("<HHHH", p, st_ofs + j * 8)
            if OP_STORE_F <= op <= OP_STORE_FNC:
                out[b].add(i)
            elif OP_STOREP_F <= op <= OP_STOREP_FNC or op in (OP_RETURN, OP_IF, OP_IFNOT, OP_GOTO, OP_STATE):
                continue
            elif OP_CALL0 <= op <= OP_CALL8:
                out[1].add(i)
            elif op:
                out[c].add(i)
    return out


def main() -> None:
    p = mdlview.pak_read(*mdlview.read_pak(sys.argv[1]), "progs.dat")
    pr = parse(p)
    h = struct.unpack_from("<16i", p, 0)
    gd_ofs, gd_n = h[4], h[5]
    named = {ofs for _, ofs, _ in (struct.unpack_from("<HHi", p, gd_ofs + i * 8) for i in range(gd_n))}
    w = writers(p, pr)
    local = set().union(*(set(r) for r in pr.locals_words.values()))
    words = range(pr.numglobals)
    groups = {
        "reserved": {g for g in words if g < RESERVED_OFS},
        "system": {g for g in words if RESERVED_OFS <= g < pr.end_sys_globals},
        "locals": local - set(range(pr.end_sys_globals)),
    }
    rest = set(words) - set().union(*groups.values())
    groups["named"] = {g for g in rest if g in w and g in named}
    groups["temps"] = {g for g in rest if g in w and g not in named}
    groups["constants"] = rest - groups["named"] - groups["temps"]
    for label, s in groups.items():
        print(f"{label:10} {len(s):6} words {len(s) * 4:7} B")
    print(f"{'total':10} {pr.numglobals:6} words {pr.numglobals * 4:7} B")
    shared = sum(1 for g in groups["temps"] if len(w[g]) > 1)
    print(f"temps written by more than one function: {shared}")


if __name__ == "__main__":
    main()
