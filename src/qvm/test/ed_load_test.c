/* ed_load_test.c -- edict.c loads a map as QuakeSpasm did, checked against QuakeSpasm's own record
 * of that load (fixtures/, made by fixtures/quakespasm-eddump.patch): every edict after its parse
 * (P) and its spawn (S), what each spawn function changed (D), and each builtin's number,
 * parameters, return and writes (B, W). The builtins are that record replayed: spawn, remove and
 * makestatic run edict.c's allocator and are checked against what QuakeSpasm's did; every other
 * builtin has its return and its writes put back.
 *
 *   ed_load_test PROGSH.DAT MAP.BSP DUMP [MAP.BSP DUMP ...]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qvm.h"
#include "ed.h"
#include "edict.h"
#include "ems.h"
#include "bspents.h"

#define MAX_EDICTS  600
#define NBUILTINS   80
#define SHOWN       6               /* failures printed a map: past the first desync the rest is noise */
#define LINE        4096

typedef struct { uint16 type, ofs; const char *name; } Def;

#define WORDS( d )        ( ( (d)->type & 0x7FFF ) == 3 ? 3 : 1 )
#define INSIDE( in, d )   ( ( (d)->type & 0x7FFF ) != 3 && (in)[(d)->ofs] )   /* a vector's _x _y _z */

static unsigned char *img;
static long img_len;
static Def *fdefs, *gdefs;
static long nfdefs, ngdefs;
static unsigned char *fcomp, *gcomp;

static Qvm m;
static EdWorld w;
static QVal glob[16384];
static float freetime[MAX_EDICTS];
static uint8 isfree[MAX_EDICTS];
static QBuiltin builtins[NBUILTINS];
static sint16 words;

static char **rec;                  /* the dump's lines */
static long nrec, cur;
static char mapname[64];
static long checks, fails, mapfails;

static QVal *snap_ed, snap_gl[16384];
static uint8 snap_free[MAX_EDICTS];
static sint16 snap_num;

static uint8 *h_map( sint16 win, sint16 hnd, uint16 page ) { return ems_map( hnd, page, win ); }
static sint16 h_alloc( sint32 bytes ) { return ems_alloc( bytes ); }
static void h_release( sint16 hnd ) { ems_free( hnd ); }
static sint16 h_read( void *dst, sint32 ofs, uint16 n )
{
    if ( ofs < 0 || ofs + n > img_len ) return 1;
    memcpy( dst, img + ofs, n );
    return 0;
}
static const QHost host = { h_map, h_alloc, h_release, h_read, ems_key };

static char *load_file( const char *path, long *len )
{
    FILE *f = fopen( path, "rb" );
    char *buf = 0;

    if ( !f ) return 0;
    if ( !fseek( f, 0, SEEK_END ) && ( *len = ftell( f ) ) >= 0 && !fseek( f, 0, SEEK_SET ) ) {
        buf = (char *) malloc( (size_t) *len + 1 );
        if ( buf && fread( buf, 1, (size_t) *len, f ) != (size_t) *len ) { free( buf ); buf = 0; }
        if ( buf ) buf[*len] = 0;
    }
    fclose( f );
    return buf;
}

/* ---- the program's defs, read from the image as the dump names them ---- */
static Def *read_defs( sint32 ofs, sint32 n, sint32 strofs )
{
    Def *d = (Def *) malloc( (size_t) n * sizeof *d );
    unsigned char *p;
    long k;

    for ( k = 0; k < n; k++ ) {
        p = img + ofs + k * 8;
        d[k].type = (uint16) ( p[0] | p[1] << 8 );
        d[k].ofs = (uint16) ( p[2] | p[3] << 8 );
        d[k].name = (const char *) img + strofs + ( p[4] | p[5] << 8 | (long) p[6] << 16 | (long) p[7] << 24 );
    }
    return d;
}

static unsigned char *components( const Def *d, long n, long nwords )
{
    unsigned char *in = (unsigned char *) calloc( (size_t) nwords + 3, 1 );
    long k;

    for ( k = 0; k < n; k++ )
        if ( ( d[k].type & 0x7FFF ) == 3 ) memset( in + d[k].ofs, 1, 3 );
    return in;
}

