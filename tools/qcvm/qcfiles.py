"""qcfiles.py -- how far QuakeC crosses .qc file boundaries, from progs.dat's call graph.

    python3 qcfiles.py PAK0.PAK out.svg

direct: the other files a file's functions call. reach: the other files reached by following calls
function by function from any of a file's functions -- not file to file, because a file that calls one
function of ai.qc does not reach what ai.qc's other functions call. per execution: the other files one
engine call into QuakeC (a think, touch, use, spawn or engine entry point) reaches. Counted by name, and
again with self.th_*() and the like resolved to every function ever stored into that field.
The SVG is the direct graph as a matrix: caller file down, callee file across, shade by function pairs.
"""

import sys
from collections import Counter

import mdlview
from qcpack import parse


def closure(calls: dict[int, set[int]], r: int) -> set[int]:
    seen, todo = {r}, [r]
    while todo:
        for c in calls[todo.pop()]:
            if c not in seen:
                seen.add(c)
                todo.append(c)
    return seen


def histogram(title: str, counts: list[int], names: list[str] | None = None) -> None:
    h = Counter(counts)
    top = max(h.values())
    print(f"\n{title}  (n={len(counts)}, max {max(counts)}, median {sorted(counts)[len(counts) // 2]})")
    for n in range(max(counts) + 1):
        if not h.get(n) and names is None:
            continue
        who = " " + " ".join(sorted(x for x, v in zip(names, counts) if v == n)) if names else ""
        print(f"  {n:3} | {'#' * round(40 * h.get(n, 0) / top):40} {h.get(n, 0):4}{who}")


def svg(path: str, files: list[str], pairs: Counter) -> None:
    cell, left, top = 18, 90, 90
    size = len(files) * cell
    peak = max(pairs.values())
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{left + size + 20}" height="{top + size + 40}" '
           f'font-family="Helvetica" font-size="11"><rect width="100%" height="100%" fill="white"/>']
    for i, f in enumerate(files):
        name = f[:-3]
        out.append(f'<text x="{left - 4}" y="{top + i * cell + 13}" text-anchor="end">{name}</text>')
        out.append(f'<text transform="translate({left + i * cell + 13},{top - 4}) rotate(-60)">{name}</text>')
    for i, a in enumerate(files):
        for j, b in enumerate(files):
            n = pairs.get((a, b), 0)
            shade = 255 - round(215 * (n / peak) ** 0.5) if n else 255
            fill = "#e8e8e8" if a == b else f"rgb({shade},{shade},255)" if n else "white"
            out.append(f'<rect x="{left + j * cell}" y="{top + i * cell}" width="{cell - 1}" height="{cell - 1}" '
                       f'fill="{fill}" stroke="#ddd"/>')
            if n and a != b:
                out.append(f'<text x="{left + j * cell + 8}" y="{top + i * cell + 13}" text-anchor="middle" '
                           f'font-size="8">{n}</text>')
    out.append(f'<text x="{left}" y="{top + size + 25}">caller file down, callee file across; '
               f'number = distinct function pairs, by name</text></svg>')
    with open(path, "w") as fh:
        fh.write("\n".join(out))


def main() -> None:
    blob, entries = mdlview.read_pak(sys.argv[1])
    pr = parse(mdlview.pak_read(blob, entries, "progs.dat"))
    files = sorted({pr.file[f] for f in pr.body})
    for calls, name in ((pr.direct, "by name"), (pr.calls, "self.th_*() resolved")):
        cl = {f: closure(calls, f) for f in pr.body}
        direct = {a: {pr.file[c] for f in pr.body if pr.file[f] == a for c in calls[f]} - {a} for a in files}
        reach = {a: {pr.file[g] for f in pr.body if pr.file[f] == a for g in cl[f]} - {a} for a in files}
        print(f"\n===== {name}")
        histogram(f"{name}: other .qc files a file calls directly", [len(direct[a]) for a in files],
                  [a[:-3] for a in files])
        histogram(f"{name}: other .qc files a file reaches, following calls", [len(reach[a]) for a in files],
                  [a[:-3] for a in files])
        roots = sorted(pr.roots)
        per_exec = [len({pr.file[g] for g in cl[r]} - {pr.file[r]}) for r in roots]
        histogram(f"{name}: other .qc files one execution reaches", per_exec)
        worst = sorted(roots, key=lambda r: -len({pr.file[g] for g in cl[r]}))[:3]
        print("  most: " + "; ".join(f"{pr.name[r]} ({pr.file[r][:-3]}) -> "
                                     + " ".join(sorted({pr.file[g][:-3] for g in cl[r]} - {pr.file[r][:-3]}))
                                     for r in worst))
    pairs = Counter((pr.file[f], pr.file[c]) for f in pr.body for c in pr.direct[f])
    svg(sys.argv[2], files, pairs)


if __name__ == "__main__":
    main()
