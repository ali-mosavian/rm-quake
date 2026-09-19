/*
 * demo.c -- see demo.h.
 */

#include "qrcfg.h"
#include <stdio.h>
#include <string.h>
#include <math.h>   /* atan2 -- the recorded yaw */
#include "dbg.h"
#include "qgl.h"
#include "ent.h"
#include "mdl.h"
#include "mdl_ai.h"
#include "dl.h"
#include "d_beam.h"
#include "snd.h"
#include "demo.h"

static char demo_magic[4] = { 'Q', 'C', 'R', '3' };
static char demo_magic2[4] = { 'Q', 'C', 'R', '2' };
static char snapshot_magic[4] = { 'Q', 'D', 'S', '1' };

static void near demo_short( void ) { sys_error( "short" ); }

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
           memcmp( magic, snapshot_magic, sizeof(magic) ) ) ||
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
             !memcmp( magic, snapshot_magic, sizeof(magic) ) ) {
            char map[9];
            dm->camera = (short) ( fread( dm->map, 1, sizeof(dm->map), dm->rf ) ==
                                   sizeof(dm->map) );
            dm->snapshot = -1;
            if ( !dm->camera ) demo_short();
            {
                long first_entities;
                if ( fread( &dm->camera_from, sizeof(dm->camera_from), 1, dm->rf ) != 1 )
                    demo_short();
                first_entities = ftell( dm->rf );
                fseek( dm->rf, (long) dm->camera_from.ent_count * sizeof(DemoEntity), SEEK_CUR );
                dm->snapshot_next_pos = ftell( dm->rf );
                if ( fread( &dm->camera_to, sizeof(dm->camera_to), 1, dm->rf ) != 1 )
                    demo_short();
                fseek( dm->rf, first_entities, SEEK_SET );
            }
            dm->map[8] = 0;
            demo_map_name( args->map_name, map );
            if ( stricmp( dm->map, map ) ) sys_error( "QDS1 belongs to another map" );
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

static void near demo_pose( const DemoCam *frame, Player *player, Camera *cam )
{
    float pitch, yaw, cp;

    player->pos.x = frame->x;
    player->pos.y = frame->y;
    player->pos.z = frame->z - 22.0f;
    cam->pos.x = player->pos.x;
    cam->pos.y = player->pos.z + 22.0f;
    cam->pos.z = player->pos.y;
    pitch = frame->pitch * 0.017453292f;
    yaw = frame->yaw * 0.017453292f;
    cp = (float) cos( pitch );
    cam->look_at.x = cam->pos.x + cp * (float) cos( yaw );
    cam->look_at.y = cam->pos.y - (float) sin( pitch );
    cam->look_at.z = cam->pos.z + cp * (float) sin( yaw );
}

static void near demo_muzzle( DemoEntity *e, World *world, Player *player,
                              Fight *fight, Renderer *rdr, Camera *cam )
{
    BspVec3 org, fwd;
    short i, key = DL_KEY_PLAYER;

    org.x = e->x; org.y = e->y; org.z = e->z;
    if ( e->number == 0 ) {
        float len;
        fwd.x = cam->look_at.x - cam->pos.x;
        fwd.y = cam->look_at.z - cam->pos.z;
        fwd.z = cam->look_at.y - cam->pos.y;
        len = (float) sqrt( fwd.x*fwd.x + fwd.y*fwd.y + fwd.z*fwd.z );
        if ( len > 0.001f ) { fwd.x /= len; fwd.y /= len; fwd.z /= len; }
        fight->flash_until = rdr->anim_time + 0.1f;
        org = player->pos;
    } else {
        fwd.x = (float) cos( e->yaw * 0.017453292f );
        fwd.y = (float) sin( e->yaw * 0.017453292f );
        fwd.z = 0.0f;
        for ( i = 0; i < world->mon_count; i++ )
            if ( world->mon[i].kind >= 0 && world->mon[i].order == e->number ) {
                world->mon[i].flash_until = rdr->anim_time + MDL_FLASH;
                key = (short) ( DL_KEY_MON + i );
                break;
            }
    }
    dl_muzzle( rdr, key, &org, &fwd );
}

static void near demo_snapshot_apply( Demo *dm, DemoFrame *frame, World *world,
                                      Fight *fight, Renderer *rdr, LightStyles *ls,
                                      Player *player, Camera *cam )
{
    DemoEntity e;
    short i, j, mon = 0, item = 0, item_applied = 0, brush = 0, projectile = 0;
    short beam_key = 0;
    BspVec3 beam_start;

    if ( !dm->snapshot_item_base ) dm->snapshot_item_base = world->item_count;
    world->item_count = world->item_max;
    for ( i = 0; i < world->mon_count; i++ ) world->mon[i].kind = -1;
    for ( i = 0; i < world->item_count; i++ ) world->item[i].gone = -1;
    for ( i = 1; i < world->model_count; i++ ) world->brush[i].draw = 0;
    for ( i = 0; i < PL_NAILS_MAX; i++ ) fight->nail[i].alive = 0;

    for ( i = 0; i < frame->ent_count; i++ ) {
        if ( fread( &e, sizeof(e), 1, dm->rf ) != 1 ) demo_short();
        if ( e.type == DEMO_ENT_BRUSH && e.kind > 0 && e.kind < world->model_count ) {
            BrushModel far *b = &world->brush[e.kind];
            b->draw = -1;
            b->ofs.x = e.x; b->ofs.y = e.y; b->ofs.z = e.z;
            b->node = ENT_NODE_DIRTY;
            brush++;
        } else if ( e.type == DEMO_ENT_MONSTER && mon < world->mon_count ) {
            MdlEnt far *m = &world->mon[mon++];
            m->kind = e.kind; m->order = e.number;
            m->state = MDL_ST_DEMO; m->anim_frame = e.frame;
            m->pos.x = e.x; m->pos.y = e.y; m->pos.z = e.z; m->yaw = e.yaw;
        } else if ( e.type == DEMO_ENT_ITEM ) {
            ItemEnt far *it = 0;
            float best = 4096.0f;
            for ( j = 0; !e.state && j < dm->snapshot_item_base; j++ ) {
                ItemEnt far *candidate = &world->item[j];
                float dx, dy, dz, dist;
                if ( !candidate->gone || candidate->kind != e.kind ) continue;
                dx = candidate->pos.x - e.x; dy = candidate->pos.y - e.y; dz = candidate->pos.z - e.z;
                dist = dx*dx + dy*dy + dz*dz;
                if ( dist < best ) { best = dist; it = candidate; }
            }
            if ( best >= 4096.0f ) it = 0;
            if ( !it && dm->snapshot_item_base + item < world->item_count ) {
                it = &world->item[dm->snapshot_item_base + item++];
                it->crate = -1;
                it->order = e.number;
            }
            if ( !it ) continue;
            it->kind = e.kind; it->gone = 0;
            it->pos.x = e.x; it->pos.y = e.y; it->pos.z = e.z;
            it->rot_at = 0.0f;
            item_applied++;
        } else if ( e.type == DEMO_ENT_LIGHT && e.kind >= 0 && e.kind <= LS_MAXSTYLE ) {
            ls->tab[e.kind].value = e.frame;
        } else if ( e.type == DEMO_ENT_SOUND ) {
            BspVec3 org;
            short ent = e.number == 0 ? SND_ENT_PLAYER :
                        ( e.number < 0 ? 0 : (short) ( e.number + 16 ) );
            org.x = e.x; org.y = e.y; org.z = e.z;
            snd_start_volume( &world->snd, player, ent, (short) ( e.state & 7 ),
                              e.kind, &org, (short) ( ( e.state >> 3 ) & 3 ), e.frame );
        } else if ( e.type == DEMO_ENT_STOPSND ) {
            short ent = e.number == 0 ? SND_ENT_PLAYER : (short) ( e.number + 16 );
            snd_stop( &world->snd, ent, (short) ( e.state & 7 ) );
        } else if ( e.type == DEMO_ENT_DAMAGE ) {
            fight->face_until = rdr->anim_time + 0.2f;
        } else if ( e.type == DEMO_ENT_MUZZLE ) {
            demo_muzzle( &e, world, player, fight, rdr, cam );
        } else if ( e.type == DEMO_ENT_PROJECTILE && projectile < PL_NAILS_MAX ) {
            Spike *s = &fight->nail[projectile++];
            memset( s, 0, sizeof(*s) );
            s->alive = -1;
            s->pos.x = e.x; s->pos.y = e.y; s->pos.z = e.z;
            s->grenade = (short) ( e.kind == MDL_FX_GRENADE || e.kind == MDL_FX_ZOMGIB );
            s->toss = (short) ( e.kind == MDL_FX_LAVABALL );
            s->wiz = (short) ( e.kind == MDL_FX_WSPIKE );
            s->rocket = (short) ( e.kind == MDL_FX_MISSILE );
            s->gib = (short) ( e.kind == MDL_FX_ZOMGIB );
            s->dmg = (float) ( e.kind == MDL_FX_SSPIKE ? PL_SNG_DMG : PL_NG_DMG );
            s->vel.x = (float) cos( e.yaw * 0.017453293f );
            s->vel.y = (float) sin( e.yaw * 0.017453293f );
        } else if ( e.type == DEMO_ENT_BEAM_START ) {
            beam_key = e.number == 0 ? BEAM_PLAYER_KEY : e.number;
            beam_start.x = e.x; beam_start.y = e.y; beam_start.z = e.z;
        } else if ( e.type == DEMO_ENT_BEAM_END && beam_key ) {
            BspVec3 end;
            end.x = e.x; end.y = e.y; end.z = e.z;
            d_beam_spawn( rdr, beam_key, e.kind, &beam_start, &end );
            beam_key = 0;
        }
    }
    fight->items = frame->items;
    fight->health = frame->health;
    fight->armor = frame->armor;
    fight->weapon = frame->weapon;
    fight->shells = frame->shells;
    fight->nails = frame->nails;
    fight->rockets = frame->rockets;
    fight->cells = frame->cells;
    if ( frame->armor_kind == 3 ) fight->armor_type = PL_ARMOR3_TYPE;
    else if ( frame->armor_kind == 2 ) fight->armor_type = PL_ARMOR2_TYPE;
    else if ( frame->armor_kind == 1 ) fight->armor_type = PL_ARMOR1_TYPE;
    else fight->armor_type = 0.0f;
    fight->invis_until = frame->power & 1 ? rdr->anim_time + 1.0f : 0.0f;
    fight->pent_until = frame->power & 2 ? rdr->anim_time + 1.0f : 0.0f;
    fight->suit_until = frame->power & 4 ? rdr->anim_time + 1.0f : 0.0f;
    fight->quad_until = frame->power & 8 ? rdr->anim_time + 1.0f : 0.0f;
    rdr->demo_view_frame = frame->weapon_model ? frame->weapon_frame : -2;
    dm->snapshot_monsters = mon;
    dm->snapshot_brushes = brush;
    dm->snapshot_items = item_applied;
    host_view_load( world, fight->weapon );
    ent_place_models( world );
}

short demo_camera( Demo *dm, Camera *cam, Player *player, World *world,
                   Fight *fight, Renderer *rdr, LightStyles *ls, float frame_dt )
{
    DemoFrame *a;
    DemoCam *b;
    float frac, dx, dy, dz, pitch, yaw, cp;

    if ( !dm->camera ) return 1;
    if ( dm->camera_eof ) return 0;
    if ( dm->snapshot && !dm->snapshot_applied ) {
        demo_pose( (DemoCam *) &dm->camera_from, player, cam );
        demo_snapshot_apply( dm, &dm->camera_from, world, fight, rdr, ls, player, cam );
        dm->snapshot_applied = -1;
    }
    dm->camera_time += frame_dt;
    while ( dm->camera_time > dm->camera_to.time ) {
        fseek( dm->rf, dm->snapshot_next_pos, SEEK_SET );
        if ( fread( &dm->camera_from, sizeof(dm->camera_from), 1, dm->rf ) != 1 )
            demo_short();
        demo_pose( (DemoCam *) &dm->camera_from, player, cam );
        demo_snapshot_apply( dm, &dm->camera_from, world, fight, rdr, ls, player, cam );
        dm->snapshot_next_pos = ftell( dm->rf );
        if ( fread( &dm->camera_to, sizeof(dm->camera_to), 1, dm->rf ) == 1 ) continue;
        {
            memcpy( &dm->camera_to, &dm->camera_from, sizeof(DemoCam) );
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
    r->in[3] = keybind_record_state( keys, KA_USE );
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
    dm->camera = dm->snapshot = dm->snapshot_applied = dm->camera_eof = 0;
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
