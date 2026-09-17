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

static char demo_magic[4] = { 'Q', 'C', 'R', '3' };
static char demo_magic2[4] = { 'Q', 'C', 'R', '2' };
static char camera_magic[4] = { 'Q', 'D', 'C', '1' };

static void demo_map_name( const char *name, char *map )
{
    short n = (short) strlen( name );
    if ( n > 4 && !stricmp( name + n - 4, ".qmp" ) ) n -= 4;
    if ( n > 8 ) n = 8;
    memcpy( map, name, n );
    map[n] = 0;
}

short demo_map( const char *name, char *map )
{
    char magic[4];
    FILE *f = fopen( name, "rb" );

    if ( !f ) return 0;
    if ( fread( magic, 1, sizeof(magic), f ) != sizeof(magic) ||
         ( memcmp( magic, demo_magic, sizeof(magic) ) &&
           memcmp( magic, camera_magic, sizeof(magic) ) ) ||
         fread( map, 1, 9, f ) != 9 ) map[0] = 0;
    map[8] = 0;
    fclose( f );
    return map[0] != 0;
}

void demo_open( Demo *dm, const RunArgs *args )
{
    memset( dm, 0, sizeof(*dm) );

    if ( args->record_name[0] ) {
        char mbuf[64];
        long raw;
        dm->buf = (RecBuf far *) qglMemAlloc( (long) sizeof(RecBuf) );
        if ( !dm->buf ) sys_error( "out of far memory for -record buffer" );
        dm->buf->count = 0;
        demo_map_name( args->map_name, dm->map );
        raw = (long) (void far *) dm->buf;
        sprintf( mbuf, "rec_buf seg=%ld ofs=%ld max=%d", raw >> 16, raw & 0xFFFFL, REC_MAX );
        mark( mbuf );
    } else if ( args->play_name[0] ) {
        char magic[4];
        dm->rf = fopen( args->play_name, "rb" );
        if ( !dm->rf ) sys_error( "could not open -play file" );
        if ( fread( magic, 1, sizeof(magic), dm->rf ) == sizeof(magic) &&
             !memcmp( magic, camera_magic, sizeof(magic) ) ) {
            char map[9];
            dm->camera = (short) ( fread( dm->map, 1, sizeof(dm->map), dm->rf ) ==
                                   sizeof(dm->map) );
            if ( !dm->camera ||
                 fread( &dm->camera_from, sizeof(dm->camera_from), 1, dm->rf ) != 1 ||
                 fread( &dm->camera_to, sizeof(dm->camera_to), 1, dm->rf ) != 1 )
                sys_error( "short QDC1 camera track" );
            dm->map[8] = 0;
            demo_map_name( args->map_name, map );
            if ( stricmp( dm->map, map ) ) sys_error( "QDC1 belongs to another map" );
            dm->camera_time = dm->camera_from.time;
        } else if ( !memcmp( magic, demo_magic, sizeof(magic) ) ) {
            char map[9];
            dm->actions = (short) ( fread( dm->map, 1, sizeof(dm->map), dm->rf ) == sizeof(dm->map) );
            if ( !dm->actions ) sys_error( "short QCR3 header" );
            dm->map[8] = 0;
            demo_map_name( args->map_name, map );
            if ( stricmp( dm->map, map ) ) sys_error( "QCR3 belongs to another map" );
        } else if ( !memcmp( magic, demo_magic2, sizeof(magic) ) ) {
            dm->actions = 1;
        } else {
            fseek( dm->rf, 0L, SEEK_SET );
        }
    }
}

/* -play: this frame's input and dt from the recording, 0 when it has
   ended. -record: this frame's real input and dt kept. Otherwise nothing, 1. */
