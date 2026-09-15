#include <stdio.h>
#include <stdlib.h>

#include "bspents.h"

char *bsp_entities( const char *path )
{
    FILE *f = fopen( path, "rb" );
    unsigned char h[8];
    long ofs, len;
    char *text = 0;

    if ( !f ) return 0;
    if ( !fseek( f, 4, SEEK_SET ) && fread( h, 1, 8, f ) == 8 ) {
        ofs = h[0] | h[1] << 8 | (long) h[2] << 16 | (long) h[3] << 24;
        len = h[4] | h[5] << 8 | (long) h[6] << 16 | (long) h[7] << 24;
        text = (char *) calloc( (size_t) len + 1, 1 );
        if ( text && ( fseek( f, ofs, SEEK_SET ) || fread( text, 1, (size_t) len, f ) != (size_t) len ) ) {
            free( text );
            text = 0;
        }
    }
    fclose( f );
    return text;
}
