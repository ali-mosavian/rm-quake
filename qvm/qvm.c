#include <string.h>

#include "qvm.h"

#ifndef QVM_COUNT
#define QVM_COUNT( op )
#endif

#ifndef __TURBOC__
#define _fmemset memset
#endif

/* Opcode numbers are progs.dat's, not ours -- see pr_comp.h. */
enum {
    OP_DONE, OP_MUL_F, OP_MUL_V, OP_MUL_FV, OP_MUL_VF, OP_DIV_F,
    OP_ADD_F, OP_ADD_V, OP_SUB_F, OP_SUB_V,
    OP_EQ_F, OP_EQ_V, OP_EQ_S, OP_EQ_E, OP_EQ_FNC,
    OP_NE_F, OP_NE_V, OP_NE_S, OP_NE_E, OP_NE_FNC,
    OP_LE, OP_GE, OP_LT, OP_GT,
    OP_LOAD_F, OP_LOAD_V, OP_LOAD_S, OP_LOAD_ENT, OP_LOAD_FLD, OP_LOAD_FNC,
    OP_ADDRESS,
    OP_STORE_F, OP_STORE_V, OP_STORE_S, OP_STORE_ENT, OP_STORE_FLD, OP_STORE_FNC,
    OP_STOREP_F, OP_STOREP_V, OP_STOREP_S, OP_STOREP_ENT, OP_STOREP_FLD, OP_STOREP_FNC,
    OP_RETURN,
    OP_NOT_F, OP_NOT_V, OP_NOT_S, OP_NOT_ENT, OP_NOT_FNC,
    OP_IF, OP_IFNOT,
    OP_CALL0, OP_CALL1, OP_CALL2, OP_CALL3, OP_CALL4,
    OP_CALL5, OP_CALL6, OP_CALL7, OP_CALL8,
    OP_STATE, OP_GOTO, OP_AND, OP_OR, OP_BITAND, OP_BITOR
};

/* A window already on the page costs a compare against the host's record
   and no call. A hit on a window this VM has not mapped since qvm_load
   answers base 0 and traps as QVM_ERR_EMS, never another page; it takes a
   host that kept a freed handle's key. Mapping every window at load to
   rule it out aliases one page into four, which a copying host corrupts. */
#define WKEY( h, pg ) ( (uint16) ( ( (uint16) (pg) << 8 ) | ( (uint16) (h) & 0xFF ) ) )
#define MAP( m, w, h, pg ) ( (m)->host->key[w] == WKEY( h, pg ) ? (m)->wbase[w] : win_map( m, w, h, (uint16) (pg) ) )

static uint8 QFAR *win_map( Qvm *m, sint16 w, sint16 h, uint16 pg )
{
    return m->wbase[w] = m->host->map( w, h, pg );
}

/* null or "": the same text, different bits */
#define STR_BLANK( v ) ( (v) == 0 || (v) == QVM_STR_EMPTY )

static sint16 ed_windows( Qvm *m );

/* a record is 36 bytes, QVM_FPP to a page, so none straddles one */
static QFunc QFAR *func_rec( Qvm *m, sint32 k )
{
    sint32 page = k / QVM_FPP;
    uint8 QFAR *pg = MAP( m, QVM_W_AUX, m->h_aux, page );
    return pg ? (QFunc QFAR *) ( pg + (uint16) ( k % QVM_FPP ) * 36 ) : 0;
}

/* text by offset into the lump; past it reads as the end */
static uint8 str_byte( Qvm *m, sint32 ofs )
{
    sint32 at;
    uint8 QFAR *pg;

    if ( ofs < 0 || ofs >= m->nstr ) return 0;
    at = m->strbase + ofs;
    pg = MAP( m, QVM_W_AUX, m->h_aux, at >> 14 );
    return pg ? pg[(uint16) at & 0x3FFF] : 0;
}

static sint16 str_is( Qvm *m, sint32 ofs, const char *s )
{
    uint8 c;

    do {
        c = str_byte( m, ofs++ );
        if ( c != (uint8) *s++ ) return 0;
    } while ( c );
    return 1;
}

