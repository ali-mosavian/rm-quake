"""qcopt.py -- pack progs.dat's functions into 16K EMS pages, one copy each, for the fewest page maps.

    python3 qcopt.py PAK0.PAK [pages ...]      default pages: 11 12 13 14

An execution is one engine call into QuakeC; it can touch the pages its reach sits on (the call graph with
field calls resolved by qcptr.py, which knows every function a field can hold). Built-ins are engine code
and cost nothing.

The cost is static, not a trace: a tick starts with the VM's pages evicted (the renderer and the mixer
remap the frame), so an execution costs one map per distinct page, and every page past CODE_SLOTS more than
that -- PAST_SLOTS each, since past three the LRU swaps inside the execution in an order only a trace knows.
Weighted by how often the execution runs: the player's per-tick entry points at 60 a second, a think at 10
(one entity's), a touch or a blocked at 1, anything else (spawn, connect, level change) at LOAD_ONLY.

Three packings a page count: qcc's own order filled page by page, a greedy placement by that cost, and the
greedy one improved by moving and swapping functions between pages until nothing lowers the cost.
"""

import random
import struct
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass

import mdlview
from qcpack import ENGINE_ROOTS, Progs, parse
from qcptr import resolve

PAGE = 16384
CODE_SLOTS = 3
PAST_SLOTS = 50.0
PER_TICK = ("StartFrame", "PlayerPreThink", "PlayerPostThink")
TICK_HZ, THINK_HZ, TOUCH_HZ, LOAD_ONLY = 60.0, 10.0, 1.0, 0.01
SWAP_TRIES = 12
MAX_PASSES = 15
ANNEAL_STEPS = 400_000


@dataclass(frozen=True, slots=True)
class Execution:
    fns: tuple[int, ...]
    weight: float
    roots: int
    name: str


def closure(graph: dict[int, set[int]], r: int) -> frozenset[int]:
    seen, todo = {r}, [r]
    while todo:
        for c in graph[todo.pop()]:
            if c not in seen:
                seen.add(c)
                todo.append(c)
    return frozenset(seen)


def executions(pr: Progs, holds: dict, graph: dict[int, set[int]]) -> list[Execution]:
    field = lambda f: holds.get(("field", f), set())  # noqa: E731
    rate: dict[int, float] = defaultdict(float)
    for i in pr.body:
        if pr.name[i] in PER_TICK:
            rate[i] = max(rate[i], TICK_HZ)
        elif pr.name[i] in ENGINE_ROOTS:
            rate[i] = max(rate[i], LOAD_ONLY)
    for f in field("think"):
        rate[f] = max(rate[f], THINK_HZ)
    for f in field("touch") | field("blocked"):
        rate[f] = max(rate[f], TOUCH_HZ)
    called = set().union(*graph.values())
    held = set().union(*(v for k, v in holds.items() if k[0] == "field"))
    for i in pr.body:   # spawn functions: the engine finds them by classname at load
        if i not in called and i not in held and i not in rate:
            rate[i] = LOAD_ONLY
    grouped: dict[frozenset[int], list[int]] = defaultdict(list)
    for r in rate:
        grouped[closure(graph, r)].append(r)
    return [Execution(tuple(sorted(c)), sum(rate[r] for r in rs), len(rs), pr.name[max(rs, key=lambda r: rate[r])])
            for c, rs in grouped.items()]


def cost_of(k: int) -> float:
    return k + PAST_SLOTS * max(0, k - CODE_SLOTS)


