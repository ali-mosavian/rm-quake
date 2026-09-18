/*
 * ent.c -- entities. C port of ent.bas.
 *
 * The per-frame math (ent_find_node, ent_place_models,
 * ent_check_teleport, ent_point_leaf) and the loading functions (ent_load_spawn,
 * ent_load_world, built on pak.h's pak_load_whole rather
 * than a port of ent_get/ent_open_bin's own sequential UAR reads --
 * ents.bin is tiny, so reading it whole and walking a far pointer
 * through the records in memory is simpler than replicating BASIC's
 * GET-one-record-at-a-time shape).
 */

#include <mem.h>    /* _fmemcpy */
#include <stdio.h>
#include "qgl.h"
#include <stdlib.h>
#include <math.h>

#include "ent.h"
#include "snd.h"
#include "doors.h"
#include "triggers.h"
#include "plats.h"
#include "pl_move.h"    /* PL_FEET/PL_TELE_LIFT -- shared with pl_move.c, one fact one place */
#include "r_bsp.h"      /* r_point_leaf -- r_bsp.bas, not yet ported */
#include "pak.h"
#include "item.h"
#include "entitems.h"
#include "mdl_ai.h"
#include "mdl.h"   /* mdl_load_monsters/mdl_spawn */
#include "part.h"
#include "sys.h"

/* Neither toolchain here defines F_FTOL@, the runtime helper bcc emits
   for a float/double-to-integer cast (checked bcpp31's and tc201's
   CC.LIB by string search, present in neither) -- so a cast goes
   through this instead, same fix already applied to FSIN in d_poly.c.
   FISTP alone rounds to nearest per the FPU control word; a C cast
   truncates toward zero, so the control word is switched to truncate
   (RC=11) around the store and restored after. */
static short ftol_short( float f )
{
    short cw, tcw, result;
    __asm {
        fld    f
        fnstcw word ptr cw
        mov    ax, cw
        or     ax, 0x0C00
        mov    tcw, ax
        fldcw  word ptr tcw
        fistp  word ptr result
        fldcw  word ptr cw
    }
    return result;
}

/*
 * name: ent_find_node
 * desc: The deepest world node whose plane submodel m's box does not
 *       straddle.
 *
 *       That node is where the entity belongs in the painter's order:
 *       every world face beyond it is drawn before the walk arrives,
 *       every face nearer is drawn after, and the entity goes in
 *       between. Descending stops as soon as a plane cuts the box,
 *       because past that point the entity is on both sides at once
 *       and no single position is right.
 *
 *       Returns a leaf, bit 15 set, when the box fits inside one.
 */
short ent_find_node( short m, World *world )
{
    Submodel far *sm = &world->models[m];
    short node_nr = 0;
    short pid;
    float dnear, dfar;
    BspVec3 far *ofs = &world->brush[m].ofs;
    float x0 = sm->mins.x + ofs->x, x1 = sm->maxs.x + ofs->x;
    float y0 = sm->mins.y + ofs->y, y1 = sm->maxs.y + ofs->y;
    float z0 = sm->mins.z + ofs->z, z1 = sm->maxs.z + ofs->z;

    while ( (node_nr & 0x8000) == 0 ) {
        Node far *n = &world->nodes[node_nr];
        Plane far *pl = &world->planes[n->plane_id];

        /* The box corner furthest along the normal and the one
           furthest against it. If both land on the same side of the
           plane, so does every other corner. */
        if ( pl->norm.x >= 0.0f ) { dnear  = pl->norm.x * x0; dfar  = pl->norm.x * x1; }
        else                      { dnear  = pl->norm.x * x1; dfar  = pl->norm.x * x0; }

        if ( pl->norm.y >= 0.0f ) { dnear += pl->norm.y * y0; dfar += pl->norm.y * y1; }
        else                      { dnear += pl->norm.y * y1; dfar += pl->norm.y * y0; }

        if ( pl->norm.z >= 0.0f ) { dnear += pl->norm.z * z0; dfar += pl->norm.z * z1; }
        else                      { dnear += pl->norm.z * z1; dfar += pl->norm.z * z0; }

        dnear -= pl->dist;
        dfar  -= pl->dist;

        if ( dnear >= 0.0f )      node_nr = n->child0;
        else if ( dfar < 0.0f )   node_nr = n->child1;
        else                      break;
    }

    return node_nr;
}

