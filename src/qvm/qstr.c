#include <string.h>

#include "qvmi.h"

#ifndef __TURBOC__
#define _fmemmove memmove
#define _fmemcpy  memcpy
#endif

/* byte ofs of a text in the len bytes from byte base of handle hnd; past them reads as the end */
static uint8 text_byte( Qvm *m, sint16 hnd, sint32 base, sint32 len, sint32 ofs )
{
    sint32 at;
    uint8 QFAR *pg;

    if ( ofs < 0 || ofs >= len ) return 0;
    at = base + ofs;
    pg = MAP( m, QVM_W_AUX, hnd, at >> 14 );
    return pg ? pg[(uint16) at & 0x3FFF] : 0;
}

static sint16 text_is( Qvm *m, sint16 hnd, sint32 base, sint32 len, sint32 ofs, const char QFAR *s )
{
    uint8 c;

    do {
        c = text_byte( m, hnd, base, len, ofs++ );
        if ( c != (uint8) *s++ ) return 0;
    } while ( c );
    return 1;
}

static sint16 text_copy( Qvm *m, sint16 hnd, sint32 base, sint32 len, sint32 ofs, char QFAR *buf, uint16 cap )
{
    uint16 n;
    uint8 c;

    for ( n = 0; n + 1 < cap && ( c = text_byte( m, hnd, base, len, ofs + n ) ) != 0; n++ ) buf[n] = c;
    buf[n] = 0;
    return (sint16) n;
}

sint16 qvm_str_is( Qvm *m, sint32 ofs, const char *s )
{
    return text_is( m, m->h_aux, m->strbase, m->nstr, ofs, s );
}

QStrIdx QFAR *qvm_idx_find( Qvm *m, sint16 hnd, sint32 base, sint32 n, sint32 key )
{
    sint32 lo = 0, hi = n - 1, mid, at;
    QStrIdx QFAR *e;
    uint8 QFAR *pg;

    while ( lo <= hi ) {
        mid = ( lo + hi ) / 2;
        at = base + mid * 8;
        if ( !( pg = MAP( m, QVM_W_AUX, hnd, at >> 14 ) ) ) return 0;
        e = (QStrIdx QFAR *) ( pg + ( (uint16) at & 0x3FFF ) );
        if ( e->hash == key ) return e;
        if ( (uint32) e->hash < (uint32) key ) lo = mid + 1;
        else hi = mid - 1;
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
    QStrIdx QFAR *e;
    uint16 n;

    if ( !cap ) return -1;
    if ( str == 0 || str == QVM_STR_EMPTY ) { buf[0] = 0; return 0; }
    if ( str == QVM_STR_TEMP ) {
        for ( n = 0; n + 1 < cap && m->temp[n]; n++ ) buf[n] = m->temp[n];
        buf[n] = 0;
        return (sint16) n;
    }
    /* each text maps over its index */
    if ( ( e = qvm_idx_find( m, m->h_aux, m->idxbase, m->nidx, str ) ) != 0 )
        return text_copy( m, m->h_aux, m->strbase, m->nstr, e->ofs, buf, cap );
    if ( ( e = qvm_idx_find( m, m->h_mstr, 0, m->nmstr, str ) ) != 0 )
        return text_copy( m, m->h_mstr, 0, QVM_PAGE, e->ofs, buf, cap );
    return -1;
}

/* values no text may take: null, the temp, "", and -0.0, which AND and OR read as false */
static sint16 reserved( sint32 h )
{
    return h == 0 || h == QVM_STR_TEMP || h == QVM_STR_EMPTY || h == (sint32) 0x80000000UL;
}

sint32 qvm_str_add( Qvm *m, const char QFAR *text )
{
    QStrIdx QFAR *e;
    uint8 QFAR *pg;
    sint32 h, ofs, len;
    uint16 lo, hi, mid, n;

    if ( !*text ) return QVM_STR_EMPTY;
    h = qvm_hash( text );
    if ( reserved( h ) ) return 0;
    if ( ( e = qvm_idx_find( m, m->h_aux, m->idxbase, m->nidx, h ) ) != 0 ) {
        ofs = e->ofs;                       /* the text maps over the index */
        return text_is( m, m->h_aux, m->strbase, m->nstr, ofs, text ) ? h : 0;
    }
    for ( n = 1; text[n - 1]; n++ )
        if ( n >= QVM_PAGE ) return 0;
    len = n;

    if ( !m->h_mstr ) {
        if ( !( m->h_mstr = m->host->alloc( QVM_PAGE ) ) ) return 0;
        if ( m->h_mstr > QVM_MAXHND ) { m->host->release( m->h_mstr ); m->h_mstr = 0; return 0; }
        m->nmstr = 0;
        m->mstr_low = (uint16) QVM_PAGE;
    }
    if ( !( pg = MAP( m, QVM_W_AUX, m->h_mstr, 0 ) ) ) return 0;
    lo = 0;
    hi = m->nmstr;
    while ( lo < hi ) {                     /* the first entry not below h, as unsigned */
        mid = ( lo + hi ) / 2;
        if ( (uint32) ( (QStrIdx QFAR *) ( pg + mid * 8 ) )->hash < (uint32) h ) lo = mid + 1;
        else hi = mid;
    }
    e = (QStrIdx QFAR *) ( pg + lo * 8 );
    if ( lo < m->nmstr && e->hash == h )
        return text_is( m, m->h_mstr, 0, QVM_PAGE, e->ofs, text ) ? h : 0;
    if ( (sint32) ( m->nmstr + 1 ) * 8 + len > m->mstr_low ) return 0;
    _fmemmove( e + 1, e, (uint16) ( m->nmstr - lo ) * 8 );
    m->mstr_low = (uint16) ( m->mstr_low - len );
    _fmemcpy( pg + m->mstr_low, text, (uint16) len );
    e->hash = h;
    e->ofs = m->mstr_low;
    m->nmstr++;
    return h;
}
