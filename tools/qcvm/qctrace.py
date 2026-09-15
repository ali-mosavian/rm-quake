"""qctrace.py -- what a real game's QuakeC costs a tick, from a QuakeSpasm trace.

    python3 qctrace.py PAK0.PAK trace.bin

The trace comes from QuakeSpasm with tools/qcvm/quakespasm-trace.patch (its trace.c lists the records).
Per server tick: statements, executions by kind, builtins, and the EMS maps of replaying the tick's page
needs, in order, through an LRU of slots that is cold at the tick's start -- the renderer and the mixer
remap the frame's four windows between ticks.

Page needs:
  code     a function entry, a return, and the resume after a nested execution, on the function's page
  edicts   a QuakeC field access on another edict; the physics loop's visit of every edict in edict
           order around the thinks; every edict for find, findradius and nextent
  strings  a string compare or a print builtin, on one string page
  globals  each function entry's parm_start page of the 45,884-byte globals

Two layouts. "as stored" needs all four. "proposed" (docs/quakec-vm.md) interns strings and keeps temps
and constants beside their function and the shared globals in conventional memory, so only code and edict
pages remain. Executions outside a tick (spawn, connect, level change) and QTRACE_WAKE's calls are counted
apart and not replayed.
"""

import json
import random
import struct
import sys
from collections import Counter, OrderedDict
from dataclasses import dataclass, field
from pathlib import Path

import mdlview
import qcopt
from qcpack import PAGE, parse
from qcptr import resolve

EDICT_BYTES = 876   # 195 QuakeC field words (780 B) + the engine's edict_t header on a 32-bit id build
EDICTS_PER_PAGE = PAGE // EDICT_BYTES
SLOTS = 4
NPAGES = 14
KIND = ("other", "StartFrame", "PlayerPreThink", "PlayerPostThink", "think", "touch", "blocked", "wake")
K_PRETHINK, K_POSTTHINK, K_THINK, K_WAKE = 2, 3, 4, 7
WALK_BUILTINS = {18: "find", 22: "findradius", 47: "nextent"}
PRINT_BUILTINS = {10: "error", 11: "objerror", 23: "bprint", 24: "sprint", 25: "dprint", 72: "centerprint"}
POKES = ("teleport", "refill", "death", "weapon not owned")


@dataclass(slots=True)
class Tick:
    time: float
    edicts: int
    awake: int
    missiles: int
    thinkers: int
    statements: int = 0
    executions: Counter = field(default_factory=Counter)
    builtins: Counter = field(default_factory=Counter)
    needs: list[tuple[str, int]] = field(default_factory=list)
    fns: set[int] = field(default_factory=set)


@dataclass(slots=True)
class Trace:
    ticks: list[Tick] = field(default_factory=list)
    executions: list[frozenset[int]] = field(default_factory=list)
    outside: Counter = field(default_factory=Counter)
    outside_statements: int = 0
    pokes: Counter = field(default_factory=Counter)
    woken: tuple[int, int] | None = None


def read(path: str) -> Trace:
    data = Path(path).read_bytes()
    tr = Trace()
    stacks: list[list[int]] = []
    entered: list[set[int]] = []
    kinds: list[int] = []
    tick: Tick | None = None
    visited = 0

    def live() -> Tick | None:
        return tick if tick is not None and K_WAKE not in kinds else None

    def walk_to(t: Tick, last: int) -> int:
        t.needs.extend(("edict", n) for n in range(visited + 1, last + 1))
        return max(visited, last)

    i = 0
    while i < len(data):
        tag = chr(data[i])
        i += 1
        match tag:
            case "T":
                tick = Tick(*struct.unpack_from("<fHHHH", data, i))
                i += 12
                tr.ticks.append(tick)
                visited = 0
            case "t":
                if tick is not None:
                    visited = walk_to(tick, tick.edicts - 1)
                tick = None
            case "X":
                kind, _depth, _fnum, self_, _cls = struct.unpack_from("<BBHHH", data, i)
                i += 8
                stacks.append([])
                entered.append(set())
                kinds.append(kind)
                if (t := live()) is None:
                    tr.outside[KIND[kind]] += 1
                    continue
                t.executions[KIND[kind]] += 1
                if kind in (K_PRETHINK, K_THINK):
                    visited = walk_to(t, self_)
            case "x":
                (n,) = struct.unpack_from("<I", data, i)
                i += 4
                t = live()
                stacks.pop()
                kinds.pop()
                fns = entered.pop()
                if t is None:
                    tr.outside_statements += n
                    continue
                t.statements += n
                tr.executions.append(frozenset(fns))
                if stacks and stacks[-1]:
                    t.needs += [("code", stacks[-1][-1]), ("glob", stacks[-1][-1])]
            case "F" | "R":
                (fnum,) = struct.unpack_from("<H", data, i)
                i += 2
                if tag == "F":
                    stacks[-1].append(fnum)
                    entered[-1].add(fnum)
                else:
                    stacks[-1].pop()
                if (t := live()) is not None:
                    t.needs += [("code", fnum), ("glob", fnum)]
                    t.fns.add(fnum)
            case "B":
                num = data[i]
                i += 1
                if (t := live()) is None:
                    continue
                t.builtins[num] += 1
                if num in WALK_BUILTINS:
                    t.needs.extend(("edict", n) for n in range(1, t.edicts))
                if num in PRINT_BUILTINS:
                    t.needs.append(("str", 0))
            case "D":
                (edict,) = struct.unpack_from("<H", data, i)
                i += 2
                if (t := live()) is not None:
                    t.needs.append(("edict", edict))
            case "S":
                if (t := live()) is not None:
                    t.needs.append(("str", 0))
            case "W":
                tr.woken = struct.unpack_from("<HH", data, i)
                i += 4
            case "P":
                tr.pokes[POKES[data[i]]] += 1
                i += 1
            case _:
                raise ValueError(f"bad tag {tag!r} at offset {i - 1}")
    if stacks:
        raise ValueError(f"{len(stacks)} executions never ended: the trace is cut short")
    return tr


