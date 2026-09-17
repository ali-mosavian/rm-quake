#include <assert.h>
#include <string.h>

#include "center.h"
#include "enttext.h"

int main( void )
{
    const char *text = "one\n\n0123456789012345678901234567890123456789TAIL\nfour\nfive";
    const char *p = text;
    char line[CENTER_LINE + 1];
    char map[9] = "????????";
    char source[6] = { 'e', '1', 'm', '1', 0, 'X' };
    float remaining = 2.0f;

    assert( center_count_lines( text ) == 5 );
    p = center_next_line( p, line ); assert( !strcmp( line, "one" ) );
    p = center_next_line( p, line ); assert( !strcmp( line, "" ) );
    p = center_next_line( p, line );
    assert( !strcmp( line, "0123456789012345678901234567890123456789" ) );
    p = center_next_line( p, line ); assert( !strcmp( line, "four" ) );
    center_next_line( p, line ); assert( !strcmp( line, "five" ) );

    assert( center_y( 4, 200 ) == 70 );
    assert( center_y( 5, 200 ) == 48 );
    assert( center_step() == 8 );
    assert( center_advance( &remaining, 0.5f ) && remaining == 1.5f );
    assert( !center_advance( &remaining, 1.5f ) && remaining == 0.0f );

    ent_copy_map( map, source );
    assert( !strcmp( map, "e1m1" ) );
    assert( map[5] == '?' );
    return 0;
}
