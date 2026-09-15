"""test_qchash.py -- qchash.py's round trip and its refusals, on a real progs.dat.

    python3 tools/qcvm/test_qchash.py PROGS.DAT
"""

import struct
import sys
from pathlib import Path

import qchash

type Check = tuple[bool, str]

HEADER = "<21i"


# a crash is not a refusal: anything but Refused with its reason fails the check
def refused(data: bytes, why: str, hasher: qchash.Hasher = qchash.fnv1a) -> bool:
    try:
        qchash.rewrite(data, hasher)
    except qchash.Refused as e:
        return why in str(e)
    except Exception:
        return False
    return False


# an index running past the image reads as empty, so the checks on it fail rather than crash
def index_at(out: bytes, h: tuple[int, ...], ofs_field: int) -> list[tuple[int, int]]:
    if h[ofs_field] + h[ofs_field + 1] * 8 > len(out):
        return []
    return [struct.unpack_from("<Ii", out, h[ofs_field] + k * 8) for k in range(h[ofs_field + 1])]


def names(data: bytes) -> tuple[list[bytes], list[bytes]]:
    ih = struct.unpack_from("<15i", data)
    istr = data[ih[10] : ih[10] + ih[11]]
    fields = [qchash.text_at(istr, struct.unpack_from("<HHi", data, ih[6] + k * 8)[2]) for k in range(ih[7])]
    funcs = [qchash.text_at(istr, struct.unpack_from("<6i", data, ih[8] + k * 36)[4]) for k in range(ih[9])]
    return fields, funcs


# id's ED_FindField and ED_FindFunction walk the defs in order and stop at the first
def first_def(texts: list[bytes]) -> dict[int, int]:
    first: dict[int, int] = {}
    for k, text in enumerate(texts):
        if text:
            first.setdefault(qchash.fnv1a(text), k)
    return first


def name_checks(index: list[tuple[int, int]], texts: list[bytes], what: str) -> list[Check]:
    return [
        (dict(index) == first_def(texts), f"each of {len(texts)} {what} names finds its first def"),
        ([hv for hv, _ in index] == sorted(hv for hv, _ in index), f"the {what} name index is sorted by hash"),
    ]


def round_trip(data: bytes) -> list[Check]:
    out = qchash.rewrite(data)
    h = struct.unpack_from(HEADER, out)
    strs = out[h[10] : h[10] + h[11]]
    texts = [qchash.text_at(strs, o) for o in qchash.string_starts(strs)]
    index = index_at(out, h, 15)
    fields, funcs = names(data)

    ih = struct.unpack_from("<15i", data)
    istr = data[ih[10] : ih[10] + ih[11]]
    iglob = struct.unpack_from(f"<{ih[13]}i", data, ih[12])
    oglob = struct.unpack_from(f"<{h[13]}I", out, h[12])
    defs = [struct.unpack_from("<HHi", data, ih[4] + k * 8) for k in range(ih[5])]
    consts = [(ofs, qchash.text_at(istr, iglob[ofs])) for t, ofs, _ in defs if t & 0x7FFF == 1 and iglob[ofs]]
    empties = [ofs for ofs, text in consts if not text]
    return [
        (h[0] == qchash.VERSION_HASHED, "the rewritten image carries its own version"),
        (refused(out, "not id's"), "a rewritten image is not rewritten twice"),
        (len(texts) == len(set(texts)), "every text is stored once"),
        (
            all(qchash.text_at(strs, o) and qchash.fnv1a(qchash.text_at(strs, o)) == hv for hv, o in index),
            "the index maps each hash to its own text",
        ),
        ([hv for hv, _ in index] == sorted(hv for hv, _ in index), "the index is sorted by hash"),
        (
            bool(consts) and all(oglob[ofs] == qchash.string_value(text) for ofs, text in consts),
            f"all {len(consts)} string constants hold their text's value",
        ),
        (
            all(oglob[ofs] == qchash.STR_EMPTY for ofs in empties),
            f'{len(empties)} "" constants are STR_EMPTY, not null: IF on one branches in id',
        ),
        (any(qchash.text_at(strs, o) == b"player" for _, o in index), 'the constant "player" is in the index'),
        *name_checks(index_at(out, h, 17), fields, "field"),
        *name_checks(index_at(out, h, 19), funcs, "function"),
        (h[19] + h[20] * 8 == len(out), "the function names end the image"),
    ]