def lru_maps(keys: list, slots: int) -> int:
    held: OrderedDict = OrderedDict()
    maps = 0
    for k in keys:
        if k in held:
            held.move_to_end(k)
            continue
        maps += 1
        held[k] = None
        if len(held) > slots:
            held.popitem(last=False)
    return maps


def packings(p: bytes, pr, cache_dir: Path) -> dict[str, dict[int, int]]:
    out = {"qcc order": qcopt.qcc_order(pr, NPAGES)}
    for slots in (3, 2):
        cache = cache_dir / f"qcpack-{slots}.json"
        if not cache.exists():
            holds, _, _, graph = resolve(p, pr)
            qcopt.CODE_SLOTS = slots
            ex = qcopt.executions(pr, holds, graph)
            pk = qcopt.Packing(pr, ex, NPAGES, qcopt.greedy(pr, ex, NPAGES))
            pk.improve(random.Random(1))
            pk.anneal(random.Random(2), qcopt.ANNEAL_STEPS, t_hot=200.0, t_cold=0.05)
            pk.improve(random.Random(3))
            pk.check()
            cache.write_text(json.dumps(pk.page_of))
        out[f"annealed for {slots}"] = {int(k): v for k, v in json.loads(cache.read_text()).items()}
    return out


def pct(values: list[int], q: float) -> int:
    s = sorted(values)
    return s[min(len(s) - 1, int(q * len(s)))]


def summary(values: list[int]) -> str:
    return f"p50 {pct(values, .5):5}  p99 {pct(values, .99):5}  max {max(values):5}  mean {sum(values) / len(values):8.1f}"


def main() -> None:
    pak, trace = sys.argv[1], sys.argv[2]
    p = mdlview.pak_read(*mdlview.read_pak(pak), "progs.dat")
    pr = parse(p)
    h = struct.unpack_from("<16i", p, 0)
    parm_start = [struct.unpack_from("<2i", p, h[8] + f * 36)[1] for f in range(h[9])]
    tr = read(trace)
    ticks = tr.ticks
    span = ticks[-1].time - ticks[0].time
    print(f"{len(ticks)} ticks (all included) over {span:.1f} s of game time = {(len(ticks) - 1) / span:.1f} ticks/s")
    print(f"bot: {dict(tr.pokes) or 'no interventions'}; woken, monsters: {tr.woken}")
    print(f"outside ticks: {tr.outside_statements} statements, executions {dict(tr.outside)}")
    print(f"statements a tick:   {summary([t.statements for t in ticks])}")
    kinds = sum((t.executions for t in ticks), Counter())
    print("executions a tick:  ", {k: round(v / len(ticks), 2) for k, v in kinds.most_common()})
    builtins = sum((t.builtins for t in ticks), Counter())
    print("builtins a tick, top:", {WALK_BUILTINS.get(k, PRINT_BUILTINS.get(k, k)): round(v / len(ticks), 2)
                                    for k, v in builtins.most_common(12)})
    print("ticks by monsters awake:", dict(sorted(Counter(min(t.awake, 8) for t in ticks).items())),
          f"(8 = 8+); edicts max {max(t.edicts for t in ticks)}, missiles max {max(t.missiles for t in ticks)}")

    print(f"\nEMS maps a tick, slots cold at the tick's start, {EDICTS_PER_PAGE} edicts a page:")
    for name, page_of in packings(p, pr, Path(trace).parent).items():
        def key(need: tuple[str, int], stored: bool):
            kind, v = need
            match kind:
                case "code":
                    return ("code", page_of[v])
                case "edict":
                    return ("edict", v // EDICTS_PER_PAGE)
                case "str":
                    return ("str", 0) if stored else None
                case "glob":
                    return ("glob", parm_start[v] * 4 // PAGE) if stored else None

        rows = {
            "code only, 2 slots": ([key(n, False) for n in t.needs if n[0] == "code"] for t in ticks),
            "code only, 3 slots": ([key(n, False) for n in t.needs if n[0] == "code"] for t in ticks),
            "proposed, 4 slots": ([k for n in t.needs if (k := key(n, False))] for t in ticks),
            "as stored, 4 slots": ([k for n in t.needs if (k := key(n, True))] for t in ticks),
        }
        per_exec = [len({page_of[f] for f in e}) for e in tr.executions]
        per_tick = [len({page_of[f] for f in t.fns}) for t in ticks]
        print(f"  packing: {name}  (pages an execution: mean {sum(per_exec) / len(per_exec):.2f}, max {max(per_exec)}, "
              f"{sum(k > 3 for k in per_exec)} over 3; pages a tick: mean {sum(per_tick) / len(per_tick):.2f}, "
              f"max {max(per_tick)})")
        for label, seqs in rows.items():
            slots = 2 if "2 slots" in label else 3 if "3 slots" in label else SLOTS
            print(f"    {label:20} {summary([lru_maps(s, slots) for s in seqs])}")
    txt = Path(trace).with_suffix(".txt")
    if txt.exists():
        print()
        for line in txt.read_text().splitlines():
            if line.startswith(("ticks ", "woken ", "path ", "profile check")):
                print(line)


if __name__ == "__main__":
    main()
