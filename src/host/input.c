/*
 * input.c -- keyboard and mouse. C port of in_main.bas.
 *
 * in_init came from sys_init.bas in the original, the toggles from
 * r_main.bas where they sat beside the camera, and the screenshot key
 * from vid.bas, where it was being polled from inside the present
 * path -- in_main.bas is where all three ended up, and this is that
 * module's own slice of cport/.
 */

#include <stdio.h>
#include "qgl.h"
#include <stdlib.h>
#include <string.h>

#include "input.h"
#include "sys.h"

/* screen.bas, not yet ported. */
#include "screen.h"

/* Fatal on failure, same as v_init_ugl (vid.c) -- sys_error (sys.bas)
   isn't ported yet, and nothing else can run without a mouse anyway. */
void in_init( Input *input, QSurf h_video_dc )
{
    /* qgl clips the cursor to a rectangle rather than to a surface, so
       the mode's own size is read off the video surface and handed over
       -- qglSfSize answers the size, where mgl's uglDcSize answered one
       less and every caller added it back. */
    if ( !qglMouseInit( &input->mouse,
                        (short) ( qglSfSize( h_video_dc, 0 ) - 1 ),
                        (short) ( qglSfSize( h_video_dc, 1 ) - 1 ) ) ) {
        sys_error( "0x0006, Could not init mouse" );
    }

    qglKbdInit( &input->keyboard );
    qglTmrInit( 1000 );   /* 1 ms; sys_time_init measures what arrives */
}

void in_reset( Input *input )
{
    memset( &input->keyboard, 0, sizeof(input->keyboard) );
    input->mouse.left = input->mouse.right = 0;
}

short in_key_event( Input *input, short *shift, short *down )
{
    Keys *keys = &input->keyboard;
    unsigned char tail = keys->event_tail;
    unsigned short event;

    if ( tail == keys->event_head ) return 0;
    event = keys->event[tail];
    keys->event_tail = (unsigned char) ( ( tail + 1 ) & ( QGL_KEYQ - 1 ) );
    *shift = (short) ( event & 0x80 );
    *down = (short) ( event & 0x100 );
    return (short) ( event & 0x7f );
}

short in_event_overflow( Input *input )
{
    Keys *keys = &input->keyboard;
    if ( !keys->event_overflow ) return 0;
    keys->event_overflow = 0;
    keys->event_tail = keys->event_head;
    return -1;
}

InputRoute in_route_key( short menu_open, short console_open, short key )
{
    if ( menu_open ) return IN_ROUTE_MENU;
    if ( key == KEY_TILDE || key == KEY_OEM102 ) return IN_ROUTE_TOGGLE_CONSOLE;
    if ( console_open ) return IN_ROUTE_CONSOLE;
    return IN_ROUTE_GAME;
}

/* F5, not S: S walks backwards. */
void in_screenshot_key( Input *input, QSurf h_dst_dc, short w, short h )
{
    char name[16];

    if ( input->screenie_pending ) {
        sprintf( name, "scrn%d.bmp", input->screenie );
        scr_screenshot( name, h_dst_dc, w, h );
        input->screenie++;
        input->screenie_pending = 0;
    }
}

void in_screenshot( Input *input )
{
    input->screenie_pending = -1;
}