/*
 * name: ent_point_leaf
 * desc: The world leaf a point falls in. The same descent as
 *       pl_point_contents, stopping one step earlier: that wants what
 *       is at the point, this wants where the point is.
 */
short ent_point_leaf( BspVec3 *p, World *world )
{
    return r_point_leaf( p, world );
}

/*
 * name: ent_place_models
 * desc: Works out where in the world's back-to-front order each
 *       submodel belongs, once per frame, and leaves the answer in
 *       brush[m].node.
 *
 *       Two earlier answers were wrong in instructive ways. Recording
 *       the leaf a sample point falls in fails because that leaf is
 *       often solid -- a lowered lift sits inside its own shaft -- and
 *       a solid leaf is never walked. Drawing at the first visible
 *       leaf the box overlaps fails because the walk reaches leaves in
 *       depth order, so "first" means "furthest", and everything the
 *       entity should hide gets drawn after it.
 */
void ent_place_models( World *world )
{
    short m;
    /* Only the brushes that moved. ent_find_node for every submodel
       every tick was 81ms of e1m3's 131ms frame; a mover stamps its
       own brush ENT_NODE_DIRTY when its offset changes, and a brush
       that did not move is still where it was placed. */
    for ( m = 1; m < world->model_count; m++ ) {
        if ( world->brush[m].node == ENT_NODE_DIRTY )
            world->brush[m].node = ent_find_node( m, world );
    }
}

/* spawn_tfog: play_teleport's sound, 0.2 s after, where the fog was made */
static void ent_fog( World *world, BspVec3 *org, float now )
{
    short k;

    for ( k = 0; k < ENT_FOGS; k++ ) {
        if ( world->fog_at[k] != 0.0f ) continue;
        world->fog_org[k] = *org;
        world->fog_at[k] = now + ENT_FOG_DELAY;
        return;
    }
}

void ent_fog_tick( World *world, Player *player, float now )
{
    short k;
    BspVec3 at;

    for ( k = 0; k < ENT_FOGS; k++ ) {
        if ( world->fog_at[k] == 0.0f || now < world->fog_at[k] ) continue;
        world->fog_at[k] = 0.0f;
        at = world->fog_org[k];
        snd_play( &world->snd, player, (short) ( SND_TELE + rng_int( &world->rng ) % 5 ), &at );
    }
}

/* teleport_touch's two fogs: where it stood, and 32 along the
   destination's facing */
static void ent_tele_fogs( World *world, Renderer *rdr, BspVec3 *from,
                           Teleporter far *t, float now )
{
    BspVec3 org;
    float a = t->yaw * ( 3.14159265f / 180.0f );

    ent_fog( world, from, now );
    part_teleport( rdr, from );
    org = t->dest;
    org.x += 32.0f * (float) cos( a );
    org.y += 32.0f * (float) sin( a );
    ent_fog( world, &org, now );
    part_teleport( rdr, &org );
}

/* teleport_use: a named teleporter takes a touch only for 0.2 s after */
static short ent_tele_live( Teleporter far *t, float now )
{
    return (short) ( !t->name || now <= t->until );
}

/*
 * name: ent_check_teleport
 * desc: Moves the player if their box overlaps a teleporter's.
 *
 *       Box against box, not point against box: a trigger is often
 *       thinner than the player, and testing the origin alone lets a
 *       fast enough player pass through one without ever having their
 *       centre inside it.
 */
