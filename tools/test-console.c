#include <assert.h>
#include <string.h>

#include "cmd.h"
#include "fov.h"

static void test_cvars( void )
{
    CmdState state;
    CmdResult out;

    cmd_init( &state );
    assert( cvar_register( &state, "skill", "1", CVAR_ARCHIVE, 0.0f, 3.0f ) >= 0 );
    assert( cvar_register( &state, "fov", "90", CVAR_ARCHIVE, 10.0f, 170.0f ) >= 0 );

    assert( cmd_enqueue( &state, "skill; set fov \"110\"; echo \"two words\"" ) == 3 );
    assert( cmd_run( &state, &out ) == CMD_CVAR );
    assert( strcmp( out.text, "\"skill\" is \"1\"" ) == 0 );
    assert( cmd_run( &state, &out ) == CMD_CVAR );
    assert( cvar_value( &state, "fov" ) == 110.0f );
    assert( cmd_run( &state, &out ) == CMD_PRINT );
    assert( strcmp( out.text, "two words" ) == 0 );

    assert( cmd_enqueue( &state, "wait" ) == 1 );
    assert( cmd_run( &state, &out ) == CMD_WAIT );

    assert( cmd_enqueue( &state, "echo \"a;b\"; echo c" ) == 2 );
    assert( cmd_run( &state, &out ) == CMD_PRINT );
    assert( strcmp( out.text, "a;b" ) == 0 );
    assert( cmd_run( &state, &out ) == CMD_PRINT );
    assert( strcmp( out.text, "c" ) == 0 );

    assert( cmd_enqueue( &state, "set \"fov\" 100; echo \"one\" two" ) == 2 );
    assert( cmd_run( &state, &out ) == CMD_CVAR );
    assert( cvar_value( &state, "fov" ) == 100.0f );
    assert( cmd_run( &state, &out ) == CMD_PRINT );
    assert( strcmp( out.text, "one two" ) == 0 );

    assert( cmd_enqueue( &state, "skill 9" ) == 1 );
    assert( cmd_run( &state, &out ) == CMD_ERROR );
    assert( cvar_value( &state, "skill" ) == 1.0f );

    assert( cmd_enqueue( &state, "not_a_command" ) == 1 );
    assert( cmd_run( &state, &out ) == CMD_EXTERNAL );
    assert( strcmp( out.name, "not_a_command" ) == 0 );
}

static void test_bounds( void )
{
    CmdState state;
    char long_line[CMD_TEXT + 8];
    int i;

    cmd_init( &state );
    for ( i = 0; i < (int) sizeof(long_line) - 1; i++ ) long_line[i] = 'x';
    long_line[sizeof(long_line) - 1] = 0;
    assert( cmd_enqueue( &state, long_line ) == -1 );

    assert( cmd_enqueue( &state,
                         "echo 1;echo 2;echo 3;echo 4;echo 5;echo 6;echo 7;echo 8;echo 9" ) == -2 );
    assert( state.queue_count == 0 );

    for ( i = 0; i < CMD_QUEUE; i++ ) assert( cmd_enqueue( &state, "echo x" ) == 1 );
    assert( cmd_enqueue( &state, "echo overflow" ) == -2 );
}

int main( void )
{
    float vertical = fov_vertical( 90.0f, 4.0f / 3.0f );
    assert( vertical > 73.73f && vertical < 73.75f );
    assert( fov_horizontal( vertical, 4.0f / 3.0f ) > 89.99f );
    assert( fov_horizontal( vertical, 4.0f / 3.0f ) < 90.01f );
    test_cvars();
    test_bounds();
    return 0;
}
