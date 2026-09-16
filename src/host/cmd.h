#ifndef __CMD_H__
#define __CMD_H__

#include <stdio.h>

#ifdef __BORLANDC__
#define CMD_FAR far
#else
#define CMD_FAR
#endif

#define CMD_CVARS  16
#define CMD_QUEUE  8
#define CMD_TEXT   256
#define CMD_NAME   16
#define CMD_VALUE  24
#define CMD_SCRIPTS 4

#define CVAR_ARCHIVE 1

typedef struct {
    char  name[CMD_NAME];
    char  string[CMD_VALUE];
    float value;
    float min_value, max_value;
    short flags;
} Cvar;

typedef struct {
    FILE *file;
    char line[CMD_TEXT];
    short pos;
} CmdScript;

typedef struct {
    Cvar  cvar[CMD_CVARS];
    short cvar_count;
    char  queue[CMD_QUEUE][CMD_TEXT];
    short queue_head, queue_tail, queue_count;
    CmdScript script[CMD_SCRIPTS];
    short script_count;
} CmdState;

typedef enum {
    CMD_NONE,
    CMD_PRINT,
    CMD_CVAR,
    CMD_CLEAR,
    CMD_CVARLIST,
    CMD_CMDLIST,
    CMD_WAIT,
    CMD_EXTERNAL,
    CMD_ERROR
} CmdKind;

typedef struct {
    char name[CMD_NAME];
    char text[CMD_TEXT];
    short argc;
} CmdResult;

void  cmd_init( CmdState CMD_FAR *state );
short cvar_register( CmdState CMD_FAR *state, const char *name, const char *value,
                     short flags, float min_value, float max_value );
short cvar_set( CmdState CMD_FAR *state, const char *name, const char *value, char *error );
float cvar_value( const CmdState CMD_FAR *state, const char *name );
short cvar_get( const CmdState CMD_FAR *state, short index, char *name, char *value, short *flags );
short cmd_enqueue( CmdState CMD_FAR *state, const char *text );
CmdKind cmd_run( CmdState CMD_FAR *state, CmdResult *result );
CmdKind cmd_run_cvar( CmdState CMD_FAR *state, CmdResult *result );
short cmd_take_queue( CmdState CMD_FAR *state, char *text );
short cmd_exec_file( CmdState CMD_FAR *state, const char *name, char *error );
void  cmd_shutdown( CmdState CMD_FAR *state );
short cmd_write_cvars( const CmdState CMD_FAR *state, FILE *file );

#endif
