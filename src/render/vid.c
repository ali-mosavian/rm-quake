/*
 * vid.c -- video mode, back buffer and page flip.
 *
 * C port of the old vid.bas, for the standalone (no-BASIC-runtime)
 * renderer. Earlier drafts of this file lived in src/vid.c, glued into
 * the still-BASIC-linked build via measured offsets into a live Game
 * struct -- abandoned once the project moved to a full rewrite with
 * its own build, since there's no Game to reach into any more and no
 * BASIC caller to hand a status code back to.
 */

#include "video.h"
#include "qgl.h"
#include "pal.h"
#include "sys.h"
#include <stdio.h>
#include <stdlib.h>

/* ugl.bi: UGL.MEM% = 0*DCTSIZE, UGL.EMS% = 2*DCTSIZE, DCTSIZE% = 64. */

/*
 * name: v_init_ugl
 * desc: Brings qgl up: the allocator, the surface layer, then EMS.
 *       In that order -- the surface layer allocates, and the EMS
 *       probe reports what it found rather than failing, since a
 *       machine with no EMS still renders from conventional memory.
 *       Fatal on failure of the first two; nothing else can run.
 */
void v_init_ugl( void )
{
    qglMemInit();
    if ( qglSfInit() == 0 ) {
        sys_error( "0x0000, Could not init qgl" );
    }
    qglGemInit();
}

/*
 * name: v_init
 * desc: Opens the video mode, allocates the backbuffer (and -comp's
 *       composite buffer) and paints the border once. v owns the
 *       result: h_video_dc/h_comp_dc/h_back_bdc are written back into
 *       it, matching the caller's own expectation of what "init"
 *       leaves behind. Fatal on failure, same reasoning as
 *       v_init_ugl -- nothing downstream can run without a mode.
 */
void v_init( Video *v, long pal )
{
    /* qgl owns the mode: one call takes it and hands back the screen
       surface, whose shape is the mode's own (320x200x8). There is no
       page count -- qgl does not page, and v_present blits. */
    v->h_video_dc = qglVgaInit();
    if ( v->h_video_dc == 0 ) {
        sys_error( "0x0001, Could not set video mode" );
    }

    {
        v->h_back_bdc = qglSfNew( v->x_res, v->y_res, QGL_SURF_CMEM );
        if ( v->h_back_bdc == 0 ) {
            sys_error( "0x0002, Could not create a backbuffer" );
        }

        if ( v->comp ) {
            /* EMS, and it has to be. A screen-sized composite is 64,000
               bytes; in conventional memory that is enough on its own to
               stop e1m1 loading -- it dies on texinf.bld, a 16 KB
               allocation, with this buffer in the way, and loads without
               it. This was briefly QGL_SURF_CMEM as a workaround for the
               live-input crash (docs/bugs/); it did not fix that crash
               and it cost the map the whole port was meant to unblock,
               so it is the wrong trade twice over. */
            v->h_comp_dc = qglSfNew( v->scr_x_res, v->scr_y_res, QGL_SURF_EMS );
            if ( v->h_comp_dc == 0 ) {
                sys_error( "0x0003, Could not create the composite buffer" );
            }
        }
        v->base_x_res = v->x_res;
        v->base_y_res = v->y_res;
        v->h_view = qglSfViewNew( v->h_back_bdc, v->base_x_res,
                                  v->base_y_res, v->base_x_res );
        if ( !v->h_view ) sys_error( "0x0004, Could not create the display view" );
        v_set_viewsize( v, 100 );
    }

    qglDrFill( v->h_video_dc, 0, 0, v->scr_x_res - 1, v->scr_y_res - 1, 0L );

    /* uglPalSet/qglMemFree both declare their far-pointer-shaped parameter
       with a real pointer type (PalRgb far *, void far *) -- pal here is
       already an encoded far pointer, carried as a plain long the same
       way every DC handle is, so both just need the matching cast. */
    pal_install( (PalRgb far *) pal );
    qglMemFree( (long) pal );
}

