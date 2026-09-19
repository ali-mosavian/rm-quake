"""Regression: exit targets must not renumber ids stored in existing saves."""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import mkassets


def lump(data: bytes, lumps: list[tuple[int, int]], index: int) -> bytes:
    offset, size = lumps[index]
    return data[offset : offset + size]


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    data = (root / "data/e1m7.bsp").read_bytes()
    lumps = mkassets.read_lumps(data)
    text = lump(data, lumps, 0).split(b"\0", 1)[0].decode("latin1")
    model_count = lumps[14][1] // 64
    models = lump(data, lumps, 14)
    boxes = [struct.unpack_from("<6f", models, index * 64) for index in range(model_count)]
    expected = {"t17": 7, "t27": 8, "t16": 9, "t22": 10, "t21": 11, "t18": 12}

    for skill in range(4):
        entities = mkassets.spawn_entities(text, model_count, boxes, skill, mkassets.CrateSet({}, 0))
        mkassets.pack_entities(entities, model_count, 800.0, entities.crates)
        actual = {name: entities.names.get(name) for name in expected}
        assert actual == expected, f"skill {skill}: exit target allocation renumbered ids: {actual}"

    print("ok: e1m7 exit targets preserve existing entity ids in all skills")


if __name__ == "__main__":
    main()
