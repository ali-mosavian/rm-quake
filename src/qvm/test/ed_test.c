/* ed_test.c -- ed.c against id's own code (idref.c): every token, unescape and vector of the maps'
 * entity text, then the cases no shipped map reaches.
 *
 *   ed_test MAP.BSP ...
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ed.h"
#include "bspents.h"

extern char com_token[1024];
char *COM_Parse( char *data );
void idref_newstring( char *string, char *new_ );
void idref_vector( char *s, float *d );

static int fails, checks;

static void fail( const char *what, const char *detail )
{
    printf( "  FAIL  %s: %s\n", what, detail );
    fails++;
}

/* ed_token and COM_Parse walk text side by side: same tokens, same resume points */
static void tokens_agree( const char *what, char *text )
{
    char tok[ED_TOKEN], msg[160];
    const char *mine = text;
    char *id = text;
    long n = 0;

    for ( ;; ) {
        mine = ed_token( mine, tok, sizeof tok );
        id = COM_Parse( id );
        checks++;
        if ( !mine != !id || strcmp( tok, com_token ) || ( id && mine != id ) ) {
            sprintf( msg, "token %ld: ed \"%.40s\" at %ld, id \"%.40s\" at %ld", n, tok,
                     mine ? (long) ( mine - text ) : -1L, com_token, id ? (long) ( id - text ) : -1L );
            fail( what, msg );
            return;
        }
        if ( !id ) return;
        n++;
    }
}

static void unescape_agrees( const char *what, char *s )
{
    char mine[ED_TOKEN], id[ED_TOKEN + 2], msg[200];

    ed_unescape( s, mine, sizeof mine );
    idref_newstring( s, id );
    checks++;
    if ( strcmp( mine, id ) ) {
        sprintf( msg, "\"%.40s\": ed \"%.40s\", id \"%.40s\"", s, mine, id );
        fail( what, msg );
    }
}

static int id_short( const char *s )     /* id's loop stepped past the end: fewer than three parts */
{
    const char *p = s;
    int i;

    for ( i = 0; i < 2; i++ ) {
        while ( *p && *p != ' ' ) p++;
        if ( !*p ) return 1;
        p++;
    }
    return 0;
}

static void vector_agrees( const char *what, char *s )
{
    float mine[3] = { 0, 0, 0 }, id[3];
    char msg[200];
    sint16 r = ed_vector( s, mine );

    idref_vector( s, id );
    checks++;
    if ( r != ( id_short( s ) ? -1 : 0 ) ) {
        sprintf( msg, "\"%.40s\": ed answers %d", s, r );
        fail( what, msg );
    } else if ( !r && memcmp( mine, id, sizeof mine ) ) {
        sprintf( msg, "\"%.40s\": ed %g %g %g, id %g %g %g", s, mine[0], mine[1], mine[2], id[0], id[1], id[2] );
        fail( what, msg );
    }
}

/* every value token: its unescape, and as a vector */
static void values_agree( const char *what, char *text )
{
    char tok[ED_TOKEN];
    const char *p = text;
    int n = 0;

    while ( ( p = ed_token( p, tok, sizeof tok ) ) != 0 ) {
        if ( tok[0] == '{' || tok[0] == '}' ) { n = 0; continue; }
        if ( n++ & 1 ) {
            unescape_agrees( what, tok );
            vector_agrees( what, tok );
        }
    }
}

int main( int argc, char **argv )
{
    static char *edges[] = {
        "", "   ", "{\"a\" \"b\"}", "// comment\n\"x\" y", "// only a comment", "word{x}", "a:b(c)'d'",
        "\"\"", "x\x8dy \x8d z", "{\n\"classname\" \"light\"\n}\n", "tab\tsep",
    };
    static char *strings[] = {
        "plain", "a\\nb", "a\\xb", "trail\\", "\\\\n", "\\", "\\n\\n", "",
    };
    static char *vectors[] = {
        "1 2 3", "1  2 3", "1 2", "", "1 2 3 4", "-8.5 0.1 1e3", " 1 2 3", "1 2 ", "12",
    };
    int i;

    for ( i = 1; i < argc; i++ ) {
        char *text = bsp_entities( argv[i] );
        if ( !text ) { fail( argv[i], "cannot read the entities lump" ); continue; }
        tokens_agree( argv[i], text );
        values_agree( argv[i], text );
        free( text );
    }
    for ( i = 0; i < (int) ( sizeof edges / sizeof *edges ); i++ ) tokens_agree( "edge tokens", edges[i] );
    {
        /* the one place ed.c leaves id: an unterminated quote. id's COM_Parse resumes past the
           text's end and reads on; ed_token stops at the end. The token itself is id's. */
        static char text[] = "\"unterminated";
        char tok[ED_TOKEN];
        const char *at = ed_token( text, tok, sizeof tok );

        COM_Parse( text );
        checks += 3;
        if ( strcmp( tok, com_token ) ) fail( "unterminated quote", "token differs from id's" );
        if ( at != text + strlen( text ) ) fail( "unterminated quote", "does not stop at the end" );
        if ( ed_token( at, tok, sizeof tok ) ) fail( "unterminated quote", "a token past the end" );
    }
    for ( i = 0; i < (int) ( sizeof strings / sizeof *strings ); i++ ) unescape_agrees( "edge strings", strings[i] );
    for ( i = 0; i < (int) ( sizeof vectors / sizeof *vectors ); i++ ) vector_agrees( "edge vectors", vectors[i] );

    printf( "ed_test: %s (%d checks, %d failures)\n", fails ? "FAILED" : "PASSED", checks, fails );
    return fails != 0;
}
