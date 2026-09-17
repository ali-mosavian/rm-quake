#ifndef __KEYBIND_H__
#define __KEYBIND_H__

#include <stdio.h>

#include "cmd.h"

#ifdef __BORLANDC__
#define KB_FAR far
#else
#define KB_FAR
#endif

#define KB_KEYS 131
#define KB_TEXT 48

typedef enum {
    KA_FORWARD,
    KA_BACK,
    KA_MOVELEFT,
    KA_MOVERIGHT,
    KA_JUMP,
    KA_ATTACK,
    KA_SHOWSCORES,
    KA_COUNT
} KeyAction;

typedef struct {
    char binding[KB_KEYS][KB_TEXT];
    signed char owner[KB_KEYS];
    unsigned char held[KA_COUNT];
    unsigned char pressed[KA_COUNT];
    unsigned char forced[KA_COUNT];
    unsigned char manual[KA_COUNT];
    unsigned char mouse[3];
} KeyBindings;

void  keybind_init( KeyBindings KB_FAR *keys );
short keybind_keynum( const char *name );
short keybind_set( KeyBindings KB_FAR *keys, const char *name, const char *command,
                   char *error );
short keybind_get( const KeyBindings KB_FAR *keys, const char *name, char *command );
short keybind_name( short key, char *name );
void  keybind_find_command( const KeyBindings KB_FAR *keys, const char *command,
                            short *first, short *second );
void  keybind_unbind_command( KeyBindings KB_FAR *keys, const char *command );
void  keybind_unbind_all( KeyBindings KB_FAR *keys );
void  keybind_event( KeyBindings KB_FAR *keys, CmdState CMD_FAR *cmd, short key, short down );
void  keybind_mouse( KeyBindings KB_FAR *keys, CmdState CMD_FAR *cmd,
                     short left, short right, short middle );
void  keybind_release_all( KeyBindings KB_FAR *keys );
short keybind_action( const KeyBindings KB_FAR *keys, KeyAction action );
short keybind_record_state( const KeyBindings KB_FAR *keys, KeyAction action );
void  keybind_replay_state( KeyBindings KB_FAR *keys, KeyAction action, short state );
void  keybind_force( KeyBindings KB_FAR *keys, KeyAction action, short down );
short keybind_command( KeyBindings KB_FAR *keys, const char *command, short down );
void  keybind_tick( KeyBindings KB_FAR *keys );
short keybind_write( const KeyBindings KB_FAR *keys, FILE *file );
short keybind_config_paths( char *temp, char *backup, short size, const char *name );
short keybind_write_config( const KeyBindings KB_FAR *keys,
                            const CmdState CMD_FAR *cmd, const char *name );

#endif
