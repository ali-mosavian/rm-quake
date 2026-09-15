#include <string.h>

#include "qimg.h"

#ifndef __TURBOC__
#define _fmemcpy memcpy
#endif

#define HDR    84
#define NAMES  256

static const char *fld_name[3] = { "frame", "nextthink", "think" };

static uint8 QFAR *img;
static sint32 img_len;

static void put( uint8 QFAR *buf, sint32 *at, void QFAR *src, sint32 n )
{
    _fmemcpy( buf + (uint16) *at, src, (uint16) n );
    *at += n;
}

/* by hash as unsigned, stably, then one entry a hash: the first def of a name, as qchash.py keeps */
static sint16 sort_names( QStrIdx *e, sint16 n )
{
    sint16 i, j, out = 0;
    QStrIdx t;

    for ( i = 1; i < n; i++ ) {
        t = e[i];
        for ( j = i; j > 0 && (uint32) e[j - 1].hash > (uint32) t.hash; j-- ) e[j] = e[j - 1];
        e[j] = t;
    }
    for ( i = 0; i < n; i++ )
        if ( !out || e[out - 1].hash != e[i].hash ) e[out++] = e[i];
    return out;
}

sint32 qimg_build( const QImgSrc *src, uint8 QFAR *buf, sint32 cap )
{
    static QStrIdx names[NAMES];
    sint32 h[21], at, fnames = 0;
    sint16 k, n;
    QStat dummy;
    QFunc f;
    QDef d;

    if ( src->nfn > NAMES ) return 0;
    for ( k = 0; k < 3; k++ )
        fnames += (sint32) strlen( fld_name[k] ) + 1;
    at = HDR + ( src->nst + 1 ) * 8 + 3 * 8 + src->nfn * 36 + src->nstr + fnames + src->ngl * 4 + src->nidx * 8
         + 3 * 8 + src->nfn * 8;
    if ( at > cap || at > 65520L ) return 0;

    memset( h, 0, sizeof h );
    h[0] = src->version ? src->version : QVM_VERSION;
    at = HDR;
    h[2] = at;  h[3] = src->nst + 1;
    memset( &dummy, 0, sizeof dummy );
    put( buf, &at, &dummy, 8 );
    put( buf, &at, src->st, src->nst * 8 );

    h[4] = h[6] = at;  h[7] = 3;
    fnames = src->nstr;
    for ( k = 0; k < 3; k++ ) {
        d.type = 2;                         /* ev_float */
        d.ofs = (uint16) ( 3 + k );
        d.s_name = fnames;
        fnames += (sint32) strlen( fld_name[k] ) + 1;
        put( buf, &at, &d, 8 );
    }

    h[8] = at;  h[9] = src->nfn;
    for ( k = 0; k < src->nfn; k++ ) {
        f = src->fn[k];
        if ( k > 0 && f.first >= 0 ) f.first++;
        put( buf, &at, &f, 36 );
    }

    h[10] = at;  h[11] = fnames;
    put( buf, &at, src->str, src->nstr );
    for ( k = 0; k < 3; k++ )
        put( buf, &at, (void QFAR *) fld_name[k], (sint32) strlen( fld_name[k] ) + 1 );

    h[12] = at;  h[13] = src->ngl;
    put( buf, &at, src->gl, src->ngl * 4 );
    h[14] = src->edwords;

    h[15] = at;  h[16] = src->nidx;
    put( buf, &at, src->idx, src->nidx * 8 );

    for ( k = 0; k < 3; k++ ) {
        names[k].hash = qvm_hash( fld_name[k] );
        names[k].ofs = k;
    }
    h[17] = at;  h[18] = sort_names( names, 3 );
    put( buf, &at, names, h[18] * 8 );

    for ( n = 0, k = 1; k < src->nfn; k++ ) {
        if ( src->fn[k].s_name <= 0 || src->fn[k].s_name >= src->nstr ) continue;
        names[n].hash = qvm_hash( src->str + (uint16) src->fn[k].s_name );
        names[n++].ofs = k;
    }
    h[19] = at;  h[20] = sort_names( names, n );
    put( buf, &at, names, h[20] * 8 );

    _fmemcpy( buf, h, HDR );
    return at;
}

void qimg_use( uint8 QFAR *buf, sint32 len )
{
    img = buf;
    img_len = len;
}

sint16 qimg_read( void QFAR *dst, sint32 ofs, uint16 n )
{
    if ( ofs < 0 || ofs + n > img_len ) return 1;
    _fmemcpy( dst, img + (uint16) ofs, n );
    return 0;
}