void ent_check_teleport( Player *player, World *world, Renderer *rdr,
                         float now, short scr_x_res )
{
    short i;
    BspVec3 pmin, pmax;

    if ( player->no_clip ) return;

    pmin.x = player->pos.x - 16.0f;
    pmin.y = player->pos.y - 16.0f;
    pmin.z = player->pos.z - PL_FEET;
    pmax.x = player->pos.x + 16.0f;
    pmax.y = player->pos.y + 16.0f;
    pmax.z = player->pos.z + 32.0f;

    for ( i = 0; i < world->tele_count; i++ ) {
        Teleporter far *t = &world->tele[i];

        if ( !ent_tele_live( t, now ) ) continue;
        if ( pmax.x >= t->mins.x && pmin.x <= t->maxs.x &&
             pmax.y >= t->mins.y && pmin.y <= t->maxs.y &&
             pmax.z >= t->mins.z && pmin.z <= t->maxs.z ) {

            ent_tele_fogs( world, rdr, &player->pos, t, now );
            player->pos   = t->dest;
            player->vel.x = 0.0f;
            player->vel.y = 0.0f;
            player->vel.z = 0.0f;
            player->fall_vz = 0.0f;

            /* Face the way the destination says. The camera reads its
               angle from the mouse, so the mouse is what has to move --
               the same trick host_main uses to apply the spawn angle. */
            qglMousePos( ftol_short( (scr_x_res - 1) * t->yaw / 360.0f ), 110 );

            return;
        }
    }
}

/* teleport_touch on a monster: its box, MDL_HALF and MDL_ZLO..MDL_ZHI,
   against every teleporter that is not PLAYER_ONLY */
void ent_monster_teleport( World *world, Renderer *rdr, float now, MdlEnt far *e )
{
    short i;
    BspVec3 from;

    for ( i = 0; i < world->tele_count; i++ ) {
        Teleporter far *t = &world->tele[i];

        if ( t->player_only || !ent_tele_live( t, now ) ) continue;
        if ( e->pos.x + MDL_HALF < t->mins.x || e->pos.x - MDL_HALF > t->maxs.x ||
             e->pos.y + MDL_HALF < t->mins.y || e->pos.y - MDL_HALF > t->maxs.y ||
             e->pos.z + MDL_ZHI  < t->mins.z || e->pos.z + MDL_ZLO  > t->maxs.z ) continue;
        from = e->pos;
        ent_tele_fogs( world, rdr, &from, t, now );
        e->pos = t->dest;
        e->yaw = e->ideal_yaw = t->yaw;
        world->mon_teles++;
        return;
    }
}

/* ents<N>.bin: mkassets ships one a skill */
static void near ent_ents_name( char *name, short skill )
{
    sprintf( name, "ents%d.bin", (int) skill );
}

/*
 * name: ent_load_spawn
 * desc: The spawn point, from ents.bin -- the entities text mkassets.py
 *       already resolved. The text itself never reaches the target:
 *       BASIC strings capped at 32,767 bytes and e1m3's entities lump
 *       is 45,762, which cport/ has no reason to inherit but keeps
 *       reading the same pre-resolved header regardless.
 */
void ent_load_spawn( World *world, Camera *cam, Fight *fight )
{
    long n;
    char name[12];
    unsigned char far *buf;
    EntsHead h;

    ent_ents_name( name, world->skill );
    buf = pak_load_whole( &world->pak, name, &n );
    _fmemcpy( &h, buf, sizeof(EntsHead) );
    qglMemFree( (long) buf );

    if ( h.nmodels != world->model_count ) {
        sys_error( "entities are from another map" );
    }

    /* SelectSpawnPoint: info_player_start2 once a rune is held */
    if ( world->serverflags ) {
        h.spawn = h.spawn2;
        h.angle = h.angle2;
    }

    /* BSP is Z-up and the camera is Y-up, so y and z swap here. */
    cam->pos.x = h.spawn.x;
    cam->pos.z = h.spawn.y;
    cam->pos.y = h.spawn.z;
    /* MIRRORED. The map's angle is CCW from +x; the freelook math
       makes the eye direction (cos a, -sin a) in bsp x,y, so a spawn
       that should face +y (angle 90) needs 270 here. Unmirrored, the
       player starts facing the wall behind them: -walk from e1m1's
       spawn went 47 units backwards and stopped, where it should walk
       the length of the hall. */
    cam->start_angle = 360.0f - h.angle;
    if ( cam->start_angle >= 360.0f ) cam->start_angle -= 360.0f;

    /* worldspawn's, and world.qc's sv_gravity: 100 on e1m8, 800
       elsewhere. Both ride in the header rather than being guessed. */
    fight->worldtype = h.worldtype;
    fight->gravity   = h.gravity > 0.0f ? h.gravity : 800.0f;

    /* the intermission camera; where the level leads is the touched
       changelevel's */
    fight->inter       = h.inter;
    fight->inter_pitch = h.inter_pitch;
    fight->inter_yaw   = h.inter_yaw;
    fight->next_map[0] = '\0';
}