static const Def *find_def( const Def *defs, long n, const unsigned char *comp, const char *name, int minofs )
{
    long k;

    for ( k = 0; k < n; k++ )
        if ( defs[k].ofs >= minofs && !INSIDE( comp, &defs[k] ) && !strcmp( defs[k].name, name ) ) return &defs[k];
    return 0;
}

/* ---- the record, and ours beside it ---- */
static const char *record( void ) { return cur < nrec ? rec[cur] : "<the end of the dump>"; }

static void report( const char *what, const char *ours, const char *theirs )
{
    fails++;
    if ( ++mapfails <= SHOWN )
        printf( "  FAIL  %s, dump line %ld: %s\n        ours   %s\n        theirs %s\n", mapname, cur + 1, what, ours, theirs );
}

static void expect( const char *ours )
{
    checks++;
    if ( strcmp( ours, record() ) ) report( "differs", ours, record() );
    if ( cur < nrec ) cur++;
}

/* a word of a record, or a quoted text unescaped; 0 at the end of the line */
static const char *word( const char *p, char *out )
{
    int n = 0, c;

    while ( *p == ' ' ) p++;
    if ( !*p ) { out[0] = 0; return 0; }
    if ( *p != '"' ) {
        while ( *p && *p != ' ' ) out[n++] = *p++;
        out[n] = 0;
        return p;
    }
    for ( p++; *p && *p != '"'; p++ ) {
        if ( *p != '\\' ) { out[n++] = *p; continue; }
        p++;
        if ( *p == 'n' ) out[n++] = '\n';
        else if ( *p == 'x' && sscanf( p + 1, "%2x", &c ) == 1 ) { out[n++] = (char) c; p += 2; }
        else out[n++] = *p;
    }
    out[n] = 0;
    return *p ? p + 1 : p;
}

static sint32 hex( const char *s ) { return (sint32) strtoul( s, 0, 16 ); }

static void put_text( char *out, const char *s )
{
    out += strlen( out );
    *out++ = '"';
    for ( ; *s; s++ ) {
        if ( *s == '\n' ) { *out++ = '\\'; *out++ = 'n'; }
        else if ( *s == '"' || *s == '\\' ) { *out++ = '\\'; *out++ = *s; }
        else if ( (unsigned char) *s < 32 ) { sprintf( out, "\\x%02x", (unsigned char) *s ); out += 4; }
        else *out++ = *s;
    }
    *out++ = '"';
    *out = 0;
}

static long ent_num( sint32 v ) { return (long) ( (uint32) v >> 16 ) * m.edpp + (long) (uint16) v / m.edsz; }

/* a def's value as the dump writes it, after what out holds */
static void put_value( char *out, const Def *d, const QVal *v )
{
    char text[ED_TOKEN];

    out += strlen( out );
    switch ( d->type & 0x7FFF ) {
    case 3:
        sprintf( out, " v %s %08lx %08lx %08lx", d->name, (unsigned long) (uint32) v[0].i,
                 (unsigned long) (uint32) v[1].i, (unsigned long) (uint32) v[2].i );
        break;
    case 1:
        if ( !v[0].i ) { sprintf( out, " s %s -", d->name ); break; }
        sprintf( out, " s %s ", d->name );
        if ( qvm_str_text( &m, v[0].i, text, sizeof text ) < 0 )
            sprintf( text, "<no text for %08lx>", (unsigned long) (uint32) v[0].i );
        put_text( out, text );
        break;
    case 4:
        sprintf( out, " e %s %ld", d->name, ent_num( v[0].i ) );
        break;
    default:
        sprintf( out, " %d %s %08lx", d->type & 0x7FFF, d->name, (unsigned long) (uint32) v[0].i );
        break;
    }
}

static void ent_words( sint16 n, const Def *d, QVal *v )
{
    int k;

    for ( k = 0; k < WORDS( d ); k++ ) v[k] = qvm_ent_get( &m, qvm_entval( &m, n ), (sint16) ( d->ofs + k ) );
}

