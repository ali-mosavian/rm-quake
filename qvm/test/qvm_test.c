/* qvm_test.c -- hand-assembled bytecode, loaded through the paged loader.
 *
 * Each program is written as statements directly rather than compiled from
 * .qc, because the point is to pin the INTERPRETER's contract: operand
 * addressing, the locals window save/restore, entity byte-offsets vs word
 * field offsets, the exact bit-test semantics of IF, and what paging owes:
 * functions whole on a page, and every window mapped back after anything
 * else may have taken it.
 *
 * The same file runs natively (ems_nat.c) and under bcc in DOS (ems_dos.c):
 *   make -C qvm/test            native
 *   qvm/test/dosconf.sh         DOS: prints a conf to launch
 *   qvm_test [ID.DAT [HASHED.DAT ...]]
 * ID.DAT is id's progs.dat, which must be refused; the rest are qchash.py's.
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "qvm.h"
#include "qimg.h"
#include "ems.h"

#ifdef __TURBOC__
#include <dos.h>
#include <io.h>
#include <alloc.h>
#else
#include <signal.h>
#include <unistd.h>
#define farmalloc malloc
#define _fmemset memset
#define _fmemcmp memcmp
#define _fmemcpy memcpy
#endif

enum { OP_DONE, OP_MUL_F, OP_MUL_V, OP_MUL_FV, OP_MUL_VF, OP_DIV_F,
       OP_ADD_F, OP_ADD_V, OP_SUB_F, OP_SUB_V,
       OP_EQ_F, OP_EQ_V, OP_EQ_S, OP_EQ_E, OP_EQ_FNC,
       OP_NE_F, OP_NE_V, OP_NE_S, OP_NE_E, OP_NE_FNC,
       OP_LE, OP_GE, OP_LT, OP_GT,
       OP_LOAD_F, OP_LOAD_V, OP_LOAD_S, OP_LOAD_ENT, OP_LOAD_FLD, OP_LOAD_FNC,
       OP_ADDRESS,
       OP_STORE_F, OP_STORE_V, OP_STORE_S, OP_STORE_ENT, OP_STORE_FLD, OP_STORE_FNC,
       OP_STOREP_F, OP_STOREP_V, OP_STOREP_S, OP_STOREP_ENT, OP_STOREP_FLD, OP_STOREP_FNC,
       OP_RETURN, OP_NOT_F, OP_NOT_V, OP_NOT_S, OP_NOT_ENT, OP_NOT_FNC,
       OP_IF, OP_IFNOT,
       OP_CALL0, OP_CALL1, OP_CALL2, OP_CALL3, OP_CALL4,
       OP_CALL5, OP_CALL6, OP_CALL7, OP_CALL8,
       OP_STATE, OP_GOTO, OP_AND, OP_OR, OP_BITAND, OP_BITOR };

/* qchash.py's values for these texts, which qvm_hash must agree with */
#define HASH_PLAYER   ( (sint32) 0x2C99C300UL )
#define HASH_5        ( (sint32) 0x300CA0D0UL )
#define HASH_ABCDEFGH ( (sint32) 0x76DAAA8DUL )
#define HASH_KNIGHT   ( (sint32) 0xDFBBF53CUL )   /* past 0x7FFFFFFF: the index is sorted unsigned */
#define HASH_HIGHBYTE ( (sint32) 0x17BEC89BUL )   /* "\x8d" "ab": a char past 0x7F is negative to bcc */

static int fails = 0;

static void say( const char *fmt, ... )
{
    char buf[200];
    va_list ap;

    va_start( ap, fmt );
    vsprintf( buf, fmt, ap );
    va_end( ap );
    fputs( buf, stdout );
#ifdef __TURBOC__
    {
        char *c;
        for ( c = buf; *c; c++ ) outportb( 0xE9, *c );
    }
#endif
}

static void okf( const char *what, float got, float want )
{
    int bad = ( got != want );
    say( "  %-5s %-48s %g", bad ? "FAIL" : "ok", what, got );
    if ( bad ) { say( "  (want %g)\n", want ); fails++; }
    else say( "\n" );
}

#ifndef __TURBOC__
static void on_alarm( int sig )
{
    (void) sig;
    fflush( stdout );              /* _exit drops a pipe's buffer */
    printf( "  FAIL  a GOTO-0 loop ran 2 s without trapping\n\nFAILED\n" );
    fflush( stdout );
    _exit( 1 );
}
#endif

/* ---- the host: the test EMS, window number as slot ---- */
static long h_calls;               /* the VM's map calls: a window already on its page makes none */
static uint8 QFAR *h_map( sint16 win, sint16 hnd, uint16 page ) { h_calls++; return ems_map( hnd, page, win ); }
static sint16 alloc_as;             /* nonzero: the next allocation answers this handle */
static sint16 h_alloc( sint32 bytes )
{
    sint16 h = ems_alloc( bytes );

    if ( alloc_as && h ) { ems_free( h ); h = alloc_as; alloc_as = 0; }
    return h;
}
static void h_release( sint16 hnd ) { if ( hnd != 255 ) ems_free( hnd ); }
static const QHost host = { h_map, h_alloc, h_release, qimg_read, ems_key };

/* ---- a tiny machine: 256 globals, 64 edicts of 191 words ---- */
#define NST    2200
#define NSTR   16500
#define EDW    191
#define EDSZ   ( EDW * 4 )
#define EDPP   ( 16384 / EDSZ )     /* 21 */
#define ENT( n ) ( ( (sint32) ( (n) / EDPP ) << 16 ) | (sint32) ( (n) % EDPP * EDSZ ) )

static QStat QFAR st[NST];
static char  QFAR strs[NSTR];
static QFunc  fn[8];
static QVal   gl[256];
static QStrIdx idx[4];
static QBuiltin bi[6];
static Qvm    m;
static uint8 QFAR *image;
static sint32 nst, nstr, nidx;
static sint16 loaded;

