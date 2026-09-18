#!/usr/bin/env python3
"""mksprite.py PAK0.PAK out.bin -- pack Quake's reachable sprites for qgl."""

from __future__ import annotations

import struct
import sys
from dataclasses import dataclass
from pathlib import Path

from pakget import member

PAGE = 16384
HEADER_BYTES = 256
SPRITES = ("s_bubble", "s_explod", "s_light")


@dataclass(frozen=True, slots=True)
class Frame:
    left: int
    right: int
    up: int
    down: int
    width: int
    height: int
    pixels: bytes


def next_power_of_two(value: int) -> int:
    return 1 << (value - 1).bit_length()


def read_sprite(pak: bytes, name: str) -> list[Frame]:
    raw = member(pak, f"progs/{name}.spr")
    if len(raw) < 36:
        raise ValueError(f"{name}: truncated sprite header")
    magic, version, kind, _radius, _width, _height, nframe, beam, sync = struct.unpack_from(
        "<3if3ifi", raw, 0
    )
    if magic != 0x50534449 or version != 1:
        raise ValueError(f"{name}: not an IDSP version-1 sprite")
    if kind != 2 or beam != 0.0 or sync != 0:
        raise ValueError(f"{name}: unsupported sprite type, beam, or sync")
    frames: list[Frame] = []
    pos = 36
    for index in range(nframe):
        if pos + 20 > len(raw):
            raise ValueError(f"{name}: truncated frame {index}")
        frame_type = struct.unpack_from("<i", raw, pos)[0]
        pos += 4
        if frame_type != 0:
            raise ValueError(f"{name}: frame groups are unsupported")
        ox, oy, width, height = struct.unpack_from("<4i", raw, pos)
        pos += 16
        count = width * height
        pixels = raw[pos : pos + count]
        if width < 1 or height < 1 or len(pixels) != count:
            raise ValueError(f"{name}: invalid frame {index}")
        pos += count
        frames.append(Frame(ox, ox + width, oy, oy - height, width, height, pixels))
    if pos != len(raw):
        raise ValueError(f"{name}: {len(raw) - pos} trailing bytes")
    return frames


def build_sprite_bank(pak: bytes) -> bytes:
    models = [read_sprite(pak, name) for name in SPRITES]
    frames = [frame for model in models for frame in model]
    if len(models) != 3 or len(frames) != 9:
        raise ValueError("stock sprite bank must contain 3 models and 9 frames")

    blob = bytearray(HEADER_BYTES)
    defs: list[tuple[int, int]] = []
    records: list[tuple[int, int, int, int, int, int, int]] = []
    first = 0
    for model in models:
        defs.append((first, len(model)))
        first += len(model)
        for frame in model:
            tw = next_power_of_two(frame.width)
            th = next_power_of_two(frame.height)
            size = tw * th
            used = len(blob) % PAGE
            if used + size > PAGE:
                blob.extend(bytes(PAGE - used))
            offset = len(blob)
            cell = bytearray([255]) * size
            for y in range(frame.height):
                src = y * frame.width
                dst = y * tw
                cell[dst : dst + frame.width] = frame.pixels[src : src + frame.width]
            blob.extend(cell)
            records.append((offset, frame.left, frame.right, frame.up, frame.down, tw, th))

    header = bytearray(struct.pack("<4s4H", b"QSP1", 1, len(models), len(frames), HEADER_BYTES))
    header.extend(b"".join(struct.pack("<2H", *definition) for definition in defs))
    header.extend(b"".join(struct.pack("<I6h", *record) for record in records))
    if len(header) > HEADER_BYTES:
        raise ValueError("sprite metadata exceeds its header row")
    blob[: len(header)] = header
    blob.extend(bytes(-len(blob) % HEADER_BYTES))
    return bytes(blob)


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(__doc__)
        return 1
    Path(argv[2]).write_bytes(build_sprite_bank(Path(argv[1]).read_bytes()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