/*
 * The movers, in the order ents.bin puts them. Each takes the running
 * offset by pointer: a record skipped by the wrong size lands the next
 * loader in the middle of a record, and the only symptom is nonsense
 * geometry several loaders later.
 */
static void near ent_load_doors( World *world, unsigned char far *buf, long *ofs, short count )
{
    short i, m;
    float fx, fz;

    world->door_count = 0;
    world->door = (DoorEnt far *) qglMemAlloc( (long) ( count ? count : 1 ) * sizeof(DoorEnt) );
    if ( !world->door ) sys_error( "ents: no room for the doors" );

    for ( i = 0; i < count; i++ ) {
        EntsDoor dr;
        DoorEnt far *d;

        _fmemcpy( &dr, buf + *ofs, sizeof(EntsDoor) ); *ofs += sizeof(EntsDoor);
        m = dr.model;
        if ( m <= 0 || m >= world->model_count ) continue;

        d = &world->door[ world->door_count ];
        d->model   = m;
        d->speed   = dr.speed;
        d->hold    = dr.hold;
        d->hold_left = 0.0f;
        d->pause_left = 0.0f;
        d->nolink  = dr.nolink;
        d->targeted = dr.targeted;
        d->secret  = dr.secret;
        d->shoot   = dr.shoot;
        d->snd     = dr.snd;
        d->key     = dr.key;
        d->say_at  = 0.0f;
        d->ofs_mid = dr.mid;
        d->msg     = dr.msg;
        d->order   = dr.order;
        d->state   = ENT_DOOR_SHUT;
        d->link    = world->door_count;

        /* DOOR_START_OPEN: drawn shut where the map put it, which is
           the far end of its travel. */
        d->ofs_shut.x = d->ofs_shut.y = d->ofs_shut.z = 0.0f;
        d->ofs_open = dr.travel;
        if ( dr.start_open ) {
            d->ofs_shut = dr.travel;
            d->ofs_open.x = d->ofs_open.y = d->ofs_open.z = 0.0f;
        }
        world->brush[m].ofs = d->ofs_shut;

        /* spawn_field: the brush where it sits, grown 60 in x and y and
           8 in z. A targeted, secret or key door has no field -- touching
           the brush itself is what says its message (door_touch). */
        fx = ENT_DOOR_FIELD; fz = ENT_DOOR_FIELDZ;
        if ( d->targeted || d->secret || d->key ) { fx = ENT_TOUCH_SLACK; fz = ENT_TOUCH_SLACK; }
        d->mins.x = world->models[m].mins.x + d->ofs_shut.x - fx;
        d->mins.y = world->models[m].mins.y + d->ofs_shut.y - fx;
        d->mins.z = world->models[m].mins.z + d->ofs_shut.z - fz;
        d->maxs.x = world->models[m].maxs.x + d->ofs_shut.x + fx;
        d->maxs.y = world->models[m].maxs.y + d->ofs_shut.y + fx;
        d->maxs.z = world->models[m].maxs.z + d->ofs_shut.z + fz;

        world->door_count++;
    }
}

