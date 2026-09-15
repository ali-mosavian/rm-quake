#include <stdlib.h>
#include <string.h>

#include "ed.h"

static sint16 is_single( char c )
{
    return c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':';
}

const char QFAR *ed_token( const char QFAR *data, char *tok, uint16 cap )
{
    uint16 len = 0;
    char c;

    tok[0] = 0;
    if ( !data ) return 0;
    for ( ;; ) {
        while ( (signed char) ( c = *data ) <= ' ' ) {    /* signed, as id's: a byte past 0x7F is white space */
            if ( !c ) return 0;
            data++;
        }
        if ( c != '/' || data[1] != '/' ) break;
        while ( *data && *data != '\n' ) data++;
    }
    if ( c == '"' ) {
        data++;
        for ( ;; ) {
            c = *data++;
            if ( c == '"' || !c ) {
                tok[len] = 0;
                return c ? data : data - 1;
            }
            if ( len + 1 < cap ) tok[len++] = c;
        }
    }
    if ( is_single( c ) ) {
        tok[0] = c;
        tok[1] = 0;
        return data + 1;
    }
    do {
        if ( len + 1 < cap ) tok[len++] = c;
        c = *++data;
    } while ( !is_single( c ) && (signed char) c > 32 );
    tok[len] = 0;
    return data;
}

uint16 ed_unescape( const char *s, char *out, uint16 cap )
{
    uint16 i, n = 0, l = (uint16) strlen( s ) + 1;
    char c;

    for ( i = 0; i < l; i++ ) {
        c = s[i];
        if ( c == '\\' && i < l - 1 ) {
            i++;
            c = s[i] == 'n' ? '\n' : '\\';
        }
        if ( c && n + 1 < cap ) out[n++] = c;
    }
    out[n] = 0;
    return n;
}

sint16 ed_vector( const char *s, float *v )
{
    char string[128];
    char *p, *w;
    sint16 i;

    if ( strlen( s ) >= sizeof string ) return -1;
    strcpy( string, s );
    p = w = string;
    for ( i = 0; i < 3; i++ ) {
        while ( *p && *p != ' ' ) p++;
        if ( !*p && i < 2 ) return -1;      /* id's next part starts past the end */
        *p = 0;
        v[i] = (float) atof( w );
        w = p = p + 1;
    }
    return 0;
}
