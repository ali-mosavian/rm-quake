#!/usr/bin/env python3
"""Regression checks for exact model animation-frame selection."""

from mdlsets import MONSTER_MODELS
from mdlsets import select_indices


def chosen(names: list[str], spec: str) -> list[str]:
    indices, _counts = select_indices(names, spec)
    return [names[index] for index in indices]


def main() -> int:
    names = ["shoot1", "shoot2", "shoot3", "attackb1", "attackb1", "attackb2"]
    assert chosen(names, "shoot1+shoot2+shoot2+shoot3") == ["shoot1", "shoot2", "shoot2", "shoot3"]
    indices, _counts = select_indices(names, "attackb1+attackb2")
    assert indices == [3, 5], "an exact duplicate name must resolve like QuakeC's first frame macro"

    specs = dict(MONSTER_MODELS)
    assert "shoot2+shoot2" in specs["ogre"], "ogre_nail3 must repeat shoot2"
    assert "magatt6+magatt5" in specs["wizard"], "wiz_fast7 must return through magatt5"
    assert "magic6+magic9" in specs["shambler"], "sham_magic skips unused magic7 and magic8"
    assert "attb13+attb13" in specs["zombie"], "zombie_attb14 displays attb13 again"
    print("ok: exact animation selectors retain repeated and skipped QuakeC frames")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