static void near ent_load_trigs( World *world, unsigned char far *buf, long *ofs, short count )
{
    short i, m;

    world->trig_count = 0;
    world->trig = (TrigEnt far *) qglMemAlloc( (long) ( count ? count : 1 ) * sizeof(TrigEnt) );
    if ( !world->trig ) sys_error( "ents: no room for the triggers" );

    for ( i = 0; i < count; i++ ) {
        EntsTrig xr;
        TrigEnt far *t;

        _fmemcpy( &xr, buf + *ofs, sizeof(EntsTrig) ); *ofs += sizeof(EntsTrig);
        m = xr.model;
        if ( m < 0 || m >= world->model_count ) continue;

        t = &world->trig[ world->trig_count ];
        t->model = m;
        t->kind  = xr.kind;
        t->target = xr.target;
        t->name  = xr.name;
        t->kill  = xr.kill;
        t->state = ENT_TRIG_READY;
        t->left  = xr.count;
        t->count = xr.count;
        t->wait  = xr.wait;
        t->wait_left = 0.0f;
        t->speed = xr.speed;
        t->snd   = xr.snd;
        t->ofs_out = xr.travel;
        t->delay = xr.delay;
        t->order = xr.order;
        t->delay_left = 0.0f;
        if ( t->kind == ENT_TRIG_FIREBALL ) ent_fireball_spawn( world, t );
        t->msg   = xr.msg;
        t->mins  = world->models[m].mins;
        t->maxs  = world->models[m].maxs;
        /* A shooter and a fireball have no brush: their origin IS the
           volume, so mins and maxs are the same point. */
        if ( xr.kind == ENT_TRIG_SHOOTER || xr.kind == ENT_TRIG_FIREBALL || xr.kind == ENT_TRIG_BOSS ||
             xr.kind == ENT_TRIG_BOLT ) {
            t->mins = xr.org;
            t->maxs = xr.org;
        }
        world->trig_count++;
    }
}

/* what the tally counts against: the map's trigger_secrets */
short ent_secret_total( World *world )
{
    short k, n = 0;

    for ( k = 0; k < world->trig_count; k++ )
        if ( world->trig[k].kind == ENT_TRIG_SECRET ) n++;
    return n;
}

static void near ent_load_trains( World *world, unsigned char far *buf, long *ofs, short count )
{
    short i, m;

    for ( i = 0; i < count; i++ ) {
        EntsTrain tr;
        PlatEnt far *p;

        _fmemcpy( &tr, buf + *ofs, sizeof(EntsTrain) ); *ofs += sizeof(EntsTrain);
        m = tr.model;
        if ( m <= 0 || m >= world->model_count ) continue;
        if ( world->plat_count >= world->plat_max ) continue;

        p = &world->plat[ world->plat_count ];
        p->model = m;
        p->speed = tr.speed > 0.0f ? tr.speed : 100.0f;
        p->travel = 0.0f;
        p->kind  = ENT_PLAT_KIND_TRAIN;
        p->targeted = tr.targeted;
        p->first = tr.first;
        p->corner = tr.first;
        p->wait_left = 0.0f;
        p->snd   = tr.snd;
        p->moving = 0;
        p->state = ENT_TRAIN_IDLE;
        p->mins  = world->models[m].mins;
        p->maxs  = world->models[m].maxs;
        world->plat_count++;
    }
}

static void near ent_load_corners( World *world, unsigned char far *buf, long *ofs, short count )
{
    world->corner_count = count;
    world->corner = (PathCorner far *) qglMemAlloc( (long) ( count ? count : 1 ) * sizeof(PathCorner) );
    if ( !world->corner ) sys_error( "ents: no room for the path corners" );
    if ( count ) _fmemcpy( world->corner, buf + *ofs, (long) count * sizeof(PathCorner) );
    *ofs += (long) count * sizeof(PathCorner);
}

static void near ent_load_lights( World *world, unsigned char far *buf, long *ofs, short count )
{
    EntsLight lr;
    short i;

    world->light_count = count;
    world->light = (LightEnt far *) qglMemAlloc( (long) ( count ? count : 1 ) * sizeof(LightEnt) );
    if ( !world->light ) sys_error( "ents: no room for the lights" );
    for ( i = 0; i < count; i++ ) {
        _fmemcpy( &lr, buf + *ofs, sizeof(EntsLight) );
        *ofs += sizeof(EntsLight);
        world->light[i].name     = lr.name;
        world->light[i].style    = lr.style;
        world->light[i].on       = lr.on;
        world->light[i].start_on = lr.on;
        world->light[i].stamp    = i;
    }
    world->light_clock = count;
}

/* The message table is last, so what is left of the file is what it
   holds -- and a header that disagrees with that is the tell that some
   record above was skipped by the wrong size. */
static void near ent_load_msgs( World *world, unsigned char far *buf, long *ofs,
                                short size, long total )
{
    if ( size < 1 || *ofs + size != total || buf[*ofs] != 0 || buf[*ofs + size - 1] != 0 ) {
        sys_error( "ents: message table size does not match its header" );
    }
    world->msg_size = size;
    world->msgs = (char far *) qglMemAlloc( size );
    if ( !world->msgs ) sys_error( "ents: no room for the messages" );
    _fmemcpy( world->msgs, buf + *ofs, size );
    *ofs += size;
}

