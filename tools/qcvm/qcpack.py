"""qcpack.py -- pack progs.dat's functions into 16K EMS pages so that no execution reaches past 3 pages,
and measure what a fourth page would have to hold.

    python3 qcpack.py PAK0.PAK

An execution is one engine call into QuakeC: an engine entry point, a think/touch/use/th_* target, or a
spawn function. What it can reach is every function it calls, transitively; a call through a field
(self.th_run()) reaches every function ever stored into that field, which is an upper bound.

State: the globals split into the engine's reserved and system block, each function's own locals, the
globals some statement writes, and the rest, which nothing writes (constants, strings, function and field
references). Edicts are counted from each map's entity lump on hard, less what the spawn code frees at once.
"""

import re
import struct
import sys
from pathlib import Path
from collections import Counter, defaultdict
from dataclasses import dataclass, field

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import mdlview  # noqa: E402

PAGE = 16384
MAX_PAGES = 3
EV_FUNCTION = 6
RESERVED_OFS = 28   # OFS_NULL, OFS_RETURN, OFS_PARM0..7, three words each
# pr_comp.h
OP_LOAD_FNC, OP_ADDRESS = 29, 30
OP_STORE_F, OP_STORE_FNC = 31, 36
OP_STOREP_F, OP_STOREP_FNC = 37, 42
OP_RETURN = 43
OP_IF, OP_IFNOT = 49, 50
OP_CALL0, OP_CALL8 = 51, 59
OP_STATE, OP_GOTO = 60, 61
ENGINE_ROOTS = ("main", "StartFrame", "PlayerPreThink", "PlayerPostThink", "ClientConnect",
                "ClientDisconnect", "PutClientInServer", "ClientKill", "SetNewParms", "SetChangeParms")
MAPS = ("start", "e1m1", "e1m2", "e1m3", "e1m4", "e1m5", "e1m6", "e1m7", "e1m8")
NOT_HARD = 1024
STATIC_OR_REMOVED = {"info_null", "light_flame_large_yellow", "light_flame_small_white", "light_flame_small_yellow",
                     "light_torch_small_walltorch"}


@dataclass(slots=True)
class Progs:
    name: list[str]
    file: list[str]
    size: list[int]
    body: dict[int, range]
    calls: dict[int, set[int]] = field(default_factory=dict)
    roots: set[int] = field(default_factory=set)
    locals_words: dict[int, range] = field(default_factory=dict)
    written: set[int] = field(default_factory=set)
    direct: dict[int, set[int]] = field(default_factory=dict)
    numglobals: int = 0
    end_sys_globals: int = 0


