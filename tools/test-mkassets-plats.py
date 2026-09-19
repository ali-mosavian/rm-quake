"""Regression: PLAT_LOW_TRIGGER must survive the packed entity record."""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import mkassets


def entities(root: Path, name: str) -> mkassets.Ents:
    data = (root / f"data/{name}.bsp").read_bytes()
    lumps = mkassets.read_lumps(data)
    ent_offset, ent_size = lumps[0]
    model_offset, model_size = lumps[14]
    text = data[ent_offset : ent_offset + ent_size].split(b"\0", 1)[0].decode("latin1")
    model_count = model_size // 64
    boxes = [struct.unpack_from("<6f", data, model_offset + index * 64) for index in range(model_count)]
    return mkassets.spawn_entities(text, model_count, boxes, 0, mkassets.CrateSet({}, 0))


def packed_plats(spawned: mkassets.Ents) -> list[tuple[int, float, float, int, int]]:
    data = mkassets.pack_entities(spawned, spawned.nmodels, 800.0, spawned.crates)
    tele_count = sum(target in spawned.dests for target, _model, _name, _only in spawned.trigs)
    offset = (
        struct.calcsize(mkassets.ENTS_HEAD)
        + len(spawned.mons) * struct.calcsize(mkassets.ENTS_MON)
        + tele_count * struct.calcsize("<h3ffhh")
    )
    size = struct.calcsize("<hffhh")
    return [struct.unpack_from("<hffhh", data, offset + index * size) for index in range(len(spawned.plats))]


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    expected = {
        "e1m1": [(7, 0), (22, 1)],
        "e1m3": [(38, 1), (39, 0)],
        "e1m4": [(17, 0), (18, 1)],
        "dm3ish": [(3, 0)],
    }
    for name, want in expected.items():
        actual = [(model, low) for model, _speed, _travel, _sound, low in packed_plats(entities(root, name))]
        assert actual == want, f"{name}: packed plat flags {actual}, wanted {want}"
    print("ok: PLAT_LOW_TRIGGER survives packing on stock and dm3ish lifts")


if __name__ == "__main__":
    main()
