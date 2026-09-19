/*
 * pl_trace.c -- sweeps the player hull through the world. C port of
 * pl_move.bas's pl_trace/pl_hull_check/pl_hull_contents.
 *
 * Already a standalone C module in the BASIC-linked build (src/pl_trace.c),
 * ported there ahead of everything else in pl_move.bas because it calls
 * no other BASIC function and takes no g as Game -- pure arithmetic plus
 * direct array/UDT access. That version took its arrays as BASARRAY
 * descriptors (BASIC's own far-array handle); this one reads world->
 * models/brush/clip/planes directly, matching every other cport/ module.
 * The recursive walk itself (pl_hull_check_c) is unchanged from that port.
 */

#include <math.h>

#include "pl_trace.h"
#include "item.h"
#include "mdl_ai.h"

static short near pl_box_axis( float lo, float hi, float start, float delta,
                               short axis, float *enter, float *leave,
                               short *enter_axis, float *enter_sign )
{
    float a, b, sign;

    if ( delta > -0.0001f && delta < 0.0001f )
        return (short) ( start >= lo && start < hi );
    a = ( lo - start ) / delta;
    b = ( hi - start ) / delta;
    sign = -1.0f;
    if ( a > b ) {
        float swap = a;
        a = b;
        b = swap;
        sign = 1.0f;
    }
    if ( a >= *enter ) {
        *enter = a;
        *enter_axis = axis;
        *enter_sign = sign;
    }
    if ( b < *leave ) *leave = b;
    return (short) ( *enter <= *leave );
}

/* Merge one Minkowski-expanded BBOX into a brush trace. Returning true
   means this box owns the trace identity, including a start-inside trace. */
static short near pl_trace_box( BspVec3 *start, BspVec3 *fin,
                                BspVec3 *mover_mins, BspVec3 *mover_maxs,
                                BspVec3 *target_mins, BspVec3 *target_maxs,
                                TraceResult *tr )
{
    BspVec3 lo, hi, delta;
    float enter = 0.0f, leave = 1.0f, sign = 0.0f;
    short axis = -1;
    short inside, end_inside;

    if ( tr->all_solid ) return 0;

    lo.x = target_mins->x - mover_maxs->x;
    lo.y = target_mins->y - mover_maxs->y;
    lo.z = target_mins->z - mover_maxs->z;
    hi.x = target_maxs->x - mover_mins->x;
    hi.y = target_maxs->y - mover_mins->y;
    hi.z = target_maxs->z - mover_mins->z;

    if ( ( start->x < lo.x && fin->x < lo.x ) ||
         ( start->x >= hi.x && fin->x >= hi.x ) ||
         ( start->y < lo.y && fin->y < lo.y ) ||
         ( start->y >= hi.y && fin->y >= hi.y ) ||
         ( start->z < lo.z && fin->z < lo.z ) ||
         ( start->z >= hi.z && fin->z >= hi.z ) ) return 0;

    inside = (short) ( start->x >= lo.x && start->x < hi.x &&
                       start->y >= lo.y && start->y < hi.y &&
                       start->z >= lo.z && start->z < hi.z );
    end_inside = (short) ( fin->x >= lo.x && fin->x < hi.x &&
                           fin->y >= lo.y && fin->y < hi.y &&
                           fin->z >= lo.z && fin->z < hi.z );
    if ( inside ) {
        tr->start_solid = 1;
        if ( end_inside ) {
            tr->frac = 0.0f;
            tr->end_pos = *start;
            tr->norm.x = tr->norm.y = tr->norm.z = 0.0f;
            tr->all_solid = 1;
            tr->hit_model = -1;
            return -1;
        }
        tr->frac = 1.0f;
        tr->end_pos = *fin;
        tr->norm.x = tr->norm.y = tr->norm.z = 0.0f;
        tr->all_solid = 0;
        tr->hit_model = -1;
        return -1;
    }

    delta.x = fin->x - start->x;
    delta.y = fin->y - start->y;
    delta.z = fin->z - start->z;
    if ( !pl_box_axis( lo.x, hi.x, start->x, delta.x, 0, &enter, &leave, &axis, &sign ) ||
         !pl_box_axis( lo.y, hi.y, start->y, delta.y, 1, &enter, &leave, &axis, &sign ) ||
         !pl_box_axis( lo.z, hi.z, start->z, delta.z, 2, &enter, &leave, &axis, &sign ) ) return 0;

    if ( axis == 0 ) enter -= PL_CLIP_EPS / (float) fabs( delta.x );
    else if ( axis == 1 ) enter -= PL_CLIP_EPS / (float) fabs( delta.y );
    else if ( axis == 2 ) enter -= PL_CLIP_EPS / (float) fabs( delta.z );
    if ( enter < 0.0f ) enter = 0.0f;
    if ( axis < 0 || enter >= tr->frac ) return 0;

    tr->frac = enter;
    tr->end_pos.x = start->x + delta.x * enter;
    tr->end_pos.y = start->y + delta.y * enter;
    tr->end_pos.z = start->z + delta.z * enter;
    tr->norm.x = tr->norm.y = tr->norm.z = 0.0f;
    tr->hit_model = -1;
    if ( axis == 0 ) tr->norm.x = sign;
    else if ( axis == 1 ) tr->norm.y = sign;
    else tr->norm.z = sign;
    return -1;
}

