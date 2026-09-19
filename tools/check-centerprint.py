#!/usr/bin/env python3
"""Check a captured centerprint against the real QMP font and palette."""

from __future__ import annotations

import struct
import sys


def member(qmp: bytes, wanted: bytes) -> bytes:
    count, directory = struct.unpack_from("<Hl", qmp, 8)
    for index in range(count):
        name, offset, size = struct.unpack_from("<16sll", qmp, directory + index * 24)
        if name.split(b"\0")[0] == wanted:
            return qmp[offset : offset + size]
    raise AssertionError(f"missing {wanted.decode()}")


def main() -> int:
    qmp = open(sys.argv[1], "rb").read()
    font = member(qmp, b"font.fnt")[4:]
    palette = member(qmp, b"pal.raw")
    bmp = open(sys.argv[2], "rb").read()
    want_width, want_height, first_y, step = map(int, sys.argv[3:7])
    mode, message = sys.argv[7:9]
    pixel_offset = struct.unpack_from("<I", bmp, 10)[0]
    width, height = struct.unpack_from("<ii", bmp, 18)
    assert (width, height) == (want_width, want_height)
    stride = (width + 3) & ~3

    def bestfit(red: int, green: int, blue: int) -> int:
        return min(
            range(256),
            key=lambda index: sum(
                (palette[index * 3 + channel] - value) ** 2
                for channel, value in enumerate((red, green, blue))
            ),
        )

    def pixel(x: int, y: int) -> int:
        return bmp[pixel_offset + (height - 1 - y) * stride + x]

    expected = set()
    for row, line in enumerate(message.split("\n")):
        x0 = (width - len(line) * 4) // 2
        y0 = first_y + row * step
        for column, character in enumerate(line):
            glyph = font[ord(character) * 8 : ord(character) * 8 + 8]
            words = struct.unpack("<4H", glyph)
            for gy in range(8):
                for gx in range(8):
                    bit = gy * 8 + gx
                    if words[bit >> 4] & (1 << (15 - (bit & 15))):
                        expected.add((x0 + column * 4 + gx, y0 + gy))

    if mode == "menu_absent":
        expected = {
            (x, y)
            for x, y in expected
            if not (56 <= x < 264 and 76 <= y < 124)
            and (x & 3) == ((y & 1) << 1)
        }
    colour = bestfit(240, 232, 216)
    matching = sum(pixel(x, y) == colour for x, y in expected)
    if mode == "present" and matching != len(expected):
        raise SystemExit(
            f"FAIL: {len(expected) - matching} of {len(expected)} centerprint pixels are missing"
        )
    if mode == "absent" and matching > len(expected) // 5:
        raise SystemExit(
            f"FAIL: {matching} of {len(expected)} hidden centerprint pixels remain"
        )
    if mode == "menu_absent" and matching:
        raise SystemExit(
            f"FAIL: {matching} of {len(expected)} menu-hidden centerprint pixels remain"
        )
    print(f"ok: centerprint {mode}, {matching}/{len(expected)} matching pixels")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