static sint16 bi_called, bi_argc, nested_fn;
static void bi_double( Qvm *v )      /* returns parm0 * 2 */
{
    bi_called++; bi_argc = v->argc;
    v->glob[QOFS_RETURN].f = v->glob[QOFS_PARM0].f * 2.0f;
}

/* takes every window for pages the running QuakeC is not on */
static void bi_clobber( Qvm *v )
{
    ems_map( v->h_aux, 0, QVM_W_ST );
    ems_map( v->h_ed, 1, QVM_W_ED0 );
    ems_map( v->h_ed, 0, QVM_W_EDX );
    ems_map( v->h_st, 0, QVM_W_AUX );
}

static void bi_nested( Qvm *v ) { qvm_run( v, nested_fn ); }

/* ftos: parm0 as "%g", through the temp buffer */
static void bi_ftos( Qvm *v )
{
    char t[24];

    sprintf( t, "%g", v->glob[QOFS_PARM0].f );
    v->glob[QOFS_RETURN].i = qvm_str_temp( v, t );
}

/* prints global 95: maps string and index pages over the function records */
static char bi_text_buf[16];
static void bi_text( Qvm *v ) { qvm_str_text( v, v->glob[95].i, bi_text_buf, sizeof bi_text_buf ); }

static void reset( void )
{
    sint16 i;

    if ( loaded ) qvm_free( &m );
    loaded = 0;
    memset( &m, 0, sizeof m );
    memset( gl, 0, sizeof gl );
    _fmemset( st, 0, sizeof( QStat ) * NST );
    _fmemset( strs, 0, NSTR );
    for ( i = 0; i < 8; i++ ) memset( &fn[i], 0, sizeof fn[i] );
    for ( i = 2; i < 8; i++ ) fn[i].first = -1;   /* unused: a builtin, never called */
    fn[1].first = 0;                 /* every program starts at statement 0 */
    nst = 128;
    nstr = 64;
    nidx = 0;
    memset( idx, 0, sizeof idx );
    bi[1] = bi_double;
    bi[2] = bi_clobber;
    bi[3] = bi_nested;
    bi[4] = bi_ftos;
    bi[5] = bi_text;
}

/* the machine loaded from the arrays as they stand, with 64 edicts */
static sint16 load( void )
{
    QImgSrc src;
    sint32 len;
    sint16 err;

    if ( loaded ) qvm_free( &m );
    src.st = st;   src.nst = nst;
    src.fn = fn;   src.nfn = 8;
    src.gl = gl;   src.ngl = 256;
    src.str = strs; src.nstr = nstr;
    src.idx = idx; src.nidx = nidx;
    src.edwords = EDW;
    src.version = 0;
    len = qimg_build( &src, image, 65520L );
    qimg_use( image, len );
    err = len ? qvm_load( &m, &host, gl, sizeof gl ) : QVM_ERR_SIZE;
    if ( !err ) err = qvm_edicts( &m, 64 );
    loaded = !err;
    m.bi = bi;
    m.nbi = 6;
    if ( err ) { say( "  FAIL  load: error %d\n", err ); fails++; }
    return err;
}

static void run( sint16 f )
{
    if ( !load() ) qvm_run( &m, f );
}

static float ed_f( sint32 ent, sint16 fld ) { return qvm_ent_get( &m, ent, fld ).f; }

/* a crash names no case: say which one runs, and flush, before it does */
static void section( const char *name ) { say( "-- %s\n", name ); fflush( stdout ); }

/* operand fields hold UNSIGNED indices in 16 bits, so narrowing is the point */
#define S(n,o,A,B,C) do{ st[n].op=(uint16)(o); st[n].a=(sint16)(uint16)(A); \
                         st[n].b=(sint16)(uint16)(B); st[n].c=(sint16)(uint16)(C); }while(0)

/* ---- a real progs.dat, read through the host from a file ---- */
#ifdef __TURBOC__
static int img_fd;
static sint16 file_read( void QFAR *dst, sint32 ofs, uint16 n )
{
    unsigned got;
    if ( lseek( img_fd, ofs, SEEK_SET ) != ofs ) return 1;
    return _dos_read( img_fd, dst, n, &got ) || got != n;
}
static sint16 file_open( const char *path ) { return _dos_open( path, 0, &img_fd ) == 0; }
static void file_close( void ) { _dos_close( img_fd ); }
#else
static FILE *img_fp;
static sint16 file_read( void QFAR *dst, sint32 ofs, uint16 n )
{
    if ( fseek( img_fp, ofs, SEEK_SET ) ) return 1;
    return fread( dst, 1, n, img_fp ) != n;
}
static sint16 file_open( const char *path ) { return ( img_fp = fopen( path, "rb" ) ) != 0; }
static void file_close( void ) { fclose( img_fp ); }
#endif

static const QHost file_host = { h_map, h_alloc, h_release, file_read, ems_key };

