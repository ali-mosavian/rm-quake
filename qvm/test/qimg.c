#include <string.h>

#include "qimg.h"

#ifndef __TURBOC__
#define _fmemcpy memcpy
#endif

static const char *fld_name[3] = { "frame", "nextthink", "think" };

static uint8 QFAR *img;
static sint32 img_len;

static void put( uint8 QFAR *buf, sint32 *at, void QFAR *src, sint32 n )
{
    _fmemcpy( buf + (uint16) *at, src, (uint16) n );
    *at += n;
}

sint32 qimg_build( const QImgSrc *src, uint8 QFAR *buf, sint32 cap )
{
    sint32 h[17], at, names = 0;
    sint16 k;
    QStat dummy;
    QFunc f;
    QDef d;

    for ( k = 0; k < 3; k++ )
        names += (sint32) strlen( fld_name[k] ) + 1;
    at = 68 + ( src->nst + 1 ) * 8 + 3 * 8 + src->nfn * 36 + src->nstr + names + src->ngl * 4 + src->nidx * 8;
    if ( at > cap || at > 65520L ) return 0;

    memset( h, 0, sizeof h );
    h[0] = src->version ? src->version : QVM_VERSION;
    at = 68;
    h[2] = at;  h[3] = src->nst + 1;
    memset( &dummy, 0, sizeof dummy );
    put( buf, &at, &dummy, 8 );
    put( buf, &at, src->st, src->nst * 8 );

    h[4] = h[6] = at;  h[7] = 3;
    names = src->nstr;
    for ( k = 0; k < 3; k++ ) {
        d.type = 2;                         /* ev_float */
        d.ofs = (uint16) ( 3 + k );
        d.s_name = names;
        names += (sint32) strlen( fld_name[k] ) + 1;
        put( buf, &at, &d, 8 );
    }

    h[8] = at;  h[9] = src->nfn;
    for ( k = 0; k < src->nfn; k++ ) {
        f = src->fn[k];
        if ( k > 0 && f.first >= 0 ) f.first++;
        put( buf, &at, &f, 36 );
    }

    h[10] = at;  h[11] = names;
    put( buf, &at, src->str, src->nstr );
    for ( k = 0; k < 3; k++ )
        put( buf, &at, (void QFAR *) fld_name[k], (sint32) strlen( fld_name[k] ) + 1 );

    h[12] = at;  h[13] = src->ngl;
    put( buf, &at, src->gl, src->ngl * 4 );
    h[14] = src->edwords;

    h[15] = at;  h[16] = src->nidx;
    put( buf, &at, src->idx, src->nidx * 8 );

    _fmemcpy( buf, h, 68 );
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