void pl_trace_items( World *world, BspVec3 *start, BspVec3 *fin,
                     BspVec3 *mover_mins, BspVec3 *mover_maxs,
                     short ignore_item, TraceResult *tr, short *hit_item )
{
    short i;

    if ( tr->all_solid ) return;

    for ( i = 0; i < world->item_count; i++ ) {
        ItemEnt far *it = &world->item[i];
        BspVec3 lo, hi;

        if ( i == ignore_item || it->gone || it->kind != ENT_ITEM_EXPLOBOX ) continue;
        lo = it->pos;
        hi.x = it->pos.x + ENT_BOX_CLIP_XY;
        hi.y = it->pos.y + ENT_BOX_CLIP_XY;
        hi.z = it->pos.z + ENT_BOX_CLIP_Z;
        if ( pl_trace_box( start, fin, mover_mins, mover_maxs,
                           &lo, &hi, tr ) && hit_item ) *hit_item = i;
        if ( tr->all_solid ) return;
    }
}

void pl_trace_actors( World *world, Player *solid_player,
                      MdlEnt far *ignore_mon,
                      BspVec3 *start, BspVec3 *fin,
                      BspVec3 *mover_mins, BspVec3 *mover_maxs,
                      TraceResult *tr, short *hit_actor )
{
    BspVec3 lo, hi;
    short i;

    if ( tr->all_solid ) return;
    if ( solid_player ) {
        lo.x = solid_player->pos.x - PL_HALF;
        lo.y = solid_player->pos.y - PL_HALF;
        lo.z = solid_player->pos.z + PL_ZLO;
        hi.x = solid_player->pos.x + PL_HALF;
        hi.y = solid_player->pos.y + PL_HALF;
        hi.z = solid_player->pos.z + PL_ZHI;
        if ( pl_trace_box( start, fin, mover_mins, mover_maxs,
                           &lo, &hi, tr ) && hit_actor )
            *hit_actor = PL_ACTOR_PLAYER;
    }
    for ( i = 0; i < world->mon_count && !tr->all_solid; i++ ) {
        MdlEnt far *ent = &world->mon[i];
        float half;

        if ( ent == ignore_mon || !MDL_BLOCKS_MOVE( ent ) ) continue;
        half = MDL_MON_HALF( ent->kind );
        lo.x = ent->pos.x - half;
        lo.y = ent->pos.y - half;
        lo.z = ent->pos.z + MDL_ZLO;
        hi.x = ent->pos.x + half;
        hi.y = ent->pos.y + half;
        hi.z = ent->pos.z + MDL_MON_ZHI( ent->kind );
        if ( pl_trace_box( start, fin, mover_mins, mover_maxs,
                           &lo, &hi, tr ) && hit_actor )
            *hit_actor = i;
    }
}

void pl_trace_move( World *world, BspVec3 *start, BspVec3 *fin,
                    short ignore_item, TraceResult *tr, short *hit_item )
{
    BspVec3 mins = { -PL_HALF, -PL_HALF, PL_ZLO };
    BspVec3 maxs = { PL_HALF, PL_HALF, PL_ZHI };
    short hit_actor = PL_ACTOR_NONE;

    if ( hit_item ) *hit_item = -1;
    pl_trace( world, start, fin, tr );
    pl_trace_items( world, start, fin, &mins, &maxs,
                    ignore_item, tr, hit_item );
    pl_trace_actors( world, (Player *) 0, (MdlEnt far *) 0,
                     start, fin, &mins, &maxs, tr, &hit_actor );
    if ( hit_actor != PL_ACTOR_NONE && hit_item ) *hit_item = -1;
}

