"""Regression: id demo snapshots retain the Thunderbolt and its beam."""

from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mkidcam


def main() -> None:
    assert mkidcam.local_items(64) == 256
    assert mkidcam.local_weapon(64) == 256
    assert mkidcam.item_kind("g_light", 0) == 21

    state = mkidcam.DemoState(view_entity=7)
    payload = bytes([6]) + struct.pack("<h6h", 7, 8, 16, 24, 80, 88, 96)
    mkidcam.parse_temp_entity(mkidcam.Reader(payload), state)
    assert state.events == [
        (0, mkidcam.ENT_BEAM_START, 21, 0, 0, 1.0, 2.0, 3.0, 0.0),
        (0, mkidcam.ENT_BEAM_END, 21, 0, 0, 10.0, 11.0, 12.0, 0.0),
    ]


if __name__ == "__main__":
    main()
    print("ok: id demo Thunderbolt state and beam survive conversion")