/* Every submodel draws and blocks unless something claims it as a
   trigger. Which submodel owns each face goes in the bits above
   Face.side's one: the world's faces come first and the submodels'
   follow in order, so this is a walk, not a search -- and an array of
   its own was 11,032 bytes on e1m1 to hold a number under 64. */
static void near ent_load_brushes( World *world )
{
    short i, j, k;

    for ( i = 0; i < world->model_count; i++ ) {
        world->brush[i].draw  = 1;
        world->brush[i].solid = 1;
        world->brush[i].ofs.x = 0.0f;
        world->brush[i].ofs.y = 0.0f;
        world->brush[i].ofs.z = 0.0f;
        world->brush[i].node  = ENT_NODE_DIRTY;
    }

    for ( i = 0; i < world->face_count; i++ )
        world->faces[i].side = (short) ( world->faces[i].side & 1 );

    for ( j = 1; j < world->model_count; j++ ) {
        for ( k = (short) world->models[j].first_face;
              k < (short) (world->models[j].first_face + world->models[j].num_faces); k++ ) {
            if ( k >= 0 && k < world->face_count )
                world->faces[k].side = (short) ( ( world->faces[k].side & 1 ) | ( j << 1 ) );
        }
    }
}

/* Teleporter pairs already matched by targetname; the volume is the
   submodel's box. */
static void near ent_load_teles( World *world, unsigned char far *buf, long *ofs, short count )
{
    short i, k, mdlnum;

    for ( i = 0; i < count; i++ ) {
        EntsTele tr;
        _fmemcpy( &tr, buf + *ofs, sizeof(EntsTele) ); *ofs += sizeof(EntsTele);

        mdlnum = tr.model;
        if ( mdlnum > 0 && mdlnum < world->model_count ) {
            Teleporter far *t = &world->tele[ world->tele_count ];
            t->mins = world->models[mdlnum].mins;
            t->maxs = world->models[mdlnum].maxs;
            t->dest = tr.dest;
            /* the arrival point is above the mapper's mark -- see PL_TELE_LIFT */
            t->dest.z += PL_TELE_LIFT;
            t->yaw = tr.yaw;
            t->name = tr.name;
            t->player_only = tr.player_only;
            t->until = -1.0f;
            world->tele_count++;
        }
    }
    for ( k = 0; k < ENT_FOGS; k++ ) world->fog_at[k] = 0.0f;
    world->mon_teles = 0;
    world->mon_jumps = 0;
}

static void near ent_load_plats( World *world, unsigned char far *buf, long *ofs, short count )
{
    short i, mdlnum;

    for ( i = 0; i < count; i++ ) {
        EntsPlat pr;
        _fmemcpy( &pr, buf + *ofs, sizeof(EntsPlat) ); *ofs += sizeof(EntsPlat);

        mdlnum = pr.model;
        if ( mdlnum > 0 && mdlnum < world->model_count ) {
            PlatEnt far *p = &world->plat[ world->plat_count ];
            p->model  = mdlnum;
            p->speed  = pr.speed;
            p->travel = pr.travel;
            p->mins   = world->models[mdlnum].mins;
            p->maxs   = world->models[mdlnum].maxs;

            if ( p->speed  <= 0.0f ) p->speed  = 150.0f;
            if ( p->travel <= 0.0f ) p->travel = world->models[mdlnum].maxs.z - world->models[mdlnum].mins.z;

            /* Quake positions the brush raised, so a lift at rest is
               one full travel below where the map drew it. */
            p->state = ENT_PLAT_DOWN;
            p->kind  = ENT_PLAT_KIND_PLAT;
            p->targeted = 0;
            p->first = p->corner = -1;
            p->wait_left = 0.0f;
            p->snd = pr.snd >= 1 && pr.snd <= 2 ? pr.snd : 2;
            p->moving = 0;
            world->brush[mdlnum].ofs.z = -p->travel;

            world->plat_count++;
        }
    }
}

