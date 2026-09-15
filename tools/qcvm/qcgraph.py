"""qcgraph.py -- which QuakeC functions must be in memory together, from progs.dat's call graph.

    python3 qcgraph.py PAK0.PAK

A function's working set is itself plus everything it calls, transitively, within one invocation: direct
CALLs to QuakeC functions. Functions stored for later (think, touch, use, th_*) run on a later invocation, so
they are roots of their own, not edges. Calls through a variable (self.th_run()) are resolved to every
function ever stored into that field, and reported separately. Builtins are engine code and cost nothing here.
"""

import struct
import sys
from pathlib import Path
from collections import Counter
from dataclasses import dataclass

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import mdlview  # noqa: E402

# pr_comp.h
OP_LOAD_FNC = 29
OP_ADDRESS = 30
OP_STORE_FNC = 36
OP_STOREP_FNC = 42
OP_CALL0, OP_CALL8 = 51, 59
OP_STATE = 60
EV_FUNCTION = 6
PAGE = 16384
ENGINE_ROOTS = ("main", "StartFrame", "PlayerPreThink", "PlayerPostThink", "ClientConnect",
                "ClientDisconnect", "PutClientInServer", "ClientKill", "SetNewParms", "SetChangeParms")


@dataclass(frozen=True, slots=True)
class Stmt:
    op: int
    a: int
    b: int
    c: int


def load(pak: str) -> bytes:
    blob, entries = mdlview.read_pak(pak)
    return mdlview.pak_read(blob, entries, "progs.dat")


