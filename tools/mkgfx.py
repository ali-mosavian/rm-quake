#!/usr/bin/env python3
"""mkgfx.py -- pack id's single-player status-bar pictures into one EMS atlas."""

from __future__ import annotations

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mdlview as mdl  # noqa: E402

ATLAS_W = 512
ATLAS_H = 272
CELL = 24
CELL_Y = 120
CELL_COLS = 21
WEAPON_Y = 192
WEAPON_COLS = 21
ITEM_Y = 224
CHARS_Y = 240

CELLS = [
    *(f"NUM_{d}" for d in range(10)), "NUM_MINUS",
    *(f"ANUM_{d}" for d in range(10)), "ANUM_MINUS",
    "SB_SHELLS", "SB_NAILS", "SB_ROCKET", "SB_CELLS",
    "SB_ARMOR1", "SB_ARMOR2", "SB_ARMOR3",
    "FACE1", "FACE_P1", "FACE2", "FACE_P2", "FACE3", "FACE_P3",
    "FACE4", "FACE_P4", "FACE5", "FACE_P5",
    "FACE_INVIS", "FACE_INVUL2", "FACE_INV2", "FACE_QUAD", "DISC",
]

WEAPONS = ("shotgun", "sshotgun", "nailgun", "snailgun", "rlaunch", "srlaunch")
WEAPON_PICS = [
    name
    for weapon in WEAPONS
    for name in (
        f"INV_{weapon}", f"INV2_{weapon}",
        *(f"INVA{i}_{weapon}" for i in range(1, 6)),
    )
]

ITEMS = (
    "SB_KEY1", "SB_KEY2", "SB_INVIS", "SB_INVULN", "SB_SUIT", "SB_QUAD",
    "SB_SIGIL1", "SB_SIGIL2", "SB_SIGIL3", "SB_SIGIL4",
)


def wad_lumps(wad: bytes) -> dict[str, bytes]:
    count, directory = struct.unpack_from("<ii", wad, 4)
    lumps = {}
    for index in range(count):
        entry = wad[directory + index * 32 : directory + index * 32 + 32]
        offset, _disk_size, size = struct.unpack_from("<iii", entry)
        name = entry[16:32].split(b"\0", 1)[0].decode("latin-1").upper()
        lumps[name] = wad[offset : offset + size]
    return lumps


def qpic(lumps: dict[str, bytes], name: str, width: int, height: int) -> bytes:
    data = lumps[name.upper()]
    actual = struct.unpack_from("<ii", data)
    if actual != (width, height):
        raise ValueError(f"{name}: {actual}, wanted {(width, height)}")
    pixels = data[8:]
    if len(pixels) != width * height:
        raise ValueError(f"{name}: short qpic")
    return pixels


def blit(atlas: bytearray, pixels: bytes, x: int, y: int, width: int, height: int) -> None:
    for row in range(height):
        start = (y + row) * ATLAS_W + x
        atlas[start : start + width] = pixels[row * width : (row + 1) * width]


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} PAK0.PAK out/", file=sys.stderr)
        return 1
    pak, outdir = sys.argv[1:]
    blob, entries = mdl.read_pak(pak)
    lumps = wad_lumps(mdl.pak_read(blob, entries, "gfx.wad"))
    atlas = bytearray([255]) * (ATLAS_W * ATLAS_H)

    blit(atlas, qpic(lumps, "SBAR", 320, 24), 0, 0, 320, 24)
    blit(atlas, qpic(lumps, "IBAR", 320, 24), 0, 24, 320, 24)
    blit(atlas, qpic(lumps, "SCOREBAR", 320, 24), 0, 48, 320, 24)

    for index, name in enumerate(CELLS):
        x = index % CELL_COLS * CELL
        y = CELL_Y + index // CELL_COLS * CELL
        blit(atlas, qpic(lumps, name, CELL, CELL), x, y, CELL, CELL)

    for index, name in enumerate(WEAPON_PICS):
        x = index % WEAPON_COLS * 24
        y = WEAPON_Y + index // WEAPON_COLS * 16
        blit(atlas, qpic(lumps, name, 24, 16), x, y, 24, 16)

    x = 0
    for name in ITEMS:
        width = 8 if name.startswith("SB_SIGIL") else 16
        blit(atlas, qpic(lumps, name, width, 16), x, ITEM_Y, width, 16)
        x += width

    chars = lumps["CONCHARS"]
    if len(chars) != 128 * 128:
        raise ValueError(f"CONCHARS: {len(chars)}, wanted 16384")
    for code in range(256):
        pixels = b"".join(
            chars[(code // 16 * 8 + row) * 128 + code % 16 * 8 :][:8]
            for row in range(8)
        )
        pixels = bytes(255 if pixel == 0 else pixel for pixel in pixels)
        blit(atlas, pixels, code % 64 * 8, CHARS_Y + code // 64 * 8, 8, 8)

    os.makedirs(outdir, exist_ok=True)
    path = os.path.join(outdir, "sbar.raw")
    open(path, "wb").write(atlas)
    count = 3 + len(CELLS) + len(WEAPON_PICS) + len(ITEMS) + 256
    print(f"  sbar.raw       {len(atlas):,} B, {count} status elements")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
