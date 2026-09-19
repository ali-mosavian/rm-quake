#ifndef __INPUT_H__
#define __INPUT_H__

#include "qgl.h"     /* QSurf */

#include "renderer.h"
#include "hud.h"

/*
 * Input -- mouse, keyboard, and the screenshot counter that rides
 * along with the F5 key. Caller-owned, one instance for the run: qglKbdInit
 * registers an interrupt handler that writes into &input->keyboard for
 * as long as the program runs, so this can't be a short-lived local --
 * see in_init.
 */
typedef struct {
    MouseInf mouse;
    Keys   keyboard;
    short screenie;    /* screenshot counter -- scrn0.bmp, scrn1.bmp, ... */
    short screenie_pending;
} Input;

typedef enum {
    IN_ROUTE_GAME,
    IN_ROUTE_MENU,
    IN_ROUTE_CONSOLE,
    IN_ROUTE_TOGGLE_CONSOLE
} InputRoute;

/*
 * name: in_init
 * desc: Mouse, keyboard and the one-second timer. h_video_dc is the
 *       video surface qglMouseInit takes its clip rect from -- Video's, not Input's
 *       own, so it's a parameter rather than a field here.
 */
void in_init( Input *input, QSurf h_video_dc );
void in_reset( Input *input );
short in_key_event( Input *input, short *shift, short *down );
short in_event_overflow( Input *input );
InputRoute in_route_key( short menu_open, short console_open, short key );

/*
 * name: in_screenshot_key
 * desc: Writes scrnNN.bmp while F5 is held. scr_screenshot is
 *       screen.c's (screen.h) -- w/h are the DC's own size, Config's
 *       x_res/y_res once Config exists.
 */
void in_screenshot_key( Input *input, QSurf h_dst_dc, short w, short h );
void in_screenshot( Input *input );

#endif
