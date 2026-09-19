#!/usr/bin/env python3
"""Build QUAKE.QPK: every converted map and title demo in one archive.

    python3 tools/mkqpak.py QUAKE.QPK map1.qmp ... --demos demo1.qdc ...
"""
import hashlib
import os
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path


QMAP_HEAD = 64
QMAP_NAME = 16
QMAP_ENT = 24
QPAK_HEAD = 64
QPAK_MAP_NAME = 8
QPAK_MAP_ENT = 16
QPAK_ALIGN = 16
QPAK_MAX_MAPS = 16
QPAK_MAX_MEMBERS = 80


@dataclass(frozen=True)
class Member:
    name: str
    data: bytes


@dataclass(frozen=True)
class Map:
    name: str
    members: tuple[Member, ...]


def align(value: int) -> int:
    return value + (-value % QPAK_ALIGN)


def read_qmap(path: Path) -> Map:
    data = path.read_bytes()
    magic, version, count, directory, _checksum, name = struct.unpack_from("<4slHlL16s", data)
    if magic != b"QMAP" or version != 11:
        raise SystemExit(f"{path}: not a QMAP v11 container")
    if count < 1 or count > 80 or directory < QMAP_HEAD or directory + count * QMAP_ENT > len(data):
        raise SystemExit(f"{path}: malformed directory")
    map_name = name.split(b"\0", 1)[0].decode("ascii")
    if not map_name or len(map_name) > QPAK_MAP_NAME:
        raise SystemExit(f"{path}: map name {map_name!r} does not fit QPAK")
    members: list[Member] = []
    for index in range(count):
        member, offset, size = struct.unpack_from("<16sll", data, directory + index * QMAP_ENT)
        member_name = member.split(b"\0", 1)[0].decode("ascii")
        if not member_name or offset < 0 or size < 0 or offset + size > len(data):
            raise SystemExit(f"{path}: malformed member {index}")
        members.append(Member(member_name, data[offset : offset + size]))
    return Map(map_name, tuple(members))


def parse(argv: list[str]) -> tuple[Path, list[Path], list[Path]]:
    if "--demos" not in argv:
        raise SystemExit("usage: mkqpak.py <out.qpk> <map.qmp>... --demos <demo.qdc>...")
    divider = argv.index("--demos")
    if divider < 2 or divider == len(argv) - 1:
        raise SystemExit("need an output, at least one map, and at least one demo")
    return Path(argv[0]), [Path(path) for path in argv[1:divider]], [Path(path) for path in argv[divider + 1 :]]


def build(output: Path, map_paths: list[Path], demo_paths: list[Path]) -> None:
    maps = [read_qmap(path) for path in map_paths]
    if len(maps) > QPAK_MAX_MAPS or len({entry.name for entry in maps}) != len(maps):
        raise SystemExit("QPAK map names must be unique and fit its fixed index")
    # e1m3 and e1m6 already use QMAP's full 80-member directory. The
    # demos live in a non-playable global directory, which pak_open_member
    # searches without growing the loaded map's real-mode directory.
    maps.append(Map("@global", tuple(Member(path.name, path.read_bytes()) for path in demo_paths)))
    if any(len(entry.members) > QPAK_MAX_MEMBERS for entry in maps):
        raise SystemExit(f"a QPAK map exceeds {QPAK_MAX_MEMBERS} members")

    index_offset = QPAK_HEAD
    directory_offsets: dict[str, int] = {}
    directory_size = len(maps) * QPAK_MAP_ENT
    cursor = align(index_offset + directory_size)
    for entry in maps:
        directory_offsets[entry.name] = cursor
        cursor = align(cursor + len(entry.members) * QMAP_ENT)
    payload_offset = cursor

    payloads: dict[bytes, tuple[int, bytes]] = {}
    for entry in maps:
        for member in entry.members:
            digest = hashlib.sha256(member.data).digest()
            if digest in payloads:
                if payloads[digest][1] != member.data:
                    raise SystemExit(f"SHA-256 collision for {member.name}")
                continue
            payloads[digest] = (cursor, member.data)
            cursor = align(cursor + len(member.data))

    parts = [bytearray(QPAK_HEAD), bytearray(len(maps) * QPAK_MAP_ENT)]
    current = QPAK_HEAD + len(parts[1])
    if current < align(current):
        parts.append(bytearray(align(current) - current))
        current = align(current)
    for entry in maps:
        want = directory_offsets[entry.name]
        parts.append(bytearray(want - current))
        directory = bytearray()
        for member in entry.members:
            digest = hashlib.sha256(member.data).digest()
            offset, _ = payloads[digest]
            directory += struct.pack("<16sll", member.name.encode(), offset, len(member.data))
        parts.append(directory)
        current = want + len(directory)
        if current < align(current):
            parts.append(bytearray(align(current) - current))
            current = align(current)
    if current != payload_offset:
        raise AssertionError((current, payload_offset))
    for offset, data in sorted(payloads.values()):
        parts.append(bytearray(offset - current))
        parts.append(bytearray(data))
        current = offset + len(data)
        if current < align(current):
            parts.append(bytearray(align(current) - current))
            current = align(current)

    blob = bytearray().join(parts)
    struct.pack_into("<4slHlL", blob, 0, b"QPAK", 1, len(maps), index_offset,
                     zlib.crc32(blob[QPAK_HEAD:]) & 0xFFFFFFFF)
    for index, entry in enumerate(maps):
        struct.pack_into("<8shlh", blob, index_offset + index * QPAK_MAP_ENT,
                         entry.name.encode(), len(entry.members), directory_offsets[entry.name], 0)
    output.write_bytes(blob)

    # Round trip through the same two-level lookup the real-mode loader uses.
    for index, entry in enumerate(maps):
        name, count, directory, _pad = struct.unpack_from("<8shlh", blob, index_offset + index * QPAK_MAP_ENT)
        assert name.split(b"\0", 1)[0].decode() == entry.name and count == len(entry.members)
        for member_index, member in enumerate(entry.members):
            member_name, offset, size = struct.unpack_from("<16sll", blob, directory + member_index * QMAP_ENT)
            assert member_name.split(b"\0", 1)[0].decode() == member.name
            assert blob[offset : offset + size] == member.data
    raw = sum(len(member.data) for entry in maps for member in entry.members)
    print(f"  {output.name}: {len(maps) - 1} maps, {sum(len(entry.members) for entry in maps)} members, "
          f"{raw:,} logical -> {len(blob):,} bytes ({raw - sum(len(data) for _, data in payloads.values()):,} shared)")


def main() -> None:
    output, maps, demos = parse(sys.argv[1:])
    build(output, maps, demos)


if __name__ == "__main__":
    main()