def duplicate_name(data: bytes) -> list[Check]:
    # constructed: pak0 names no two fields alike, so the last named field is renamed as the first
    ih = struct.unpack_from("<15i", data)
    fields, _ = names(data)
    named = [k for k, text in enumerate(fields) if text]
    first, last = named[0], named[-1]
    s_first = struct.unpack_from("<HHi", data, ih[6] + first * 8)[2]
    dup = bytearray(data)
    struct.pack_into("<i", dup, ih[6] + last * 8 + 4, s_first)
    out = qchash.rewrite(bytes(dup))
    h = struct.unpack_from(HEADER, out)
    found = dict(index_at(out, h, 17)).get(qchash.fnv1a(fields[first]))
    return [(found == first, f"of two fields named {fields[first]!r}, the first ({first}) is found, not {last}")]


def refusals(data: bytes) -> list[Check]:
    ih = struct.unpack_from("<15i", data)
    istr = data[ih[10] : ih[10] + ih[11]]
    iglob = struct.unpack_from(f"<{ih[13]}i", data, ih[12])
    defs = [struct.unpack_from("<HHi", data, ih[4] + k * 8) for k in range(ih[5])]
    stmts = [struct.unpack_from("<HHHH", data, ih[2] + i * 8) for i in range(ih[3])]
    compared = {b for op, _, b, _ in stmts if qchash.OPS[op] in ("EQ_S", "NE_S")}

    # a string constant retyped as a float keeps its offset: the operand scan must see it
    k = next(k for k, (t, ofs, _) in enumerate(defs) if t & 0x7FFF == 1 and iglob[ofs] and ofs in compared)
    retyped = bytearray(data)
    struct.pack_into("<H", retyped, ih[4] + k * 8, 2)

    # a nonzero float global whose only def is moved away
    covered = [ofs for _, ofs, _ in defs]
    k = next(
        k
        for k, (t, ofs, _) in enumerate(defs)
        if t & 0x7FFF == 2 and iglob[ofs] and covered.count(ofs) == 1 and ofs - 1 not in covered
    )
    uncovered = bytearray(data)
    struct.pack_into("<H", uncovered, ih[4] + k * 8 + 2, 0)

    # a string global one byte into its text would hash a suffix
    ofs = next(
        ofs for t, ofs, _ in defs if t & 0x7FFF == 1 and iglob[ofs] and len(qchash.text_at(istr, iglob[ofs])) > 1
    )
    suffix = bytearray(data)
    struct.pack_into("<i", suffix, ih[12] + ofs * 4, iglob[ofs] + 1)

    # a field named one byte into its text: its name would hash a text no gate saw
    fields, _ = names(data)
    k = next(k for k, text in enumerate(fields) if len(text) > 1)
    midname = bytearray(data)
    struct.pack_into("<i", midname, ih[6] + k * 8 + 4, struct.unpack_from("<i", data, ih[6] + k * 8 + 4)[0] + 1)

    reserved = [
        (
            refused(data, f"hashes to {value:08x}", hasher=lambda t, v=value: v),
            f"a text hashing to {value:08x} is refused",
        )
        for value in (0, qchash.STR_TEMP, qchash.STR_EMPTY, 0x80000000)
    ]
    return [
        (refused(data, "share hash", hasher=lambda t: 7), "two texts with one hash are refused"),
        *reserved,
        (refused(bytes(retyped), "not string-typed"), "a string read through a non-string global is refused"),
        (refused(bytes(uncovered), "no def covers it"), "a nonzero global with no def is refused"),
        (refused(bytes(suffix), "not the start of a string"), "a string global pointing mid-string is refused"),
        (refused(bytes(midname), "names offset"), "a def named mid-string is refused, not a crash"),
    ]


def main() -> None:
    data = Path(sys.argv[1]).read_bytes()
    checks = [*round_trip(data), *duplicate_name(data), *refusals(data)]
    for passed, what in checks:
        print(f"  {'ok  ' if passed else 'FAIL'} {what}")
    failures = sum(not passed for passed, _ in checks)
    print(f"test_qchash: {'FAILED' if failures else 'PASSED'} ({failures} failures)")
    sys.exit(failures != 0)


if __name__ == "__main__":
    main()