class Packing:
    def __init__(self, pr: Progs, ex: list[Execution], npages: int, page_of: dict[int, int]) -> None:
        self.pr, self.ex, self.n = pr, ex, npages
        self.page_of = dict(page_of)
        self.member: dict[int, list[int]] = defaultdict(list)
        for ci, e in enumerate(ex):
            for f in e.fns:
                self.member[f].append(ci)
        self.used = [0] * npages
        self.cnt = [[0] * npages for _ in ex]
        self.k = [0] * len(ex)
        for f, pg in self.page_of.items():
            self.used[pg] += pr.size[f]
        for ci, e in enumerate(ex):
            for f in e.fns:
                self.cnt[ci][self.page_of[f]] += 1
            self.k[ci] = sum(1 for c in self.cnt[ci] if c)
        self.cost = sum(e.weight * cost_of(self.k[ci]) for ci, e in enumerate(ex))

    def delta(self, f: int, q: int) -> float:
        p = self.page_of[f]
        d = 0.0
        for ci in self.member[f]:
            k = self.k[ci] - (self.cnt[ci][p] == 1) + (self.cnt[ci][q] == 0)
            if k != self.k[ci]:
                d += self.ex[ci].weight * (cost_of(k) - cost_of(self.k[ci]))
        return d

    def move(self, f: int, q: int) -> None:
        p = self.page_of[f]
        self.cost += self.delta(f, q)
        for ci in self.member[f]:
            self.cnt[ci][p] -= 1
            self.cnt[ci][q] += 1
            self.k[ci] += (self.cnt[ci][q] == 1) - (self.cnt[ci][p] == 0)
        self.used[p] -= self.pr.size[f]
        self.used[q] += self.pr.size[f]
        self.page_of[f] = q

    def improve(self, rng: random.Random, pinned: frozenset[int] = frozenset()) -> int:
        fns = sorted((f for f in self.page_of if f not in pinned), key=lambda f: -len(self.member[f]))
        passes = 0
        for passes in range(1, MAX_PASSES + 1):
            better = False
            for f in fns:
                p, s = self.page_of[f], self.pr.size[f]
                moves = [(self.delta(f, q), q) for q in range(self.n) if q != p and self.used[q] + s <= PAGE]
                if moves and (best := min(moves))[0] < -1e-9:
                    self.move(f, best[1])
                    better = True
                    continue
                for g in rng.sample(fns, min(SWAP_TRIES, len(fns))):
                    q = self.page_of[g]
                    if q == p or self.used[p] - s + self.pr.size[g] > PAGE or self.used[q] - self.pr.size[g] + s > PAGE:
                        continue
                    before = self.cost
                    self.move(f, q)
                    self.move(g, p)
                    if self.cost < before - 1e-9:
                        better = True
                        break
                    self.move(g, q)
                    self.move(f, p)
            if not better:
                break
        return passes

    def anneal(self, rng: random.Random, steps: int, t_hot: float, t_cold: float,
               pinned: frozenset[int] = frozenset()) -> None:
        # Metropolis over single moves (any page with room, empty ones included) and pairwise swaps;
        # the best packing seen is kept, since the walk ends wherever the last cold steps leave it
        import math
        fns = sorted(f for f in self.page_of if f not in pinned)
        best_cost, best = self.cost, dict(self.page_of)
        for step in range(steps):
            temp = t_hot * (t_cold / t_hot) ** (step / steps)
            f = rng.choice(fns)
            p, s = self.page_of[f], self.pr.size[f]
            q = rng.randrange(self.n)
            if q == p:
                continue
            if self.used[q] + s <= PAGE:
                d = self.delta(f, q)
                if d <= 0 or rng.random() < math.exp(-d / temp):
                    self.move(f, q)
            else:
                on_q = [g for g in fns if self.page_of[g] == q] if rng.random() < 0.02 else None
                g = rng.choice(on_q) if on_q else rng.choice(fns)
                if self.page_of[g] != q or self.used[p] - s + self.pr.size[g] > PAGE \
                        or self.used[q] - self.pr.size[g] + s > PAGE:
                    continue
                before = self.cost
                self.move(f, q)
                self.move(g, p)
                d = self.cost - before
                if not (d <= 0 or rng.random() < math.exp(-d / temp)):
                    self.move(g, q)
                    self.move(f, p)
            if self.cost < best_cost - 1e-9:
                best_cost, best = self.cost, dict(self.page_of)
        for f, q in best.items():
            if self.page_of[f] != q:
                self.move(f, q)

    def check(self) -> None:
        assert set(self.page_of) == set(self.pr.body)
        assert all(u <= PAGE for u in self.used), self.used
        fresh = sum(e.weight * cost_of(len({self.page_of[f] for f in e.fns})) for e in self.ex)
        assert abs(fresh - self.cost) < 1e-6 * max(1.0, fresh), (fresh, self.cost)


def qcc_order(pr: Progs, npages: int) -> dict[int, int] | None:
    page_of, pg, used = {}, 0, 0
    for f in sorted(pr.body, key=lambda f: pr.body[f].start):
        if used + pr.size[f] > PAGE:
            pg, used = pg + 1, 0
        if pg >= npages:
            return None
        page_of[f] = pg
        used += pr.size[f]
    return page_of


def greedy(pr: Progs, ex: list[Execution], npages: int, fixed: dict[int, int] | None = None) -> dict[int, int] | None:
    heat: dict[int, float] = defaultdict(float)
    member: dict[int, list[int]] = defaultdict(list)
    for ci, e in enumerate(ex):
        for f in e.fns:
            heat[f] += e.weight
            member[f].append(ci)
    placed_on = [[0] * npages for _ in ex]
    used = [0] * npages
    page_of: dict[int, int] = dict(fixed or {})
    for f, q in page_of.items():
        used[q] += pr.size[f]
        for ci in member[f]:
            placed_on[ci][q] += 1
    for f in sorted((f for f in pr.body if f not in page_of), key=lambda f: (-heat[f], -pr.size[f])):
        s = pr.size[f]
        # the page that adds the fewest weighted page memberships, judged against what is placed so far
        options = [(sum(ex[ci].weight for ci in member[f] if not placed_on[ci][q]), -used[q], q)
                   for q in range(npages) if used[q] + s <= PAGE]
        if not options:
            return None
        q = min(options)[2]
        page_of[f] = q
        used[q] += s
        for ci in member[f]:
            placed_on[ci][q] += 1
    return page_of


