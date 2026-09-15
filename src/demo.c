/*
 * demo.c -- see demo.h.
 */

#include "qrcfg.h"
#include <stdio.h>
#include <string.h>
#include <math.h>   /* atan2 -- the recorded yaw */
#include "dbg.h"
#include "qgl.h"
#include "demo.h"

void demo_open( Demo *dm, const RunArgs *args )
{
    memset( dm, 0, sizeof(*dm) );

    if ( args->record_name[0] ) {
        char mbuf[64];
        long raw;
        dm->buf = (RecBuf far *) qglMemAlloc( (long) sizeof(RecBuf) );
        if ( !dm->buf ) sys_error( "out of far memory for -record buffer" );
        dm->buf->count = 0;
        raw = (long) (void far *) dm->buf;
        sprintf( mbuf, "rec_buf seg=%ld ofs=%ld max=%d", raw >> 16, raw & 0xFFFFL, REC_MAX );
        mark( mbuf );
    } else if ( args->play_name[0] ) {
        dm->rf = fopen( args->play_name, "rb" );
        if ( !dm->rf ) sys_error( "could not open -play file" );
    }
}

/* -play: this frame's input and dt from the recording, 0 when it has
   ended. -record: this frame's real input and dt kept. Otherwise nothing, 1. */
short demo_input( Demo *dm, Input *input, float *frame_dt )
{
    RecEntry *r = &dm->rec;

    if ( dm->rf ) {
        if ( fread( r, sizeof(*r), 1, dm->rf ) != 1 ) return 0;
        *frame_dt                   = r->dt;
        input->mouse.x              = r->in[0];
        input->mouse.y              = r->in[1];
        input->mouse.left           = r->in[2];
        input->mouse.right          = r->in[3];
        input->keyboard.k[KEY_W]      = r->in[4];
        input->keyboard.k[KEY_A]      = r->in[5];
        input->keyboard.k[KEY_S]      = r->in[6];
        input->keyboard.k[KEY_D]      = r->in[7];
        input->keyboard.k[KEY_SPCBAR] = r->in[8];
    } else if ( dm->buf ) {
        r->dt    = *frame_dt;
        r->in[0] = (short) input->mouse.x;
        r->in[1] = (short) input->mouse.y;
        r->in[2] = (short) input->mouse.left;
        r->in[3] = (short) input->mouse.right;
        r->in[4] = (short) input->keyboard.k[KEY_W];
        r->in[5] = (short) input->keyboard.k[KEY_A];
        r->in[6] = (short) input->keyboard.k[KEY_S];
        r->in[7] = (short) input->keyboard.k[KEY_D];
        r->in[8] = (short) input->keyboard.k[KEY_SPCBAR];
    }
    return 1;
}

/* -play: the camera is not pinned, so a replay that has left the
   recorded run is reported, once, and left to run on. */
void demo_check( Demo *dm, const Camera *cam, short frame )
{
    char  buf[112];
    float dx, dy, dz;

    if ( !dm->rf || dm->drift ) return;
    dx = cam->pos.x - dm->rec.px;
    dy = cam->pos.y - dm->rec.py;
    dz = cam->pos.z - dm->rec.pz;
    if ( dx*dx + dy*dy + dz*dz <= 1.0f ) return;
    sprintf( buf, "play DIVERGED at frame %d: got %ld,%ld,%ld want %ld,%ld,%ld",
             frame, (long) cam->pos.x, (long) cam->pos.y, (long) cam->pos.z,
             (long) dm->rec.px, (long) dm->rec.py, (long) dm->rec.pz );
    mark( buf );
    dm->drift = 1;
}

/* -record: where the frame left the camera. Every frame takes a slot,
   still or not: a replay skipping one would run the next record's input
   a frame early. */
void demo_capture( Demo *dm, const Camera *cam )
{
    RecEntry *r = &dm->rec;
    RecBuf far *b = dm->buf;

    if ( !b || b->count >= REC_MAX ) return;
    r->px  = cam->pos.x;
    r->py  = cam->pos.y;
    r->pz  = cam->pos.z;
    /* mirrored as -yaw is, in degrees as the HUD shows it */
    r->yaw = (float) ( atan2( -(double) ( cam->look_at.z - cam->pos.z ),
                               (double) ( cam->look_at.x - cam->pos.x ) ) * 57.29578 );
    b->e[b->count] = *r;
    b->count++;
}

/* a clean exit writes the whole recording once, for -play. fwrite takes a
   near pointer: handed the far buffer it drops the segment and writes
   DGROUP, so each record goes out through a copy on the stack. */
void demo_close( Demo *dm, const RunArgs *args )
{
    RecEntry e;
    FILE *wf;
    short i;

    if ( dm->rf ) fclose( dm->rf );
    if ( !dm->buf || dm->buf->count == 0 ) return;
    wf = fopen( args->record_name, "wb" );
    if ( !wf ) return;
    for ( i = 0; i < dm->buf->count; i++ ) {
        e = dm->buf->e[i];
        fwrite( &e, sizeof(e), 1, wf );
    }
    fclose( wf );
}
