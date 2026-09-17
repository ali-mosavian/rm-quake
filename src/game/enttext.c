/* enttext.c -- bounded strings from the map entity pool. */

#include "enttext.h"

void ent_copy_map( char *dst, const char ENTTEXT_FAR *src )
{
    short i, last = -1;

    for ( i = 0; i < 8 && src[i]; i++ ) {
        dst[i] = src[i];
        if ( src[i] != ' ' ) last = i;
    }
    dst[last + 1] = '\0';
}
