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

short in_key_event( Input *input, short *shift )
{
    Keys *keys = &input->keyboard;
    unsigned char tail = keys->event_tail;
    short key;

    if ( tail == keys->event_head ) return 0;
    key = keys->event[tail];
    keys->event_tail = (unsigned char) ( ( tail + 1 ) & ( QGL_KEYQ - 1 ) );
    *shift = (short) ( key & 0x80 );
    return (short) ( key & 0x7f );
}

void in_handle_key( short key, Renderer *rdr, Camera *cam, Player *player, Hud far *hud )
{
    if ( key == KEY_F1 ) rdr->use_mips = !rdr->use_mips;

    /* Perspective / wireframe only -- affine dropped with the fan path
       that was its only renderer (uglPolyTP has no affine equivalent). */
    if ( key == KEY_F2 ) rdr->rend_mode = ( rdr->rend_mode == 0 ) ? 2 : 0;
    if ( key == KEY_F3 ) cam->fps_view    = !cam->fps_view;
    if ( key == KEY_F12 ) hud->stats       = !hud->stats;
    if ( key == KEY_B ) rdr->backface    = !rdr->backface;
    if ( key == KEY_L ) rdr->lightmap    = !rdr->lightmap;
    if ( key == KEY_P ) rdr->portal      = !rdr->portal;
    if ( key == KEY_O ) hud->portal_wire = !hud->portal_wire;
    if ( key == KEY_F4 ) player->no_clip  = !player->no_clip;
}

/* F5, not S: S walks backwards. */
void in_screenshot_key( Input *input, QSurf h_dst_dc, short w, short h )
{
    char name[16];

    if ( input->keyboard.k[KEY_F5] ) {
        sprintf( name, "scrn%d.bmp", input->screenie );
        scr_screenshot( name, h_dst_dc, w, h );
        input->screenie++;
    }
}
