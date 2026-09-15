"""qcseg.py -- with a function copied into every group that needs it, how many 3-page groups hold every
execution's whole reach, and how much EMS that is.

    python3 qcseg.py PAK0.PAK
"""

import sys

import mdlview
from qcpack import MAX_PAGES, PAGE, closure, maximal, parse

SLACK = 1024   # 16K pages hold whole functions; the largest is 2,800 B


def main() -> None:
    blob, entries = mdlview.read_pak(sys.argv[1])
    pr = parse(mdlview.pak_read(blob, entries, "progs.dat"))
    cl = [closure(pr, r) for r in sorted(pr.roots)]
    mx = maximal(cl)
    limit = MAX_PAGES * PAGE - SLACK
    groups: list[set[int]] = []
    for c in sorted(mx, key=lambda c: -sum(pr.size[f] for f in c)):
        best = None
        for g in groups:
            size = sum(pr.size[f] for f in g | c)
            if size <= limit and (best is None or size < best[0]):
                best = (size, g)
        if best is None:
            groups.append(set(c))
        else:
            best[1].update(c)
    sizes = sorted((sum(pr.size[f] for f in g) for g in groups), reverse=True)
    copies = sum(len(g) for g in groups)
    print(f"{len(mx)} maximal reach sets -> {len(groups)} groups of at most {limit} B; "
          f"{sum(sizes)} B of EMS ({sum(sizes) / PAGE:.1f} pages' worth) for {sum(pr.size)} B of code; "
          f"{copies} function copies for {len(pr.body)} functions")
    print("group sizes:", sizes)


if __name__ == "__main__":
    main()


def pages_fit(sizes: list[int], pages: int) -> list[int] | None:
    # first-fit decreasing into whole pages: a function never straddles one
    bins = [0] * pages
    for s in sorted(sizes, reverse=True):
        k = next((i for i, used in enumerate(bins) if used + s <= PAGE), None)
        if k is None:
            return None
        bins[k] += s
    return bins