static void check_image( const char *path, sint16 want_frame )
{
    static QVal QFAR *glob;
    sint32 h[15], k, first;
    QFunc QFAR *f;
    QStat file_st, QFAR *pst;
    Qvm r;
    sint16 fnum, ok;

    if ( !glob ) glob = (QVal QFAR *) farmalloc( 65520L );
    if ( !file_open( path ) ) { say( "  skip  no image at %s\n", path ); return; }
    file_read( (void QFAR *) h, 0, sizeof h );
    okf( "loads a real progs.dat, paged", (float) qvm_load( &r, &file_host, glob, 65520L ), 0.0f );
    say( "        %s: %ld statements on %u pages, %ld functions\n", path, r.nstat, r.st_pages, r.nfunc );
    okf( "  frame field", (float) r.f_frame, (float) want_frame );
    okf( "  edict size",  (float) r.edsz, (float) ( h[14] * 4 ) );
    fnum = qvm_func( &r, "ai_run" );
    okf( "  finds ai_run",    (float) ( fnum != 0 ), 1.0f );
    okf( "  no such function", (float) qvm_func( &r, "nope_not_here" ), 0.0f );
    {
        char buf[16];
        okf( "  \"player\" prints through the index",
             (float) ( qvm_str_text( &r, HASH_PLAYER, buf, sizeof buf ) == 6 && !strcmp( buf, "player" ) ), 1.0f );
    }

    /* every function's first two statements read back from its placement */
    ok = 1;
    for ( k = 1; k < r.nfunc && ok; k++ ) {
        file_read( (void QFAR *) &first, h[8] + k * 36, 4 );
        if ( first <= 0 ) continue;
        f = (QFunc QFAR *) ( ems_map( r.h_aux, (uint16) ( k / QVM_FPP ), QVM_W_AUX ) + (uint16) ( k % QVM_FPP ) * 36 );
        pst = (QStat QFAR *) ems_map( r.h_st, (uint16) ( f->first >> 16 ), QVM_W_ST ) + (uint16) f->first;
        file_read( (void QFAR *) &file_st, h[2] + first * 8, 8 );
        ok = !_fmemcmp( pst, &file_st, 8 );
        if ( !ok ) say( "        function %ld: placed %08lx\n", k, f->first );
    }
    okf( "  every function's first statement is where it was placed", (float) ok, 1.0f );
    qvm_free( &r );
    file_close();
}

/* id's progs.dat holds offsets where the VM wants hashes */
static void check_refused( const char *path )
{
    static QVal QFAR *glob;
    static Qvm r;

    if ( !glob ) glob = (QVal QFAR *) farmalloc( 65520L );
    if ( !file_open( path ) ) { say( "  skip  no image at %s\n", path ); return; }
    okf( "id's own progs.dat is refused", (float) qvm_load( &r, &file_host, glob, 65520L ), (float) QVM_ERR_VERSION );
    file_close();
}