void v_set_viewsize( Video *v, short viewsize )
{
    short display_w, display_h, available_h;
    short x_res, y_res;
    float size;

    v->sbar_lines = viewsize >= 120 ? 0 : ( viewsize >= 110 ? 24 : 48 );
    size = (float) ( viewsize > 100 ? 100 : viewsize ) / 100.0f;
    display_w = (short) ( v->scr_x_res * size );
    if ( display_w < 96 ) display_w = 96;
    display_w &= ~7;
    if ( display_w > v->base_x_res * v->view_scale )
        display_w = (short) ( v->base_x_res * v->view_scale );

    display_h = (short) ( v->scr_y_res * size ) & ~1;
    available_h = (short) ( v->scr_y_res - v->sbar_lines );
    if ( display_h > available_h ) display_h = available_h;
    if ( display_h > v->base_y_res * v->view_scale )
        display_h = (short) ( v->base_y_res * v->view_scale );
    if ( display_h < v->view_scale ) display_h = v->view_scale;

    x_res = (short) ( display_w / v->view_scale );
    y_res = (short) ( display_h / v->view_scale );
    if ( x_res < 1 ) x_res = 1;
    if ( y_res < 1 ) y_res = 1;

    if ( !qglSfViewResize( v->h_view, v->h_back_bdc,
                           x_res, y_res, v->base_x_res ) )
        sys_error( "0x0004, Could not resize the display view" );
    v->x_res = x_res;
    v->y_res = y_res;
    v->view_x = (short) ( ( v->scr_x_res - x_res * v->view_scale ) / 2 );
    v->view_y = (short) ( ( available_h - y_res * v->view_scale ) / 2 );
}

void v_shutdown( Video *v )
{
    if ( v->h_view ) qglDelView( v->h_view );
    if ( v->h_back_bdc ) qglSfFree( v->h_back_bdc );
    if ( v->h_comp_dc ) qglSfFree( v->h_comp_dc );
    v->h_view = v->h_back_bdc = v->h_comp_dc = 0;
}

/*
 * name: v_present
 * desc: Page flip or backbuffer blit, once per frame at the end of it.
 *       Returns the next work page; v itself never changes.
 */
static void near v_clear_border( Video *v, QSurf target )
{
    short right = (short) ( v->view_x + v->x_res * v->view_scale );
    short bottom = (short) ( v->view_y + v->y_res * v->view_scale );

    if ( v->view_y > 0 )
        qglDrFill( target, 0, 0, (short) ( v->scr_x_res - 1 ),
                   (short) ( v->view_y - 1 ), 0L );
    if ( bottom < v->scr_y_res )
        qglDrFill( target, 0, bottom, (short) ( v->scr_x_res - 1 ),
                   (short) ( v->scr_y_res - 1 ), 0L );
    if ( v->view_x > 0 )
        qglDrFill( target, 0, v->view_y, (short) ( v->view_x - 1 ),
                   (short) ( bottom - 1 ), 0L );
    if ( right < v->scr_x_res )
        qglDrFill( target, right, v->view_y, (short) ( v->scr_x_res - 1 ),
                   (short) ( bottom - 1 ), 0L );
}

short v_present( Video *v, QSurf h_dst_dc, short page )
{
    /* qgl has no page flip, so there is one path: blit the backbuffer
       up. qglDrBlitScl takes the destination SIZE where uglPutScl took
       a scale factor -- same rectangle, stated the other way round. */
    QSurf target = v->comp ? v->h_comp_dc : v->h_video_dc;
    QSurf source = v->h_view;

    v_clear_border( v, target );
    qglDrBlitScl( target, v->view_x, v->view_y,
                  (short) ( v->x_res * v->view_scale ),
                  (short) ( v->y_res * v->view_scale ),
                  source );
    return page;
}