static short near pl_point_contents_c( short node, BspVec3 far *p,
                                        World *world )
{
    while ( !( node & 0x8000 ) ) {
        Node far *n = &world->nodes[node];
        Plane far *pl = &world->planes[n->plane_id];
        float d = p->x * pl->norm.x + p->y * pl->norm.y +
                  p->z * pl->norm.z - pl->dist;
        node = d >= 0.0f ? n->child0 : n->child1;
    }
    return world->leaves[~node].cont;
}

static short near pl_point_check_c( short node, float p1f, float p2f,
                                    BspVec3 far *p1, BspVec3 far *p2,
                                    TraceResult *tr, World *world )
{
    short side, other, cont, pid;
    float t1, t2, frac, midf;
    BspVec3 midp;
    Node far *n;
    Plane far *pl;

    if ( node & 0x8000 ) {
        cont = world->leaves[~node].cont;
        if ( cont == CONTENTS_SOLID ) return 1;
        tr->all_solid = 0;
        if ( cont == CONTENTS_EMPTY ) tr->in_open = 1;
        if ( cont <= CONTENTS_WATER && cont >= CONTENTS_LAVA ) tr->in_water = 1;
        return 1;
    }

    n = &world->nodes[node];
    pid = n->plane_id;
    pl = &world->planes[pid];
    t1 = p1->x * pl->norm.x + p1->y * pl->norm.y + p1->z * pl->norm.z - pl->dist;
    t2 = p2->x * pl->norm.x + p2->y * pl->norm.y + p2->z * pl->norm.z - pl->dist;
    if ( t1 >= 0.0f && t2 >= 0.0f )
        return pl_point_check_c( n->child0, p1f, p2f, p1, p2, tr, world );
    if ( t1 < 0.0f && t2 < 0.0f )
        return pl_point_check_c( n->child1, p1f, p2f, p1, p2, tr, world );

    if ( t1 < 0.0f ) { frac = ( t1 + PL_CLIP_EPS ) / ( t1 - t2 ); side = 1; }
    else             { frac = ( t1 - PL_CLIP_EPS ) / ( t1 - t2 ); side = 0; }
    if ( frac < 0.0f ) frac = 0.0f;
    if ( frac > 1.0f ) frac = 1.0f;
    midf = p1f + ( p2f - p1f ) * frac;
    midp.x = p1->x + ( p2->x - p1->x ) * frac;
    midp.y = p1->y + ( p2->y - p1->y ) * frac;
    midp.z = p1->z + ( p2->z - p1->z ) * frac;

    if ( side == 0 ) {
        if ( !pl_point_check_c( n->child0, p1f, midf, p1, &midp, tr, world ) ) return 0;
        other = n->child1;
    } else {
        if ( !pl_point_check_c( n->child1, p1f, midf, p1, &midp, tr, world ) ) return 0;
        other = n->child0;
    }
    if ( pl_point_contents_c( other, &midp, world ) != CONTENTS_SOLID )
        return pl_point_check_c( other, midf, p2f, &midp, p2, tr, world );
    if ( tr->all_solid ) { tr->start_solid = 1; return 0; }
    if ( tr->frac > midf ) {
        tr->frac = midf;
        tr->end_pos = midp;
        tr->norm.x = side ? -pl->norm.x : pl->norm.x;
        tr->norm.y = side ? -pl->norm.y : pl->norm.y;
        tr->norm.z = side ? -pl->norm.z : pl->norm.z;
    }
    return 0;
}

static short near pl_hull_contents_c( short node, BspVec3 far *p, ClipNode far *clip, Plane far *planes )
{
    short pid;
    float d;

    while ( node >= 0 ) {
        pid = clip[node].plane_num;
        d = p->x * planes[pid].norm.x + p->y * planes[pid].norm.y +
            p->z * planes[pid].norm.z - planes[pid].dist;
        if ( d >= 0.0f ) node = clip[node].front;
        else             node = clip[node].back;
    }

    return node;
}

/* Same epsilon-backed split at the plane crossing as pl_hull_contents_c's
   caller expects, same near-half-first order (an earlier hit wins), and
   clip[node] is re-read fresh after the near-half recursion returns
   rather than cached -- it's a plain array index each time, never
   hoisted into a local, which is what the original BASIC's own comment
   ("the recursion above moved the window; map before reading again")
   was protecting against under BASIC's EMS-mapped arrays. Nothing here
   is EMS-backed, but the read order is kept identical regardless. */
