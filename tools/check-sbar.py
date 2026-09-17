#!/usr/bin/env python3
"""Check a screenshot's status bar directly against PAK0's gfx.wad."""

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


def qpic(lumps: dict[str, bytes], name: str) -> tuple[int, int, bytes]:
    data = lumps[name.upper()]
    width, height = struct.unpack_from("<ii", data)
    return width, height, data[8:]


def bmp_pixels(path: str) -> bytearray:
    data = open(path, "rb").read()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    if (width, height) != (320, 200):
        raise AssertionError(f"{path}: expected 320x200, got {width}x{height}")
    stride = (width + 3) & ~3
    return bytearray(
        pixel
        for y in range(height)
        for pixel in data[
            offset + (height - 1 - y) * stride : offset + (height - 1 - y) * stride + width
        ]
    )


def picture(dst: bytearray, lumps: dict[str, bytes], name: str, x: int, y: int) -> None:
    width, height, pixels = qpic(lumps, name)
    for row in range(height):
        for column in range(width):
            pixel = pixels[row * width + column]
            if pixel != 255:
                dst[(y + row) * 320 + x + column] = pixel


def character(dst: bytearray, chars: bytes, x: int, y: int, code: int) -> None:
    sx = code % 16 * 8
    sy = code // 16 * 8
    for row in range(8):
        for column in range(8):
            pixel = chars[(sy + row) * 128 + sx + column]
            if pixel:
                dst[(y + row) * 320 + x + column] = pixel


def text(dst: bytearray, chars: bytes, x: int, y: int, value: str) -> None:
    for index, glyph in enumerate(value):
        character(dst, chars, x + index * 8, y, ord(glyph))


def big_number(
    dst: bytearray,
    lumps: dict[str, bytes],
    x: int,
    y: int,
    value: int,
    red: bool,
) -> None:
    value_text = str(value)[-3:]
    x += (3 - len(value_text)) * 24
    for index, glyph in enumerate(value_text):
        name = "MINUS" if glyph == "-" else glyph
        picture(dst, lumps, f"{'A' if red else ''}NUM_{name}", x + index * 24, y)


def small_number(dst: bytearray, chars: bytes, x: int, value: int) -> None:
    for index, glyph in enumerate(f"{value:3d}"[-3:]):
        if glyph != " ":
            character(dst, chars, x + index * 8, 0, 18 + int(glyph))


def base_bar(lumps: dict[str, bytes], lower: str) -> bytearray:
    _width, _height, upper = qpic(lumps, "IBAR")
    _width, _height, bottom = qpic(lumps, lower)
    return bytearray(upper + bottom)


def inventory(
    dst: bytearray,
    lumps: dict[str, bytes],
    chars: bytes,
    serverflags: int,
) -> None:
    picture(dst, lumps, "INVA1_SHOTGUN", 0, 8)
    for x, value in ((10, 25), (58, 0), (106, 0), (154, 0)):
        small_number(dst, chars, x, value)
    for index in range(4):
        if serverflags & (1 << index):
            picture(dst, lumps, f"SB_SIGIL{index + 1}", 288 + index * 8, 8)


def expected_normal(lumps: dict[str, bytes], serverflags: int) -> bytearray:
    chars = lumps["CONCHARS"]
    result = base_bar(lumps, "SBAR")
    inventory(result, lumps, chars, serverflags)
    big_number(result, lumps, 24, 24, 0, True)
    picture(result, lumps, "FACE1", 112, 24)
    big_number(result, lumps, 136, 24, 100, False)
    picture(result, lumps, "SB_SHELLS", 224, 24)
    big_number(result, lumps, 248, 24, 25, False)
    return result


def expected_scores(lumps: dict[str, bytes], title: str, serverflags: int) -> bytearray:
    chars = lumps["CONCHARS"]
    _width, _height, scorebar = qpic(lumps, "SCOREBAR")
    result = bytearray(scorebar)
    text(result, chars, 8, 4, "Monsters:  0 /  0")
    text(result, chars, 8, 12, "Secrets :  0 /  0")
    text(result, chars, 184, 4, "Time :  0:00")
    text(result, chars, 232 - len(title) * 4, 12, title)
    if serverflags >= 0:
        _width, _height, ibar = qpic(lumps, "IBAR")
        result = bytearray(ibar) + result
        inventory(result, lumps, chars, serverflags)
    return result


def main() -> int:
    if len(sys.argv) not in (4, 5):
        print(f"usage: {sys.argv[0]} PAK0.PAK SCREEN.BMP normal|scores [serverflags]", file=sys.stderr)
        return 2
    pak, entries = mdlview.read_pak(sys.argv[1])
    lumps = wad_lumps(mdlview.pak_read(pak, entries, "gfx.wad"))
    serverflags = int(sys.argv[4]) if len(sys.argv) == 5 else 0
    match sys.argv[3]:
        case "normal":
            expected = expected_normal(lumps, serverflags)
        case "scores":
            expected = expected_scores(lumps, "DM3-ish by Aardappel", serverflags)
        case "scores24":
            expected = expected_scores(lumps, "DM3-ish by Aardappel", -1)
        case mode:
            raise AssertionError(f"unknown mode {mode}")
    actual = bmp_pixels(sys.argv[2])[-len(expected) :]
    changed = sum(left != right for left, right in zip(actual, expected))
    if changed:
        raise AssertionError(f"status bar differs from id art at {changed} pixels")
    print(f"ok: {sys.argv[3]} status bar matches id art ({len(expected)} pixels)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
