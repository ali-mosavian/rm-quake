"""Every mutant of edict.c must make ed_load_test fail.

    python3 mut_edict.py BUILD_DIR
"""

import concurrent.futures
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
QVM = os.path.dirname(HERE)
SOURCES = ["edict.c", "edict.h", "ed.c", "ed.h", "qvm.c", "qvm.h", "qvmi.h", "qstr.c", "qtypes.h"]
TEST_SRC = ["ed_load_test.c", "bspents.c", "ems_nat.c"]
MAPS = ["e1m3", "e1m4", "start"]

# name: (pattern, replacement), each pattern met exactly once in edict.c
MUTANTS = {
    "nullfn_freed": ("        if ( classname == QVM_STR_EMPTY ) return ED_ERR_NULLFN;\n", ""),
    "empty_fn_error": ("if ( !( fn = qvm_func( m, s ) ) && s[0] )", "if ( !( fn = qvm_func( m, s ) ) )"),
    "entity_unchecked": ("        if ( num < 0 || num >= w->max ) return ED_ERR_ENTITY;\n", ""),
    "skill_easy": ("( w->skill == 0 && ( flags & 256 ) )", "( w->skill == 0 && ( flags & 0 ) )"),
    "skill_medium": ("( w->skill == 1 && ( flags & 512 ) )", "( w->skill == 1 && ( flags & 0 ) )"),
    "skill_nightmare": ("( w->skill >= 2 && ( flags & 1024 ) )", "( w->skill == 2 && ( flags & 1024 ) )"),
    "trim_first": (
        '        anglehack = !strcmp( tok, "angle" );',
        '        for ( len = (sint16) strlen( tok ); len && tok[len - 1] == \' \'; ) tok[--len] = 0;\n'
        '        anglehack = !strcmp( tok, "angle" );',
    ),
    "angle_overrun": ("if ( strlen( tok ) >= ANGLE_MAX )", "if ( strlen( tok ) > ANGLE_MAX )"),
    "init_after_skip": (
        "        init = 1;\n        if ( keyname[0] == '_' ) continue;",
        "        if ( keyname[0] == '_' ) continue;\n        init = 1;",
    ),
    "no_anglehack": ('if ( anglehack ) strcpy( tok, "angles" );', 'if ( 0 ) strcpy( tok, "angles" );'),
    "no_light_hack": ('        if ( !strcmp( tok, "light" ) ) strcpy( tok, "light_lev" );\n', ""),
    "missing_field_fatal": (
        "if ( qvm_field_def( m, keyname, &key ) ) continue;",
        "if ( qvm_field_def( m, keyname, &key ) ) return ED_ERR_VALUE;",
    ),
    "world_cleared": ("if ( n != 0 && qvm_ent_zero( m, e ) )", "if ( qvm_ent_zero( m, e ) )"),
    "unspawnable_kept": (
        "if ( !( fn = qvm_func_str( m, classname ) ) ) { ed_free( w, n ); continue; }",
        "if ( !( fn = qvm_func_str( m, classname ) ) ) continue;",
    ),
    "no_unescape": ("        ed_unescape( s, s, ED_TOKEN );", "       "),
    "reuse_no_relax": ("w->freetime[i] < 2.0f || ", ""),
    "free_keeps_origin": ("qvm_ent_set( m, e, (sint16) ( w->f_origin + i ), fval( 0.0f ) );", ";"),
    "free_nextthink_0": ("fval( -1.0f )", "fval( 0.0f )"),
    "no_time": ("    m->glob[QG_TIME].f = (float) w->time;\n", ""),
    "self_unset": ("        m->glob[QG_SELF].i = e;\n", ""),
}


def run(build: str, name: str, pattern: str, replacement: str) -> tuple[str, str]:
    with tempfile.TemporaryDirectory() as work:
        for f in SOURCES:
            shutil.copy(os.path.join(QVM, f), work)
        path = os.path.join(work, "edict.c")
        text = open(path).read()
        if text.count(pattern) != 1:
            return name, f"pattern met {text.count(pattern)} times"
        open(path, "w").write(text.replace(pattern, replacement))
        exe = os.path.join(work, "ed_load_test")
        cc = ["clang", "-std=c89", "-Wall", "-Werror", f"-I{work}", f"-I{HERE}", "-o", exe]
        cc += [os.path.join(HERE, f) for f in TEST_SRC]
        cc += [os.path.join(work, f) for f in SOURCES if f.endswith(".c")]
        if subprocess.run(cc, capture_output=True).returncode:
            return name, "does not build"
        args = [exe, os.path.join(build, "PROGSH.DAT")]
        for m in MAPS:
            args += [os.path.join(build, "maps", f"{m}.bsp"), os.path.join(HERE, "fixtures", f"{m}.dump")]
        r = subprocess.run(args, capture_output=True, text=True, cwd=HERE, timeout=120)
        if r.returncode == 0:
            return name, "SURVIVED"
        fail = next((line.strip() for line in r.stdout.splitlines() if "FAIL" in line), f"exit {r.returncode}")
        return name, f"caught: {fail}"


def main() -> None:
    build = sys.argv[1]
    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        results = list(pool.map(lambda kv: run(build, kv[0], *kv[1]), MUTANTS.items()))
    for name, verdict in results:
        print(f"  {name}: {verdict}")
    caught = sum(v.startswith("caught") for _, v in results)
    print(f"mut_edict: {caught} of {len(results)} caught")
    sys.exit(caught != len(results))


if __name__ == "__main__":
    main()
