"""mutate.py -- every mutant of qvm.c must fail qvm_test.

    python3 qvm/test/mutate.py BUILD_DIR          (or: make -C qvm/test mutate)

Each mutant of qvm.c, or of ems_nat.c for HOST_MUTANTS, puts back one bug the suite was written against, builds natively with $CC (clang)
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
    ("key_page_only", "(m)->host->key[w] == WKEY( h, pg )", "( (m)->host->key[w] >> 8 ) == (uint16) (pg)"),
    ("key_never_hits", "(m)->host->key[w] == WKEY( h, pg ) ?", "0 ?"),
    ("key_no_page", "( (uint16) (pg) << 8 ) | ", ""),
    ("edpages_unbounded", "    if ( edpages > QVM_MAXPAGES ) return QVM_ERR_SIZE;\n", ""),
    ("keeps_big_handle", "{ m->host->release( m->h_ed ); m->h_ed = 0; return QVM_ERR_EMS; }", "return QVM_ERR_EMS;"),
]

# the host the VM trusts: its record of each window, as gem.asm keeps qgl$gem_key
HOST_MUTANTS = [
    ("free_keeps_key", "{ key_hnd[i] = 0; ems_key[i] = 0xFFFF; }", "key_hnd[i] = 0;", "edicts made again read zero"),
    (
        "free_keeps_ed0_key",
        "{ key_hnd[i] = 0; ems_key[i] = 0xFFFF; }",
        "{ key_hnd[i] = 0; if ( i != 2 ) ems_key[i] = 0xFFFF; }",
        "edicts made again read zero",
    ),
]

# a mutant naming a section counts as caught only when something in that section fails


def caught(exe: Path, build: Path, expect: str | None) -> tuple[bool, str]:
    images = [str(build / "PROGS.DAT"), str(build / "PROGSH.DAT")]
    try:
        r = subprocess.run([str(exe), *images], capture_output=True, text=True, timeout=20, stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired:
        return True, "timeout"
    fails = [ln.strip() for ln in r.stdout.splitlines() if ln.lstrip().startswith("FAIL")]
    if expect is not None:
        section, named = "", []
        for ln in r.stdout.splitlines():
            if ln.startswith("-- "):
                section = ln[3:]
            elif ln.lstrip().startswith("FAIL") and section == expect:
                named.append(ln.strip())
        return bool(named), (named or [f"nothing in {expect!r}: {(fails or ['no FAIL'])[0]}"])[0]
    return r.returncode != 0, (fails or [f"exit {r.returncode}"])[0]


def main() -> None:
    build = Path(sys.argv[1]).resolve()
    cc = os.environ.get("CC", "clang")
    sources = {"qvm.c": QVM / "qvm.c", "ems_nat.c": HERE / "ems_nat.c"}
    fixed = [str(HERE / f) for f in ("qvm_test.c", "qimg.c")]
    runs = [("qvm.c", *m, None) for m in MUTANTS] + [("ems_nat.c", *m) for m in HOST_MUTANTS]
    survived = 0
    with tempfile.TemporaryDirectory() as tmp:
        for target, name, old, new, expect in runs:
            src = sources[target].read_text()
            if src.count(old) != 1:
                sys.exit(f"mutant {name}: its pattern matches {src.count(old)} times in {target}, not once")
            mutant, exe = Path(tmp) / f"{name}.c", Path(tmp) / name
            mutant.write_text(src.replace(old, new))
            others = [str(v) for k, v in sources.items() if k != target]
            cmd = [cc, "-std=c89", "-w", f"-I{QVM}", f"-I{HERE}", "-o", str(exe), *fixed, *others, str(mutant)]
            subprocess.run(cmd, check=True, stdin=subprocess.DEVNULL)
            ok, why = caught(exe, build, expect)
            survived += not ok
            print(f"  {'caught  ' if ok else 'SURVIVED'} {name}: {why}")
    print(f"mutate: {len(runs) - survived} of {len(runs)} caught")
    sys.exit(survived != 0)


if __name__ == "__main__":
    main()
