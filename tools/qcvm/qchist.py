"""qchist.py -- how many other QuakeC functions (and .qc files) one function can call, as histograms.

    python3 qchist.py PAK0.PAK

direct: distinct functions named in its own CALLs. reach: everything those call in turn. Each is counted
twice: calls by name alone, and with self.th_*() and the like resolved to every function ever stored into
that field.
"""

import sys
from collections import Counter

import mdlview
from qcpack import parse

BUCKETS = (0, 1, 2, 3, 5, 10, 25, 50, 100, 200, 400)


def reach(calls: dict[int, set[int]], r: int) -> set[int]:
    seen, todo = set(), [r]
    while todo:
        for c in calls[todo.pop()]:
            if c not in seen and c != r:
                seen.add(c)
                todo.append(c)
    return seen


def label(k: int) -> str:
    lo = BUCKETS[k]
    hi = BUCKETS[k + 1] - 1 if k + 1 < len(BUCKETS) else None
    return f"{lo}" if hi == lo else f"{lo}-{hi}" if hi is not None else f"{lo}+"


def bucket(n: int) -> int:
    return max(k for k, lo in enumerate(BUCKETS) if n >= lo)


def show(title: str, counts: list[int]) -> None:
    h = Counter(bucket(n) for n in counts)
    top = max(h.values())
    print(f"\n{title}  (max {max(counts)}, median {sorted(counts)[len(counts) // 2]})")
    for k in range(len(BUCKETS)):
        n = h.get(k, 0)
        print(f"  {label(k):>7} | {'#' * round(50 * n / top):50} {n}")


def main() -> None:
    blob, entries = mdlview.read_pak(sys.argv[1])
    pr = parse(mdlview.pak_read(blob, entries, "progs.dat"))
    fns = sorted(pr.body)
    for graph, name in ((pr.direct, "calls by name only"), (pr.calls, "with self.th_*() etc. resolved")):
        show(f"{name}: other functions called directly", [len(graph[f] - {f}) for f in fns])
        r = {f: reach(graph, f) for f in fns}
        show(f"{name}: other functions reachable", [len(r[f]) for f in fns])
        show(f"{name}: other .qc files reachable", [len({pr.file[g] for g in r[f]} - {pr.file[f]}) for f in fns])
        worst = sorted(fns, key=lambda f: -len(r[f]))[:3]
        print("  most: " + ", ".join(f"{pr.name[f]} ({pr.file[f]}) {len(r[f])}" for f in worst))


if __name__ == "__main__":
    main()