static void near ent_load_hides( World *world, unsigned char far *buf, long *ofs, short count )
{
    short i, hideidx;

    for ( i = 0; i < count; i++ ) {
        _fmemcpy( &hideidx, buf + *ofs, sizeof(short) ); *ofs += sizeof(short);

        if ( hideidx > 0 && hideidx < world->model_count ) {
            world->brush[hideidx].draw  = 0;
            world->brush[hideidx].solid = 0;
        }
    }
}

static void near ent_load_gates( World *world, unsigned char far *buf, long *ofs, short count )
{
    short i, held;

    for ( i = 0; i < count; i++ ) {
        EntsGate g;
        _fmemcpy( &g, buf + *ofs, sizeof(EntsGate) ); *ofs += sizeof(EntsGate);

        held = (short) ( world->serverflags & g.bits );
        if ( ( g.boss ? held != g.bits : held != 0 ) ) continue;
        if ( g.model > 0 && g.model < world->model_count ) {
            world->brush[g.model].draw  = 0;
            world->brush[g.model].solid = 0;
        }
    }
}

/* The map's ambient_* points, recorded now and started by snd_init with
   the card: their volume is the map's, their place the mixer's to
   re-derive every frame. */
static void near ent_load_ambs( World *world, unsigned char far *buf, long *ofs, short count )
{
    short i;

    snd_statics( &world->snd, count );
    for ( i = 0; i < count; i++ ) {
        EntsAmb ar;
        _fmemcpy( &ar, buf + *ofs, sizeof(EntsAmb) ); *ofs += sizeof(EntsAmb);
        snd_ambient( &world->snd, ar.snd, ar.vol, &ar.org );
    }
}

/* What needs everything loaded. Every train stands on its first corner,
   which needs the corner table, and every door learns its group, which
   needs them all. Then every pickup onto the floor under it, which needs
   every plat and door at the position it loaded in: the trace walks
   their hulls too. Then every monster onto it, which needs the same
   hulls and, for a patrol, the corner table. */
static void near ent_settle( World *world )
{
    short i;

    for ( i = 0; i < world->plat_count; i++ )
        if ( world->plat[i].kind == ENT_PLAT_KIND_TRAIN ) ent_train_init( world, &world->plat[i] );
    ent_link_doors( world );
    ent_items_drop( world );
    for ( i = 0; i < world->mon_count; i++ ) mdl_spawn( world, &world->mon[i] );
}

/*
 * name: ent_load_world
 * desc: Loads ents.bin a record kind at a time, in the file's order.
 *       What mkassets.py can resolve offline arrives resolved; what
 *       needs the loaded submodels -- trigger volumes and plat defaults
 *       come from world->models -- is filled in here.
 */
void ent_load_world( World *world )
{
    long n, ofs;
    char name[12];
    unsigned char far *buf;
    EntsHead h;

    ent_ents_name( name, world->skill );
    buf = pak_load_whole( &world->pak, name, &n );

    _fmemcpy( &h, buf, sizeof(EntsHead) );
    ofs = sizeof(EntsHead);

    if ( h.nmodels != world->model_count ) {
        sys_error( "entities are from another map" );
    }

    /* Sized to the map, not a fixed bound: e1m3 has 106 submodels, and
       ent_place_models/pl_trace walk every one of them. */
    world->brush = (BrushModel far *) qglMemAlloc( (long) world->model_count * sizeof(BrushModel) );
    world->tele = (Teleporter far *) qglMemAlloc( (long) (h.ntele ? h.ntele : 1) * sizeof(Teleporter) );
    /* the trains ride this array too, so it is sized for both */
    world->plat_max = (short) ( h.nplat + h.ntrain );
    world->plat = (PlatEnt far *) qglMemAlloc( (long) (world->plat_max ? world->plat_max : 1) * sizeof(PlatEnt) );
    if ( !world->brush || !world->tele || !world->plat ) {
        sys_error( "ents: out of memory" );
    }

    world->tele_count = 0;
    world->plat_count = 0;

    /* The monsters come first in the file, and the models the kinds
       among them need come with them. */
    mdl_load_monsters( world, buf, &ofs, h.nmon );
    ent_load_brushes( world );
    ent_load_teles( world, buf, &ofs, h.ntele );
    ent_load_plats( world, buf, &ofs, h.nplat );
    ent_load_hides( world, buf, &ofs, h.nhide );
    ent_load_gates( world, buf, &ofs, h.ngate );
    ent_load_items( world, buf, &ofs, h.nitem );
    ent_load_doors( world, buf, &ofs, h.ndoor );
    ent_load_trigs( world, buf, &ofs, h.ntrig );
    mdl_load_boss( world );
    ent_load_ambs( world, buf, &ofs, h.namb );
    ent_load_trains( world, buf, &ofs, h.ntrain );
    ent_load_corners( world, buf, &ofs, h.ncorner );
    ent_load_crates( world, buf, &ofs, h.ncrate );
    ent_load_lights( world, buf, &ofs, h.nlight );
    ent_load_msgs( world, buf, &ofs, h.msg_size, n );
    world->title = ent_msg( world, h.title );
    ent_settle( world );

    qglMemFree( (long) buf );
}

