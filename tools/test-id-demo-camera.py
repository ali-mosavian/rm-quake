"""Regression: title snapshots retain id's camera, HUD, and visible entities."""

import math
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mkidcam
from pakget import member


ROOT = Path(__file__).resolve().parent.parent
PAK = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.home() / "dos/QUAKE_SW/ID1/PAK0.PAK"
EXPECTED = {"demo1": "e1m3", "demo2": "e1m4", "demo3": "e1m6"}
FRAME = struct.Struct("<6fl12h")
ENTITY = struct.Struct("<5h4f")


def read_track(path: Path) -> tuple[str, list[tuple[tuple[float, ...], list[tuple[float, ...]]]]]:
    data = path.read_bytes()
    assert data[:4] == b"QDS1", f"{path.name}: bad magic"
    map_name = data[4:13].split(b"\0", 1)[0].decode("ascii")
    records = []
    offset = 13
    while offset < len(data):
        frame = FRAME.unpack_from(data, offset)
        offset += FRAME.size
        entities = [ENTITY.unpack_from(data, offset + i * ENTITY.size) for i in range(frame[-1])]
        offset += len(entities) * ENTITY.size
        records.append((frame, entities))
    assert offset == len(data), f"{path.name}: trailing snapshot bytes"
    return map_name, records


def main() -> None:
    pak_data = PAK.read_bytes()
    ogre_names = mkidcam.mdl_frame_names(member(pak_data, "progs/ogre.mdl"))
    ogre_frames = mkidcam.compact_frame_map(pak_data, "ogre", mkidcam.MONSTERS["ogre"][1])
    alternate_deaths = [i for i, name in enumerate(ogre_names) if name.startswith("bdeath")]
    assert alternate_deaths and all(ogre_frames[i] > 0 for i in alternate_deaths), "ogre bdeath mapped to stand"
    assert mkidcam.armor_kind(8192 | 16384 | 32768) == 3, "red armor was not retained"
    assert mkidcam.item_kind("armor", 2) == 18, "red armor pickup became another item"
    assert mkidcam.power_bits(524288 | 1048576 | 2097152 | 4194304) == 15, "powerups were not retained"
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp)
        for demo, expected_map in EXPECTED.items():
            track = out / f"{demo}.qdc"
            subprocess.run(
                [sys.executable, ROOT / "tools/mkidcam.py", PAK, f"{demo}.dem", track],
                check=True,
            )
            map_name, records = read_track(track)
            assert map_name == expected_map, f"{demo}: {map_name}, want {expected_map}"
            assert len(records) > 100, f"{demo}: only {len(records)} camera records"
            frames = [frame for frame, _entities in records]
            assert all(a[0] < b[0] for a, b in zip(frames, frames[1:])), f"{demo}: time did not advance"
            travel = sum(math.dist(a[1:4], b[1:4]) for a, b in zip(frames, frames[1:]))
            turn = sum(abs((b[5] - a[5] + 180) % 360 - 180) for a, b in zip(frames, frames[1:]))
            assert travel > 1000, f"{demo}: camera only travelled {travel:g} units"
            assert turn > 180, f"{demo}: camera only turned {turn:g} degrees"
            types = {entity[1] for _frame, entities in records for entity in entities}
            assert {1, 2, 3, 4} <= types, f"{demo}: missing brush, monster, pickup, or light snapshots: {types}"
            item_kinds = {entity[2] for _frame, entities in records for entity in entities if entity[1] == 3}
            assert 0 in item_kinds, f"{demo}: health packs disappeared"
            if demo == "demo3":
                assert item_kinds & {10, 11}, "demo3: medieval keys disappeared"
            assert any(frame[7] > 0 and frame[9] > 0 for frame in frames), f"{demo}: no HUD health/weapon state"
            assert all(frame[17] > 0 for frame in frames), f"{demo}: view-model identity disappeared"
            if demo != "demo1":
                assert any(frame[15] > 0 for frame in frames), f"{demo}: armor type disappeared"
    print("ok: id demo snapshots retain cameras, HUD state, brushes, monsters, and pickups")


if __name__ == "__main__":
    main()