/*
 * Places code functions whole on statement pages, in function order, and
 * with write set reads their statements in and rewrites each record's first
 * to its placement. Functions sharing a first share a placement. Answers
 * the page count, or -1 with m->trap-style error in *err.
 */
static sint32 pack( Qvm *m, sint32 ofs_st, sint16 write, sint16 *err )
{
    sint32 k, g, first, gfirst = -1, gstart = 0, place = 0, off = 0, n;
    uint16 page = 0;
    QFunc QFAR *f;
    uint8 QFAR *pg;

    for ( k = 0; k <= m->nfunc; k++ ) {
        if ( k < m->nfunc ) {
            if ( !( f = func_rec( m, k ) ) ) { *err = QVM_ERR_EMS; return -1; }
            first = f->first;
            if ( first <= 0 || first == gfirst ) continue;
            if ( first < gfirst || first >= m->nstat ) { *err = QVM_ERR_ORDER; return -1; }
        } else {
            first = m->nstat;
        }
        if ( gfirst > 0 ) {
            /* close the group at gfirst: its statements end where this one's begin */
            n = first - gfirst;
            if ( n * 8 > QVM_PAGE ) { *err = QVM_ERR_SIZE; return -1; }
            if ( ( off + n ) * 8 > QVM_PAGE ) { page++; off = 0; }
            place = ( (sint32) page << 16 ) | off;
            if ( write ) {
                if ( !( pg = MAP( m, QVM_W_ST, m->h_st, page ) ) ) { *err = QVM_ERR_EMS; return -1; }
                if ( m->host->read( pg + (uint16) off * 8, ofs_st + gfirst * 8, (uint16) ( n * 8 ) ) ) {
                    *err = QVM_ERR_READ; return -1;
                }
                for ( g = gstart; g < k; g++ ) {
                    f = func_rec( m, g );
                    if ( f->first == gfirst ) f->first = place;
                }
            }
            off += n;
        }
        gfirst = first;
        gstart = k;
    }
    return (sint32) page + 1;
}

/* reads n bytes of the image at ofs into consecutive aux pages from byte at */
static sint16 read_aux( Qvm *m, sint32 at, sint32 ofs, sint32 n )
{
    uint8 QFAR *pg;
    sint32 chunk;

    while ( n > 0 ) {
        chunk = QVM_PAGE - ( at & 0x3FFF );
        if ( chunk > n ) chunk = n;
        if ( !( pg = MAP( m, QVM_W_AUX, m->h_aux, at >> 14 ) ) ) return QVM_ERR_EMS;
        if ( m->host->read( pg + (uint16) ( at & 0x3FFF ), ofs, (uint16) chunk ) ) return QVM_ERR_READ;
        at += chunk; ofs += chunk; n -= chunk;
    }
    return 0;
}