def main() -> None:
    p = load(sys.argv[1])
    h = struct.unpack_from("<16i", p, 0)
    st_ofs, st_n, gd_ofs, gd_n, fd_ofs, fd_n, fn_ofs, fn_n, str_ofs, _, gl_ofs = h[2:13]

    def cstr(o: int) -> str:
        return p[str_ofs + o:p.index(b"\0", str_ofs + o)].decode("latin-1")

    fns = [struct.unpack_from("<7i", p, fn_ofs + i * 36) for i in range(fn_n)]
    name = [cstr(f[4]) for f in fns]
    file = [cstr(f[5]) for f in fns]
    stmts = [Stmt(*struct.unpack_from("<HHHH", p, st_ofs + j * 8)) for j in range(st_n)]

    fconst = {}
    for i in range(gd_n):
        t, ofs, _ = struct.unpack_from("<HHi", p, gd_ofs + i * 8)
        if t & 0x7FFF == EV_FUNCTION:
            fconst[ofs] = struct.unpack_from("<i", p, gl_ofs + ofs * 4)[0]
    field_name = {}
    for i in range(fd_n):
        t, ofs, s = struct.unpack_from("<HHi", p, fd_ofs + i * 8)
        if t & 0x7FFF == EV_FUNCTION:
            field_name[ofs] = cstr(s)
    # a field offset is a constant global holding the field's number
    field_of_global = {g: field_name[v] for g, v in
                       ((g, struct.unpack_from("<i", p, gl_ofs + g * 4)[0]) for g in range(h[13]))
                       if v in field_name}

    starts = sorted((f[0], i) for i, f in enumerate(fns) if f[0] > 0)
    size = [0] * fn_n
    body: dict[int, range] = {}
    for k, (first, i) in enumerate(starts):
        end = starts[k + 1][0] if k + 1 < len(starts) else st_n
        size[i] = (end - first) * 8
        body[i] = range(first, end)

    calls: dict[int, set[int]] = {i: set() for i in body}
    indirect_sites: dict[int, set[str]] = {}
    stored_in: dict[str, set[int]] = {}
    roots = {i for i in body if name[i] in ENGINE_ROOTS}
    for i, rng in body.items():
        last_field: dict[int, str] = {}   # temp global -> field loaded into it
        addr_field: dict[int, str] = {}   # temp global -> field address taken into it
        for j in rng:
            s = stmts[j]
            if s.op == OP_LOAD_FNC and s.b in field_of_global:
                last_field[s.c] = field_of_global[s.b]
            elif s.op == OP_ADDRESS and s.b in field_of_global:
                addr_field[s.c] = field_of_global[s.b]
            elif OP_CALL0 <= s.op <= OP_CALL8:
                if s.a in fconst and fconst[s.a] in body:
                    calls[i].add(fconst[s.a])
                elif s.a not in fconst:
                    indirect_sites.setdefault(i, set()).add(last_field.get(s.a, "?"))
            elif s.op in (OP_STORE_FNC, OP_STOREP_FNC, OP_STATE):
                src = s.b if s.op == OP_STATE else s.a
                if src in fconst and fconst[src] in body:
                    roots.add(fconst[src])
                    fld = "think" if s.op == OP_STATE else addr_field.get(s.b, "?") if s.op == OP_STOREP_FNC else "?"
                    stored_in.setdefault(fld, set()).add(fconst[src])
    # spawn functions: the engine calls a classname's function at load, so every function is a potential root;
    # count only those with no QuakeC caller and no stored reference as spawn roots
    called = set().union(*calls.values())
    spawn_roots = {i for i in body if i not in called and i not in roots}

    for i, fields in indirect_sites.items():
        for fld in fields:
            calls[i] |= stored_in.get(fld, set()) if fld != "?" else set()

    def closure(r: int) -> set[int]:
        seen, todo = {r}, [r]
        while todo:
            for c in calls[todo.pop()]:
                if c not in seen:
                    seen.add(c)
                    todo.append(c)
        return seen

    deepest_memo: dict[int, tuple[int, list[int]]] = {}

    def deepest(r: int, on_stack: frozenset[int] = frozenset()) -> tuple[int, list[int]]:
        # the heaviest caller-to-callee chain: what is on the call stack at once; a cycle is cut where it closes
        if r in deepest_memo:
            return deepest_memo[r]
        best = (0, [])
        for c in calls[r]:
            if c not in on_stack and c != r:
                best = max(best, deepest(c, on_stack | {r}), key=lambda t: t[0])
        got = (size[r] + best[0], [r] + best[1])
        if not on_stack & closure(r):
            deepest_memo[r] = got
        return got

    run_roots = sorted(roots, key=lambda r: name[r])
    sets = {r: closure(r) for r in run_roots}
    cost = {r: sum(size[f] for f in s) for r, s in sets.items()}
    chain = {r: deepest(r) for r in run_roots}
    worst = max(run_roots, key=lambda r: chain[r][0])
    print(f"heaviest call chain from any run-time root: {chain[worst][0]} B, "
          + " -> ".join(f"{name[f]}({size[f]})" for f in chain[worst][1]))
    print(f"run-time roots whose call chain exceeds {PAGE} B: {sum(1 for r in run_roots if chain[r][0] > PAGE)}")
    over = [r for r in run_roots if cost[r] > PAGE]
    print(f"{len(body)} functions, {sum(size)} B of statements; {len(run_roots)} run-time roots "
          f"(engine entry points + every think/touch/use/th_* target), {len(spawn_roots)} load-only roots")
    print(f"indirect call sites: {sum(len(v) for v in indirect_sites.values())} in "
          f"{len(indirect_sites)} functions, through fields {sorted(set().union(*indirect_sites.values()))}")
    print(f"run-time roots whose working set exceeds {PAGE} B: {len(over)} of {len(run_roots)}")
    hist = Counter(min(cost[r] // 4096, 8) for r in run_roots)
    print("working-set sizes:", ", ".join(f"<{(k + 1) * 4}K: {hist[k]}" if k < 8 else f">=32K: {hist[k]}"
                                        for k in sorted(hist)))
    print("largest working sets:")
    for r in sorted(run_roots, key=lambda r: -cost[r])[:12]:
        files = Counter(file[f] for f in sets[r])
        print(f"  {cost[r]:6} B  {len(sets[r]):4} fns  {name[r]:24} ({file[r]})  files: "
              + ", ".join(f"{f}" for f, _ in files.most_common(5)))
    spawn_cost = {r: sum(size[f] for f in closure(r)) for r in spawn_roots}
    print(f"load-only (spawn) roots over {PAGE} B: {sum(1 for v in spawn_cost.values() if v > PAGE)}; largest "
          + ", ".join(f"{name[r]} {spawn_cost[r]}" for r in sorted(spawn_cost, key=lambda r: -spawn_cost[r])[:4]))
    per_tick = set().union(*(sets[r] for r in run_roots if name[r] in ("StartFrame", "PlayerPreThink",
                                                                        "PlayerPostThink")))
    print(f"the player's every-frame set (StartFrame + PlayerPreThink + PlayerPostThink): "
          f"{sum(size[f] for f in per_tick)} B, {len(per_tick)} fns")


if __name__ == "__main__":
    main()
