#!/usr/bin/env python3
"""Check the single-player intermission art directly against PAK0."""

from __future__ import annotations

import struct
import sys

import mdlview


def wad_lumps(wad: bytes) -> dict[str, bytes]:
    count, directory = struct.unpack_from("<ii", wad, 4)
    result = {}
    for index in range(count):
        entry = wad[directory + index * 32 : directory + index * 32 + 32]
        offset, _disk_size, size = struct.unpack_from("<iii", entry)
        name = entry[16:32].split(b"\0", 1)[0].decode("latin-1").upper()
        result[name] = wad[offset : offset + size]
    return result


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


def place(expected: dict[int, int], data: bytes, x: int, y: int) -> None:
    width, height, pixels = qpic(data)
    for row in range(height):
        for column in range(width):
            pixel = pixels[row * width + column]
            if pixel != 255:
                expected[(y + row) * 320 + x + column] = pixel


def number(expected: dict[int, int], lumps: dict[str, bytes], x: int, y: int, value: int) -> None:
    text = str(value)[-3:]
    x += (3 - len(text)) * 24
    for index, glyph in enumerate(text):
        place(expected, lumps[f"NUM_{glyph}"], x + index * 24, y)


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} PAK0.PAK SCREEN.BMP", file=sys.stderr)
        return 2
    pak, entries = mdlview.read_pak(sys.argv[1])
    lumps = wad_lumps(mdlview.pak_read(pak, entries, "gfx.wad"))
    expected: dict[int, int] = {}
    place(expected, mdlview.pak_read(pak, entries, "gfx/complete.lmp"), 64, 24)
    place(expected, mdlview.pak_read(pak, entries, "gfx/inter.lmp"), 0, 56)
    number(expected, lumps, 160, 64, 0)
    place(expected, lumps["NUM_COLON"], 234, 64)
    place(expected, lumps["NUM_0"], 246, 64)
    place(expected, lumps["NUM_0"], 266, 64)
    number(expected, lumps, 160, 104, 0)
    place(expected, lumps["NUM_SLASH"], 232, 104)
    number(expected, lumps, 240, 104, 6)
    number(expected, lumps, 160, 144, 0)
    place(expected, lumps["NUM_SLASH"], 232, 144)
    number(expected, lumps, 240, 144, 10)

    actual = bmp_pixels(sys.argv[2])
    changed = sum(actual[index] != pixel for index, pixel in expected.items())
    if changed:
        raise AssertionError(f"intermission differs from id art at {changed} opaque pixels")
    print(f"ok: intermission matches id art ({len(expected)} opaque pixels)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