def parse(p: bytes) -> Progs:
    h = struct.unpack_from("<16i", p, 0)
    st_ofs, st_n, gd_ofs, gd_n, fd_ofs, fd_n, fn_ofs, fn_n, str_ofs, _, gl_ofs, gl_n = h[2:14]

    def cstr(o: int) -> str:
        return p[str_ofs + o:p.index(b"\0", str_ofs + o)].decode("latin-1")

    def gword(g: int) -> int:
        return struct.unpack_from("<i", p, gl_ofs + g * 4)[0]

    fns = [struct.unpack_from("<7i", p, fn_ofs + i * 36) for i in range(fn_n)]
    starts = sorted((f[0], i) for i, f in enumerate(fns) if f[0] > 0)
    pr = Progs([cstr(f[4]) for f in fns], [cstr(f[5]) for f in fns], [0] * fn_n, {}, numglobals=gl_n)
    for k, (first, i) in enumerate(starts):
        end = starts[k + 1][0] if k + 1 < len(starts) else st_n
        pr.size[i] = (end - first) * 8
        pr.body[i] = range(first, end)
        pr.locals_words[i] = range(fns[i][1], fns[i][1] + fns[i][2])
    fconst: dict[int, int] = {}
    for i in range(gd_n):
        t, ofs, s = struct.unpack_from("<HHi", p, gd_ofs + i * 8)
        if t & 0x7FFF == EV_FUNCTION:
            fconst[ofs] = gword(ofs)
        if cstr(s) == "end_sys_globals":
            pr.end_sys_globals = ofs
    fn_fields = {ofs: cstr(s) for t, ofs, s in (struct.unpack_from("<HHi", p, fd_ofs + i * 8) for i in range(fd_n))
                 if t & 0x7FFF == EV_FUNCTION}
    field_of_global = {g: fn_fields[gword(g)] for g in range(gl_n) if gword(g) in fn_fields}

    stored_in: dict[str, set[int]] = defaultdict(set)
    indirect: dict[int, set[str]] = defaultdict(set)
    pr.calls = {i: set() for i in pr.body}
    pr.roots = {i for i in pr.body if pr.name[i] in ENGINE_ROOTS}
    for i, rng in pr.body.items():
        loaded: dict[int, str] = {}
        addressed: dict[int, str] = {}
        for j in rng:
            op, a, b, c = struct.unpack_from("<HHHH", p, st_ofs + j * 8)
            match op:
                case _ if OP_CALL0 <= op <= OP_CALL8:
                    if fconst.get(a) in pr.body:
                        pr.calls[i].add(fconst[a])
                    elif a not in fconst:
                        indirect[i].add(loaded.get(a, "?"))
                case _ if op == OP_STATE:
                    if fconst.get(b) in pr.body:
                        pr.roots.add(fconst[b])
                        stored_in["think"].add(fconst[b])
                case _ if op in (OP_STORE_FNC, OP_STOREP_FNC):
                    if fconst.get(a) in pr.body:
                        pr.roots.add(fconst[a])
                        stored_in[addressed.get(b, "?")].add(fconst[a])
                    if op == OP_STORE_FNC:
                        pr.written.add(b)
                case _ if OP_STORE_F <= op <= OP_STORE_FNC:
                    pr.written.add(b)
                case _ if OP_STOREP_F <= op <= OP_STOREP_FNC or op in (OP_RETURN, OP_IF, OP_IFNOT, OP_GOTO):
                    pass
                case _:
                    if op == OP_LOAD_FNC and b in field_of_global:
                        loaded[c] = field_of_global[b]
                    if op == OP_ADDRESS and b in field_of_global:
                        addressed[c] = field_of_global[b]
                    if op:
                        pr.written.add(c)
    pr.direct = {i: set(c) for i, c in pr.calls.items()}
    for i, fields in indirect.items():
        for f in fields:
            pr.calls[i] |= stored_in[f] if f != "?" else set().union(*stored_in.values())
    called = set().union(*pr.calls.values())
    pr.roots |= {i for i in pr.body if i not in called}   # spawn functions, called by classname
    return pr


def closure(pr: Progs, r: int) -> frozenset[int]:
    seen, todo = {r}, [r]
    while todo:
        for c in pr.calls[todo.pop()]:
            if c not in seen:
                seen.add(c)
                todo.append(c)
    return frozenset(seen)


def maximal(sets: list[frozenset[int]]) -> list[frozenset[int]]:
    uniq = sorted(set(sets), key=len, reverse=True)
    keep: list[frozenset[int]] = []
    for s in uniq:
        if not any(s <= k for k in keep):
            keep.append(s)
    return keep


def pack(pr: Progs, closures: list[frozenset[int]]) -> dict[int, int]:
    # functions that belong to exactly the same executions move as one atom, most shared first
    member: dict[int, list[int]] = defaultdict(list)
    for ci, c in enumerate(closures):
        for f in c:
            member[f].append(ci)
    atoms: dict[tuple[int, ...], list[int]] = defaultdict(list)
    for f, cs in member.items():
        atoms[tuple(cs)].append(f)
    order = sorted(atoms.items(), key=lambda kv: (-len(kv[0]), -sum(pr.size[f] for f in kv[1])))
    page_of: dict[int, int] = {}
    free: list[int] = []
    pages_of: list[set[int]] = [set() for _ in closures]
    for sig, fns in order:
        for f in sorted(fns, key=lambda f: -pr.size[f]):
            # the page that widens the fewest executions' page sets; ties to the fullest
            best = None
            for pg, room in enumerate(free):
                if room >= pr.size[f]:
                    grow = sum(1 for ci in sig if pg not in pages_of[ci])
                    key = (grow, room)
                    if best is None or key < best[0]:
                        best = (key, pg)
            if best is None:
                free.append(PAGE)
                pg = len(free) - 1
            else:
                pg = best[1]
            free[pg] -= pr.size[f]
            page_of[f] = pg
            for ci in sig:
                pages_of[ci].add(pg)
    return page_of