static short near pl_hull_check_c(
    short node, float p1f, float p2f,
    BspVec3 far *p1, BspVec3 far *p2,
    TraceResult *tr,
    ClipNode far *clip, Plane far *planes
)
{
    short pid, side, other, descent = 0;
    float t1, t2, frac, midf;
    BspVec3 midp;

    for ( ; ; ) {
        if ( node < 0 ) {
            tr->all_solid = ( node == CONTENTS_SOLID ) ? 1 : 0;
            if ( node == CONTENTS_SOLID ) tr->start_solid = 1;
            return 1;
        }
        if ( ++descent > 256 ) return 0;

        pid = clip[node].plane_num;
        t1 = p1->x * planes[pid].norm.x + p1->y * planes[pid].norm.y +
             p1->z * planes[pid].norm.z - planes[pid].dist;
        t2 = p2->x * planes[pid].norm.x + p2->y * planes[pid].norm.y +
             p2->z * planes[pid].norm.z - planes[pid].dist;

        if ( t1 >= 0.0f && t2 >= 0.0f ) node = clip[node].front;
        else if ( t1 < 0.0f && t2 < 0.0f ) node = clip[node].back;
        else break;
    }

    if ( t1 < 0.0f ) {
        frac = (t1 + PL_CLIP_EPS) / (t1 - t2);
        side = 1;
    } else {
        frac = (t1 - PL_CLIP_EPS) / (t1 - t2);
        side = 0;
    }
    if ( frac < 0.0f ) frac = 0.0f;
    if ( frac > 1.0f ) frac = 1.0f;

    midf   = p1f + (p2f - p1f) * frac;
    midp.x = p1->x + (p2->x - p1->x) * frac;
    midp.y = p1->y + (p2->y - p1->y) * frac;
    midp.z = p1->z + (p2->z - p1->z) * frac;

    if ( side == 0 ) {
        if ( !pl_hull_check_c( clip[node].front, p1f, midf, p1, &midp, tr, clip, planes ) )
            return 0;
        other = clip[node].back;
    } else {
        if ( !pl_hull_check_c( clip[node].back, p1f, midf, p1, &midp, tr, clip, planes ) )
            return 0;
        other = clip[node].front;
    }

    if ( pl_hull_contents_c( other, &midp, clip, planes ) != CONTENTS_SOLID )
        return pl_hull_check_c( other, midf, p2f, &midp, p2, tr, clip, planes );

    if ( tr->all_solid ) {
        tr->start_solid = 1;
        return 0;
    }

    if ( tr->frac > midf ) {
        tr->frac    = midf;
        tr->end_pos = midp;
        if ( side == 1 ) {
            tr->norm.x = -planes[pid].norm.x;
            tr->norm.y = -planes[pid].norm.y;
            tr->norm.z = -planes[pid].norm.z;
        } else {
            tr->norm.x = planes[pid].norm.x;
            tr->norm.y = planes[pid].norm.y;
            tr->norm.z = planes[pid].norm.z;
        }
    }

    return 0;
}

/*
 * A brush entity is traced by moving the LINE rather than the hull:
 * its tree sits where the map compiled it, so subtracting the entity's
 * its offset from both ends of the sweep asks the same question of a
 * stationary tree that moving the tree would ask of a stationary line.
 *
 * tr keeps the earliest hit by itself -- pl_hull_check_c only writes
 * when it beats tr->frac -- so the hulls can be walked in any order.
 * all_solid is the exception: each walk sets it, so it's gathered by
 * hand into any_solid.
 */
