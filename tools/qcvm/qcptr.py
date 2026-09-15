"""qcptr.py -- every function each function-typed entity field can hold, by following function values
through progs.dat: named functions into globals, parameters, returns and fields, and back out.

    python3 qcptr.py PAK0.PAK

A location is a global word or an entity field (by name, whatever the entity). Moves: STORE_F/STORE_V/STORE_FNC
(global -> global), STOREP_F/STOREP_FNC through an ADDRESS (global -> field), LOAD_F/LOAD_FNC (field ->
global), a CALL (argument words -> the callee's parameter words, for every possible callee), RETURN (-> the
return word). Flow-insensitive, so a superset; seeded only with named functions, so nothing else is counted.
"""

import struct
import sys
from collections import defaultdict

import mdlview
from qcpack import Progs, parse

OP_LOAD_F, OP_LOAD_V, OP_LOAD_FNC, OP_ADDRESS = 24, 25, 29, 30
OP_STORE_F, OP_STORE_V, OP_STORE_FNC, OP_STOREP_F, OP_STOREP_V, OP_STOREP_FNC, OP_RETURN = 31, 32, 36, 37, 38, 42, 43
OP_CALL0, OP_CALL8, OP_STATE = 51, 59, 60
OFS_RETURN, OFS_PARM0 = 1, 4


def resolve(p: bytes, pr: Progs) -> tuple[dict, set[str], int, dict[int, set[int]]]:
    # what every function-typed location can hold, and the call graph with field calls resolved by it
    h = struct.unpack_from("<16i", p, 0)
    st_ofs, gd_ofs, gd_n, fd_ofs, fd_n, fn_ofs, fn_n, str_ofs, gl_ofs, gl_n = (
        h[2], h[4], h[5], h[6], h[7], h[8], h[9], h[10], h[12], h[13])

    def cstr(o: int) -> str:
        return p[str_ofs + o:p.index(b"\0", str_ofs + o)].decode("latin-1")

    def gword(g: int) -> int:
        return struct.unpack_from("<i", p, gl_ofs + g * 4)[0]

    fns = [struct.unpack_from("<7i8B", p, fn_ofs + i * 36) for i in range(fn_n)]
    named = {ofs: gword(ofs) for t, ofs, s in (struct.unpack_from("<HHi", p, gd_ofs + i * 8) for i in range(gd_n))
             if t & 0x7FFF == 6 and gword(ofs) > 0}
    fields = {ofs: cstr(s) for t, ofs, s in (struct.unpack_from("<HHi", p, fd_ofs + i * 8) for i in range(fd_n))}
    fn_fields = {n for (ofs, n) in fields.items()
                 if any(t & 0x7FFF == 6 and o == ofs for t, o, _ in
                        (struct.unpack_from("<HHi", p, fd_ofs + i * 8) for i in range(fd_n)))}
    field_of_global = {g: fields[gword(g)] for g in range(gl_n) if gword(g) in fields and gword(g) > 0}

    def param_words(f: int) -> list[int]:
        start, sizes, n = fns[f][1], fns[f][7:15], fns[f][6]
        out, at = [], start
        for k in range(min(n, 8)):
            out.append(at)
            at += sizes[k]
        return out

    edges: dict[object, set[object]] = defaultdict(set)   # location -> locations its value reaches
    # (function, callee global, argc, argument word -> the global stored into it before this call)
    call_sites: list[tuple[int, int, int, dict[int, int]]] = []
    via_field: dict[int, set[str]] = defaultdict(set)      # function -> fields it calls through
    for i, rng in pr.body.items():
        addressed: dict[int, str] = {}
        loaded: dict[int, str] = {}
        args: dict[int, int] = {}
        for j in rng:
            op, a, b, c = struct.unpack_from("<HHHH", p, st_ofs + j * 8)
            if op == OP_ADDRESS and b in field_of_global:
                addressed[c] = field_of_global[b]
            elif op in (OP_LOAD_F, OP_LOAD_V, OP_LOAD_FNC) and b in field_of_global:
                loaded[c] = field_of_global[b]
                edges[("field", field_of_global[b])].add(("g", c))
            elif op in (OP_STORE_F, OP_STORE_V, OP_STORE_FNC) and OFS_PARM0 <= b < OFS_PARM0 + 24:
                args[b] = a   # every call's arguments share these words: wire them per call site
            elif op in (OP_STORE_F, OP_STORE_V, OP_STORE_FNC):
                edges[("g", a)].add(("g", b))
            elif op in (OP_STOREP_F, OP_STOREP_V, OP_STOREP_FNC) and b in addressed:
                edges[("g", a)].add(("field", addressed[b]))
            elif op == OP_STATE:
                edges[("g", b)].add(("field", "think"))
            elif op == OP_RETURN:
                edges[("g", a)].add(("g", OFS_RETURN))
            elif OP_CALL0 <= op <= OP_CALL8:
                call_sites.append((i, a, op - OP_CALL0, dict(args)))
                if a in loaded:
                    via_field[i].add(loaded[a])
                args = {}

    holds: dict[object, set[int]] = defaultdict(set)
    for g, f in named.items():
        if f in pr.body:
            holds[("g", g)].add(f)

    def propagate() -> bool:
        changed = False
        todo = [loc for loc in list(holds) if holds[loc]]
        while todo:
            loc = todo.pop()
            for dst in edges.get(loc, ()):
                if not holds[loc] <= holds[dst]:
                    holds[dst] |= holds[loc]
                    todo.append(dst)
                    changed = True
        return changed

    # calls move argument words into the callee's parameters; the callee set can grow as fields fill in
    while True:
        grew = propagate()
        for i, callee_g, argc, site_args in call_sites:
            for callee in holds.get(("g", callee_g), set()):
                for k, word in enumerate(param_words(callee)[:argc]):
                    if OFS_PARM0 + 3 * k not in site_args:
                        continue
                    src, dst = ("g", site_args[OFS_PARM0 + 3 * k]), ("g", word)
                    if dst not in edges[src]:
                        edges[src].add(dst)
                        grew = True
        if not grew:
            break

    graph = {i: set(pr.direct[i]) for i in pr.body}
    for i, flds in via_field.items():
        for f in flds:
            graph[i] |= holds.get(("field", f), set())
    return holds, fn_fields, st_ofs, graph