sint16 qvm_load( Qvm *m, const QHost *host, QVal QFAR *glob, sint32 globcap )
{
    sint32 h[17], k, fpages;
    sint32 pages;
    sint16 err = 0;

    memset( m, 0, sizeof *m );
    m->host = host;
    if ( host->read( (void QFAR *) h, 0, sizeof h ) ) return QVM_ERR_READ;
    if ( h[0] != QVM_VERSION ) return QVM_ERR_VERSION;
    if ( h[13] * 4 > globcap || h[13] * 4 > QVM_LUMP_MAX || h[16] < 0 ) return QVM_ERR_SIZE;

    m->glob = glob;
    m->nglob = h[13];
    if ( host->read( (void QFAR *) glob, h[12], (uint16) ( h[13] * 4 ) ) ) return QVM_ERR_READ;

    m->nstat = h[3];
    m->nfunc = h[9];
    m->ofs_fld = h[6];
    m->nfld = h[7];
    m->nstr = h[11];
    m->nidx = h[16];
    m->edsz = (sint16) ( h[14] * 4 );

    /* function records QVM_FPP to a page, then the strings, then the index
       QVM_IPP to a page, each from a page boundary */
    fpages = ( m->nfunc + QVM_FPP - 1 ) / QVM_FPP;
    m->strbase = fpages * QVM_PAGE;
    m->idxbase = ( m->strbase + m->nstr + QVM_PAGE - 1 ) / QVM_PAGE * QVM_PAGE;
    if ( m->idxbase + m->nidx * 8 > QVM_MAXPAGES * QVM_PAGE ) return QVM_ERR_SIZE;
    if ( !( m->h_aux = host->alloc( m->idxbase + m->nidx * 8 ) ) ) return QVM_ERR_EMS;
    if ( m->h_aux > QVM_MAXHND ) { qvm_free( m ); return QVM_ERR_EMS; }
    for ( k = 0; k < m->nfunc && !err; k += QVM_FPP )
        err = read_aux( m, k / QVM_FPP * QVM_PAGE, h[8] + k * 36,
                        ( m->nfunc - k < QVM_FPP ? m->nfunc - k : QVM_FPP ) * 36L );
    if ( !err ) err = read_aux( m, m->strbase, h[10], m->nstr );
    if ( !err ) err = read_aux( m, m->idxbase, h[15], m->nidx * 8 );

    if ( !err && ( pages = pack( m, h[2], 0, &err ) ) > QVM_MAXPAGES ) err = QVM_ERR_SIZE;
    if ( !err && pages > 0 ) {
        m->st_pages = (uint16) pages;
        if ( !( m->h_st = host->alloc( pages * QVM_PAGE ) ) ) err = QVM_ERR_EMS;
        else if ( m->h_st > QVM_MAXHND ) err = QVM_ERR_EMS;
        else pack( m, h[2], 1, &err );
    }
    if ( err ) { qvm_free( m ); return err; }

    m->f_frame     = qvm_field( m, "frame" );
    m->f_nextthink = qvm_field( m, "nextthink" );
    m->f_think     = qvm_field( m, "think" );
    return 0;
}

void qvm_free( Qvm *m )
{
    if ( m->h_st )  m->host->release( m->h_st );
    if ( m->h_aux ) m->host->release( m->h_aux );
    if ( m->h_ed )  m->host->release( m->h_ed );
    m->h_st = m->h_aux = m->h_ed = 0;
}

sint16 qvm_edicts( Qvm *m, sint16 nedict )
{
    uint16 i;
    sint16 edpp;
    sint32 edpages;
    uint8 QFAR *pg;

    if ( m->edsz < 4 || m->edsz > QVM_PAGE || nedict < 1 ) return QVM_ERR_SIZE;
    edpp = (sint16) ( QVM_PAGE / m->edsz );
    edpages = ( nedict + edpp - 1 ) / edpp;
    if ( edpages > QVM_MAXPAGES ) return QVM_ERR_SIZE;
    m->edpp = edpp;
    m->edpages = (uint16) edpages;
    if ( m->h_ed ) m->host->release( m->h_ed );
    if ( !( m->h_ed = m->host->alloc( m->edpages * QVM_PAGE ) ) ) return QVM_ERR_EMS;
    if ( m->h_ed > QVM_MAXHND ) { m->host->release( m->h_ed ); m->h_ed = 0; return QVM_ERR_EMS; }
    for ( i = 0; i < m->edpages; i++ ) {
        if ( !( pg = MAP( m, QVM_W_EDX, m->h_ed, i ) ) ) return QVM_ERR_EMS;
        _fmemset( pg, 0, (uint16) QVM_PAGE );
    }
    m->nedict = nedict;
    return ed_windows( m );
}

/* ED0 mapped again, as nothing can be trusted to have left it. EDX is asked
   at every far access. */
static sint16 ed_windows( Qvm *m )
{
    if ( !( m->ed0 = MAP( m, QVM_W_ED0, m->h_ed, 0 ) ) ) return m->trap = QVM_ERR_EMS;
    return 0;
}

/* A page past the edicts is page 0, and an offset past the last place a
   vector fits in the window is 0: a STOREP through a garbage value writes
   inside the edict pages. A LOAD through a garbage entity plus its field
   can still read into the next window, as id's reads past its edicts. */
#define ED_MAX ( (uint16) ( QVM_PAGE - 12 ) )
#define ED_OFS( m, v ) ( ( (uint16) (v) & 0x3FFF ) > ED_MAX ? 0 : ( (uint16) (v) & 0x3FFF ) )

