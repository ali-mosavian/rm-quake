"""mutate.py -- every mutant of the VM must fail qvm_test.

    python3 qvm/test/mutate.py BUILD_DIR          (or: make -C qvm/test mutate)

Each mutant of qvm.c, qstr.c or qvmi.h, or of ems_nat.c for HOST_MUTANTS, puts back one bug the
suite was written against, builds natively with $CC (clang) from a copy of the sources, and runs
qvm_test on BUILD_DIR's PROGS.DAT and PROGSH.DAT. One that passes names a bug the suite no longer
sees.
"""

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
QVM = HERE.parent

MUTANTS = {
    "qvm.c": [
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
        (
            "accept_nameless",
            "if ( h[0] != QVM_VERSION ) return QVM_ERR_VERSION;",
            "if ( h[0] != QVM_VERSION && h[0] != 0x51480007L ) return QVM_ERR_VERSION;",
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
        ("no_field_names", "    if ( !err ) err = read_aux( m, m->fldbase, h[17], m->nfldname * 8 );\n", ""),
        ("func_names_at_fields", "    m->fnbase = m->fldbase + m->nfldname * 8;", "    m->fnbase = m->fldbase;"),
        ("field_no_text", "return qvm_str_is( m, d->s_name, name ) ? 0 : -1;", "return 0;"),
        ("zero_one_word", "_fmemset( ED( m, ent ), 0, (uint16) m->edsz );", "_fmemset( ED( m, ent ), 0, 4 );"),
        ("func_no_text", "return qvm_str_is( m, s_name, name ) ? (sint16) k : 0;", "return (sint16) k;"),
        ("run_builtin", "    if ( f.first < 0 ) { m->trap = QVM_ERR_BUILTIN; return; }\n", ""),
        ("no_ed_after_builtin", "                if ( ed_windows( m ) ) goto unwind;\n", ""),
        (
            "no_st_after_builtin",
            "                if ( !( sbase = (QStat QFAR *) MAP( m, QVM_W_ST, m->h_st, stpage ) ) ) {",
            "                if ( 0 ) {",
        ),
        ("no_ed_at_run", "    if ( ed_windows( m ) ) return;\n", ""),
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
        ("edpages_unbounded", "    if ( edpages > QVM_MAXPAGES ) return QVM_ERR_SIZE;\n", ""),
        (
            "keeps_big_handle",
            "{ m->host->release( m->h_ed ); m->h_ed = 0; return QVM_ERR_EMS; }",
            "return QVM_ERR_EMS;",
        ),
    ],
    "qstr.c": [
        (
            "index_page",
            "qvm_idx_find( m, m->h_aux, m->idxbase, m->nidx, str )",
            "qvm_idx_find( m, m->h_aux, m->strbase, m->nidx, str )",
        ),
        ("search_signed", "if ( (uint32) e->hash < (uint32) key )", "if ( e->hash < key )"),
        (
            "search_one_page",
            "MAP( m, QVM_W_AUX, hnd, at >> 14 ) ) ) return 0;",
            "MAP( m, QVM_W_AUX, hnd, base >> 14 ) ) ) return 0;",
        ),
        (
            "insert_signed",
            "if ( (uint32) ( (QStrIdx QFAR *) ( pg + mid * 8 ) )->hash < (uint32) h )",
            "if ( ( (QStrIdx QFAR *) ( pg + mid * 8 ) )->hash < h )",
        ),
        ("add_skips_progs", "    if ( ( e = qvm_idx_find( m, m->h_aux, m->idxbase, m->nidx, h ) ) != 0 ) {", "    if ( 0 ) {"),
        ("add_progs_no_text", "return text_is( m, m->h_aux, m->strbase, m->nstr, ofs, text ) ? h : 0;", "return h;"),
        ("add_map_no_text", "return text_is( m, m->h_mstr, 0, QVM_PAGE, e->ofs, text ) ? h : 0;", "return h;"),
        ("add_no_room", "    if ( (sint32) ( m->nmstr + 1 ) * 8 + len > m->mstr_low ) return 0;\n", ""),
        ("add_no_reserved", "    if ( reserved( h ) ) return 0;\n", ""),
        ("add_no_move", "    _fmemmove( e + 1, e, (uint16) ( m->nmstr - lo ) * 8 );\n", ""),
        ("text_no_map", "    if ( ( e = qvm_idx_find( m, m->h_mstr, 0, m->nmstr, str ) ) != 0 )\n", "    if ( 0 )\n"),
        ("temp_is_hash", "    return QVM_STR_TEMP;\n", "    return qvm_hash( text );\n"),
        ("text_no_temp", "    if ( str == QVM_STR_TEMP ) {", "    if ( 0 ) {"),
        ("text_no_empty", "if ( str == 0 || str == QVM_STR_EMPTY )", "if ( str == 0 )"),
        ("text_cap", "n + 1 < cap && ( c = text_byte", "n < cap && ( c = text_byte"),
        ("hash_signed_char", "h = ( h ^ (uint8) *s++ )", "h = ( h ^ *s++ )"),
        ("hash_empty_null", "    if ( !*s ) return QVM_STR_EMPTY;", "    if ( !*s ) return 0;"),
        (
            "str_one_page",
            "    pg = MAP( m, QVM_W_AUX, hnd, at >> 14 );",
            "    pg = MAP( m, QVM_W_AUX, hnd, base >> 14 );",
        ),
    ],
    "qvmi.h": [
        ("key_page_only", "(m)->host->key[w] == WKEY( h, pg )", "( (m)->host->key[w] >> 8 ) == (uint16) (pg)"),
        ("key_never_hits", "(m)->host->key[w] == WKEY( h, pg ) ?", "0 ?"),
        ("key_no_page", "( (uint16) (pg) << 8 ) | ", ""),
    ],
}

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

# every file a mutant can land in, copied whole so a mutated header is the one included
SOURCES = {
    "qvm.c": QVM / "qvm.c",
    "qstr.c": QVM / "qstr.c",
    "qvmi.h": QVM / "qvmi.h",
    "qvm.h": QVM / "qvm.h",
    "qtypes.h": QVM / "qtypes.h",
    "ems_nat.c": HERE / "ems_nat.c",
}

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


def build_mutant(work: Path, target: str, old: str, new: str) -> Path:
    for name, path in SOURCES.items():
        shutil.copy(path, work / name)
    src = (work / target).read_text()
    if src.count(old) != 1:
        sys.exit(f"mutant in {target}: its pattern matches {src.count(old)} times, not once:\n{old}")
    (work / target).write_text(src.replace(old, new))
    exe = work / "qvm_test"
    units = [str(work / name) for name in SOURCES if name.endswith(".c")]
    tests = [str(HERE / f) for f in ("qvm_test.c", "qimg.c")]
    cmd = [os.environ.get("CC", "clang"), "-std=c89", "-w", f"-I{work}", f"-I{HERE}", "-o", str(exe), *tests, *units]
    subprocess.run(cmd, check=True, stdin=subprocess.DEVNULL)
    return exe


def main() -> None:
    build = Path(sys.argv[1]).resolve()
    runs = [(target, *m, None) for target, group in MUTANTS.items() for m in group]
    runs += [("ems_nat.c", *m) for m in HOST_MUTANTS]
    survived = 0
    with tempfile.TemporaryDirectory() as tmp:
        for target, name, old, new, expect in runs:
            work = Path(tmp) / name
            work.mkdir()
            ok, why = caught(build_mutant(work, target, old, new), build, expect)
            survived += not ok
            print(f"  {'caught  ' if ok else 'SURVIVED'} {name}: {why}")
    print(f"mutate: {len(runs) - survived} of {len(runs)} caught")
    sys.exit(survived != 0)


if __name__ == "__main__":
    main()
