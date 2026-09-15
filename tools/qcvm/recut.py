"""recut.py -- the traced ticks cut to the QuakeC the monsters-only progs.dat keeps.

    python3 recut.py FULL_PROGS.DAT TRIM_PROGS.DAT trace.bin...

An execution (X..x) counts as kept when every function it entered is in the trimmed image by name,
dropped when none is, mixed otherwise. Every kept or mixed execution is one C-to-QuakeC crossing.
"""

import struct
import sys
from collections import Counter
from pathlib import Path

RECORD = {"T": 12, "t": 0, "X": 8, "x": 4, "F": 2, "R": 2, "B": 1, "D": 2, "S": 0, "W": 4, "P": 1}
KIND = ("other", "StartFrame", "PlayerPreThink", "PlayerPostThink", "think", "touch", "blocked", "wake")


def names(path: str) -> list[str]:
    d = Path(path).read_bytes()
    h = struct.unpack_from("<15i", d)
    strs = d[h[10]:h[10] + h[11]]
    out = []
    for k in range(h[9]):
        first, _, _, _, s_name = struct.unpack_from("<5i", d, h[8] + k * 36)
        out.append(strs[s_name:strs.index(b"\0", s_name)].decode())
    return out


def check_names(full: list[str], trace: str) -> None:
    """The trace's function numbers index the progs.dat the engine ran. Read against another build,
    every name is wrong and the cut still prints plausible numbers: mixed 68.5 for 151.5 on e1m3."""
    txt = Path(trace).with_suffix(".txt")
    for line in txt.read_text().splitlines():
        if line.startswith("fn "):
            _, n, name, *_ = line.split()
            if int(n) >= len(full) or full[int(n)] != name:
                got = full[int(n)] if int(n) < len(full) else "past the end"
                sys.exit(f"{trace}: function {n} is {name} in {txt.name} but {got} in the full image")


def pct(v: list[int], q: float) -> int:
    s = sorted(v)
    return s[min(len(s) - 1, int(q * len(s)))]


def main() -> None:
    full = names(sys.argv[1])
    trim = set(names(sys.argv[2]))
    for path in sys.argv[3:]:
        check_names(full, path)
        data = Path(path).read_bytes()
        stack: list[tuple[int, set[int]]] = []
        ticks: list[Counter] = []
        tick: Counter | None = None
        dropped_fns, mixed_fns = Counter(), Counter()
        i = 0
        while i < len(data):
            tag = chr(data[i])
            i += 1
            match tag:
                case "T":
                    tick = Counter()
                    ticks.append(tick)
                case "t":
                    tick = None
                case "X":
                    kind = data[i]
                    stack.append((kind, set()))
                case "F":
                    stack[-1][1].add(struct.unpack_from("<H", data, i)[0])
                case "x":
                    (n,) = struct.unpack_from("<I", data, i)
                    kind, fns = stack.pop()
                    if tick is None or KIND[kind] == "wake" or any(k == 7 for k, _ in stack):
                        pass
                    else:
                        kept = sum(full[f] in trim for f in fns)
                        cls = "kept" if kept == len(fns) and fns else "dropped" if kept == 0 else "mixed"
                        tick["stmts"] += n
                        tick[cls] += n
                        if cls != "dropped":
                            tick["crossings"] += 1
                        if cls == "mixed":
                            mixed_fns.update(full[f] for f in fns if full[f] not in trim)
                        if cls == "dropped":
                            dropped_fns[KIND[kind]] += n
            i += RECORD[tag]
        tot = sum(t["stmts"] for t in ticks) or 1
        line = [Path(path).stem, f"ticks {len(ticks)}"]
        for key in ("stmts", "kept", "mixed", "crossings"):
            v = [t[key] for t in ticks]
            line.append(f"{key} {sum(v) / len(v):.1f} (p99 {pct(v, 0.99)})")
        line.append(f"kept+mixed share {(sum(t['kept'] + t['mixed'] for t in ticks)) / tot:.3f}")
        print("  ".join(line))
        print("   dropped by kind:", dict(dropped_fns.most_common()))
        print("   outside the trim in mixed executions:", mixed_fns.most_common(8))


if __name__ == "__main__":
    main()