static QVal QFAR *ed_far( Qvm *m, sint32 v )
{
    uint16 pg = (uint16) ( v >> 16 );

    if ( pg >= m->edpages )
        return (QVal QFAR *) ( m->ed0 + ED_OFS( m, v ) );
    return (QVal QFAR *) ( MAP( m, QVM_W_EDX, m->h_ed, pg ) + ED_OFS( m, v ) );
}

#define ED( m, v ) ( (uint16) ( (v) >> 16 ) ? ed_far( m, v ) : (QVal QFAR *) ( (m)->ed0 + ED_OFS( m, v ) ) )

QVal qvm_ent_get( Qvm *m, sint32 ent, sint16 fld )
{
    QVal QFAR *e;
    QVal v;

    v.i = 0;
    if ( fld < 0 || fld >= m->edsz / 4 || ed_windows( m ) ) return v;
    e = ED( m, ent );
    return e[fld];
}

void qvm_ent_set( Qvm *m, sint32 ent, sint16 fld, QVal v )
{
    QVal QFAR *e;

    if ( fld < 0 || fld >= m->edsz / 4 || ed_windows( m ) ) return;
    e = ED( m, ent );
    e[fld] = v;
}

sint16 qvm_field( Qvm *m, char *name )
{
    QDef d;
    sint32 i;

    for ( i = 0; i < m->nfld; i++ ) {
        if ( m->host->read( (void QFAR *) &d, m->ofs_fld + i * 8, 8 ) ) return -1;
        if ( str_is( m, d.s_name, name ) ) return (sint16) d.ofs;
    }
    return -1;
}

sint16 qvm_func( Qvm *m, char *name )
{
    sint32 i, s_name;
    QFunc QFAR *f;

    for ( i = 1; i < m->nfunc; i++ ) {
        if ( !( f = func_rec( m, i ) ) ) return 0;
        s_name = f->s_name;                 /* the compare maps strings over the record */
        if ( str_is( m, s_name, name ) ) return (sint16) i;
    }
    return 0;
}

sint32 qvm_hash( const char QFAR *s )
{
    uint32 h = 0x811C9DC5UL;

    if ( !*s ) return QVM_STR_EMPTY;
    while ( *s ) h = ( h ^ (uint8) *s++ ) * 0x01000193UL;
    return (sint32) h;
}

sint32 qvm_str_temp( Qvm *m, const char QFAR *text )
{
    uint16 i;

    for ( i = 0; i + 1 < QVM_TEMP && text[i]; i++ ) m->temp[i] = text[i];
    m->temp[i] = 0;
    return QVM_STR_TEMP;
}

sint16 qvm_str_text( Qvm *m, sint32 str, char QFAR *buf, uint16 cap )
{
    sint32 lo = 0, hi = m->nidx - 1, mid, ofs;
    QStrIdx QFAR *e = 0;
    uint8 QFAR *pg;
    uint16 n;
    uint8 c;

    if ( !cap ) return -1;
    if ( str == 0 || str == QVM_STR_EMPTY ) { buf[0] = 0; return 0; }
    if ( str == QVM_STR_TEMP ) {
        for ( n = 0; n + 1 < cap && m->temp[n]; n++ ) buf[n] = m->temp[n];
        buf[n] = 0;
        return (sint16) n;
    }
    while ( lo <= hi ) {
        mid = ( lo + hi ) / 2;
        if ( !( pg = MAP( m, QVM_W_AUX, m->h_aux, ( m->idxbase >> 14 ) + mid / QVM_IPP ) ) ) return -1;
        e = (QStrIdx QFAR *) ( pg + (uint16) ( mid % QVM_IPP ) * 8 );
        if ( e->hash == str ) break;
        if ( (uint32) e->hash < (uint32) str ) lo = mid + 1;
        else hi = mid - 1;
    }
    if ( lo > hi ) return -1;
    ofs = e->ofs;                           /* the text maps over the index */
    for ( n = 0; n + 1 < cap && ( c = str_byte( m, ofs + n ) ) != 0; n++ ) buf[n] = c;
    buf[n] = 0;
    return (sint16) n;
}