def main() -> None:
    p = mdlview.pak_read(*mdlview.read_pak(sys.argv[1]), "progs.dat")
    pr = parse(p)
    holds, fn_fields, st_ofs, graph = resolve(p, pr)

    # completeness: a function value an unmodeled op reads could go somewhere this never follows
    modeled = {OP_LOAD_F, OP_LOAD_V, OP_LOAD_FNC, OP_ADDRESS, OP_STORE_F, OP_STORE_V, OP_STORE_FNC,
               OP_STOREP_F, OP_STOREP_V, OP_STOREP_FNC, OP_RETURN, OP_STATE} | set(range(OP_CALL0, OP_CALL8 + 1))
    compares = {14, 19, 48, 49, 50, 62, 63}   # EQ_FNC, NE_FNC, NOT_FNC, IF, IFNOT, AND, OR: read, never move
    compares.add(61)   # GOTO: its first operand is a jump offset, not a global
    leaks = defaultdict(int)
    for i, rng in pr.body.items():
        for j in rng:
            op, a, b, c = struct.unpack_from("<HHHH", p, st_ofs + j * 8)
            if op in modeled or op in compares:
                continue
            if holds.get(("g", a)) or holds.get(("g", b)):
                leaks[(op, pr.name[i])] += 1
    print("function values read by an op this does not follow:", dict(leaks) or "none")
    returned = {pr.name[i] for i, rng in pr.body.items() for j in rng
                if struct.unpack_from("<H", p, st_ofs + j * 8)[0] == OP_RETURN
                and holds.get(("g", struct.unpack_from("<HHHH", p, st_ofs + j * 8)[1]))}
    print("functions returning a function value:", sorted(returned) or "none")
    total = 0
    for fld in sorted(fn_fields):
        targets = holds.get(("field", fld), set())
        total += len(targets)
        files = sorted({pr.file[t][:-3] for t in targets})
        print(f".{fld:10} {len(targets):5} possible functions, from {len(files)} files")
    think1 = holds.get(("field", "think1"), set())
    print("think1 targets:", sorted(pr.name[t] for t in think1))
    print("use targets:", sorted(pr.name[t] for t in holds.get(("field", "use"), set())))
    print("th_die targets:", sorted(pr.name[t] for t in holds.get(("field", "th_die"), set())))


if __name__ == "__main__":
    main()
