#!/usr/bin/env python3
"""Prove the six-bit surface-cache light hash is exact for every shipped map."""

import re
import sys
from pathlib import Path

import lighthash


def main() -> int:
    root = Path(__file__).parent.parent
    strings = tuple(re.findall(r'^\s*"([a-z]+)",?$', (root / "src/render/ls.c").read_text(), re.MULTILINE))
    if strings[: len(lighthash.WORLD_PATTERNS)] != lighthash.WORLD_PATTERNS:
        print("FAIL: Python and runtime light-style patterns differ", file=sys.stderr)
        return 1

    synthetic = {
        (0, 2, 36): lighthash.reachable_keys((0, 2, 36), {36}),
        (33, 34, 35, 0): lighthash.reachable_keys((33, 34, 35, 0), {33, 34, 35}),
        (0, 1, 6): lighthash.reachable_keys((0, 1, 6), set()),
        (63, 64): lighthash.reachable_keys((63, 64), set()),
        (2, 12): lighthash.reachable_keys((2, 12), {12}),
    }
    if tuple(map(len, synthetic.values())) != (53, 9, 16, 2, 27):
        print(f"FAIL: synthetic reachable-state counts are {tuple(map(len, synthetic.values()))}", file=sys.stderr)
        return 1
    for styles, keys in synthetic.items():
        curve = lighthash.collision_curve(keys)
        if curve[-1].collisions:
            print(f"FAIL: synthetic styles {styles} have no exact six-bit hash", file=sys.stderr)
            return 1
        for buckets, choice in enumerate(curve, 1):
            collisions = len(keys) - len(
                {
                    lighthash.cache_hash(key, buckets, choice.multiplier, choice.shift)
                    for key in keys
                }
            )
            if collisions != choice.collisions:
                print(f"FAIL: {buckets}-bucket result misreports its collisions", file=sys.stderr)
                return 1

    maps = sorted((root / "data").glob("*.bsp"))
    if not maps:
        print("FAIL: no BSP maps", file=sys.stderr)
        return 1

    worst = (0, "", ())
    for path in maps:
        for styles, keys in lighthash.bsp_light_keys(path.read_bytes()).items():
            states = len(keys)
            if states > worst[0]:
                worst = (states, path.stem, styles)
            choice = lighthash.best_hash(keys, 64)
            hashes = {
                lighthash.cache_hash(key, 64, choice.multiplier, choice.shift)
                for key in keys
            }
            if len(hashes) != states:
                print(
                    f"FAIL: {path.stem} styles {styles}: {states - len(hashes)} collisions",
                    file=sys.stderr,
                )
                return 1

    print(f"ok: six-bit light hash is exact; worst available set is {worst}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