void pl_trace( World *world, BspVec3 *start, BspVec3 *fin, TraceResult *tr )
{
    short i;
    BspVec3 s2, f2;

    tr->frac        = 1.0f;
    tr->end_pos     = *fin;
    tr->norm.x      = 0.0f;
    tr->norm.y      = 0.0f;
    tr->norm.z      = 0.0f;
    tr->start_solid = 0;
    tr->in_open = tr->in_water = 0;
    tr->hit_model = -1;

    tr->all_solid = 1;
    pl_hull_check_c( (short) world->models[0].head_node1, 0.0f, 1.0f, start, fin, tr, world->clip, world->planes );
    if ( tr->frac < 1.0f || tr->start_solid ) tr->hit_model = 0;

    for ( i = 1; i < world->model_count && !tr->all_solid; i++ ) {
        if ( world->brush[i].solid ) {
            TraceResult br;
            short old_start_solid;

            s2 = *start;
            f2 = *fin;
            s2.x -= world->brush[i].ofs.x;
            s2.y -= world->brush[i].ofs.y;
            s2.z -= world->brush[i].ofs.z;
            f2.x -= world->brush[i].ofs.x;
            f2.y -= world->brush[i].ofs.y;
            f2.z -= world->brush[i].ofs.z;

            br.frac = 1.0f;
            br.end_pos = f2;
            br.norm.x = br.norm.y = br.norm.z = 0.0f;
            br.start_solid = 0;
            br.in_open = br.in_water = 0;
            br.hit_model = -1;
            br.all_solid = 1;
            pl_hull_check_c( (short) world->models[i].head_node1,
                             0.0f, 1.0f, &s2, &f2, &br,
                             world->clip, world->planes );
            if ( br.all_solid || br.start_solid || br.frac < tr->frac ) {
                old_start_solid = tr->start_solid;
                tr->frac = br.frac;
                tr->norm = br.norm;
                tr->all_solid = br.all_solid;
                tr->start_solid = (short) ( old_start_solid || br.start_solid );
                tr->in_open = br.in_open;
                tr->in_water = br.in_water;
                tr->hit_model = i;
            }
        }
    }

    tr->end_pos.x = start->x + (fin->x - start->x) * tr->frac;
    tr->end_pos.y = start->y + (fin->y - start->y) * tr->frac;
    tr->end_pos.z = start->z + (fin->z - start->z) * tr->frac;
}

void pl_trace_point( World *world, BspVec3 *start, BspVec3 *fin, TraceResult *tr )
{
    short i, any_solid;
    BspVec3 s2, f2;
    TraceResult br;

    tr->frac = 1.0f;
    tr->end_pos = *fin;
    tr->norm.x = tr->norm.y = tr->norm.z = 0.0f;
    tr->start_solid = 0;
    tr->in_open = tr->in_water = 0;
    tr->hit_model = -1;
    tr->all_solid = 1;
    pl_point_check_c( world->models[0].head_node0, 0.0f, 1.0f,
                      start, fin, tr, world );
    any_solid = tr->all_solid;
    if ( tr->frac < 1.0f ) tr->hit_model = 0;

    for ( i = 1; i < world->model_count; i++ ) {
        Submodel far *sm;
        BspVec3 far *ofs;
        if ( !world->brush[i].solid ) continue;
        sm = &world->models[i];
        ofs = &world->brush[i].ofs;
        if ( ( start->x < sm->mins.x + ofs->x && fin->x < sm->mins.x + ofs->x ) ||
             ( start->x > sm->maxs.x + ofs->x && fin->x > sm->maxs.x + ofs->x ) ||
             ( start->y < sm->mins.y + ofs->y && fin->y < sm->mins.y + ofs->y ) ||
             ( start->y > sm->maxs.y + ofs->y && fin->y > sm->maxs.y + ofs->y ) ||
             ( start->z < sm->mins.z + ofs->z && fin->z < sm->mins.z + ofs->z ) ||
             ( start->z > sm->maxs.z + ofs->z && fin->z > sm->maxs.z + ofs->z ) ) continue;
        s2 = *start; f2 = *fin;
        s2.x -= world->brush[i].ofs.x; f2.x -= world->brush[i].ofs.x;
        s2.y -= world->brush[i].ofs.y; f2.y -= world->brush[i].ofs.y;
        s2.z -= world->brush[i].ofs.z; f2.z -= world->brush[i].ofs.z;
        br.frac = 1.0f; br.end_pos = f2;
        br.norm.x = br.norm.y = br.norm.z = 0.0f;
        br.start_solid = 0; br.in_open = br.in_water = 0;
        br.hit_model = -1; br.all_solid = 1;
        pl_point_check_c( world->models[i].head_node0, 0.0f, 1.0f,
                          &s2, &f2, &br, world );
        if ( br.frac < tr->frac ) {
            tr->frac = br.frac;
            tr->norm = br.norm;
            tr->hit_model = i;
        }
        if ( br.start_solid ) tr->start_solid = 1;
        if ( br.all_solid ) any_solid = 1;
    }
    tr->all_solid = any_solid;
    if ( tr->frac < 1.0f ) {
        tr->end_pos.x = start->x + ( fin->x - start->x ) * tr->frac;
        tr->end_pos.y = start->y + ( fin->y - start->y ) * tr->frac;
        tr->end_pos.z = start->z + ( fin->z - start->z ) * tr->frac;
    }
}