/* A function's locals are a window in the SAME globals array, so a call
   saves that window and a return puts it back. */
static void qvm_enter( Qvm *m, QFunc *f, sint32 ret )
{
    QVal QFAR *l;
    sint16 i, j, o;

    if ( m->depth >= QVM_DEPTH )             { m->trap = QVM_ERR_DEPTH;  return; }
    if ( m->lused + f->locals > QVM_LOCALS ) { m->trap = QVM_ERR_LOCALS; return; }

    m->cstk[m->depth].ret = ret;
    m->cstk[m->depth].pstart = m->x_pstart;
    m->cstk[m->depth].locals = m->x_locals;
    m->depth++;

    l = &m->glob[(uint16) f->pstart];
    for ( i = 0; i < (sint16) f->locals; i++ )
        m->lstk[m->lused + i] = l[i].i;
    m->lused = (sint16) ( m->lused + f->locals );

    o = 0;
    for ( i = 0; i < (sint16) f->nparm; i++ )
        for ( j = 0; j < f->psz[i]; j++ )
            l[o++].i = m->glob[QOFS_PARM0 + i * 3 + j].i;

    m->x_pstart = f->pstart;
    m->x_locals = (sint16) f->locals;
}

static sint32 qvm_leave( Qvm *m )
{
    QVal QFAR *l = &m->glob[(uint16) m->x_pstart];
    sint16 i;

    m->lused = (sint16) ( m->lused - m->x_locals );
    for ( i = 0; i < m->x_locals; i++ )
        l[i].i = m->lstk[m->lused + i];

    m->depth--;
    m->x_pstart = m->cstk[m->depth].pstart;
    m->x_locals = m->cstk[m->depth].locals;
    return m->cstk[m->depth].ret;
}

/* Operands decode where a case reads them. They are UNSIGNED indices,
   while IF/IFNOT/GOTO read the same fields as signed displacements. */
#define OPA ( &G[(uint16) s->a] )
#define OPB ( &G[(uint16) s->b] )
#define OPC ( &G[(uint16) s->c] )

