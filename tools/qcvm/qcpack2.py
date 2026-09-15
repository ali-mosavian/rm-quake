"""qcpack2.py -- qcpack's packing under four orderings; the fewest executions past 3 pages wins.

    python3 qcpack2.py PAK0.PAK
"""

import sys
from collections import Counter, defaultdict
from collections.abc import Callable

import mdlview
from qcpack import MAX_PAGES, PAGE, Progs, closure, maximal, parse


def pack(pr: Progs, cls: list[frozenset[int]], order: str, rule: str) -> dict[int, int]:
    csize = [sum(pr.size[f] for f in c) for c in cls]
    member: dict[int, list[int]] = defaultdict(list)
    for ci, c in enumerate(cls):
        for f in c:
            member[f].append(ci)
    atoms: dict[tuple[int, ...], list[int]] = defaultdict(list)
    for f, cs in member.items():
        atoms[tuple(cs)].append(f)
    keys: dict[str, Callable] = {
        "shared": lambda kv: (-len(kv[0]), -sum(pr.size[f] for f in kv[1])),
        "biggest": lambda kv: (-max(csize[ci] for ci in kv[0]), -len(kv[0])),
    }
    page_of: dict[int, int] = {}
    free: list[int] = []
    pages_of: list[set[int]] = [set() for _ in cls]
    for sig, fns in sorted(atoms.items(), key=keys[order]):
        for f in sorted(fns, key=lambda f: -pr.size[f]):
            best = None
            for pg, room in enumerate(free + [PAGE]):
                if room < pr.size[f]:
                    continue
                grown = [len(pages_of[ci] | {pg}) for ci in sig]
                grow = sum(1 for ci in sig if pg not in pages_of[ci])
                key = (grow, room) if rule == "fewest" else (max(grown), sum(g > MAX_PAGES for g in grown), grow, room)
                if best is None or key < best[0]:
                    best = (key, pg)
            pg = best[1]
            if pg == len(free):
                free.append(PAGE)
            free[pg] -= pr.size[f]
            page_of[f] = pg
            for ci in sig:
                pages_of[ci].add(pg)
    return page_of


def main() -> None:
    blob, entries = mdlview.read_pak(sys.argv[1])
    pr = parse(mdlview.pak_read(blob, entries, "progs.dat"))
    roots = sorted(pr.roots)
    cl = {r: closure(pr, r) for r in roots}
    mx = maximal(list(cl.values()))
    for order in ("shared", "biggest"):
        for rule in ("fewest", "minmax"):
            page_of = pack(pr, mx, order, rule)
            span = Counter(len({page_of[f] for f in cl[r]}) for r in roots)
            print(f"{order:8} {rule:7} pages {max(page_of.values()) + 1:3}  executions by pages reached "
                  + ", ".join(f"{k}: {v}" for k, v in sorted(span.items()))
                  + f"  -> over {MAX_PAGES}: {sum(v for k, v in span.items() if k > MAX_PAGES)}")


if __name__ == "__main__":
    main()
