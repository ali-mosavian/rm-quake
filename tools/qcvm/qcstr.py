"""qcstr.py -- what progs.dat's string table holds, and who reads each string.

    python3 qcstr.py PAK0.PAK

Every string is found by what points at it: a string-typed global a statement uses (a constant, since
qcc gives each literal a global), a def's or a function's name, or nothing. A statement's use is
classified by what happens to the string: compared (EQ_S, NE_S, NOT_S), passed to a function or builtin
(stored into an argument word, attributed per call site), stored into an entity field, stored into a
global, or anything else.
"""

import struct
import sys
from collections import Counter, defaultdict

import mdlview
from qcpack import parse

EV_STRING = 1
OP_EQ_S, OP_NE_S, OP_NOT_S = 12, 17, 46
OP_ADDRESS = 30
OP_STORE_V, OP_STORE_S = 32, 33
OP_STOREP_V, OP_STOREP_S = 38, 39
OP_CALL0, OP_CALL8 = 51, 59
OFS_PARM0 = 4


def string_uses(p: bytes, pr) -> tuple[dict[int, Counter], dict[int, int], callable]:
    # every statement-read string offset -> consumer -> uses, each string's size, and a reader
    h = struct.unpack_from("<16i", p, 0)
    st_ofs, gd_ofs, gd_n, fd_ofs, fd_n, fn_ofs, fn_n, str_ofs, str_n, gl_ofs, gl_n = (
        h[2], h[4], h[5], h[6], h[7], h[8], h[9], h[10], h[11], h[12], h[13])
    table = p[str_ofs:str_ofs + str_n]

    def text(o: int) -> bytes:
        return table[o:table.index(b"\0", o)]

    def gword(g: int) -> int:
        return struct.unpack_from("<i", p, gl_ofs + g * 4)[0]

    gdefs = [struct.unpack_from("<HHi", p, gd_ofs + i * 8) for i in range(gd_n)]
    fdefs = [struct.unpack_from("<HHi", p, fd_ofs + i * 8) for i in range(fd_n)]
    fns = [struct.unpack_from("<7i", p, fn_ofs + i * 36) for i in range(fn_n)]
    str_globals = {ofs: gword(ofs) for t, ofs, _ in gdefs if t & 0x7FFF == EV_STRING}
    field_name = {ofs: text(s).decode("latin-1") for t, ofs, s in fdefs}
    field_of_global = {g: field_name[gword(g)] for g in range(gl_n) if gword(g) in field_name and gword(g) > 0}
    fn_of_global = {ofs: gword(ofs) for t, ofs, _ in gdefs if t & 0x7FFF == 6 and gword(ofs) > 0}

    uses: dict[int, Counter] = defaultdict(Counter)       # string offset -> consumer -> count
    for i, rng in pr.body.items():
        args: dict[int, int] = {}
        addressed: dict[int, str] = {}
        for j in rng:
            op, a, b, c = struct.unpack_from("<HHHH", p, st_ofs + j * 8)
            if op == OP_ADDRESS and b in field_of_global:
                addressed[c] = field_of_global[b]
            if OP_CALL0 <= op <= OP_CALL8:
                callee = fn_of_global.get(a)
                who = text(fns[callee][4]).decode("latin-1") if callee is not None else "<through a field>"
                kind = "builtin" if callee is not None and fns[callee][0] < 0 else "qc"
                for word, src in args.items():
                    if src in str_globals:
                        uses[str_globals[src]][f"{kind} {who}()"] += 1
                args = {}
                continue
            for g in {a, b} & set(str_globals):
                if op in (OP_EQ_S, OP_NE_S, OP_NOT_S):
                    uses[str_globals[g]]["compare"] += 1
                elif op in (OP_STORE_S, OP_STORE_V) and g == a and OFS_PARM0 <= b < OFS_PARM0 + 24:
                    args[b] = a
                elif op in (OP_STOREP_S, OP_STOREP_V) and g == a:
                    uses[str_globals[g]][f"field .{addressed.get(b, '?')}"] += 1
                elif op in (OP_STORE_S, OP_STORE_V) and g == a:
                    uses[str_globals[g]]["global store"] += 1
                else:
                    uses[str_globals[g]][f"op {op}"] += 1

    offsets, o = [], 0
    while o < len(table):
        offsets.append(o)
        o = table.index(b"\0", o) + 1
    return uses, {o: len(text(o)) + 1 for o in offsets}, text


def main() -> None:
    p = mdlview.pak_read(*mdlview.read_pak(sys.argv[1]), "progs.dat")
    pr = parse(p)
    uses, size, text = string_uses(p, pr)
    h = struct.unpack_from("<16i", p, 0)
    gd_ofs, gd_n, fd_ofs, fd_n, fn_ofs, fn_n, str_ofs, str_n = h[4], h[5], h[6], h[7], h[8], h[9], h[10], h[11]
    table = p[str_ofs:str_ofs + str_n]
    gdefs = [struct.unpack_from("<HHi", p, gd_ofs + i * 8) for i in range(gd_n)]
    fdefs = [struct.unpack_from("<HHi", p, fd_ofs + i * 8) for i in range(fd_n)]
    fns = [struct.unpack_from("<7i", p, fn_ofs + i * 36) for i in range(fn_n)]
    name_refs = {s for _, _, s in gdefs} | {s for _, _, s in fdefs} | {f[4] for f in fns} | {f[5] for f in fns}
    # the table, string by string (offset 0 is the empty string)
    offsets, o = [], 0
    while o < len(table):
        offsets.append(o)
        o = table.index(b"\0", o) + 1
    size = {o: len(text(o)) + 1 for o in offsets}
    code = {o for o in offsets if o in uses}
    names = {o for o in offsets if o in name_refs} - code
    rest = set(offsets) - code - names
    print(f"string table {len(table)} B, {len(offsets)} strings")
    for label, s in (("read by statements", code), ("only a def's or function's name", names), ("nothing points at it", rest)):
        print(f"  {label:34} {len(s):5} strings {sum(size[o] for o in s):7} B")
    dup = Counter(text(o) for o in code)
    print(f"  duplicate texts among the statement strings: {sum(n - 1 for n in dup.values())}, "
          f"{sum((n - 1) * (len(t) + 1) for t, n in dup.items())} B")

    by_consumer_bytes, by_consumer_uses, by_consumer_strings = Counter(), Counter(), Counter()
    for o in code:
        for who, n in uses[o].items():
            by_consumer_uses[who] += n
            by_consumer_strings[who] += 1
            by_consumer_bytes[who] += size[o]
    print("\nconsumers of statement strings (a string read by two consumers counts under both):")
    print(f"  {'consumer':32} {'uses':>5} {'strings':>8} {'bytes':>7}")
    for who, b in by_consumer_bytes.most_common():
        print(f"  {who:32} {by_consumer_uses[who]:5} {by_consumer_strings[who]:8} {b:7}")
    samples = defaultdict(list)
    for o in code:
        for who in uses[o]:
            if len(samples[who]) < 4:
                samples[who].append(text(o).decode("latin-1")[:28])
    for who in ("compare", "field .classname", "field .noise", "builtin sound()", "builtin centerprint()"):
        print(f"  e.g. {who}: {samples.get(who)}")


if __name__ == "__main__":
    main()
