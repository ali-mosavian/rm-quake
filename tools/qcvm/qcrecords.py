"""qcrecords.py -- the ranges a 4-byte inline function header would have to hold.

    python3 qcrecords.py PAK0.PAK

A QuakeC function's 36-byte record keeps first_statement, parm_start, locals, profile, s_name, s_file,
numparms and eight parm sizes. The VM needs only parm_start, locals, numparms and the sizes; a builtin
needs only its number.
"""

import struct
import sys

import mdlview


def main() -> None:
    p = mdlview.pak_read(*mdlview.read_pak(sys.argv[1]), "progs.dat")
    h = struct.unpack_from("<16i", p, 0)
    fn_ofs, fn_n = h[8], h[9]
    fns = [struct.unpack_from("<7i8B", p, fn_ofs + i * 36) for i in range(fn_n)]
    qc = [f for f in fns if f[0] > 0]
    builtins = [-f[0] for f in fns if f[0] < 0]
    sizes = {s for f in qc for s in f[7:7 + f[6]]}
    print(f"records {fn_n} x 36 = {fn_n * 36} B; QuakeC {len(qc)}, builtins {len(builtins)}")
    print(f"parm_start max {max(f[1] for f in qc)} ({max(f[1] for f in qc).bit_length()} bits)")
    print(f"locals max {max(f[2] for f in qc)} words ({max(f[2] for f in qc).bit_length()} bits)")
    print(f"numparms max {max(f[6] for f in qc)}; parm sizes used {sorted(sizes)}")
    print(f"builtin numbers {min(builtins)}..{max(builtins)}")


if __name__ == "__main__":
    main()