static void dump_edict( char tag, sint16 n )
{
    char line[LINE];
    QVal v[3];
    long i;

    sprintf( line, "%c %d %d", tag, n, isfree[n] );
    expect( line );
    for ( i = 1; i < nfdefs; i++ ) {
        if ( INSIDE( fcomp, &fdefs[i] ) ) continue;
        ent_words( n, &fdefs[i], v );
        if ( !v[0].i && ( WORDS( &fdefs[i] ) == 1 || ( !v[1].i && !v[2].i ) ) ) continue;
        line[0] = 0;
        put_value( line, &fdefs[i], v );
        expect( line );
    }
}

static void snapshot( void )
{
    sint16 n, k;

    for ( n = 0; n < w.num; n++ )
        for ( k = 0; k < words; k++ )
            snap_ed[(long) n * words + k] = qvm_ent_get( &m, qvm_entval( &m, n ), k );
    memcpy( snap_free, isfree, sizeof isfree );
    memcpy( snap_gl, glob, sizeof glob );
    snap_num = w.num;
}

/* what the spawn function changed: every edict, then every named global past the engine's parms */
static void diff( void )
{
    static const QVal zero[3];
    char line[LINE];
    const QVal *was;
    QVal v[3];
    const Def *d;
    sint16 n;
    long i;

    for ( n = 0; n < w.num; n++ ) {
        if ( n >= snap_num || snap_free[n] != isfree[n] ) {
            sprintf( line, "D %d free %d", n, isfree[n] );
            expect( line );
        }
        for ( i = 1; i < nfdefs; i++ ) {
            d = &fdefs[i];
            if ( INSIDE( fcomp, d ) ) continue;
            ent_words( n, d, v );
            was = n < snap_num ? snap_ed + (long) n * words + d->ofs : zero;
            if ( !memcmp( v, was, WORDS( d ) * sizeof *v ) ) continue;
            sprintf( line, "D %d", n );
            put_value( line, d, v );
            expect( line );
        }
    }
    for ( i = 0; i < ngdefs; i++ ) {
        d = &gdefs[i];
        if ( d->ofs < 28 || INSIDE( gcomp, d ) ) continue;
        if ( !memcmp( glob + d->ofs, snap_gl + d->ofs, WORDS( d ) * sizeof *glob ) ) continue;
        strcpy( line, "D G" );
        put_value( line, d, glob + d->ofs );
        expect( line );
    }
}

static void hook( EdWorld *wp, sint16 stage, sint16 n )
{
    (void) wp;
    if ( stage == ED_PARSED ) dump_edict( 'P', n );
    else if ( stage == ED_SPAWNING ) snapshot();
    else { dump_edict( 'S', n ); diff(); }
}

/* ---- the builtins: QuakeSpasm's record of each call, replayed ---- */
static sint16 parse_value( const Def *d, const char *p, QVal *v )
{
    char tok[LINE], text[LINE];
    int k;

    switch ( d->type & 0x7FFF ) {
    case 3:
        for ( k = 0; k < 3; k++ ) {
            if ( !( p = word( p, tok ) ) ) return -1;
            v[k].i = hex( tok );
        }
        return 0;
    case 1:
        while ( *p == ' ' ) p++;
        if ( *p == '-' ) { v[0].i = 0; return 0; }
        /* a lookup, not an add: a text the VM has never seen means it has diverged */
        if ( !word( p, tok ) ) return -1;
        v[0].i = qvm_hash( tok );
        return qvm_str_text( &m, v[0].i, text, sizeof text ) >= 0 && !strcmp( text, tok ) ? 0 : -1;
    case 4:
        if ( !word( p, tok ) ) return -1;
        v[0].i = qvm_entval( &m, atol( tok ) );
        return 0;
    default:
        if ( !word( p, tok ) ) return -1;
        v[0].i = hex( tok );
        return 0;
    }
}

