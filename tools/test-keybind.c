#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cmd.h"
#include "keybind.h"
#include "qgltypes.h"

int main( void )
{
    CmdState cmd;
    KeyBindings keys;
    CmdResult result;
    char error[64], binding[KB_TEXT], key_name[16];
    short first, second;
    char saved[4096], temp[68], backup[68];
    FILE *file;
    size_t saved_len;

    assert( sizeof(Keys) == 324 );

    cmd_init( &cmd );
    keybind_init( &keys );
    assert( keybind_set( &keys, "w", "+forward", error ) );
    assert( keybind_set( &keys, "SPACE", "+jump", error ) );
    assert( keybind_set( &keys, "CTRL", "+attack", error ) );
    assert( !keybind_set( &keys, "x", "+unknown", error ) );
    assert( keybind_get( &keys, "w", binding ) && !strcmp( binding, "+forward" ) );
    assert( keybind_name( KEY_SPCBAR, key_name ) && !strcmp( key_name, "SPACE" ) );
    keybind_find_command( &keys, "+forward", &first, &second );
    assert( first == KEY_W && second < 0 );
    assert( keybind_set( &keys, "UPARROW", "+forward", error ) );
    keybind_find_command( &keys, "+forward", &first, &second );
    assert( first == KEY_W && second == KEY_UP );
    keybind_unbind_command( &keys, "+forward" );
    keybind_find_command( &keys, "+forward", &first, &second );
    assert( first < 0 && second < 0 );
    assert( keybind_set( &keys, "w", "+forward", error ) );

    assert( keybind_set( &keys, "q", "+attack", error ) );
    keybind_event( &keys, &cmd, KEY_CTRL, 1 );
    keybind_event( &keys, &cmd, KEY_Q, 1 );
    keybind_event( &keys, &cmd, KEY_CTRL, 0 );
    assert( keybind_action( &keys, KA_ATTACK ) );
    keybind_event( &keys, &cmd, KEY_Q, 0 );
    assert( keybind_action( &keys, KA_ATTACK ) );
    keybind_tick( &keys );
    assert( !keybind_action( &keys, KA_ATTACK ) );

    keybind_event( &keys, &cmd, KEY_SPCBAR, 1 );
    keybind_event( &keys, &cmd, KEY_SPCBAR, 0 );
    assert( keybind_action( &keys, KA_JUMP ) );
    assert( keybind_record_state( &keys, KA_JUMP ) == 2 );
    keybind_tick( &keys );
    assert( !keybind_action( &keys, KA_JUMP ) );
    keybind_replay_state( &keys, KA_JUMP, 2 );
    assert( keybind_action( &keys, KA_JUMP ) );
    keybind_tick( &keys );
    assert( !keybind_action( &keys, KA_JUMP ) );

    assert( keybind_set( &keys, "MOUSE2", "+forward", error ) );
    assert( keybind_set( &keys, "MOUSE3", "+back", error ) );
    keybind_mouse( &keys, &cmd, 0, 1, 0 );
    assert( keybind_action( &keys, KA_FORWARD ) );
    keybind_mouse( &keys, &cmd, 0, 0, 0 );
    keybind_tick( &keys );
    keybind_mouse( &keys, &cmd, 0, 0, 1 );
    assert( keybind_action( &keys, KA_BACK ) );
    keybind_mouse( &keys, &cmd, 0, 0, 0 );
    keybind_tick( &keys );

    assert( keybind_set( &keys, "e", "+attack;echo fired", error ) );
    keybind_event( &keys, &cmd, KEY_E, 1 );
    assert( keybind_action( &keys, KA_ATTACK ) );
    assert( cmd_run( &cmd, &result ) == CMD_PRINT );
    assert( !strcmp( result.text, "fired" ) );
    keybind_event( &keys, &cmd, KEY_E, 0 );
    keybind_tick( &keys );
    assert( !keybind_action( &keys, KA_ATTACK ) );
    assert( keybind_command( &keys, "+forward", 1 ) );
    assert( keybind_action( &keys, KA_FORWARD ) );
    assert( keybind_command( &keys, "-forward", 0 ) );
    assert( !keybind_action( &keys, KA_FORWARD ) );

    keybind_event( &keys, &cmd, KEY_W, 1 );
    assert( keybind_action( &keys, KA_FORWARD ) );
    assert( keybind_set( &keys, "w", "echo rebound", error ) );
    assert( !keybind_action( &keys, KA_FORWARD ) );
    keybind_event( &keys, &cmd, KEY_W, 1 );
    assert( cmd_run( &cmd, &result ) == CMD_PRINT );
    assert( !strcmp( result.text, "rebound" ) );

    file = tmpfile();
    assert( file );
    assert( keybind_set( &keys, "w", "+forward", error ) );
    assert( keybind_set( &keys, "MOUSE1", "+attack", error ) );
    assert( keybind_set( &keys, "1", "impulse 2", error ) );
    assert( keybind_write( &keys, file ) );
    rewind( file );
    saved_len = fread( saved, 1, sizeof(saved) - 1, file );
    assert( saved_len > 0 );
    saved[saved_len] = 0;
    assert( strstr( saved, "bind \"w\" \"+forward\"" ) );
    assert( strstr( saved, "bind \"MOUSE1\" \"+attack\"" ) );
    assert( strstr( saved, "bind \"1\" \"impulse 2\"" ) );
    fclose( file );

    assert( cvar_register( &cmd, "skill", "2", CVAR_ARCHIVE, 0.0f, 3.0f ) >= 0 );
    assert( keybind_write_config( &keys, &cmd, "saved.tmp" ) );
    file = fopen( "saved.tmp", "rt" );
    assert( file ); fclose( file );
    assert( keybind_write_config( &keys, &cmd, "saved.old" ) );
    file = fopen( "saved.old", "rt" );
    assert( file ); fclose( file );
    assert( keybind_config_paths( temp, backup, sizeof(temp), "SAVED.BAK" ) );
    assert( strcmp( temp, "SAVED.BAK" ) && strcmp( backup, "SAVED.BAK" ) );
    assert( !strcmp( backup, "SAVED.old" ) );
    assert( keybind_write_config( &keys, &cmd, "SAVED.BAK" ) );
    remove( "saved.tmp" );
    remove( "saved.old" );
    remove( "SAVED.BAK" );

    return 0;
}
