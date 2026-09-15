#ifndef __QIMG_H__
#define __QIMG_H__

/*
 * qimg -- a progs.dat image assembled in memory from a test's own arrays,
 * and the host read over it, so a hand-written program goes through the
 * same loader and paging a real image does.
 *
 * The image puts id's dummy statement 0 before st[0] and shifts every
 * fn[k].first >= 0 (k >= 1) past it. It adds the fielddefs "frame",
 * "nextthink" and "think" at words 3, 4 and 5, named after str[]. It is
 * qchash.py's format -- the string index after the globals, then the field
 * and function name indexes, a function named by a nonzero s_name inside
 * str[] -- stamped with version, or QVM_VERSION when it is 0.
 */

#include "qvm.h"

typedef struct {
    QStat QFAR *st;  sint32 nst;
    QFunc QFAR *fn;  sint32 nfn;
    QVal  QFAR *gl;  sint32 ngl;
    char  QFAR *str; sint32 nstr;
    QStrIdx QFAR *idx; sint32 nidx;
    sint16 edwords;
    sint32 version;
} QImgSrc;

/* the image's length, 0 when it does not fit in cap */
sint32 qimg_build( const QImgSrc *src, uint8 QFAR *buf, sint32 cap );

/* the image qimg_read reads from */
void qimg_use( uint8 QFAR *buf, sint32 len );
sint16 qimg_read( void QFAR *dst, sint32 ofs, uint16 n );

#endif