/* the W records after a builtin: put back, or for the allocator's own calls, already ours */
static void writes( int apply )
{
    char who[16], kind[16], name[256], line[LINE];
    const char *r, *p;
    const Def *d;
    QVal v[3];
    sint16 n;
    int k;

    while ( cur < nrec && rec[cur][0] == 'W' ) {
        r = rec[cur];
        p = word( r + 1, who );
        p = word( p, kind );
        n = (sint16) atoi( who );
        if ( !strcmp( kind, "free" ) ) {
            if ( apply ) { report( "a builtin that is not the allocator freed or took an edict", "", r ); cur++; continue; }
            sprintf( line, "W %d free %d", n, isfree[n] );
            expect( line );
            continue;
        }
        p = word( p, name );
        d = strcmp( who, "G" ) ? find_def( fdefs, nfdefs, fcomp, name, 0 ) : find_def( gdefs, ngdefs, gcomp, name, 28 );
        if ( !d ) { report( "a write naming no def", "", r ); cur++; continue; }
        if ( !apply ) {
            if ( strcmp( who, "G" ) ) { ent_words( n, d, v ); sprintf( line, "W %d", n ); }
            else { memcpy( v, glob + d->ofs, WORDS( d ) * sizeof *v ); strcpy( line, "W G" ); }
            put_value( line, d, v );
            expect( line );
            continue;
        }
        checks++;
        if ( parse_value( d, p, v ) ) { report( "a write that does not parse", "", r ); cur++; continue; }
        for ( k = 0; k < WORDS( d ); k++ ) {
            if ( strcmp( who, "G" ) ) qvm_ent_set( &m, qvm_entval( &m, n ), (sint16) ( d->ofs + k ), v[k] );
            else glob[d->ofs + k] = v[k];
        }
        cur++;
    }
}

/* each parameter the record names, as QuakeC passed it */
static void parameters( Qvm *vm, const char *p, const char *r )
{
    char tok[LINE], text[LINE], what[32];
    unsigned long a, b, c;
    const QVal *q;
    int k, ok;

    for ( k = 0; p && *p; k++ ) {
        while ( *p == ' ' ) p++;
        if ( !*p ) break;
        q = vm->glob + QOFS_PARM0 + 3 * k;
        switch ( *p++ ) {
        case 'e':
            p = word( p, tok );
            ok = q->i == qvm_entval( vm, atol( tok ) );
            break;
        case 's':
            if ( *p == '-' ) { p++; ok = !q->i; break; }
            p = word( p, tok );
            ok = q->i && qvm_str_text( vm, q->i, text, sizeof text ) >= 0 && !strcmp( text, tok );
            break;
        case 'v':
            p = word( p, tok );
            ok = sscanf( tok, "%lx,%lx,%lx", &a, &b, &c ) == 3 && (uint32) q[0].i == a && (uint32) q[1].i == b
                 && (uint32) q[2].i == c;
            break;
        default:
            p = word( p, tok );
            ok = q->i == hex( tok );
            break;
        }
        checks++;
        if ( !ok ) {
            sprintf( what, "parameter %d", k );
            report( "a builtin's parameter", what, r );
        }
    }
}

