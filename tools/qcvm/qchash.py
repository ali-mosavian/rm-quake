"""qchash.py -- id's progs.dat with its strings as hashes, for qvm/qvm.c.

    python3 tools/qcvm/qchash.py PROGS.DAT PROGSH.DAT

A string value becomes the FNV-1a 32 hash of its text, so EQ_S is an integer compare and needs
no text. Null stays 0 and "" becomes STR_EMPTY: equal to EQ_S, but only null is false to IF, as
id's offsets are. The text stays, deduplicated, for printing, with a hash-sorted index after the
globals. The field and function names get hash-sorted indexes of their own after it, {hash, def
number}, the first def of a name kept as ED_FindField and ED_FindFunction find it. The header grows
six ints -- each index's offset and count -- and a version of its own, so qvm_load refuses an image
that was never rewritten, or rewritten to an older meaning.

Refused, by name:
  - two distinct texts with one hash, or a text hashing to a reserved value: 0, STR_TEMP,
    STR_EMPTY, or 0x80000000, which is -0.0 and false to AND/OR. Names are texts, so this
    gate is also theirs.
  - a nonzero global no def covers, or a string global not at the start of a string: the
    rewrite would miss it or hash a text no gate saw
  - a global read as a string with a nonzero initial value that is not string-typed
"""

import struct
import sys
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

VERSION_ID = 6
VERSION_HASHED = 0x51480008  # 6 held "" as 0, 7 had no name indexes; bump on any change in meaning
STR_TEMP = 1
STR_EMPTY = 2
EV_STRING = 1
EV_VECTOR = 3
RESERVED = {
    0: "null",
    STR_TEMP: "STR_TEMP",
    STR_EMPTY: "STR_EMPTY",
    0x80000000: "-0.0 to AND/OR",
}

OPS = (
    "DONE MUL_F MUL_V MUL_FV MUL_VF DIV_F ADD_F ADD_V SUB_F SUB_V "
    "EQ_F EQ_V EQ_S EQ_E EQ_FNC NE_F NE_V NE_S NE_E NE_FNC LE GE LT GT "
    "LOAD_F LOAD_V LOAD_S LOAD_ENT LOAD_FLD LOAD_FNC ADDRESS "
    "STORE_F STORE_V STORE_S STORE_ENT STORE_FLD STORE_FNC "
    "STOREP_F STOREP_V STOREP_S STOREP_ENT STOREP_FLD STOREP_FNC RETURN "
    "NOT_F NOT_V NOT_S NOT_ENT NOT_FNC IF IFNOT CALL0 CALL1 CALL2 CALL3 CALL4 CALL5 CALL6 CALL7 CALL8 "
    "STATE GOTO AND OR BITAND BITOR"
).split()
STRING_READS = {"EQ_S": "ab", "NE_S": "ab", "NOT_S": "a", "STORE_S": "a", "STOREP_S": "a"}

type Hasher = Callable[[bytes], int]


class Refused(Exception):
    pass


def fnv1a(text: bytes) -> int:
    h = 0x811C9DC5
    for c in text:
        h = ((h ^ c) * 0x01000193) & 0xFFFFFFFF
    return h


def string_value(text: bytes, hasher: Hasher = fnv1a) -> int:
    return hasher(text) if text else STR_EMPTY


@dataclass(frozen=True, slots=True)
class Image:
    header: tuple[int, ...]
    data: bytes

    def lump(self, ofs_field: int, count_field: int, size: int) -> bytes:
        ofs = self.header[ofs_field]
        return self.data[ofs : ofs + self.header[count_field] * size]


def text_at(strs: bytes, ofs: int) -> bytes:
    return strs[ofs : strs.index(b"\0", ofs)]


def string_starts(strs: bytes) -> list[int]:
    return [0] + [i + 1 for i, c in enumerate(strs[:-1]) if c == 0]


def check_hashes(texts: set[bytes], hasher: Hasher) -> None:
    by_hash: dict[int, bytes] = {}
    for text in sorted(t for t in texts if t):
        h = hasher(text)
        if h in RESERVED:
            raise Refused(f"{text!r} hashes to {h:08x}, {RESERVED[h]}")
        if h in by_hash:
            raise Refused(f"{by_hash[h]!r} and {text!r} share hash {h:08x}")
        by_hash[h] = text


def check_complete(glob: tuple[int, ...], defs: list[tuple[int, int]], starts: set[int]) -> None:
    covered = {ofs for _, ofs in defs} | {ofs + k for t, ofs in defs if t == EV_VECTOR for k in (1, 2)}
    for g, v in enumerate(glob):
        if v and g not in covered:
            raise Refused(f"global {g} holds {v:#x} and no def covers it")
    for t, g in defs:
        if t == EV_STRING and glob[g] and glob[g] not in starts:
            raise Refused(f"string global {g} holds {glob[g]}, not the start of a string")