def freed_at_spawn(e: dict[str, str]) -> bool:
    # read from the spawn functions' builtin calls: remove (15) or makestatic (69) on every path
    cn = e.get("classname", "")
    return cn in STATIC_OR_REMOVED or (cn == "light" and "targetname" not in e)


def entity_counts(blob: bytes, entries: dict) -> dict[str, tuple[int, int]]:
    out = {}
    for m in MAPS:
        bsp = mdlview.pak_read(blob, entries, f"maps/{m}.bsp")
        ofs, ln = struct.unpack_from("<ii", bsp, 4)
        text = bsp[ofs:ofs + ln].decode("latin-1")
        ents = [dict(re.findall(r'"([^"]*)"\s*"([^"]*)"', blk)) for blk in re.findall(r"\{([^}]*)\}", text)]
        # single player: skill decides; NOT_DEATHMATCH only matters in deathmatch
        kept = [e for e in ents if not int(float(e.get("spawnflags", "0"))) & NOT_HARD]
        live = [e for e in kept if not freed_at_spawn(e)]
        out[m] = (len(ents), len(live) + 1)
    return out


def main() -> None:
    blob, entries = mdlview.read_pak(sys.argv[1])
    p = mdlview.pak_read(blob, entries, "progs.dat")
    pr = parse(p)
    roots = sorted(pr.roots)
    cl = {r: closure(pr, r) for r in roots}
    csize = {r: sum(pr.size[f] for f in cl[r]) for r in roots}
    print(f"{len(roots)} executions; reach over {MAX_PAGES * PAGE} B (cannot fit {MAX_PAGES} pages at all): "
          f"{sum(1 for r in roots if csize[r] > MAX_PAGES * PAGE)}; largest {max(csize.values())} B")
    mx = maximal(list(cl.values()))
    page_of = pack(pr, mx)
    npages = max(page_of.values()) + 1
    used = Counter()
    for f, pg in page_of.items():
        used[pg] += pr.size[f]
    span = {r: len({page_of[f] for f in cl[r]}) for r in roots}
    worst = sorted(roots, key=lambda r: (-span[r], -csize[r]))
    print(f"{len(mx)} distinct maximal reach sets packed into {npages} pages "
          f"({sum(used.values())} B, pages {min(used.values())}..{max(used.values())} B full)")
    print(f"executions by pages reached: " + ", ".join(f"{k}: {v}" for k, v in sorted(Counter(span.values()).items())))
    over = [r for r in roots if span[r] > MAX_PAGES]
    print(f"executions over {MAX_PAGES} pages: {len(over)}")
    for r in worst[:8]:
        if span[r] <= MAX_PAGES:
            break
        print(f"  {span[r]} pages  {csize[r]:6} B  {pr.name[r]} ({pr.file[r]})")

    # state
    sys_words = max(pr.end_sys_globals, RESERVED_OFS)
    local_words = set().union(*(set(r) for r in pr.locals_words.values()))
    user = set(range(sys_words, pr.numglobals)) - local_words
    mutable = user & pr.written
    biggest_locals = max(pr.locals_words, key=lambda i: len(pr.locals_words[i]))
    print(f"globals {pr.numglobals} words: reserved+system {sys_words} ({sys_words * 4} B); "
          f"function locals {len(local_words)} ({len(local_words) * 4} B, largest "
          f"{len(pr.locals_words[biggest_locals]) * 4} B in {pr.name[biggest_locals]}); "
          f"other globals written {len(mutable)} ({len(mutable) * 4} B); never written {len(user - mutable)} "
          f"({len(user - mutable) * 4} B)")
    edict = struct.unpack_from("<i", p, 56)[0] * 4
    base = (sys_words + len(mutable)) * 4
    print(f"edict fields {edict} B; a page after the system and written globals ({base} B) holds "
          f"{(PAGE - base) // edict} edicts")
    for m, (n, live) in entity_counts(blob, entries).items():
        print(f"  {m:5}: {n:4} entities in the lump, {live:4} edicts on hard at load = {live * edict:7} B")


if __name__ == "__main__":
    main()
