"""mut_qchash.py -- every mutant of qchash.py must fail test_qchash.py.

    python3 tools/qcvm/mut_qchash.py PROGS.DAT

Each mutant takes out one gate or one step of the rewrite. One that passes names a refusal or a
round-trip property the test no longer sees.
"""

import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent

MUTANTS = [
    ("no_operand_scan", "    check_operands(img, glob_in, string_globals)\n", ""),
    ("no_complete", "    check_complete(glob_in, defs, start_set)\n", ""),
    ("no_def_cover", "        if v and g not in covered:", "        if False:"),
    ("no_start", "        if t == EV_STRING and glob[g] and glob[g] not in starts:", "        if False:"),
    ("vector_covers_one", "for k in (1, 2)}", "for k in (1,)}"),
    ("no_collision", "        if h in by_hash:", "        if False:"),
    ("no_reserved", "        if h in RESERVED:", "        if False:"),
    ("no_high_bit", '    0x80000000: "-0.0 to AND/OR",\n', ""),
    ("empty_is_null", "    return hasher(text) if text else STR_EMPTY", "    return hasher(text) if text else 0"),
    (
        "no_global_hash",
        '            struct.pack_into("<I", glob, g * 4,',
        '            (lambda *a: None)("<I", glob, g * 4,',
    ),
    ("no_dedupe", "        if t not in new_ofs:", "        if True:"),
    ("unsorted_index", "    index = sorted(", "    index = list("),
    ("id_version", "    header = [VERSION_HASHED,", "    header = [VERSION_ID,"),
    ("names_mid_string", "        if o not in start_set:", "        if False:"),
    ("names_keep_last", "        if name and hasher(name) not in first:", "        if name:"),
    ("names_unsorted", "    return sorted(first.items())", "    return list(first.items())"),
    ("names_no_funcs", "        pack_index(func_index),\n", "        b'',\n"),
]


def caught(work: Path, image: Path) -> tuple[bool, str]:
    cmd = [sys.executable, "test_qchash.py", str(image)]
    r = subprocess.run(cmd, cwd=work, capture_output=True, text=True, stdin=subprocess.DEVNULL)
    fails = [ln.strip() for ln in r.stdout.splitlines() if "FAIL " in ln] or r.stderr.strip().splitlines()[-1:]
    return r.returncode != 0, "; ".join(fails[:2])


def main() -> None:
    image = Path(sys.argv[1]).resolve()
    src = (HERE / "qchash.py").read_text()
    survived = 0
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        (work / "test_qchash.py").write_text((HERE / "test_qchash.py").read_text())
        for name, old, new in MUTANTS:
            if src.count(old) != 1:
                sys.exit(f"mutant {name}: its pattern matches {src.count(old)} times, not once")
            (work / "qchash.py").write_text(src.replace(old, new))
            ok, why = caught(work, image)
            survived += not ok
            print(f"  {'caught  ' if ok else 'SURVIVED'} {name}: {why}")
    print(f"mut_qchash: {len(MUTANTS) - survived} of {len(MUTANTS)} caught")
    sys.exit(survived != 0)


if __name__ == "__main__":
    main()