void qvm_run( Qvm *m, sint16 fnum )
{
    /* one declarator each: `QVal far *a, *b, *c` makes b and c near */
    QStat QFAR *s;
    QStat QFAR *sbase;
    QVal  QFAR *G = m->glob;
    QVal  QFAR *a;
    QVal  QFAR *b;
    QVal  QFAR *c;
    QVal  QFAR *e;
    QFunc QFAR *fr;
    QFunc f;
    uint16 stpage, idx;
    sint32 ret, runaway = QVM_RUNAWAY;
    sint16 exitdepth = m->depth;

    m->trap = 0;
    if ( ed_windows( m ) ) return;
    if ( fnum < 1 || fnum >= m->nfunc || !( fr = func_rec( m, fnum ) ) ) { m->trap = QVM_ERR_NULLFN; return; }
    f = *fr;
    if ( f.first < 0 ) { m->trap = QVM_ERR_BUILTIN; return; }
    qvm_enter( m, &f, 0L );
    if ( m->trap ) return;
    stpage = (uint16) ( f.first >> 16 );
    if ( !( sbase = (QStat QFAR *) MAP( m, QVM_W_ST, m->h_st, stpage ) ) ) { m->trap = QVM_ERR_EMS; goto unwind; }
    s = sbase + (uint16) f.first - 1;

    for ( ;; ) {
        s++;
        if ( !--runaway ) { m->trap = QVM_ERR_RUNAWAY; goto unwind; }
        QVM_COUNT( s->op );

        switch ( s->op ) {

        case OP_MUL_F:  OPC->f = OPA->f * OPB->f; break;
        case OP_DIV_F:  OPC->f = OPA->f / OPB->f; break;
        case OP_ADD_F:  OPC->f = OPA->f + OPB->f; break;
        case OP_SUB_F:  OPC->f = OPA->f - OPB->f; break;

        case OP_MUL_V:
            a = OPA; b = OPB;
            OPC->f = a[0].f*b[0].f + a[1].f*b[1].f + a[2].f*b[2].f;
            break;
        case OP_ADD_V:
            a = OPA; b = OPB; c = OPC;
            c[0].f=a[0].f+b[0].f; c[1].f=a[1].f+b[1].f; c[2].f=a[2].f+b[2].f;
            break;
        case OP_SUB_V:
            a = OPA; b = OPB; c = OPC;
            c[0].f=a[0].f-b[0].f; c[1].f=a[1].f-b[1].f; c[2].f=a[2].f-b[2].f;
            break;

        case OP_MUL_FV: a = OPB; b = OPA; goto scale;      /* the vector in a, the scalar in b */
        case OP_MUL_VF: a = OPA; b = OPB;
        scale:
            c = OPC;
            c[0].f=a[0].f*b->f; c[1].f=a[1].f*b->f; c[2].f=a[2].f*b->f;
            break;

        case OP_EQ_F:   OPC->f = (float) ( OPA->f == OPB->f ); break;
        case OP_NE_F:   OPC->f = (float) ( OPA->f != OPB->f ); break;
        case OP_LE:     OPC->f = (float) ( OPA->f <= OPB->f ); break;
        case OP_GE:     OPC->f = (float) ( OPA->f >= OPB->f ); break;
        case OP_LT:     OPC->f = (float) ( OPA->f <  OPB->f ); break;
        case OP_GT:     OPC->f = (float) ( OPA->f >  OPB->f ); break;

        case OP_EQ_V:
            a = OPA; b = OPB;
            OPC->f = (float) ( a[0].f==b[0].f && a[1].f==b[1].f && a[2].f==b[2].f );
            break;
        case OP_NE_V:
            a = OPA; b = OPB;
            OPC->f = (float) ( a[0].f!=b[0].f || a[1].f!=b[1].f || a[2].f!=b[2].f );
            break;

        /* a string is its text's hash, so equal text is equal bits, but for
           null and "": compared as floats a NaN would not equal itself */
        case OP_EQ_S:
            a = OPA; b = OPB;
            OPC->f = (float) ( a->i == b->i || ( STR_BLANK( a->i ) && STR_BLANK( b->i ) ) );
            break;
        case OP_NE_S:
            a = OPA; b = OPB;
            OPC->f = (float) !( a->i == b->i || ( STR_BLANK( a->i ) && STR_BLANK( b->i ) ) );
            break;

        case OP_EQ_E: case OP_EQ_FNC: OPC->f = (float) ( OPA->i == OPB->i ); break;
        case OP_NE_E: case OP_NE_FNC: OPC->f = (float) ( OPA->i != OPB->i ); break;

        /* A field offset is in 32-bit words from the start of the edict. */
        case OP_LOAD_F: case OP_LOAD_S: case OP_LOAD_ENT:
        case OP_LOAD_FLD: case OP_LOAD_FNC:
            a = OPA;
            e = ED( m, a->i );
            OPC->i = e[(uint16) OPB->i].i;
            break;

        case OP_LOAD_V:
            a = OPA;
            e = ED( m, a->i ) + (uint16) OPB->i;
            c = OPC;
            c[0].i = e[0].i; c[1].i = e[1].i; c[2].i = e[2].i;
            break;

        /* stays inside the edict's page: a page of edicts plus a field is under 16K */
        case OP_ADDRESS: OPC->i = OPA->i + OPB->i * 4; break;

        case OP_STORE_F: case OP_STORE_S: case OP_STORE_ENT:
        case OP_STORE_FLD: case OP_STORE_FNC:
            OPB->i = OPA->i;
            break;

        case OP_STORE_V:
            a = OPA; b = OPB;
            b[0].i=a[0].i; b[1].i=a[1].i; b[2].i=a[2].i;
            break;

        case OP_STOREP_F: case OP_STOREP_S: case OP_STOREP_ENT:
        case OP_STOREP_FLD: case OP_STOREP_FNC:
            b = OPB;
            e = ED( m, b->i );
            e->i = OPA->i;
            break;

        case OP_STOREP_V:
            b = OPB;
            e = ED( m, b->i );
            a = OPA;
            e[0].i=a[0].i; e[1].i=a[1].i; e[2].i=a[2].i;
            break;

        case OP_NOT_F:   OPC->f = (float) ( !OPA->f ); break;
        case OP_NOT_V:
            a = OPA;
            OPC->f = (float) ( !a[0].f && !a[1].f && !a[2].f );
            break;
        case OP_NOT_S:   OPC->f = (float) STR_BLANK( OPA->i ); break;
        case OP_NOT_ENT: case OP_NOT_FNC: OPC->f = (float) ( !OPA->i ); break;

        /* Tests the raw bits, as Quake does: -0.0 is nonzero here. Jumps stay
           inside their function, and so inside its page. */
        case OP_IF:    if (  OPA->i ) s += s->b - 1; break;
        case OP_IFNOT: if ( !OPA->i ) s += s->b - 1; break;
        case OP_GOTO:  s += s->a - 1; break;

        case OP_AND:    OPC->f = (float) ( OPA->f && OPB->f ); break;
        case OP_OR:     OPC->f = (float) ( OPA->f || OPB->f ); break;
        case OP_BITAND: OPC->f = (float) ( (sint32) OPA->f & (sint32) OPB->f ); break;
        case OP_BITOR:  OPC->f = (float) ( (sint32) OPA->f | (sint32) OPB->f ); break;

        case OP_CALL0: case OP_CALL1: case OP_CALL2: case OP_CALL3:
        case OP_CALL4: case OP_CALL5: case OP_CALL6: case OP_CALL7:
        case OP_CALL8:
            m->argc = (sint16) ( s->op - OP_CALL0 );
            a = OPA;
            if ( a->i <= 0 || a->i >= m->nfunc ) { m->trap = QVM_ERR_NULLFN; goto unwind; }
            if ( !( fr = func_rec( m, a->i ) ) ) { m->trap = QVM_ERR_EMS; goto unwind; }
            f = *fr;
            idx = (uint16) ( s - sbase );
            if ( f.first < 0 ) {                   /* negative => builtin */
                if ( -f.first >= m->nbi ) { m->trap = QVM_ERR_BUILTIN; goto unwind; }
                m->bi[(uint16) -f.first]( m );
                if ( m->trap ) goto unwind;
                /* the builtin may have taken any window */
                if ( ed_windows( m ) ) goto unwind;
                if ( !( sbase = (QStat QFAR *) MAP( m, QVM_W_ST, m->h_st, stpage ) ) ) {
                    m->trap = QVM_ERR_EMS; goto unwind;
                }
                s = sbase + idx;
                break;
            }
            qvm_enter( m, &f, ( (sint32) stpage << 16 ) | idx );
            if ( m->trap ) goto unwind;
            stpage = (uint16) ( f.first >> 16 );
            if ( !( sbase = (QStat QFAR *) MAP( m, QVM_W_ST, m->h_st, stpage ) ) ) {
                m->trap = QVM_ERR_EMS; goto unwind;
            }
            s = sbase + (uint16) f.first - 1;
            break;

        case OP_STATE:
            e = ED( m, G[QG_SELF].i );
            e[m->f_nextthink].f = G[QG_TIME].f + 0.1f;
            e[m->f_frame].f     = OPA->f;
            e[m->f_think].i     = OPB->i;
            break;

        case OP_DONE: case OP_RETURN:
            a = OPA;
            G[QOFS_RETURN    ].i = a[0].i;
            G[QOFS_RETURN + 1].i = a[1].i;
            G[QOFS_RETURN + 2].i = a[2].i;
            ret = qvm_leave( m );
            if ( m->depth == exitdepth ) return;
            stpage = (uint16) ( ret >> 16 );
            if ( !( sbase = (QStat QFAR *) MAP( m, QVM_W_ST, m->h_st, stpage ) ) ) {
                m->trap = QVM_ERR_EMS; goto unwind;
            }
            s = sbase + (uint16) ret;
            break;

        default:
            m->trap = QVM_ERR_OPCODE;
            goto unwind;
        }
    }

    /* a trap leaves frames and saved locals behind; the next run needs them gone */
unwind:
    while ( m->depth > exitdepth )
        qvm_leave( m );
}