static void replay( Qvm *vm )
{
    char tok[LINE], text[LINE], line[LINE];
    const char *r = record(), *p;
    sint32 ret = 0;
    int k, argc;
    sint16 n;

    checks++;
    if ( r[0] != 'B' ) { report( "a builtin QuakeSpasm did not call here", "", r ); vm->trap = QVM_ERR_BUILTIN; return; }
    p = word( r + 1, tok );
    if ( atoi( tok ) != vm->bnum ) {
        sprintf( line, "B %d", vm->bnum );
        report( "another builtin", line, r );
        vm->trap = QVM_ERR_BUILTIN;
        return;
    }
    for ( k = 0; k < 3; k++ ) {
        p = word( p, tok );
        vm->glob[QOFS_RETURN + k].i = hex( tok );
    }
    text[0] = 0;
    while ( *p == ' ' ) p++;
    if ( *p == '"' ) p = word( p, text );
    else if ( *p != ':' ) { p = word( p, tok ); ret = atol( tok ); }
    p = word( p, tok );
    p = word( p, tok );
    argc = atoi( tok );
    checks++;
    if ( argc != vm->argc ) { sprintf( line, "argc %d", vm->argc ); report( "a builtin's argument count", line, r ); }
    parameters( vm, p, r );
    cur++;

    switch ( vm->bnum ) {
    case 14:                                    /* spawn */
        n = ed_alloc( &w );
        checks++;
        if ( n != ret ) { sprintf( line, "edict %d", n ); report( "spawn's edict", line, r ); }
        vm->glob[QOFS_RETURN].i = qvm_entval( vm, n < 0 ? 0 : n );
        writes( 0 );
        return;
    case 15: case 69:                           /* remove, makestatic */
        ed_free( &w, (sint16) ent_num( vm->glob[QOFS_PARM0].i ) );
        writes( 0 );
        return;
    case 17: case 18: case 22: case 47:         /* checkclient, find, findradius, nextent */
        vm->glob[QOFS_RETURN].i = qvm_entval( vm, ret );
        break;
    case 19: case 20: case 68: case 75: case 76: case 77:   /* precache_*: their parameter back */
        checks++;
        if ( qvm_str_text( vm, vm->glob[QOFS_PARM0].i, line, sizeof line ) < 0 || strcmp( line, text ) )
            report( "a precache's return", line, r );
        vm->glob[QOFS_RETURN].i = vm->glob[QOFS_PARM0].i;
        break;
    case 26: case 27:                           /* ftos, vtos: the one temp */
        vm->glob[QOFS_RETURN].i = qvm_str_temp( vm, text );
        break;
    }
    writes( 1 );
}

/* ---- a map ---- */
static void world_field( const char *name, QVal v )
{
    const Def *d = find_def( fdefs, nfdefs, fcomp, name, 0 );

    if ( d ) qvm_ent_set( &m, 0, (sint16) d->ofs, v );
    else report( "no such field", name, "" );
}

static void load_map( const char *bsp, const char *dump )
{
    char *text, *d, *c, line[LINE];
    const char *base = strrchr( bsp, '/' ) ? strrchr( bsp, '/' ) + 1 : bsp;
    const Def *g;
    long len, i, nf = 0, ng = 0;
    sint16 err;
    QVal v;

    strncpy( mapname, base, sizeof mapname - 1 );
    if ( ( c = strstr( mapname, ".bsp" ) ) != 0 ) *c = 0;
    mapfails = 0;

    text = bsp_entities( bsp );
    d = load_file( dump, &len );
    if ( !text || !d ) { printf( "  FAIL  %s: cannot read %s\n", mapname, text ? dump : bsp ); fails++; return; }
    for ( nrec = 0, c = d; *c; c++ ) nrec += *c == '\n';
    rec = (char **) malloc( (size_t) nrec * sizeof *rec + 1 );
    for ( i = 0, c = d; i < nrec; i++ ) {
        rec[i] = c;
        c = strchr( c, '\n' );
        *c++ = 0;
    }
    cur = 0;

    if ( ( err = qvm_load( &m, &host, glob, sizeof glob ) ) != 0 || ( err = qvm_edicts( &m, MAX_EDICTS ) ) != 0 ) {
        sprintf( line, "error %d", err );
        report( "the image does not load", line, "" );
        return;
    }
    m.bi = builtins;
    m.nbi = NBUILTINS;
    words = (sint16) ( m.edsz / 4 );
    if ( !snap_ed ) snap_ed = (QVal *) malloc( (size_t) MAX_EDICTS * words * sizeof *snap_ed );

    for ( i = 1; i < nfdefs; i++ ) nf += !INSIDE( fcomp, &fdefs[i] );
    for ( i = 0; i < ngdefs; i++ ) ng += gdefs[i].ofs >= 28 && !INSIDE( gcomp, &gdefs[i] );
    sprintf( line, "F %ld G %ld", nf, ng );
    expect( line );

    if ( ( err = ed_init( &w, &m, MAX_EDICTS, freetime, isfree ) ) != 0 ) {
        sprintf( line, "error %d", err );
        report( "ed_init", line, "" );
        return;
    }
    w.time = 1.0f;
    w.hook = hook;

    /* SV_SpawnServer, before the entities */
    sprintf( line, "maps/%s.bsp", mapname );
    v.i = qvm_str_add( &m, line );  world_field( "model", v );
    v.f = 1.0f;                     world_field( "modelindex", v );
    v.f = 4.0f;                     world_field( "solid", v );          /* SOLID_BSP */
    v.f = 7.0f;                     world_field( "movetype", v );       /* MOVETYPE_PUSH */
    if ( ( g = find_def( gdefs, ngdefs, gcomp, "mapname", 28 ) ) != 0 ) glob[g->ofs].i = qvm_str_add( &m, mapname );

    if ( ( err = ed_load( &w, text ) ) != 0 ) {
        sprintf( line, "error %d, trap %d", err, m.trap );
        report( "ed_load", line, record() );
    }
    checks++;
    if ( cur < nrec ) report( "the dump goes on past the load", "", record() );
    printf( "        %s: %d edicts, %d map strings, %ld records\n", mapname, w.num, m.nmstr, nrec );

    qvm_free( &m );
    free( rec );
    free( d );
    free( text );
}

