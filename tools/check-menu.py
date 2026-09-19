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
    pixels = bytes(
        pixel
        for y in range(height)
        for pixel in data[offset + (height - 1 - y) * stride :][:width]
    )
    if len(pixels) != width * height:
        raise AssertionError(f"{path}: short pixel data ({len(pixels)} bytes)")
    return pixels


def place(screen: bytearray, data: bytes, x: int, y: int) -> None:
    width, height, pixels = qpic(data)
    for row in range(height):
        for column in range(width):
            pixel = pixels[row * width + column]
            if pixel != 255:
                screen[(y + row) * 320 + x + column] = pixel


def main() -> int:
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} PAK0.PAK MAIN.BMP HELP.BMP", file=sys.stderr)
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
    help_width, help_height, help_pixels = qpic(
        mdlview.pak_read(pak, entries, "gfx/help0.lmp")
    )
    if (help_width, help_height) != (320, 200):
        raise AssertionError(f"help0 is {help_width}x{help_height}, wanted 320x200")
    if len(help_pixels) != 320 * 200:
        raise AssertionError(f"help0 has {len(help_pixels)} pixels, wanted 64000")
    help_actual = bmp_pixels(sys.argv[3])
    help_changed = sum(a != b for a, b in zip(help_actual, help_pixels))
    if help_changed:
        raise AssertionError(f"help menu differs from id art at {help_changed} pixels")
    print("ok: main and help menus match id art and clear the playfield")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