def check_operands(img: Image, glob: tuple[int, ...], string_globals: set[int]) -> None:
    h = img.header
    for i in range(h[3]):
        op, a, b, _ = struct.unpack_from("<HHHH", img.data, h[2] + i * 8)
        name = OPS[op] if op < len(OPS) else f"op{op}"
        for g in [{"a": a, "b": b}[slot] for slot in STRING_READS.get(name, "")]:
            if glob[g] and g not in string_globals:
                raise Refused(f"statement {i} {name} reads global {g} as a string, which is not string-typed")


def name_index(names: list[bytes], hasher: Hasher) -> list[tuple[int, int]]:
    first: dict[int, int] = {}
    for k, name in enumerate(names):
        if name and hasher(name) not in first:
            first[hasher(name)] = k
    return sorted(first.items())


def pack_index(index: list[tuple[int, int]]) -> bytes:
    return b"".join(struct.pack("<Ii", hv, o) for hv, o in index)


def rewrite(data: bytes, hasher: Hasher = fnv1a) -> bytes:
    img = Image(struct.unpack_from("<15i", data), data)
    h = img.header
    if h[0] != VERSION_ID:
        raise Refused(f"version {h[0]}, not id's {VERSION_ID}")
    strs = img.lump(10, 11, 1)
    starts = string_starts(strs)
    check_hashes({text_at(strs, o) for o in starts}, hasher)

    # the deduplicated text, first occurrences in order, "" at 0
    new_ofs: dict[bytes, int] = {b"": 0}
    out_strs = bytearray(b"\0")
    for o in starts:
        t = text_at(strs, o)
        if t not in new_ofs:
            new_ofs[t] = len(out_strs)
            out_strs += t + b"\0"

    start_set = set(starts)

    def moved(o: int, what: str) -> int:
        if o not in start_set:
            raise Refused(f"{what} names offset {o}, not the start of a string")
        return new_ofs[text_at(strs, o)]

    glob_in = struct.unpack_from(f"<{h[13]}i", data, h[12])
    globaldefs = bytearray(img.lump(4, 5, 8))
    defs: list[tuple[int, int]] = []
    for k in range(h[5]):
        t, ofs, name = struct.unpack_from("<HHi", globaldefs, k * 8)
        struct.pack_into("<HHi", globaldefs, k * 8, t, ofs, moved(name, f"globaldef {k}"))
        defs.append((t & 0x7FFF, ofs))
    string_globals = {ofs for t, ofs in defs if t == EV_STRING}
    check_complete(glob_in, defs, start_set)
    check_operands(img, glob_in, string_globals)

    fielddefs = bytearray(img.lump(6, 7, 8))
    field_names: list[bytes] = []
    for k in range(h[7]):
        t, ofs, name = struct.unpack_from("<HHi", fielddefs, k * 8)
        struct.pack_into("<HHi", fielddefs, k * 8, t, ofs, moved(name, f"fielddef {k}"))
        field_names.append(text_at(strs, name))

    functions = bytearray(img.lump(8, 9, 36))
    func_names: list[bytes] = []
    for k in range(h[9]):
        rec = list(struct.unpack_from("<6i", functions, k * 36))
        func_names.append(text_at(strs, rec[4]))
        rec[4], rec[5] = moved(rec[4], f"function {k}"), moved(rec[5], f"function {k}'s file")
        struct.pack_into("<6i", functions, k * 36, *rec)

    glob = bytearray(img.lump(12, 13, 4))
    for g in string_globals:
        if glob_in[g]:
            struct.pack_into("<I", glob, g * 4, string_value(text_at(strs, glob_in[g]), hasher))

    index = sorted((hasher(t), o) for t, o in new_ofs.items() if t)
    field_index = name_index(field_names, hasher)
    func_index = name_index(func_names, hasher)
    lumps = [
        img.lump(2, 3, 8),
        bytes(globaldefs),
        bytes(fielddefs),
        bytes(functions),
        bytes(out_strs),
        bytes(glob),
        pack_index(index),
        pack_index(field_index),
        pack_index(func_index),
    ]
    at = 84
    ofs = []
    for lump in lumps:
        ofs.append(at)
        at += len(lump)
    header = [VERSION_HASHED, h[1], ofs[0], h[3], ofs[1], h[5], ofs[2], h[7], ofs[3], h[9], ofs[4], len(out_strs),
              ofs[5], h[13], h[14], ofs[6], len(index), ofs[7], len(field_index), ofs[8], len(func_index)]
    return struct.pack("<21i", *header) + b"".join(lumps)


def main() -> None:
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    try:
        out = rewrite(src.read_bytes())
    except Refused as e:
        sys.exit(f"{src}: refused: {e}")
    dst.write_bytes(out)
    h = struct.unpack_from("<21i", out)
    print(f"{src} -> {dst}: {h[11]} bytes of text, {h[16]} hashed strings, {h[18]} field and {h[20]} function "
          f"names, {len(out)} bytes")


if __name__ == "__main__":
    main()
