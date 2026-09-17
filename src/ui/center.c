/* center.c -- Quake centerprint line layout and real-time expiry. */

#include "center.h"

short center_count_lines( const char CENTER_FAR *text )
{
    short i, lines = 1;
    for ( i = 0; i < CENTER_TEXT_MAX && text[i]; i++ )
        if ( text[i] == '\n' ) lines++;
    return lines;
}

const char CENTER_FAR *center_next_line( const char CENTER_FAR *text, char *line )
{
    short n = 0;
    while ( *text && *text != '\n' && n < CENTER_LINE ) line[n++] = *text++;
    line[n] = 0;
    while ( *text && *text != '\n' ) text++;
    if ( *text == '\n' ) text++;
    return text;
}

short center_y( short lines, short height )
{
    if ( lines <= 4 ) return (short) ( height * 35L / 100L );
    return 48;
}

short center_step( void )
{
    return 8;
}

short center_advance( float *remaining, float frame_dt )
{
    if ( *remaining <= 0.0f ) return 0;
    *remaining -= frame_dt;
    if ( *remaining <= 0.0f ) { *remaining = 0.0f; return 0; }
    return -1;
}
