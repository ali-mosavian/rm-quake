"""qcslots.py -- run-time executions reaching more code pages than there are EMS slots for code, packed for
each slot count.

    python3 qcslots.py PAK0.PAK [slots ...]      default slots: 3 2

Greedy, local search and annealing on 14 pages (qcopt.py), targeting the given slot count. Any 2-slot
arrangement -- one fixed page for the executing script and one swapped for shared code included -- is
bounded by the 2-slot result.
"""

import random
import sys
from collections import Counter

import mdlview
import qcopt
from qcpack import parse
from qcptr import resolve

NPAGES = 14


def main() -> None:
    p = mdlview.pak_read(*mdlview.read_pak(sys.argv[1]), "progs.dat")
    pr = parse(p)
    holds, _, _, graph = resolve(p, pr)
    ex = qcopt.executions(pr, holds, graph)
    size = lambda e: sum(pr.size[f] for f in e.fns)  # noqa: E731
    run = [e for e in ex if e.weight >= qcopt.TOUCH_HZ]
    print(f"run-time executions: {sum(e.roots for e in run)}; reach over 2 pages: "
          f"{sum(e.roots for e in run if size(e) > 2 * qcopt.PAGE)}, over 1: "
          f"{sum(e.roots for e in run if size(e) > qcopt.PAGE)}")
    for slots in [int(a) for a in sys.argv[2:]] or [3, 2]:
        qcopt.CODE_SLOTS = slots
        pk = qcopt.Packing(pr, ex, NPAGES, qcopt.greedy(pr, ex, NPAGES))
        pk.improve(random.Random(1))
        pk.anneal(random.Random(2), qcopt.ANNEAL_STEPS, t_hot=200.0, t_cold=0.05)
        pk.improve(random.Random(3))
        pk.check()
        pages = Counter()
        for ci, e in enumerate(ex):
            if e.weight >= qcopt.TOUCH_HZ:
                pages[pk.k[ci]] += e.roots
        over = [(e, pk.k[ci]) for ci, e in enumerate(ex) if e.weight >= qcopt.TOUCH_HZ and pk.k[ci] > slots]
        print(f"\n{slots} code slots: run-time executions by pages reached {dict(sorted(pages.items()))}")
        print(f"  over {slots}: {sum(e.roots for e, _ in over)} executions, {sum(e.weight for e, _ in over):.0f} "
              f"of {sum(e.weight for e in run):.0f} runs/s")
        for e, k in sorted(over, key=lambda t: -t[0].weight)[:6]:
            print(f"    {k} pages  {e.weight:5.1f}/s  {size(e):6} B  {e.name}")


if __name__ == "__main__":
    main()
