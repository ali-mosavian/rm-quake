"""qcfields.py -- the 195 entity fields: which QuakeC reads or writes, and what an edict costs with ids.

    python3 qcfields.py PAK0.PAK

A field is touched when a statement loads it (LOAD_*) or takes its address to store into it (ADDRESS),
naming it by a field-typed global. System fields, up to end_sys_fields, are the engine's as well. With
strings, entities and functions as 16-bit ids -- a function value is a page and a 4-byte-aligned offset,
4 + 12 bits -- a field costs 2 B for those types, 4 for a float, 12 for a vector.
"""

import struct
import sys
from collections import Counter, defaultdict

import mdlview
from qcpack import parse

EV_STRING, EV_FLOAT, EV_VECTOR, EV_ENTITY, EV_FIELD, EV_FUNCTION = 1, 2, 3, 4, 5, 6
OP_LOAD_F, OP_LOAD_FNC, OP_ADDRESS = 24, 29, 30
WIDE = {EV_STRING: 4, EV_FLOAT: 4, EV_VECTOR: 12, EV_ENTITY: 4, EV_FUNCTION: 4}
NARROW = {EV_STRING: 2, EV_FLOAT: 4, EV_VECTOR: 12, EV_ENTITY: 2, EV_FUNCTION: 2}
TYPE_NAME = {EV_STRING: "string", EV_FLOAT: "float", EV_VECTOR: "vector", EV_ENTITY: "entity",
             EV_FUNCTION: "function"}


def main() -> None:
    p = mdlview.pak_read(*mdlview.read_pak(sys.argv[1]), "progs.dat")
    pr = parse(p)
    h = struct.unpack_from("<16i", p, 0)
    st_ofs, gd_ofs, gd_n, fd_ofs, fd_n, str_ofs, gl_ofs, entityfields = h[2], h[4], h[5], h[6], h[7], h[10], h[12], h[14]

    def cstr(o: int) -> str:
        return p[str_ofs + o:p.index(b"\0", str_ofs + o)].decode("latin-1")

    fdefs = [struct.unpack_from("<HHi", p, fd_ofs + i * 8) for i in range(fd_n)]
    names = {cstr(s): (t & 0x7FFF, ofs) for t, ofs, s in fdefs}
    # end_sys_fields is a void global, not a field: entvars_t (progdefs.q1) ends with .noise3
    end_sys = names["noise3"][1] + 1
    # a vector's _x/_y/_z are float defs at its own words: count the vector once
    fields = {n: (t, ofs) for n, (t, ofs) in names.items()
              if t in WIDE and not (n[-2:] in ("_x", "_y", "_z") and names.get(n[:-2], (0,))[0] == EV_VECTOR)}
    by_word = {}
    for n, (t, ofs) in fields.items():
        for w in range(ofs, ofs + (3 if t == EV_VECTOR else 1)):
            by_word[w] = n
    field_global = {ofs: struct.unpack_from("<i", p, gl_ofs + ofs * 4)[0]
                    for t, ofs, _ in (struct.unpack_from("<HHi", p, gd_ofs + i * 8) for i in range(gd_n))
                    if t & 0x7FFF == EV_FIELD}

    readers: dict[str, set[int]] = defaultdict(set)
    writers: dict[str, set[int]] = defaultdict(set)
    for i, rng in pr.body.items():
        for j in rng:
            op, a, b, c = struct.unpack_from("<HHHH", p, st_ofs + j * 8)
            if OP_LOAD_F <= op <= OP_ADDRESS and b in field_global and field_global[b] in by_word:
                (writers if op == OP_ADDRESS else readers)[by_word[field_global[b]]].add(i)

    words = sum(3 if t == EV_VECTOR else 1 for t, _ in fields.values())
    print(f"entityfields {entityfields} words = {entityfields * 4} B; {len(fields)} fields over {words} words, "
          f"{sum(1 for _, o in fields.values() if o < end_sys)} of them system")
    print("by type:", dict(Counter(TYPE_NAME[t] for t, _ in fields.values())))
    untouched = sorted(n for n in fields if n not in readers and n not in writers)
    sys_untouched = [n for n in untouched if fields[n][1] < end_sys]
    user_untouched = [n for n in untouched if fields[n][1] >= end_sys]
    print(f"no QuakeC statement touches: {len(sys_untouched)} system (the engine's) {sys_untouched}")
    print(f"                             {len(user_untouched)} user {user_untouched}")
    read_only = sorted(n for n in fields if n in readers and n not in writers)
    print(f"read, never written by QuakeC: {len(read_only)} {read_only}")
    live = {n: tv for n, tv in fields.items() if n in readers or n in writers or tv[1] < end_sys}
    wide = sum(WIDE[t] for t, _ in fields.values())
    print(f"edict: {wide} B as is; {sum(NARROW[t] for t, _ in fields.values())} B with 16-bit ids; "
          f"{sum(NARROW[t] for t, _ in live.values())} B with ids, less the user fields nothing touches")
    spread = Counter(min(len(readers[n] | writers[n]), 50) // 10 * 10 for n in fields if fields[n][1] >= end_sys)
    print("user fields by functions touching them:",
          ", ".join(f"{k}-{k + 9 if k < 50 else '+'}: {v}" for k, v in sorted(spread.items())))


if __name__ == "__main__":
    main()
