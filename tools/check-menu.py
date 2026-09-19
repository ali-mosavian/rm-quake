#!/usr/bin/env python3
"""Check the main, Help and DOS quit menus directly against id's PAK0 pictures."""

from __future__ import annotations

import struct
import sys

import mdlview


QUIT_MESSAGES = (
    ("  Are you gonna quit    ", "  this game just like   ",
     "   everything else?     ", "                        "),
    (" Milord, methinks that  ", "   thou art a lowly     ",
     " quitter. Is this true? ", "                        "),
    (" Do I need to bust your ", "  face open for trying  ",
     "        to quit?        ", "                        "),
    (" Man, I oughta smack you", "   for trying to quit!  ",
     "     Press Y to get     ", "      smacked out.      "),
    (" Press Y to quit like a ", "   big loser in life.   ",
     "  Press N to stay proud ", "    and successful!     "),
    ("   If you press Y to    ", "  quit, I will summon   ",
     "  Satan all over your   ", "      hard drive!       "),
    ("  Um, Asmodeus dislikes ", " his children trying to ",
     " quit. Press Y to return", "   to your Tinkertoys.  "),
    ("  If you quit now, I'll ", "  throw a blanket-party ",
     "   for you next time!   ", "                        "),
)


def qpic(data: bytes) -> tuple[int, int, bytes]:
    width, height = struct.unpack_from("<ii", data)
    return width, height, data[8:]


def wad_lumps(wad: bytes) -> dict[str, bytes]:
    count, directory = struct.unpack_from("<ii", wad, 4)
    lumps = {}
    for index in range(count):
        entry = wad[directory + index * 32 : directory + index * 32 + 32]
        offset, _disk_size, size = struct.unpack_from("<iii", entry)
        name = entry[16:32].split(b"\0", 1)[0].decode("latin-1").upper()
        lumps[name] = wad[offset : offset + size]
    return lumps


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


def glyph(chars: bytes, code: int) -> bytes:
    return bytes(
        255 if pixel == 0 else pixel
        for row in range(8)
        for pixel in chars[(code // 16 * 8 + row) * 128 + code % 16 * 8 :][:8]
    )


def place_raw(
    screen: bytearray,
    pixels: bytes,
    width: int,
    height: int,
    x: int,
    y: int,
    stride: int,
) -> None:
    for row in range(height):
        for column in range(width):
            pixel = pixels[row * width + column]
            if pixel != 255:
                screen[(y + row) * stride + x + column] = pixel


def quit_dialog(pak: bytes, entries: dict[str, tuple[int, int]], message: tuple[str, ...]) -> bytes:
    width, height = 208, 48
    dialog = bytearray([255]) * (width * height)

    def pic(name: str, x: int, y: int) -> None:
        pic_width, pic_height, pixels = qpic(mdlview.pak_read(pak, entries, name))
        place_raw(dialog, pixels, pic_width, pic_height, x, y, width)

    pic("gfx/box_tl.lmp", 0, 0)
    pic("gfx/box_bl.lmp", 0, 40)
    pic("gfx/box_tr.lmp", 200, 0)
    pic("gfx/box_br.lmp", 200, 40)
    for y in range(8, 40, 8):
        pic("gfx/box_ml.lmp", 0, y)
        pic("gfx/box_mr.lmp", 200, y)
    for x in range(8, 200, 16):
        pic("gfx/box_tm.lmp", x, 0)
        pic("gfx/box_mm.lmp", x, 8)
        for y in range(16, 40, 8):
            pic("gfx/box_mm2.lmp", x, y)
        pic("gfx/box_bm.lmp", x, 40)

    chars = wad_lumps(mdlview.pak_read(pak, entries, "gfx.wad"))["CONCHARS"]
    for y, line in enumerate(message, 1):
        for x, code in enumerate(line.encode("ascii"), 1):
            place_raw(dialog, glyph(chars, code + 128), 8, 8, x * 8, y * 8, width)
    return bytes(dialog)


def main() -> int:
    if len(sys.argv) != 7:
        print(
            f"usage: {sys.argv[0]} PAK0.PAK MAIN.BMP HELP.BMP GAME.BMP QUIT.BMP CONSOLE.BMP",
            file=sys.stderr,
        )
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
    game = bmp_pixels(sys.argv[4])
    quit_actual = bmp_pixels(sys.argv[5])
    faded = bytearray(
        pixel if (x & 3) == ((y & 1) << 1) else 0
        for y in range(200)
        for x, pixel in enumerate(game[y * 320 : (y + 1) * 320])
    )
    candidates = []
    for message in QUIT_MESSAGES:
        expected = bytearray(faded)
        place_raw(expected, quit_dialog(pak, entries, message), 208, 48, 56, 76, 320)
        candidates.append(
            sum(
                a != b
                for index, (a, b) in enumerate(zip(quit_actual, expected))
                if index // 320 >= 8
            )
        )
    changed = min(candidates)
    if changed:
        raise AssertionError(f"quit menu differs from every id dialog at {changed} pixels")
    console = bmp_pixels(sys.argv[6])
    for message in QUIT_MESSAGES:
        dialog = quit_dialog(pak, entries, message)
        opaque = (
            (pixel, (76 + index // 208) * 320 + 56 + index % 208)
            for index, pixel in enumerate(dialog)
            if pixel != 255
        )
        if all(console[index] == pixel for pixel, index in opaque):
            raise AssertionError("toggleconsole left the quit dialog owning the screen")
    print("ok: main, Help and DOS quit menus match id art and fade the playfield")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