/* ---- cases no stock map reaches, each against id's answer. pak0's progs has no field-typed
   entity field, so ED_ParseEpair's "" for a field needs another image and is not here. No
   spawn function runs in any of them: the record is empty, so a builtin call fails. ---- */
static sint16 parsed_free;
static QVal parsed_ed[1024];        /* the last edict as parsed: an unspawnable one is freed, and a free clears angles */

static void note_parsed( EdWorld *wp, sint16 stage, sint16 n )
{
    sint16 k;

    (void) wp;
    if ( stage != ED_PARSED ) return;
    parsed_free = isfree[n];
    for ( k = 0; k < m.edsz / 4 && k < 1024; k++ ) parsed_ed[k] = qvm_ent_get( &m, qvm_entval( &m, n ), k );
}

static sint16 load_text( sint16 skill, const char *text )
{
    sint16 err = ed_init( &w, &m, MAX_EDICTS, freetime, isfree );

    if ( err ) return err;
    /* a fresh world: the parse leaves edict 0 as the server set it, so one case's world leaked into the next */
    if ( qvm_ent_zero( &m, 0 ) ) return -1;
    w.skill = skill;
    w.time = 1.0;
    w.hook = note_parsed;
    return ed_load( &w, text );
}

static void same( const char *what, double got, double want )
{
    checks++;
    if ( got == want ) return;
    fails++;
    printf( "  FAIL  constructed: %s: %g, id's %g\n", what, got, want );
}

/* a field of the last edict parsed */
static QVal field_of( const char *name, sint16 word )
{
    const Def *d = find_def( fdefs, nfdefs, fcomp, name, 0 );
    QVal v;

    v.i = 0x7FFFFFFFL;
    if ( !d ) { fails++; printf( "  FAIL  constructed: no field %s\n", name ); return v; }
    return parsed_ed[d->ofs + word];
}

#define GATED( flags )  "{ } { \"classname\" \"\" \"spawnflags\" \"" flags "\" }"

