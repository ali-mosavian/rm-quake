#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "cmd.h"
#include "hud.h"
#include "input.h"

#define CON_LINES   16
#define CON_LINE    80
#define CON_WRAP    38
#define CON_HISTORY 8

typedef struct {
    short open;
    float height;
    short line_head, line_count, scroll;
    char  line[CON_LINES][CON_LINE];
    char  input[CON_LINE];
    short input_len;
    char  history[CON_HISTORY][CON_LINE];
    short history_count, history_pos; /* -1 is the live empty line */
    short mouse_x, mouse_y;
} Console;

void  con_init( Console CMD_FAR *con );
void  con_toggle( Console CMD_FAR *con, Input *input );
short con_open( const Console CMD_FAR *con );
void  con_animate( Console CMD_FAR *con, float dt, short screen_h );
void  con_key( Console CMD_FAR *con, CmdState CMD_FAR *cmd, short key, short shift );
void  con_print( Console CMD_FAR *con, const char *text );
void  con_clear( Console CMD_FAR *con );
void  con_draw( const Console CMD_FAR *con, Hud far *hud, QSurf dc, short w, short h );

#endif
