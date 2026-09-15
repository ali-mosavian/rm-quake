"""qcrescore.py -- do qcopt's three packings keep their order when the assumptions they were tuned on change?

    python3 qcrescore.py PAK0.PAK

Builds qcc order, greedy and annealed on 14 pages under the base weights, then rescores each unchanged
packing with the per-tick, think or touch rate x3 and /3, PAST_SLOTS at 5, and without the two per-tick
player entry points.
"""

import random
import sys

import mdlview
import qcopt
from qcpack import parse
from qcptr import resolve

NPAGES = 14


def score(pr, holds, graph, pages: dict[int, int], drop: tuple[str, ...] = ()) -> tuple[float, int]:
    ex = [e for e in qcopt.executions(pr, holds, graph) if e.name not in drop]
    pk = qcopt.Packing(pr, ex, NPAGES, pages)
    return pk.cost, sum(e.roots for ci, e in enumerate(ex) if pk.k[ci] > qcopt.CODE_SLOTS)


def main() -> None:
    p = mdlview.pak_read(*mdlview.read_pak(sys.argv[1]), "progs.dat")
    pr = parse(p)
    holds, _, _, graph = resolve(p, pr)
    ex = qcopt.executions(pr, holds, graph)
    packings = {"qcc order": qcopt.qcc_order(pr, NPAGES), "greedy": qcopt.greedy(pr, ex, NPAGES)}
    pk = qcopt.Packing(pr, ex, NPAGES, packings["greedy"])
    pk.improve(random.Random(1))
    pk.anneal(random.Random(2), qcopt.ANNEAL_STEPS, t_hot=200.0, t_cold=0.05)
    pk.improve(random.Random(3))
    pk.check()
    packings["annealed"] = dict(pk.page_of)

    base = {k: getattr(qcopt, k) for k in ("TICK_HZ", "THINK_HZ", "TOUCH_HZ", "PAST_SLOTS")}
    variants = [("base", {}, ())]
    for k in ("TICK_HZ", "THINK_HZ", "TOUCH_HZ"):
        variants += [(f"{k} x3", {k: base[k] * 3}, ()), (f"{k} /3", {k: base[k] / 3}, ())]
    variants += [("PAST_SLOTS 5", {"PAST_SLOTS": 5.0}, ()),
                 ("no player per-tick", {}, ("PlayerPreThink", "PlayerPostThink"))]
    print(f"{'variant':20}" + "".join(f"{n:>26}" for n in packings) + "   order")
    for label, change, drop in variants:
        for k, v in base.items():
            setattr(qcopt, k, change.get(k, v))
        got = {n: score(pr, holds, graph, pg, drop) for n, pg in packings.items()}
        order = " < ".join(sorted(got, key=lambda n: got[n][0]))
        print(f"{label:20}" + "".join(f"{c:14.0f} ({o:3} >3)" + " " * 3 for c, o in got.values()) + f"  {order}")


if __name__ == "__main__":
    main()
