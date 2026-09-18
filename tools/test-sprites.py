#!/usr/bin/env python3
"""Check sprites.bin against the original PAK frames, including transparent padding."""

from __future__ import annotations

import struct
import sys
from pathlib import Path

import mksprite


def main(argv: list[str]) -> int:
    pak = Path(argv[1]).read_bytes()
    bank = mksprite.build_sprite_bank(pak)
    magic, version, models, frames, header = struct.unpack_from("<4s4H", bank, 0)
    assert (magic, version, models, frames, header) == (b"QSP1", 1, 3, 9, 256)
    definitions = [struct.unpack_from("<2H", bank, 12 + i * 4) for i in range(models)]
    assert definitions == [(0, 2), (2, 6), (8, 1)]
    source = [frame for name in mksprite.SPRITES for frame in mksprite.read_sprite(pak, name)]
    for i, frame in enumerate(source):
        offset, left, right, up, down, tw, th = struct.unpack_from("<I6h", bank, 24 + i * 16)
        assert (left, right, up, down) == (frame.left, frame.right, frame.up, frame.down)
        assert offset // mksprite.PAGE == (offset + tw * th - 1) // mksprite.PAGE
        cell = bank[offset : offset + tw * th]
        assert len(cell) == tw * th
        for y in range(frame.height):
            assert cell[y * tw : y * tw + frame.width] == frame.pixels[y * frame.width : (y + 1) * frame.width]
            assert cell[y * tw + frame.width : (y + 1) * tw] == bytes([255]) * (tw - frame.width)
        assert cell[frame.height * tw :] == bytes([255]) * ((th - frame.height) * tw)
    print(f"ok: {frames} original sprite frames round-trip in {len(bank):,} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