int main( int argc, char **argv )
{
    int i;

    if ( !ems_init() ) { say( "no EMS\nFAILED\n" ); return 1; }
    image = (uint8 QFAR *) farmalloc( 65520L );

    section( "arithmetic and the three-address form" );
    /* ---- arithmetic and the three-address form ---- */
    reset();
    gl[30].f = 3.0f; gl[31].f = 4.0f;
    S(0, OP_MUL_F, 30,31,32);
    S(1, OP_ADD_F, 32,30,33);
    S(2, OP_SUB_F, 33,31,34);
    S(3, OP_DIV_F, 34,30,35);
    S(4, OP_DONE, 0,0,0);
    run( 1 );
    okf( "3*4",            gl[32].f, 12.0f );
    okf( "12+3",           gl[33].f, 15.0f );
    okf( "15-4",           gl[34].f, 11.0f );
    okf( "11/3",           gl[35].f, 11.0f/3.0f );

    section( "vectors occupy three consecutive slots" );
    /* ---- vectors occupy three consecutive slots ---- */
    reset();
    gl[40].f=1; gl[41].f=2; gl[42].f=3;
    gl[43].f=4; gl[44].f=5; gl[45].f=6;
    gl[46].f=2.5f;
    S(0, OP_ADD_V,  40,43,50);
    S(1, OP_MUL_V,  40,43,53);        /* dot product -> scalar */
    S(2, OP_MUL_VF, 40,46,54);
    S(3, OP_MUL_FV, 46,43,57);
    S(4, OP_DONE, 0,0,0);
    run( 1 );
    okf( "add_v x", gl[50].f, 5.0f );
    okf( "add_v z", gl[52].f, 9.0f );
    okf( "dot(1,2,3)(4,5,6)", gl[53].f, 32.0f );
    okf( "vec*float y", gl[55].f, 5.0f );
    okf( "float*vec z", gl[59].f, 15.0f );

    section( "entity: value is a BYTE offset, field is a WORD offset" );
    /* ---- entity: value is a BYTE offset, field is a WORD offset ---- */
    reset();
    gl[60].i = ENT( 2 );
    gl[61].i = 7;                     /* field 7 */
    gl[62].f = 99.0f;
    S(0, OP_ADDRESS,  60,61,63);      /* &ent.fld -> byte offset */
    S(1, OP_STOREP_F, 62,63,0);
    S(2, OP_LOAD_F,   60,61,64);
    S(3, OP_DONE, 0,0,0);
    run( 1 );
    okf( "ADDRESS = ent + fld*4", (float) gl[63].i, (float) ( 2 * EDSZ + 7 * 4 ) );
    okf( "STOREP then LOAD round-trips", gl[64].f, 99.0f );
    okf( "landed in edict 2 field 7", ed_f( ENT( 2 ), 7 ), 99.0f );

    section( "an edict on page 3 while self is on page 0" );
    /* ---- an edict on page 3 while self is on page 0 ---- */
    reset();
    gl[QG_SELF].i = ENT( 1 );
    gl[QG_TIME].f = 1.0f;
    gl[60].i = ENT( 70 );             /* page 3 */
    gl[61].i = 7;
    gl[62].f = 99.0f;
    gl[110].f = 12.0f;
    S(0, OP_ADDRESS,  60,61,63);
    S(1, OP_STOREP_F, 62,63,0);
    S(2, OP_STATE,   110,111,0);      /* writes self, on page 0 */
    S(3, OP_LOAD_F,   60,61,64);
    S(4, OP_DONE, 0,0,0);
    run( 1 );
    okf( "page 3: the value is page 3's", (float) ( gl[60].i >> 16 ), 3.0f );
    okf( "page 3: LOAD after a page-0 STATE reads it back", gl[64].f, 99.0f );
    okf( "page 3: edict 70 holds it", ed_f( ENT( 70 ), 7 ), 99.0f );
    okf( "page 3: page 0's edict at the same offset does not", ed_f( ENT( 7 ), 7 ), 0.0f );
    okf( "page 3: self's frame is on page 0", ed_f( ENT( 1 ), 3 ), 12.0f );

    section( "branches; IF tests raw bits, so -0.0 is TRUE" );
    /* ---- branches; IF tests raw bits, so -0.0 is TRUE ---- */
    reset();
    gl[70].f = 0.0f;
    gl[71].i = (sint32) 0x80000000UL; /* -0.0 */
    gl[73].f = 5.0f;
    /* A jump lands on (this statement + b), not (b): Quake does s += b - 1
       then s++. Writing these as absolute targets is off by one. */
    S(0, OP_IF,    70, 3, 0);         /* +0.0 has zero bits -> not taken */
    S(1, OP_IF,    71, 3, 0);         /* -0.0 has the sign bit -> taken, to 4 */
    S(2, OP_GOTO,   1, 0, 0);
    S(3, OP_DONE,   0, 0, 0);
    S(4, OP_STORE_F, 73, 72, 0);
    S(5, OP_DONE, 0,0,0);
    run( 1 );
    okf( "IF(-0.0) branches, as in Quake", gl[72].f, 5.0f );

    /* GOTO's displacement is in a; the one above is jumped over and never runs.
       Read from b (0 in qcc's output) it jumps to itself until the runaway trap. */
    reset();
    gl[73].f = 5.0f;
    S(0, OP_GOTO,   2, 0, 0);
    S(1, OP_STORE_F, 73, 74, 0);       /* skipped */
    S(2, OP_STORE_F, 73, 75, 0);
    S(3, OP_DONE, 0,0,0);
    run( 1 );
    okf( "GOTO skips the statement it jumps over", gl[74].f, 0.0f );
    okf( "GOTO lands on its target",               gl[75].f, 5.0f );
    okf( "GOTO leaves no trap",                    (float) m.trap, 0.0f );

    section( "a function past a page edge is placed whole on the next page" );
    /* ---- a function past a page edge is placed whole on the next page ----
       fn 2 fills page 0 to 56 bytes short; fn 3 is 72 bytes and jumps. Laid
       out as in the file its last statements sit past page 0's window. */
    reset();
    gl[70].f = 0.0f;
    gl[73].f = 5.0f;
    fn[2].first = 0;
    for ( i = 0; i < 2040; i++ ) S(i, OP_STORE_F, 73, 77, 0);
    S(2040, OP_DONE, 0,0,0);
    fn[3].first = 2041;
    S(2041, OP_IFNOT,   70, 3, 0);    /* taken, to 2044 */
    S(2042, OP_STORE_F, 73, 74, 0);
    S(2043, OP_DONE, 0,0,0);
    S(2044, OP_GOTO,     3, 0, 0);    /* to 2047 */
    S(2045, OP_STORE_F, 73, 74, 0);
    S(2046, OP_DONE, 0,0,0);
    S(2047, OP_STORE_F, 73, 76, 0);
    S(2048, OP_STORE_F, 73, 75, 0);
    S(2049, OP_DONE, 0,0,0);
    nst = 2050;
    run( 3 );
    okf( "page edge: the function is on page 1", (float) m.st_pages, 2.0f );
    okf( "page edge: its jumps land",            gl[76].f, 5.0f );
    okf( "page edge: its last statements run",   gl[75].f, 5.0f );
    okf( "page edge: nothing it jumps over ran", gl[74].f, 0.0f );
    okf( "page edge: no trap",                   (float) m.trap, 0.0f );

    section( "builtin call: args via PARM slots, no C arguments" );
    /* ---- builtin call: args via PARM slots, no C arguments ---- */
    reset();
    fn[2].first = -1;                 /* negative => builtin 1 */
    gl[80].i = 2;                     /* function number to call */
    gl[QOFS_PARM0].f = 21.0f;
    S(0, OP_CALL1, 80,0,0);
    S(1, OP_STORE_F, QOFS_RETURN, 81, 0);
    S(2, OP_DONE, 0,0,0);
    run( 1 );
    okf( "builtin got PARM0 and set RETURN", gl[81].f, 42.0f );
    okf( "argc came from the CALLn opcode", (float) bi_argc, 1.0f );

    section( "a builtin takes every window; the QuakeC after it must not notice" );
    /* ---- a builtin takes every window; the QuakeC after it must not notice ---- */
    reset();
    fn[2].first = -2;
    gl[QG_SELF].i = ENT( 1 );
    gl[60].i = ENT( 70 );
    gl[65].i = ENT( 1 );
    gl[61].i = 7;
    gl[62].f = 99.0f;
    gl[66].f = 44.0f;
    gl[73].f = 5.0f;
    gl[80].i = 2;
    S(0, OP_ADDRESS,  60,61,63);
    S(1, OP_STOREP_F, 62,63,0);       /* page 3 into EDX */
    S(2, OP_ADDRESS,  65,61,67);
    S(3, OP_STOREP_F, 66,67,0);       /* page 0 */
    S(4, OP_CALL0,    80,0,0);
    S(5, OP_LOAD_F,   60,61,64);
    S(6, OP_LOAD_F,   65,61,68);
    S(7, OP_STORE_F,  73,75,0);
    S(8, OP_DONE, 0,0,0);
    fn[3].first = 9;                  /* reads both edicts before anything maps */
    S(9,  OP_LOAD_F,  60,61,64);
    S(10, OP_LOAD_F,  65,61,68);
    S(11, OP_DONE, 0,0,0);
    run( 1 );
    okf( "after a builtin: page 3's edict reads back", gl[64].f, 99.0f );
    okf( "after a builtin: page 0's edict reads back", gl[68].f, 44.0f );
    okf( "after a builtin: the statements go on",      gl[75].f, 5.0f );
    okf( "after a builtin: no trap",                   (float) m.trap, 0.0f );

    section( "C maps between two runs" );
    /* ---- C maps between two runs ---- */
    gl[64].f = gl[68].f = gl[75].f = 0.0f;
    /* not reloaded: C takes every window, then a run that only reads */
    bi_clobber( &m );
    qvm_run( &m, 3 );
    okf( "between runs: page 3's edict reads back", gl[64].f, 99.0f );
    okf( "between runs: page 0's edict reads back", gl[68].f, 44.0f );
    okf( "between runs: no trap",                   (float) m.trap, 0.0f );

    section( "a builtin runs QuakeC on another statement page" );
    /* ---- a builtin runs QuakeC on another statement page ----
       fn 2 (inner) and the filler share page 0; fn 4 (outer) is on page 1. */
    reset();
    fn[2].first = 0;
    S(0, OP_ADDRESS,  90,61,91);
    S(1, OP_STOREP_F, 92,91,0);       /* edict 50, page 2 */
    S(2, OP_DONE, 0,0,0);
    fn[3].first = 3;
    for ( i = 3; i < 2043; i++ ) S(i, OP_STORE_F, 73, 77, 0);
    S(2043, OP_DONE, 0,0,0);
    fn[4].first = 2044;
    S(2044, OP_ADDRESS,  60,61,63);
    S(2045, OP_STOREP_F, 62,63,0);    /* edict 70, page 3 */
    S(2046, OP_CALL0,    80,0,0);
    S(2047, OP_LOAD_F,   60,61,64);
    S(2048, OP_STORE_F,  73,75,0);
    S(2049, OP_DONE, 0,0,0);
    nst = 2050;
    fn[5].first = -3;
    nested_fn = 2;
    gl[60].i = ENT( 70 );
    gl[90].i = ENT( 50 );
    gl[61].i = 7;
    gl[62].f = 99.0f;
    gl[92].f = 33.0f;
    gl[73].f = 5.0f;
    gl[80].i = 5;
    run( 4 );
    okf( "nested: the inner run wrote its edict",      ed_f( ENT( 50 ), 7 ), 33.0f );
    okf( "nested: the outer run's edict reads back",  gl[64].f, 99.0f );
    okf( "nested: the outer statements go on",        gl[75].f, 5.0f );
    okf( "nested: no trap, depth 0",                  (float) ( m.trap + m.depth ), 0.0f );

    section( "strings: hashes compare as bits; null and \"\" are one to EQ_S, not to IF" );
    /* ---- strings: hashes compare as bits; null and "" are one to EQ_S, not to IF ---- */
    reset();
    gl[80].i = HASH_ABCDEFGH; gl[81].i = HASH_ABCDEFGH;
    gl[82].i = (sint32) 0x80000000UL; /* -0.0 as a float */
    gl[88].i = 0;                     /* null */
    gl[89].i = 0x7FC00000L;           /* a NaN as a float */
    gl[96].i = QVM_STR_EMPTY;         /* "" */
    gl[73].f = 5.0f;
    S(0,  OP_EQ_S,  80,81,83);
    S(1,  OP_EQ_S,  82,88,84);
    S(2,  OP_NE_S,  80,81,85);
    S(3,  OP_NOT_S, 80,0,86);
    S(4,  OP_NOT_S, 88,0,87);
    S(5,  OP_EQ_S,  89,89,90);
    S(6,  OP_NOT_S, 82,0,91);
    S(7,  OP_EQ_S,  88,96,92);
    S(8,  OP_NE_S,  88,96,93);
    S(9,  OP_NOT_S, 96,0,94);
    S(10, OP_IFNOT, 96,2,0);          /* "" is true in id: not taken */
    S(11, OP_STORE_F, 73,97,0);
    S(12, OP_IF,    88,2,0);          /* null is false: not taken */
    S(13, OP_STORE_F, 73,98,0);
    S(14, OP_NE_S,  96,80,99);
    S(15, OP_DONE, 0,0,0);
    run( 1 );
    okf( "EQ_S of one hash",                            gl[83].f, 1.0f );
    okf( "EQ_S of -0.0's bits and null is 0",           gl[84].f, 0.0f );
    okf( "NE_S of one hash",                            gl[85].f, 0.0f );
    okf( "NOT_S of a hash",                             gl[86].f, 0.0f );
    okf( "NOT_S of null",                               gl[87].f, 1.0f );
    okf( "EQ_S of a NaN's bits with itself",            gl[90].f, 1.0f );
    okf( "NOT_S of -0.0's bits",                        gl[91].f, 0.0f );
    okf( "null EQ_S \"\"",                              gl[92].f, 1.0f );
    okf( "null NE_S \"\"",                              gl[93].f, 0.0f );
    okf( "NOT_S of \"\"",                               gl[94].f, 1.0f );
    okf( "IFNOT \"\" does not branch",                  gl[97].f, 5.0f );
    okf( "IF null does not branch",                     gl[98].f, 5.0f );
    okf( "\"\" NE_S a text",                            gl[99].f, 1.0f );

    section( "qvm_str_text: through the index, across an aux page" );
    /* ---- qvm_str_text: through the index, across an aux page ---- */
    reset();
    nstr = NSTR;
    _fmemcpy( strs + 10,    "player", 7 );
    _fmemcpy( strs + 30,    "5", 2 );
    _fmemcpy( strs + 40,    "knight", 7 );
    _fmemcpy( strs + 16380, "abcdefgh", 9 );
    idx[0].hash = HASH_PLAYER;   idx[0].ofs = 10;
    idx[1].hash = HASH_5;        idx[1].ofs = 30;
    idx[2].hash = HASH_ABCDEFGH; idx[2].ofs = 16380;
    idx[3].hash = HASH_KNIGHT;   idx[3].ofs = 40;
    nidx = 4;
    load();
    {
        char buf[16];
        okf( "text of a string across an aux page",
             (float) ( qvm_str_text( &m, HASH_ABCDEFGH, buf, sizeof buf ) == 8 && !strcmp( buf, "abcdefgh" ) ), 1.0f );
        okf( "text of the index's first",
             (float) ( qvm_str_text( &m, HASH_PLAYER, buf, sizeof buf ) == 6 && !strcmp( buf, "player" ) ), 1.0f );
        okf( "text of a hash past 0x7FFFFFFF",
             (float) ( qvm_str_text( &m, HASH_KNIGHT, buf, sizeof buf ) == 6 && !strcmp( buf, "knight" ) ), 1.0f );
        okf( "a hash not in the index has no text", (float) qvm_str_text( &m, 12345L, buf, sizeof buf ), -1.0f );
        okf( "null prints empty", (float) ( qvm_str_text( &m, 0L, buf, sizeof buf ) == 0 && !buf[0] ), 1.0f );
        okf( "\"\" prints empty", (float) ( qvm_str_text( &m, QVM_STR_EMPTY, buf, sizeof buf ) == 0 && !buf[0] ), 1.0f );
        okf( "text is cut to the buffer",
             (float) ( qvm_str_text( &m, HASH_ABCDEFGH, buf, 4 ) == 3 && !strcmp( buf, "abc" ) ), 1.0f );
        okf( "C hashes \"player\" as qchash.py does", (float) ( qvm_hash( "player" ) == HASH_PLAYER ), 1.0f );
        okf( "C hashes a byte past 0x7F as qchash.py does", (float) ( qvm_hash( "\x8d" "ab" ) == HASH_HIGHBYTE ), 1.0f );
        okf( "C gives \"\" STR_EMPTY", (float) ( qvm_hash( "" ) == QVM_STR_EMPTY ), 1.0f );
    }

    section( "ftos: the temp prints, and equals no constant" );
    /* ---- ftos: the temp prints, and equals no constant ----
       id compares text, so its ftos(5) == "5"; stock QuakeC never asks. */
    reset();
    _fmemcpy( strs + 30, "5", 2 );
    idx[0].hash = HASH_5; idx[0].ofs = 30;
    nidx = 1;
    fn[2].first = -4;
    gl[80].i = 2;
    gl[82].i = HASH_5;
    gl[QOFS_PARM0].f = 5.0f;
    S(0, OP_CALL1,   80,0,0);
    S(1, OP_STORE_S, QOFS_RETURN,81,0);
    S(2, OP_EQ_S,    81,82,83);
    S(3, OP_DONE, 0,0,0);
    run( 1 );
    {
        char buf[16];
        okf( "ftos answers the temp", (float) ( gl[81].i == QVM_STR_TEMP ), 1.0f );
        okf( "the temp prints \"5\"",
             (float) ( qvm_str_text( &m, gl[81].i, buf, sizeof buf ) == 1 && !strcmp( buf, "5" ) ), 1.0f );
        okf( "the temp does not EQ_S the constant \"5\"", gl[83].f, 0.0f );
    }

    section( "an image not in this qchash.py's format is refused" );
    /* ---- an image not in this qchash.py's format is refused ----
       0x51480006 held "" as 0: loaded, if ("") would be false. */
    reset();
    {
        static const sint32 old[2] = { 6L, 0x51480006L };
        QImgSrc src;
        sint32 len;
        memset( &src, 0, sizeof src );
        src.st = st; src.nst = nst; src.fn = fn; src.nfn = 8; src.gl = gl; src.ngl = 256;
        src.str = strs; src.nstr = nstr; src.edwords = EDW;
        for ( i = 0; i < 2; i++ ) {
            src.version = old[i];
            len = qimg_build( &src, image, 65520L );
            qimg_use( image, len );
            okf( i ? "the \"\" == null rewrite's version is refused" : "id's version 6 is refused",
                 (float) qvm_load( &m, &host, gl, sizeof gl ), (float) QVM_ERR_VERSION );
        }
    }

    section( "recursion: the locals window must survive a self-call" );
    /* ---- recursion: the locals window must survive a self-call ----
       fn[3]: f(n) = n<=1 ? 1 : n * f(n-1), locals at 100..101. Each frame
       first prints a string across a page through a builtin, so the function
       records are mapped out between the call and its return. */
    reset();
    nstr = NSTR;
    _fmemcpy( strs + 16380, "abcdefgh", 9 );
    idx[0].hash = HASH_ABCDEFGH; idx[0].ofs = 16380;
    nidx = 1;
    fn[2].first = -5;
    fn[3].first = 0; fn[3].pstart = 100; fn[3].locals = 2; fn[3].nparm = 1;
    fn[3].psz[0] = 1;
    gl[90].i = 3;                     /* fn number, for the recursive CALL */
    gl[91].f = 1.0f;
    gl[94].i = 2;                     /* bi_text */
    gl[95].i = HASH_ABCDEFGH;
    bi_text_buf[0] = 0;
    S(0, OP_CALL0,    94, 0, 0);
    S(1, OP_LE,      100, 91, 102);   /* n <= 1 ? */
    S(2, OP_IFNOT,   102, 2, 0);      /* -> statement 4 */
    S(3, OP_DONE,     91, 0, 0);      /* return 1 */
    S(4, OP_SUB_F,   100, 91, 101);   /* local: n-1 */
    S(5, OP_STORE_F, 101, QOFS_PARM0, 0);
    S(6, OP_CALL1,    90, 0, 0);
    S(7, OP_MUL_F,   100, QOFS_RETURN, 103);   /* n * f(n-1); n must be intact */
    S(8, OP_DONE,    103, 0, 0);
    gl[QOFS_PARM0].f = 5.0f;
    run( 3 );
    okf( "5! through 5 nested frames", gl[QOFS_RETURN].f, 120.0f );
    okf( "each frame printed through the index", (float) !strcmp( bi_text_buf, "abcdefgh" ), 1.0f );
    okf( "recursion left no trap", (float) m.trap, 0.0f );
    okf( "call stack unwound to 0",  (float) m.depth, 0.0f );
    okf( "locals stack unwound to 0", (float) m.lused, 0.0f );

    section( "OP_STATE writes three entity fields and sets the 10Hz think" );
    /* ---- OP_STATE writes three entity fields and sets the 10Hz think ---- */
    reset();
    gl[QG_SELF].i = ENT( 3 );
    gl[QG_TIME].f = 7.5f;
    gl[110].f = 12.0f;                /* frame */
    gl[111].i = 6;                    /* think function number */
    S(0, OP_STATE, 110, 111, 0);
    S(1, OP_DONE, 0,0,0);
    run( 1 );
    okf( "STATE sets frame",              ed_f( ENT( 3 ), 3 ), 12.0f );
    okf( "STATE sets nextthink = t+0.1",  ed_f( ENT( 3 ), 4 ), 7.6f );
    okf( "STATE sets think",       (float) qvm_ent_get( &m, ENT( 3 ), 5 ).i, 6.0f );
    okf( "STATE program left no trap", (float) m.trap, 0.0f );

    section( "a call past the function table traps" );
    /* ---- a call past the function table traps ---- */
    reset();
    gl[80].i = 99;
    gl[73].f = 5.0f;
    S(0, OP_CALL0,   80, 0, 0);
    S(1, OP_STORE_F, 73, 75, 0);
    S(2, OP_DONE, 0,0,0);
    run( 1 );
    okf( "a call past the function table traps", (float) m.trap, (float) QVM_ERR_NULLFN );
    okf( "... before the statement after it",    gl[75].f, 0.0f );

    section( "runaway: a stubbed builtin can leave QuakeC spinning" );
    /* ---- runaway: a stubbed builtin can leave QuakeC spinning ----
       id stops an execution after 100,000 statements; without that qvm_run
       never returns. The trap must unwind too: a run left at depth 2 with
       saved locals would start the next run from a broken stack. */
    reset();
    fn[2].first = 0; fn[2].pstart = 120; fn[2].locals = 1;   /* outer: calls fn 3 */
    fn[3].first = 3; fn[3].pstart = 121; fn[3].locals = 1;   /* inner: spins */
    fn[4].first = 5;                                        /* clean: returns 5 */
    gl[120].f = 11.0f;
    gl[121].f = 22.0f;
    gl[122].i = 3;
    gl[123].f = 5.0f;
    S(0, OP_STORE_F, 123, 120, 0);
    S(1, OP_CALL0,   122, 0, 0);
    S(2, OP_DONE, 0,0,0);
    S(3, OP_STORE_F, 123, 121, 0);
    S(4, OP_GOTO, 0, 0, 0);
    S(5, OP_DONE,  123, 0, 0);
#ifndef __TURBOC__
    signal( SIGALRM, on_alarm );
    alarm( 2 );
#endif
    run( 2 );
#ifndef __TURBOC__
    alarm( 0 );
#endif
    okf( "a spinning loop traps as a runaway", (float) m.trap, (float) QVM_ERR_RUNAWAY );
    okf( "the trap unwinds the call stack",   (float) m.depth, 0.0f );
    okf( "the trap unwinds the locals stack", (float) m.lused, 0.0f );
    okf( "the caller's local is restored",    gl[120].f, 11.0f );
    okf( "the callee's local is restored",    gl[121].f, 22.0f );
    qvm_run( &m, 4 );
    okf( "the next run on the machine returns", gl[QOFS_RETURN].f, 5.0f );

    section( "a STOREP through a garbage value writes inside the edicts" );
    /* ---- a STOREP through a garbage value writes inside the edicts ----
       Offset 0x3FFF plus a vector ran past ED0's window into EDX's page. */
    reset();
    gl[60].i = ENT( 21 );             /* page 1, in EDX */
    gl[61].i = 0;
    gl[62].f = 55.0f;
    gl[63].i = 0x3FFF;                /* page 0, past the last edict */
    gl[40].f = 1; gl[41].f = 2; gl[42].f = 3;
    S(0, OP_ADDRESS,  60,61,64);
    S(1, OP_STOREP_F, 62,64,0);
    S(2, OP_STOREP_V, 40,63,0);
    S(3, OP_LOAD_F,   60,61,65);
    S(4, OP_DONE, 0,0,0);
    run( 1 );
    okf( "garbage STOREP_V: page 1's edict is untouched", gl[65].f, 55.0f );
    okf( "garbage STOREP_V: no trap", (float) m.trap, 0.0f );

    section( "C reads edicts through values, never a kept pointer" );
    /* ---- C reads edicts through values, never a kept pointer ---- */
    reset();
    load();
    {
        QVal v;
        v.f = 1.0f; qvm_ent_set( &m, ENT( 21 ), 7, v );
        v.f = 2.0f; qvm_ent_set( &m, ENT( 42 ), 7, v );
        v.f = 9.0f; qvm_ent_set( &m, ENT( 21 ), EDW, v );
        okf( "accessors: page 1's edict", qvm_ent_get( &m, ENT( 21 ), 7 ).f, 1.0f );
        ems_map( m.h_ed, 2, QVM_W_EDX );    /* C takes the window for page 2 */
        okf( "accessors: page 1's again after C took the window", qvm_ent_get( &m, ENT( 21 ), 7 ).f, 1.0f );
        okf( "accessors: page 2's edict", qvm_ent_get( &m, ENT( 42 ), 7 ).f, 2.0f );
        v.f = 3.0f; qvm_ent_set( &m, ENT( 1 ), 7, v );
        ems_map( m.h_ed, 2, QVM_W_ED0 );    /* and ED0's, which page 0 is read through */
        okf( "accessors: page 0's after C took ED0", qvm_ent_get( &m, ENT( 1 ), 7 ).f, 3.0f );
        okf( "accessors: a field past the edict writes nothing", qvm_ent_get( &m, ENT( 22 ), 0 ).f, 0.0f );
    }

    section( "edicts made again read zero" );
    /* ---- edicts made again read zero ----
       The EMM hands a freed handle's number straight back. A window record
       that survived the release answered the new handle's page 0 with the
       old one, and its edicts read what the old ones held. */
    reset();
    load();
    {
        QVal v;
        v.f = 5.0f; qvm_ent_set( &m, ENT( 1 ), 7, v );
        v.f = 6.0f; qvm_ent_set( &m, ENT( 21 ), 7, v );
        okf( "edicts made again", (float) qvm_edicts( &m, 64 ), 0.0f );
        okf( "... page 0's edict reads zero", qvm_ent_get( &m, ENT( 1 ), 7 ).f, 0.0f );
        okf( "... page 1's edict reads zero", qvm_ent_get( &m, ENT( 21 ), 7 ).f, 0.0f );
        /* a window's key holds a page in a byte */
        okf( "edicts past 255 pages are refused", (float) qvm_edicts( &m, (sint16) ( EDPP * 255 + 1 ) ),
             (float) QVM_ERR_SIZE );
        okf( "... and the edicts there still read", qvm_ent_get( &m, ENT( 21 ), 7 ).f, 0.0f );
    }

    section( "a handle past a key's byte is refused, and not kept" );
    /* ---- a handle past a key's byte is refused, and not kept ----
       A kept handle 255 went on to ed_windows, where its low byte matched
       another handle's key. */
    reset();
    load();
    alloc_as = 255;
    okf( "edicts on handle 255 are refused", (float) qvm_edicts( &m, 64 ), (float) QVM_ERR_EMS );
    okf( "... and the VM keeps no edict handle", (float) m.h_ed, 0.0f );

    section( "a run on pages already mapped calls map 0 times" );
    /* ---- a run on pages already mapped calls map 0 times ----
       Every CALL, RETURN and builtin asks for its windows. Asked through the
       host, that was a far call each: 129 a round of real QuakeC, 0.62 us
       each, where the page had changed 6 times. */
    reset();
    gl[60].i = ENT( 1 );
    gl[61].i = ENT( 21 );
    gl[62].i = 7;
    gl[65].i = 2;                     /* function 2: builtin 1 */
    gl[QOFS_PARM0].f = 4.0f;
    S(0, OP_LOAD_F, 60,62,63);
    S(1, OP_LOAD_F, 61,62,64);
    S(2, OP_CALL1,  65,0,0);
    S(3, OP_STORE_F, QOFS_RETURN,66,0);  /* DONE copies global 0 over RETURN */
    S(4, OP_LOAD_F, 61,62,64);
    S(5, OP_DONE,   0,0,0);
    if ( !load() ) {
        qvm_run( &m, 1 );
        h_calls = 0;
        qvm_run( &m, 1 );
        okf( "second run: no trap", (float) m.trap, 0.0f );
        okf( "second run: the builtin ran", gl[66].f, 8.0f );
        okf( "second run: map calls", (float) h_calls, 0.0f );
    }

    section( "qvm_run on a builtin's number" );
    /* ---- qvm_run on a builtin's number ---- */
    reset();
    load();
    qvm_run( &m, 2 );
    okf( "qvm_run on a builtin traps as one", (float) m.trap, (float) QVM_ERR_BUILTIN );

    section( "an edict bigger than a page is refused" );
    /* ---- an edict bigger than a page is refused ---- */
    reset();
    {
        QImgSrc src;
        sint32 len;
        memset( &src, 0, sizeof src );
        src.st = st; src.nst = 1; src.fn = fn; src.nfn = 2; src.gl = gl; src.ngl = 64;
        src.str = strs; src.nstr = 1; src.edwords = 4200;
        len = qimg_build( &src, image, 65520L );
        qimg_use( image, len );
        okf( "an image with 16,800-byte edicts loads", (float) qvm_load( &m, &host, gl, sizeof gl ), 0.0f );
        okf( "... and its edicts are refused", (float) qvm_edicts( &m, 4 ), (float) QVM_ERR_SIZE );
        qvm_free( &m );
    }

#ifndef __TURBOC__
    section( "operands are UNSIGNED indices" );
    /* ---- operands are UNSIGNED indices ----
       A global past 32767 read as a signed short addresses backwards off
       the array. The loader refuses globals past 64K, so the machine is
       loaded small and pointed at a big array. */
    {
        static QVal big[40100];
        reset();
        S(0, OP_MUL_F, 40000, 40001, 40002);
        S(1, OP_DONE, 0,0,0);
        load();
        m.glob = big; m.nglob = 40100;
        big[40000].f = 7.0f;
        big[40001].f = 6.0f;
        qvm_run( &m, 1 );
        okf( "operand 40000 addresses forwards", big[40002].f, 42.0f );
    }
#endif

    okf( "QVal is 4 bytes, as progs.dat requires", (float) sizeof(QVal), 4.0f );
    okf( "uint16 is 2", (float) sizeof(uint16), 2.0f );
    okf( "uint32 is 4", (float) sizeof(uint32), 4.0f );
    okf( "QStat is 8",  (float) sizeof(QStat),  8.0f );

    reset();
    if ( argc > 1 ) check_refused( argv[1] );
    for ( i = 2; i < argc; i++ ) check_image( argv[i], 30 );

    say( "\nqvm_test: %s (%d failures)\n", fails ? "FAILED" : "PASSED", fails );
    return fails != 0;
}