/* A message by its byte offset in the map's NUL string pool. */
char far *ent_msg( World *world, short id )
{
    if ( id < 1 || id >= world->msg_size || !world->msgs ) return 0;
    return world->msgs + id;
}

/* The player's box against one grown by slack. */
short ent_box_touched( Player *player, BspVec3 far *mins, BspVec3 far *maxs, float slack )
{
    if ( player->pos.x + PL_HALF + slack < mins->x ) return 0;
    if ( player->pos.x - PL_HALF - slack > maxs->x ) return 0;
    if ( player->pos.y + PL_HALF + slack < mins->y ) return 0;
    if ( player->pos.y - PL_HALF - slack > maxs->y ) return 0;
    if ( player->pos.z + PL_ZHI  + slack < mins->z ) return 0;
    if ( player->pos.z + PL_ZLO  - slack > maxs->z ) return 0;
    return 1;
}

void ent_brush_sound( Player *player, World *world, short m, short id )
{
    BspVec3 c;
    Submodel far *sm = &world->models[m];
    BrushModel far *b = &world->brush[m];

    c.x = ( sm->mins.x + sm->maxs.x ) * 0.5f + b->ofs.x;
    c.y = ( sm->mins.y + sm->maxs.y ) * 0.5f + b->ofs.y;
    c.z = ( sm->mins.z + sm->maxs.z ) * 0.5f + b->ofs.z;
    snd_start( &world->snd, player, (short) ( SND_ENT_BRUSH + m ), CHAN_VOICE, id, &c, ATTN_NORM );
}

void ent_reset( World *world, Fight *fight )
{
    short k;

    for ( k = 0; k < world->door_count; k++ ) {
        world->door[k].state = ENT_DOOR_SHUT;
        world->door[k].hold_left = 0.0f;
        world->door[k].pause_left = 0.0f;
        world->brush[ world->door[k].model ].ofs = world->door[k].ofs_shut;
    }
    for ( k = 0; k < world->trig_count; k++ ) {
        world->trig[k].state = ENT_TRIG_READY;
        world->trig[k].left = world->trig[k].count;
        world->trig[k].wait_left = 0.0f;
        world->trig[k].delay_left = 0.0f;
        if ( world->trig[k].kind == ENT_TRIG_FIREBALL ) ent_fireball_spawn( world, &world->trig[k] );
        if ( world->trig[k].kind == ENT_TRIG_BUTTON ||
             world->trig[k].kind == ENT_TRIG_SHOOT_BUTTON ) {
            world->brush[ world->trig[k].model ].ofs.x = 0.0f;
            world->brush[ world->trig[k].model ].ofs.y = 0.0f;
            world->brush[ world->trig[k].model ].ofs.z = 0.0f;
        }
    }
    for ( k = 0; k < world->plat_count; k++ )
        if ( world->plat[k].kind == ENT_PLAT_KIND_TRAIN ) ent_train_init( world, &world->plat[k] );
    for ( k = 0; k < world->light_count; k++ ) {
        world->light[k].on    = world->light[k].start_on;
        world->light[k].stamp = k;
    }
    world->light_clock = world->light_count;

    fight->msg_time = 0.0f;
    fight->msg = 0;
    for ( k = 1; k < world->model_count; k++ ) world->brush[k].node = ENT_NODE_DIRTY;
}
