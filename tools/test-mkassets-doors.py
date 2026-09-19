"""Regression: a secret door's outgoing target must survive entity packing."""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import mkassets


DOOR = struct.Struct("<h3f3fffhhhhhhhhh")


def main() -> None:
    text = """
    { "classname" "worldspawn" }
    { "classname" "func_door_secret" "model" "*1" "target" "open_lamp" }
    { "classname" "trigger_changelevel" "model" "*2" "target" "exit_first" "map" "e1m2" }
    """
    boxes = [
        (0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
        (0.0, 0.0, 0.0, 64.0, 16.0, 64.0),
        (0.0, 0.0, 0.0, 16.0, 16.0, 16.0),
    ]
    entities = mkassets.spawn_entities(text, 3, boxes, 0, mkassets.CrateSet({}, 0))
    data = mkassets.pack_entities(entities, 3, 800.0, entities.crates)
    offset = (
        struct.calcsize(mkassets.ENTS_HEAD)
        + len(entities.mons) * struct.calcsize(mkassets.ENTS_MON)
        + len(entities.plats) * struct.calcsize("<hffhh")
        + len(entities.hides) * 2
        + len(entities.gates) * struct.calcsize("<hhh")
        + len(entities.items) * struct.calcsize("<hhhhh3f")
    )
    door = DOOR.unpack_from(data, offset)
    assert door[10] == entities.names["open_lamp"], (
        f"packed secret target {door[10]}, wanted {entities.names['open_lamp']}"
    )
    assert entities.names["exit_first"] < entities.names["open_lamp"], entities.names
    print("ok: secret-door output targets survive packed entities")


if __name__ == "__main__":
    main()
