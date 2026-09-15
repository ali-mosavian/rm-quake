"""qcstrsplit.py -- statement strings by when they are needed, what string compares compare, and the map
entity strings.

    python3 qcstrsplit.py PAK0.PAK

A statement string counts once, under the hottest of its consumers: run time (compares, sound, setmodel,
find, field and global stores, QuakeC calls), print (bprint, sprint, centerprint, dprint, objerror, error,
WriteString) or load (precache_*, ambientsound). A compare operand is a string constant, an entity field
loaded by LOAD_S, a named global (mapname), a call's return, or a local or parameter.
"""

import re
import struct
import sys
from collections import Counter

import mdlview
from qcglobals import writers
from qcpack import MAPS, parse
from qcstr import string_uses

PRINT = {"bprint", "sprint", "centerprint", "dprint", "objerror", "error", "WriteString"}
LOAD = {"precache_sound", "precache_sound2", "precache_model", "precache_model2", "precache_file",
        "precache_file2", "ambientsound"}
OP_EQ_S, OP_NE_S, OP_NOT_S, OP_LOAD_S = 12, 17, 46, 26
OP_CALL0, OP_CALL8 = 51, 59
EV_STRING = 1
OFS_RETURN = 1
NUMERIC = re.compile(r"^-?[\d.]+( -?[\d.]+)*$")


def when(consumers: Counter) -> str:
    names = {c.split()[-1].removesuffix("()") for c in consumers}
    kinds = {"print" if n in PRINT else "load" if n in LOAD else "run" for n in names}
    return "run" if "run" in kinds else "print" if "print" in kinds else "load"


def compare_operands(p: bytes, pr) -> Counter:
    h = struct.unpack_from("<16i", p, 0)
    st_ofs, gd_ofs, gd_n = h[2], h[4], h[5]
    defs = {ofs: t & 0x7FFF for t, ofs, _ in (struct.unpack_from("<HHi", p, gd_ofs + i * 8) for i in range(gd_n))}
    local = set().union(*(set(r) for r in pr.locals_words.values()))
    written = set(writers(p, pr))
    out: Counter = Counter()
    for rng in pr.body.values():
        src: dict[int, str] = {}
        for j in rng:
            op, a, b, c = struct.unpack_from("<HHHH", p, st_ofs + j * 8)
            if op == OP_LOAD_S:
                src[c] = "entity field"
            elif OP_CALL0 <= op <= OP_CALL8:
                src[OFS_RETURN] = "call return"
            if op not in (OP_EQ_S, OP_NE_S, OP_NOT_S):
                continue
            for g in (a,) if op == OP_NOT_S else (a, b):
                out[src.get(g) or ("local or parameter" if g in local
                                   else "system global" if g < pr.end_sys_globals
                                   else "written global" if g in written
                                   else "string constant" if defs.get(g) == EV_STRING
                                   else f"global {g}")] += 1
    return out


def main() -> None:
    blob, entries = mdlview.read_pak(sys.argv[1])
    p = mdlview.pak_read(blob, entries, "progs.dat")
    pr = parse(p)
    uses, size, _ = string_uses(p, pr)
    split: Counter = Counter()
    for o, consumers in uses.items():
        split[when(consumers)] += size[o]
    print("statement strings by when they are needed:", dict(split), f"total {sum(split.values())} B")
    print("string compare operands:", dict(compare_operands(p, pr)))
    for m in MAPS:
        bsp = mdlview.pak_read(blob, entries, f"maps/{m}.bsp")
        ofs, ln = struct.unpack_from("<ii", bsp, 4)
        text = bsp[ofs:ofs + ln].decode("latin-1")
        values = [v for k, v in re.findall(r'"([^"]*)"\s*"([^"]*)"', text) if not NUMERIC.match(v)]
        print(f"  {m:5} entity strings: {sum(len(v) + 1 for v in set(values)):5} B unique, "
              f"{sum(len(v) + 1 for v in values):5} B as stored")


if __name__ == "__main__":
    main()
