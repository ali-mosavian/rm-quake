"""mutate.py -- every mutant of qvm.c must fail qvm_test.

    python3 qvm/test/mutate.py BUILD_DIR          (or: make -C qvm/test mutate)

Each mutant puts back one bug the suite was written against, builds natively with $CC (clang)
and runs qvm_test on BUILD_DIR's PROGS.DAT and PROGSH.DAT. One that passes names a bug the suite
no longer sees.
"""

import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
QVM = HERE.parent

MUTANTS = [
    ("contiguous", "            if ( ( off + n ) * 8 > QVM_PAGE ) { page++; off = 0; }\n", ""),
    (
        "ed_no_page",
        "#define ED( m, v ) ( (uint16) ( (v) >> 16 ) ? ed_far",
        "#define ED( m, v ) ( 0 ? ed_far",
    ),
    (
        "no_clamp",
        "( ( (uint16) (v) & 0x3FFF ) > ED_MAX ? 0 : ( (uint16) (v) & 0x3FFF ) )",
        "( (uint16) (v) & 0x3FFF )",
    ),
    (
        "get_no_windows",
        "if ( fld < 0 || fld >= m->edsz / 4 || ed_windows( m ) ) return v;",
        "if ( fld < 0 || fld >= m->edsz / 4 ) return v;",
    ),
    (
        "set_no_bound",
        "if ( fld < 0 || fld >= m->edsz / 4 || ed_windows( m ) ) return;",
        "if ( ed_windows( m ) ) return;",
    ),
    ("id_version", "if ( h[0] != QVM_VERSION ) return QVM_ERR_VERSION;", "if ( 0 ) return QVM_ERR_VERSION;"),
    (
        "accept_previous",
        "if ( h[0] != QVM_VERSION ) return QVM_ERR_VERSION;",
        "if ( h[0] != QVM_VERSION && h[0] != 0x51480006L ) return QVM_ERR_VERSION;",
    ),
    ("eq_s_float", "OPC->f = (float) ( a->i == b->i ||", "OPC->f = (float) ( a->f == b->f ||"),
    (
        "eq_s_no_blank",
        "OPC->f = (float) ( a->i == b->i || ( STR_BLANK( a->i ) && STR_BLANK( b->i ) ) );",
        "OPC->f = (float) ( a->i == b->i );",
    ),
    (
        "ne_s_no_blank",
        "OPC->f = (float) !( a->i == b->i || ( STR_BLANK( a->i ) && STR_BLANK( b->i ) ) );",
        "OPC->f = (float) ( a->i != b->i );",
    ),
    ("not_s_null_only", "(float) STR_BLANK( OPA->i )", "(float) ( !OPA->i )"),
    (
        "blank_null_only",
        "#define STR_BLANK( v ) ( (v) == 0 || (v) == QVM_STR_EMPTY )",
        "#define STR_BLANK( v ) ( (v) == 0 )",
    ),
    ("no_index_read", "    if ( !err ) err = read_aux( m, m->idxbase, h[15], m->nidx * 8 );\n", ""),
    ("index_page", "( m->idxbase >> 14 ) + mid / QVM_IPP", "( m->strbase >> 14 ) + mid / QVM_IPP"),
    ("search_signed", "if ( (uint32) e->hash < (uint32) str )", "if ( e->hash < str )"),
    ("temp_is_hash", "    return QVM_STR_TEMP;\n", "    return qvm_hash( text );\n"),
    ("text_no_temp", "    if ( str == QVM_STR_TEMP ) {", "    if ( 0 ) {"),
    ("text_no_empty", "if ( str == 0 || str == QVM_STR_EMPTY )", "if ( str == 0 )"),
    ("text_cap", "n + 1 < cap && ( c = str_byte", "n < cap && ( c = str_byte"),
    ("hash_signed_char", "h = ( h ^ (uint8) *s++ )", "h = ( h ^ *s++ )"),
    ("hash_empty_null", "    if ( !*s ) return QVM_STR_EMPTY;", "    if ( !*s ) return 0;"),
    ("run_builtin", "    if ( f.first < 0 ) { m->trap = QVM_ERR_BUILTIN; return; }\n", ""),
    ("no_ed_after_builtin", "                if ( ed_windows( m ) ) goto unwind;\n", ""),
    (
        "no_st_after_builtin",
        "                if ( !( sbase = (QStat QFAR *) MAP( m, QVM_W_ST, m->h_st, stpage ) ) ) {",
        "                if ( 0 ) {",
    ),
    ("no_ed_at_run", "    if ( ed_windows( m ) ) return;\n", ""),
    (
        "str_one_page",
        "    pg = MAP( m, QVM_W_AUX, m->h_aux, at >> 14 );",
        "    pg = MAP( m, QVM_W_AUX, m->h_aux, m->strbase >> 14 );",
    ),
    ("no_call_bound", "if ( a->i <= 0 || a->i >= m->nfunc )", "if ( a->i == 0 )"),
    ("no_edsz_refusal", "m->edsz < 4 || m->edsz > QVM_PAGE ||", "m->edsz < 4 ||"),
    (
        "no_return_addr",
        "qvm_enter( m, &f, ( (sint32) stpage << 16 ) | idx );",
        "qvm_enter( m, &f, 0L );",
    ),
    ("jump_no_minus1", "if (  OPA->i ) s += s->b - 1;", "if (  OPA->i ) s += s->b;"),
    ("goto_reads_b", "case OP_GOTO:  s += s->a - 1;", "case OP_GOTO:  s += s->b - 1;"),
    ("mul_fv_no_swap", "case OP_MUL_FV: a = OPB; b = OPA;", "case OP_MUL_FV: a = OPA; b = OPB;"),
    ("no_guard", "        if ( !--runaway ) { m->trap = QVM_ERR_RUNAWAY; goto unwind; }\n", ""),
    ("no_unwind", "    while ( m->depth > exitdepth )\n        qvm_leave( m );\n", "    ;\n"),
    ("no_clear", "    m->trap = 0;\n", ""),
]


def caught(exe: Path, build: Path) -> tuple[bool, str]:
    images = [str(build / "PROGS.DAT"), str(build / "PROGSH.DAT")]
    try:
        r = subprocess.run([str(exe), *images], capture_output=True, text=True, timeout=20, stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired:
        return True, "timeout"
    fails = [ln.strip() for ln in r.stdout.splitlines() if ln.lstrip().startswith("FAIL")]
    return r.returncode != 0, (fails or [f"exit {r.returncode}"])[0]


def main() -> None:
    build = Path(sys.argv[1]).resolve()
    src = (QVM / "qvm.c").read_text()
    cc = os.environ.get("CC", "clang")
    tests = [str(HERE / f) for f in ("qvm_test.c", "qimg.c", "ems_nat.c")]
    survived = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, old, new in MUTANTS:
            if src.count(old) != 1:
                sys.exit(f"mutant {name}: its pattern matches {src.count(old)} times, not once")
            mutant, exe = Path(tmp) / f"{name}.c", Path(tmp) / name
            mutant.write_text(src.replace(old, new))
            cmd = [cc, "-std=c89", "-w", f"-I{QVM}", f"-I{HERE}", "-o", str(exe), *tests, str(mutant)]
            subprocess.run(cmd, check=True, stdin=subprocess.DEVNULL)
            ok, why = caught(exe, build)
            survived += not ok
            print(f"  {'caught  ' if ok else 'SURVIVED'} {name}: {why}")
    print(f"mutate: {len(MUTANTS) - survived} of {len(MUTANTS)} caught")
    sys.exit(survived != 0)


if __name__ == "__main__":
    main()