def pages_ffd(fns: list[int], pr: Progs, pages: list[int], used: list[int]) -> dict[int, int] | None:
    # whole functions, largest first, onto the given pages; None when they do not go
    out = {}
    for f in sorted(fns, key=lambda f: -pr.size[f]):
        q = next((q for q in pages if used[q] + pr.size[f] <= PAGE), None)
        if q is None:
            return None
        out[f] = q
        used[q] += pr.size[f]
    return out


def player_seed(pr: Progs, ex: list[Execution], npages: int) -> dict[int, int] | None:
    # PlayerPreThink and PlayerPostThink on three pages each: what both reach on pages 0 and 1, what only
    # Pre reaches on 0, 1, then 2, what only Post reaches on 0, 1, then 3
    by = {e.name: set(e.fns) for e in ex}
    pre, post = by["PlayerPreThink"], by["PlayerPostThink"]
    used = [0] * npages
    shared = pages_ffd(sorted(pre & post), pr, [0, 1], used)
    only_pre = pages_ffd(sorted(pre - post), pr, [0, 1, 2], used) if shared is not None else None
    only_post = pages_ffd(sorted(post - pre), pr, [0, 1, 3], used) if only_pre is not None else None
    if only_post is None:
        return None
    return shared | only_pre | only_post


def report(tag: str, pk: Packing) -> None:
    ex = pk.ex
    by_pages = Counter()
    for ci, e in enumerate(ex):
        by_pages[pk.k[ci]] += e.roots
    over = [(e, pk.k[ci]) for ci, e in enumerate(ex) if pk.k[ci] > CODE_SLOTS]
    rate_over = sum(e.weight for e, _ in over if e.weight >= TOUCH_HZ)
    fills = [u for u in pk.used if u]
    print(f"  {tag:14} cost {pk.cost:9.1f}  pages {len(fills):2} ({min(fills)}..{max(fills)} B)  executions by pages: "
          + " ".join(f"{k}:{v}" for k, v in sorted(by_pages.items()))
          + f"  | over {CODE_SLOTS}: {sum(e.roots for e, _ in over)} executions, {rate_over:.0f}/s of them at run time")
    for e, k in sorted(over, key=lambda t: -t[0].weight)[:4]:
        print(f"      {k} pages  {e.weight:6.2f}/s  {e.name} (+{e.roots - 1} with the same reach)")


def main() -> None:
    p = mdlview.pak_read(*mdlview.read_pak(sys.argv[1]), "progs.dat")
    pr = parse(p)
    holds, _, _, graph = resolve(p, pr)
    ex = executions(pr, holds, graph)
    print(f"memberships (function in an execution's reach): {sum(len(e.fns) for e in ex)}")
    total = sum(pr.size)
    print(f"{len(pr.body)} functions, {total} B = {total / PAGE:.2f} pages; {sum(e.roots for e in ex)} executions, "
          f"{len(ex)} distinct reaches; largest reach {max(sum(pr.size[f] for f in e.fns) for e in ex)} B")
    for npages in [int(a) for a in sys.argv[2:]] or [11, 12, 14]:
        print(f"\n{npages} pages:")
        if (start := qcc_order(pr, npages)) is not None:
            pk = Packing(pr, ex, npages, start)
            pk.check()
            report("qcc order", pk)
        start = greedy(pr, ex, npages)
        if start is None:
            print("  greedy: does not fit")
            continue
        pk = Packing(pr, ex, npages, start)
        pk.check()
        report("greedy", pk)
        passes = pk.improve(random.Random(1))
        pk.check()
        report(f"improved ({passes})", pk)
        steps = int(ANNEAL_STEPS)
        pk.anneal(random.Random(2), steps, t_hot=200.0, t_cold=0.05)
        pk.improve(random.Random(3))
        pk.check()
        report(f"annealed {steps // 1000}k", pk)
        seed = player_seed(pr, ex, npages)
        if seed is None:
            print("  player seed: the two per-tick reaches do not go on three pages each")
        else:
            pinned = frozenset(seed)
            pk = Packing(pr, ex, npages, greedy(pr, ex, npages, seed))
            pk.check()
            report("seeded", pk)
            pk.anneal(random.Random(4), int(ANNEAL_STEPS), t_hot=200.0, t_cold=0.05, pinned=pinned)
            pk.improve(random.Random(5), pinned)
            pk.check()
            report("seeded, pinned", pk)
            pk.anneal(random.Random(6), int(ANNEAL_STEPS), t_hot=20.0, t_cold=0.05)
            pk.improve(random.Random(7))
            pk.check()
            report("then unpinned", pk)


if __name__ == "__main__":
    main()
