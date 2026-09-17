#!/usr/bin/env python3
"""Check the main menu directly against id's PAK0 pictures."""

from __future__ import annotations

import struct
import sys

import mdlview


def qpic(data: bytes) -> tuple[int, int, bytes]:
    width, height = struct.unpack_from("<ii", data)
    return width, height, data[8:]


def bmp_pixels(path: str) -> bytes:
    data = open(path, "rb").read()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    if (width, height) != (320, 200):
        raise AssertionError(f"{path}: expected 320x200, got {width}x{height}")
    stride = (width + 3) & ~3
    return bytes(
        pixel
        for y in range(height)
        for pixel in data[offset + (height - 1 - y) * stride :][:width]
    )


def place(screen: bytearray, data: bytes, x: int, y: int) -> None:
    width, height, pixels = qpic(data)
    for row in range(height):
        for column in range(width):
            pixel = pixels[row * width + column]
            if pixel != 255:
                screen[(y + row) * 320 + x + column] = pixel


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} PAK0.PAK SCREEN.BMP", file=sys.stderr)
        return 2
    pak, entries = mdlview.read_pak(sys.argv[1])
    expected = bytearray(320 * 200)
    place(expected, mdlview.pak_read(pak, entries, "gfx/qplaque.lmp"), 16, 4)
    place(expected, mdlview.pak_read(pak, entries, "gfx/ttl_main.lmp"), 112, 4)
    place(expected, mdlview.pak_read(pak, entries, "gfx/mainmenu.lmp"), 72, 32)

    actual = bmp_pixels(sys.argv[2])
    ignored = {
        y * 320 + x
        for y in range(32, 56)
        for x in range(54, 70)
    }
    changed = sum(
        actual[index] != pixel
        for index, pixel in enumerate(expected)
        if index not in ignored
    )
    if changed:
        raise AssertionError(f"main menu differs from id art at {changed} pixels")
    print("ok: main menu matches id art and clears the playfield")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
