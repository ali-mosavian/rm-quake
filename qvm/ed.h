#ifndef __ED_H__
#define __ED_H__

/*
 * ed -- id's entity text, read as pr_edict.c and common.c read it: the
 * tokens of COM_Parse, ED_NewString's unescape and ED_ParseEpair's vector.
 */

#include "qtypes.h"

#ifndef QFAR
#ifdef __TURBOC__
#define QFAR far
#else
#define QFAR
#endif
#endif

#define ED_TOKEN   1024           /* com_token's size */

/*
 * name: ed_token
 * desc: COM_Parse: the next token of data into tok, cut to cap - 1 bytes.
 *       Answers where parsing goes on, 0 at the end of the text. A quoted
 *       token has no escapes and ends at a quote or the end of the text;
 *       unterminated, parsing stops at that end, where id's goes on past it.
 */
const char QFAR *ed_token( const char QFAR *data, char *tok, uint16 cap );

/*
 * name: ed_unescape
 * desc: ED_NewString's text into out, cut to cap - 1 bytes: a backslash
 *       and 'n' is a newline, a backslash and anything else is a
 *       backslash, the other character dropped; a final backslash stays.
 *       Answers the length.
 */
uint16 ed_unescape( const char *s, char *out, uint16 cap );

/*
 * name: ed_vector
 * desc: ED_ParseEpair's vector: three atof values split at single spaces,
 *       so two spaces make a 0. Answers 0, or -1 where id would read past
 *       the value's end: fewer than three parts, or past its 128 bytes.
 */
sint16 ed_vector( const char *s, float *v );

#endif