static void constructed( void )
{
    static const struct { const char *what; sint16 skill; const char *text; sint16 err; } cases[] = {
        { "a classname of \"\" runs function 0, which is fatal", 0, "{ } { \"classname\" \"\" }", ED_ERR_NULLFN },
        { "NOT_EASY on easy is freed before its spawn", 0, GATED( "256" ), 0 },
        { "NOT_EASY on normal spawns", 1, GATED( "256" ), ED_ERR_NULLFN },
        { "NOT_MEDIUM on normal is freed", 1, GATED( "512" ), 0 },
        { "NOT_MEDIUM on easy spawns", 0, GATED( "512" ), ED_ERR_NULLFN },
        { "NOT_HARD on hard is freed", 2, GATED( "1024" ), 0 },
        { "NOT_HARD on nightmare is freed", 3, GATED( "1024" ), 0 },
        { "NOT_HARD on normal spawns", 1, GATED( "1024" ), ED_ERR_NULLFN },
        { "an entity value past the edicts is EDICT_NUM's error", 0, "{ } { \"enemy\" \"600\" }", ED_ERR_ENTITY },
        { "a negative entity value too", 0, "{ } { \"enemy\" \"-1\" }", ED_ERR_ENTITY },
        { "a function naming nothing is a parse error", 0, "{ } { \"think\" \"no_such_function\" }", ED_ERR_VALUE },
        { "an angle of 32 bytes overruns id's temp (our refusal)", 0, "{ } { \"angle\" \"0123456789012345678901234567890X\" }", ED_ERR_LONG },
        { "an angle of 31 bytes fits it", 0, "{ } { \"angle\" \"0123456789012345678901234567890\" }", 0 },
        { "the skill filter runs on the world too", 0, "{ \"classname\" \"\" \"spawnflags\" \"256\" }", 0 },
        { "spawnflags -1 is inhibited at every skill", 1, GATED( "-1" ), 0 },
        { "a key naming no field is skipped", 0, "{ } { \"no_such_key\" \"x\" }", 0 },
        { "no opening brace", 0, "x", ED_ERR_BRACE },
        { "a closing brace for a value", 0, "{ } { \"a\" }", ED_ERR_BRACE },
        { "the text ending after a key", 0, "{ } { \"a\"", ED_ERR_EOF },
        { "the text ending after a value", 0, "{ } { \"a\" \"b\"", ED_ERR_EOF },
        { "a quoted \"}\" key ends the edict", 0, "{ } { \"}\"", 0 },
    };
    char line[LINE], big[600];
    sint16 err, i, a;

    strcpy( mapname, "constructed" );
    mapfails = 0;
    nrec = cur = 0;
    if ( ( err = qvm_load( &m, &host, glob, sizeof glob ) ) != 0 || ( err = qvm_edicts( &m, MAX_EDICTS ) ) != 0 ) {
        sprintf( line, "error %d", err );
        report( "the image does not load", line, "" );
        return;
    }
    m.bi = builtins;
    m.nbi = NBUILTINS;

    for ( i = 0; i < (sint16) ( sizeof cases / sizeof *cases ); i++ )
        same( cases[i].what, load_text( cases[i].skill, cases[i].text ), cases[i].err );

    same( "an entity value inside the edicts", load_text( 0, "{ } { \"enemy\" \"599\" }" ), 0 );
    same( "  is that edict", field_of( "enemy", 0 ).i, qvm_entval( &m, 599 ) );
    load_text( 0, "{ } { \"think\" \"SUB_Null\" }" );
    same( "a function by name", field_of( "think", 0 ).i != 0, 1 );
    same( "\"\" for a function is function 0", load_text( 0, "{ } { \"think\" \"SUB_Null\" \"think\" \"\" }" ), 0 );
    same( "  and stores it", field_of( "think", 0 ).i, 0 );
    load_text( 0, "{ } { \"angle\" \"90\" }" );
    same( "angle is angles' yaw", field_of( "angles", 1 ).f, 90.0 );
    load_text( 0, "{ } { \"angle \" \"90\" }" );
    same( "\"angle \" is compared before the trim, so it is no angle", field_of( "angles", 1 ).f, 0.0 );
    load_text( 0, "{ } { \"_note\" \"x\" }" );
    same( "an entity of _ keys alone is not free", parsed_free, 0 );
    load_text( 0, "{ } { }" );
    same( "an entity of no keys is", parsed_free, 1 );
    load_text( 0, "{ } { \"classname\" \"no_such_class\" }" );
    same( "a classname naming no function frees the edict", isfree[2], 1 );

    load_text( 0, "{ } { \"target\" \"\" }" );
    same( "\"\" for a string is QVM_STR_EMPTY, not null", field_of( "target", 0 ).i, QVM_STR_EMPTY );
    load_text( 0, "{ } { \"enemy\" \"599x\" }" );
    same( "an entity value is atoi's: 599x is 599", field_of( "enemy", 0 ).i, qvm_entval( &m, 599 ) );
    load_text( 0, "{ } { \"enemy\" \"5\" \"enemy\" \"abc\" }" );
    same( "a later key overwrites, and abc is the world", field_of( "enemy", 0 ).i, 0 );
    load_text( 0, "{ } { \"angle\" \"90 10\" }" );
    same( "an angle of two parts is \"0 90 10 0\": pitch 0", field_of( "angles", 0 ).f, 0.0 );
    same( "  yaw 90", field_of( "angles", 1 ).f, 90.0 );
    same( "  roll 10", field_of( "angles", 2 ).f, 10.0 );
    load_text( 0, "{ } { \"angle\" \"90\" \"angles\" \"1 2 3\" }" );
    same( "angles after angle overwrites it", field_of( "angles", 1 ).f, 2.0 );
    load_text( 0, "{ } { \"light\" \"300\" }" );
    same( "light is light_lev", field_of( "light_lev", 0 ).f, 300.0 );
    load_text( 0, "{ } { \"light \" \"300\" }" );
    same( "\"light \" is compared before the trim, so it is nothing", field_of( "light_lev", 0 ).f, 0.0 );
    load_text( 0, "{ } { \"health\" \"-0\" }" );
    same( "-0 keeps its sign bit", (double) (uint32) field_of( "health", 0 ).i, (double) 0x80000000UL );

    strcpy( big, "{ } { \"" );
    memset( big + 7, 'k', 255 );
    strcpy( big + 7 + 255, "\" \"1\" }" );
    same( "a key of 255 bytes fits id's keyname", load_text( 0, big ), 0 );
    memset( big + 7, 'k', 256 );
    strcpy( big + 7 + 256, "\" \"1\" }" );
    same( "a key of 256 bytes overruns it (our refusal)", load_text( 0, big ), ED_ERR_LONG );

    /* ED_Alloc outside the load */
    ed_init( &w, &m, MAX_EDICTS, freetime, isfree );
    w.time = 1.0;
    a = ed_alloc( &w );
    ed_free( &w, a );
    same( "a free younger than 2 seconds is reused at once", ed_alloc( &w ), a );
    ed_init( &w, &m, MAX_EDICTS, freetime, isfree );
    w.time = 2.6;
    a = ed_alloc( &w );
    ed_alloc( &w );
    ed_free( &w, a );
    w.time = 3.1;
    same( "3.1 less a free at 2.6f is past 0.5 in double, so reused", ed_alloc( &w ), a );
    ed_init( &w, &m, MAX_EDICTS, freetime, isfree );
    w.time = 2.5;
    a = ed_alloc( &w );
    ed_alloc( &w );
    ed_free( &w, a );
    w.time = 3.0;
    same( "a free exactly 0.5 ago is not reused", ed_alloc( &w ), a + 2 );
    ed_init( &w, &m, 4, freetime, isfree );
    w.time = 1.0;
    ed_alloc( &w );
    ed_alloc( &w );
    same( "no edict left", ed_alloc( &w ), -1 );
    ed_free( &w, 3 );
    same( "  but a free one is still reused", ed_alloc( &w ), 3 );

    qvm_free( &m );
}

int main( int argc, char **argv )
{
    sint32 h[21];
    int i;

    if ( argc < 4 || argc % 2 ) {
        fprintf( stderr, "usage: ed_load_test PROGSH.DAT MAP.BSP DUMP [MAP.BSP DUMP ...]\n" );
        return 2;
    }
    if ( !( img = (unsigned char *) load_file( argv[1], &img_len ) ) || img_len < (long) sizeof h ) {
        printf( "ed_load_test: FAILED (cannot read %s)\n", argv[1] );
        return 1;
    }
    memcpy( h, img, sizeof h );
    fdefs = read_defs( h[6], h[7], h[10] );
    gdefs = read_defs( h[4], h[5], h[10] );
    nfdefs = h[7];
    ngdefs = h[5];
    fcomp = components( fdefs, nfdefs, h[14] );
    gcomp = components( gdefs, ngdefs, h[13] );
    for ( i = 0; i < NBUILTINS; i++ ) builtins[i] = replay;
    ems_init();

    for ( i = 2; i < argc; i += 2 ) load_map( argv[i], argv[i + 1] );
    constructed();
    printf( "ed_load_test: %s (%ld checks, %ld failures)\n", fails ? "FAILED" : "PASSED", checks, fails );
    return fails != 0;
}
