#!/usr/bin/env python3
"""mkgfx.py -- pack id's single-player status-bar pictures into one EMS atlas."""

from __future__ import annotations

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mdlview as mdl  # noqa: E402

ATLAS_W = 512
ATLAS_H = 320
CELL = 24
INTER_X = 0
INTER_Y = 0
COMPLETE_X = 160
COMPLETE_Y = 0
FINALE_X = 160
FINALE_Y = 24
BAR_X = 160
SBAR_Y = 48
IBAR_Y = 72
SCORE_Y = 96
WORK_X = 160
WORK_Y = 120
CELL_Y = 168
CELL_COLS = 21
WEAPON_Y = 240
WEAPON_COLS = 21
ITEM_Y = 272
ITEM_X = 336
SYMBOL_X = 48
SYMBOL_Y = 216
CHARS_Y = 288

MENU_W = 256
MENU_H = 384
MENU_CHARS_Y = 320

CELLS = [
    *(f"NUM_{d}" for d in range(10)), "NUM_MINUS",
    *(f"ANUM_{d}" for d in range(10)), "ANUM_MINUS",
    "SB_SHELLS", "SB_NAILS", "SB_ROCKET", "SB_CELLS",
    "SB_ARMOR1", "SB_ARMOR2", "SB_ARMOR3",
    "FACE1", "FACE_P1", "FACE2", "FACE_P2", "FACE3", "FACE_P3",
    "FACE4", "FACE_P4", "FACE5", "FACE_P5",
    "FACE_INVIS", "FACE_INVUL2", "FACE_INV2", "FACE_QUAD", "DISC",
]

WEAPONS = (
    ("shotgun", 24), ("sshotgun", 24), ("nailgun", 24),
    ("snailgun", 24), ("rlaunch", 24), ("srlaunch", 24), ("lightng", 48),
)
WEAPON_PICS = [
    (name, width)
    for weapon, width in WEAPONS
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


def blit(
    atlas: bytearray,
    pixels: bytes,
    x: int,
    y: int,
    width: int,
    height: int,
    stride: int = ATLAS_W,
) -> None:
    for row in range(height):
        start = (y + row) * stride + x
        atlas[start : start + width] = pixels[row * width : (row + 1) * width]


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} PAK0.PAK out/", file=sys.stderr)
        return 1
    pak, outdir = sys.argv[1:]
    blob, entries = mdl.read_pak(pak)
    lumps = wad_lumps(mdl.pak_read(blob, entries, "gfx.wad"))
    atlas = bytearray([255]) * (ATLAS_W * ATLAS_H)

    def pak_pic(name: str, width: int, height: int) -> bytes:
        data = mdl.pak_read(blob, entries, name)
        if struct.unpack_from("<ii", data) != (width, height):
            raise ValueError(f"{name}: wrong dimensions")
        return data[8:]

    blit(atlas, pak_pic("gfx/inter.lmp", 160, 144), INTER_X, INTER_Y, 160, 144)
    blit(atlas, pak_pic("gfx/complete.lmp", 192, 24), COMPLETE_X, COMPLETE_Y, 192, 24)
    blit(atlas, pak_pic("gfx/finale.lmp", 288, 24), FINALE_X, FINALE_Y, 288, 24)
    blit(atlas, qpic(lumps, "SBAR", 320, 24), BAR_X, SBAR_Y, 320, 24)
    blit(atlas, qpic(lumps, "IBAR", 320, 24), BAR_X, IBAR_Y, 320, 24)
    blit(atlas, qpic(lumps, "SCOREBAR", 320, 24), BAR_X, SCORE_Y, 320, 24)

    for index, name in enumerate(CELLS):
        x = index % CELL_COLS * CELL
        y = CELL_Y + index // CELL_COLS * CELL
        blit(atlas, qpic(lumps, name, CELL, CELL), x, y, CELL, CELL)

    x, y = 0, WEAPON_Y
    for name, width in WEAPON_PICS:
        if x + width > ATLAS_W:
            x, y = 0, y + 16
        blit(atlas, qpic(lumps, name, width, 16), x, y, width, 16)
        x += width
    if y != ITEM_Y or x > ITEM_X:
        raise ValueError("weapon pictures overlap the item row")

    x = ITEM_X
    for name in ITEMS:
        width = 8 if name.startswith("SB_SIGIL") else 16
        blit(atlas, qpic(lumps, name, width, 16), x, ITEM_Y, width, 16)
        x += width

    blit(atlas, qpic(lumps, "NUM_COLON", 16, 24), SYMBOL_X, SYMBOL_Y, 16, 24)
    blit(atlas, qpic(lumps, "NUM_SLASH", 16, 24), SYMBOL_X + 16, SYMBOL_Y, 16, 24)

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
    count = 8 + len(CELLS) + len(WEAPON_PICS) + len(ITEMS) + 256
    print(f"  sbar.raw       {len(atlas):,} B, {count} status elements")

    menu = bytearray([255]) * (MENU_W * MENU_H)
    menu_pics = (
        ("gfx/mainmenu.lmp", 0, 0, 240, 112),
        ("gfx/sp_menu.lmp", 0, 112, 232, 64),
        ("gfx/qplaque.lmp", 0, 176, 32, 144),
        ("gfx/ttl_main.lmp", 32, 176, 96, 24),
        ("gfx/ttl_sgl.lmp", 128, 176, 128, 24),
        ("gfx/p_option.lmp", 32, 200, 144, 24),
        ("gfx/ttl_cstm.lmp", 32, 248, 184, 24),
        ("gfx/p_load.lmp", 32, 272, 104, 24),
        ("gfx/p_save.lmp", 136, 272, 88, 24),
    )
    for name, x, y, width, height in menu_pics:
        blit(menu, pak_pic(name, width, height), x, y, width, height, MENU_W)
    for frame in range(6):
        blit(menu, pak_pic(f"gfx/menudot{frame + 1}.lmp", 16, 24),
             32 + frame * 16, 224, 16, 24, MENU_W)
    for code in range(256):
        pixels = b"".join(
            chars[(code // 16 * 8 + row) * 128 + code % 16 * 8 :][:8]
            for row in range(8)
        )
        pixels = bytes(255 if pixel == 0 else pixel for pixel in pixels)
        blit(menu, pixels, code % 32 * 8, MENU_CHARS_Y + code // 32 * 8, 8, 8, MENU_W)
    for page in range(6):
        menu += pak_pic(f"gfx/help{page}.lmp", 320, 200)
    open(os.path.join(outdir, "menu.raw"), "wb").write(menu)
    print(f"  menu.raw       {len(menu):,} B, one menu surface swapped in at a time")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