short demo_input( Demo *dm, Input *input, float *frame_dt )
{
    RecEntry *r = &dm->rec;

    if ( dm->rf && !dm->camera ) {
        if ( fread( r, sizeof(*r), 1, dm->rf ) != 1 ) return 0;
        *frame_dt                   = r->dt;
        input->mouse.x              = r->in[0];
        input->mouse.y              = r->in[1];
        if ( dm->actions ) {
            input->mouse.left = input->mouse.right = 0;
            input->keyboard.k[KEY_W] = input->keyboard.k[KEY_A] = 0;
            input->keyboard.k[KEY_S] = input->keyboard.k[KEY_D] = 0;
            input->keyboard.k[KEY_SPCBAR] = 0;
        } else {
            input->mouse.left             = r->in[2];
            input->mouse.right            = r->in[3];
            input->keyboard.k[KEY_W]       = r->in[4];
            input->keyboard.k[KEY_A]       = r->in[5];
            input->keyboard.k[KEY_S]       = r->in[6];
            input->keyboard.k[KEY_D]       = r->in[7];
            input->keyboard.k[KEY_SPCBAR]  = r->in[8];
        }
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

static float near demo_lerp_angle( float a, float b, float frac )
{
    float d = b - a;
    while ( d > 180.0f ) d -= 360.0f;
    while ( d < -180.0f ) d += 360.0f;
    return a + d * frac;
}

short demo_camera( Demo *dm, Camera *cam, Player *player, float frame_dt )
{
    DemoCam *a, *b;
    float frac, dx, dy, dz, pitch, yaw, cp;

    if ( !dm->camera ) return 1;
    if ( dm->camera_eof ) return 0;
    dm->camera_time += frame_dt;
    while ( dm->camera_time > dm->camera_to.time ) {
        dm->camera_from = dm->camera_to;
        if ( fread( &dm->camera_to, sizeof(dm->camera_to), 1, dm->rf ) != 1 ) {
            dm->camera_to = dm->camera_from;
            dm->camera_time = dm->camera_from.time;
            dm->camera_eof = -1;
            break;
        }
    }
    a = &dm->camera_from;
    b = &dm->camera_to;
    frac = b->time > a->time ? ( dm->camera_time - a->time ) / ( b->time - a->time ) : 1.0f;
    if ( frac < 0.0f ) frac = 0.0f;
    if ( frac > 1.0f ) frac = 1.0f;
    dx = b->x - a->x; dy = b->y - a->y; dz = b->z - a->z;
    /* CL_LerpPoint does not sweep a teleport through the intervening wall. */
    if ( fabs( dx ) > 100.0f || fabs( dy ) > 100.0f || fabs( dz ) > 100.0f ) frac = 0.0f;

    player->pos.x = a->x + dx * frac;
    player->pos.y = a->y + dy * frac;
    player->pos.z = a->z + dz * frac - 22.0f;
    player->vel.x = player->vel.y = player->vel.z = 0.0f;
    player->fall_vz = 0.0f;

    cam->pos.x = player->pos.x;
    cam->pos.y = player->pos.z + 22.0f;
    cam->pos.z = player->pos.y;
    pitch = demo_lerp_angle( a->pitch, b->pitch, frac ) * 0.017453292f;
    yaw = demo_lerp_angle( a->yaw, b->yaw, frac ) * 0.017453292f;
    cp = (float) cos( pitch );
    cam->look_at.x = cam->pos.x + cp * (float) cos( yaw );
    cam->look_at.y = cam->pos.y - (float) sin( pitch );
    cam->look_at.z = cam->pos.z + cp * (float) sin( yaw );
    return 1;
}

void demo_actions( Demo *dm, const KeyBindings KB_FAR *keys )
{
    RecEntry *r;
    if ( !dm->buf ) return;
    r = &dm->rec;
    r->in[2] = keybind_record_state( keys, KA_ATTACK );
    r->in[3] = 0;
    r->in[4] = keybind_record_state( keys, KA_FORWARD );
    r->in[5] = keybind_record_state( keys, KA_MOVELEFT );
    r->in[6] = keybind_record_state( keys, KA_BACK );
    r->in[7] = keybind_record_state( keys, KA_MOVERIGHT );
    r->in[8] = keybind_record_state( keys, KA_JUMP );
}

/* -play: the camera is not pinned, so a replay that has left the
   recorded run is reported, once, and left to run on. */
void demo_check( Demo *dm, const Camera *cam, short frame )
{
    char  buf[112];
    float dx, dy, dz;

    if ( !dm->rf || dm->camera || dm->drift ) return;
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
    dm->rf = 0;
    dm->camera = dm->camera_eof = 0;
    if ( dm->buf && dm->buf->count != 0 ) {
        wf = fopen( args->record_name, "wb" );
        if ( wf ) {
            fwrite( demo_magic, 1, sizeof(demo_magic), wf );
            fwrite( dm->map, 1, sizeof(dm->map), wf );
            for ( i = 0; i < dm->buf->count; i++ ) {
                e = dm->buf->e[i];
                fwrite( &e, sizeof(e), 1, wf );
            }
            fclose( wf );
        }
    }
    if ( dm->buf ) qglMemFree( (long) (void far *) dm->buf );
    dm->buf = 0;
}
