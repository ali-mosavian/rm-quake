/* mover.c -- player transport shared by BSP pushers. */

#include "mover.h"
#include <math.h>
#include "pl_trace.h"
#include "combat.h"

static short near ent_mover_xy( Player *player, BspVec3 far *mins,
                                BspVec3 far *maxs, BrushModel far *brush )
{
    if ( player->pos.x + 16.0f < mins->x + brush->ofs.x ) return 0;
    if ( player->pos.x - 16.0f > maxs->x + brush->ofs.x ) return 0;
    if ( player->pos.y + 16.0f < mins->y + brush->ofs.y ) return 0;
    if ( player->pos.y - 16.0f > maxs->y + brush->ofs.y ) return 0;
    return -1;
}

short ent_mover_supported( Player *player, BspVec3 far *mins,
                           BspVec3 far *maxs, BrushModel far *brush,
                           short head )
{
    float bottom, top;

    if ( !player->on_ground || !ent_mover_xy( player, mins, maxs, brush ) ) return 0;
    bottom = player->pos.z - ( head ? 0.0f : PL_FEET );
    top = maxs->z + brush->ofs.z;
    return (short) ( bottom >= top - 8.0f && bottom <= top + 8.0f );
}

short ent_mover_intersects( World *world, Player *player,
                            BspVec3 far *mins, BspVec3 far *maxs,
                            BrushModel far *brush, short head )
{
    float bottom, top;
    BspVec3 org;
    TraceResult tr;

    if ( !ent_mover_xy( player, mins, maxs, brush ) ) return 0;
    bottom = player->pos.z - ( head ? 0.0f : PL_FEET );
    top = player->pos.z + ( head ? 56.0f : 32.0f );
    if ( top < mins->z + brush->ofs.z ) return 0;
    if ( bottom > maxs->z + brush->ofs.z ) return 0;

    org = player->pos;
    if ( head ) org.z += PL_FEET;
    pl_trace( world, &org, &org, &tr );
    return tr.all_solid;
}

short ent_mover_push( World *world, Player *player, BspVec3 far *mins,
                      BspVec3 far *maxs, BrushModel far *brush,
                      BspVec3 *was, short riding, short rider_kind,
                      PlayerTouches far *touches )
{
    BspVec3 saved, start, fin;
    TraceResult tr;
    short head = (short) ( rider_kind == ENT_RIDER_HEAD );
    short solid;

    if ( player->no_clip ) return -1;
    if ( !riding )
        riding = ent_mover_intersects( world, player, mins, maxs, brush, head );
    if ( !riding ) return -1;
    if ( rider_kind != ENT_RIDER_LIVE ) player->on_ground = 0;

    saved = player->pos;
    start = player->pos;
    if ( head ) start.z += PL_FEET;
    fin.x = start.x + brush->ofs.x - was->x;
    fin.y = start.y + brush->ofs.y - was->y;
    fin.z = start.z + brush->ofs.z - was->z;
    solid = brush->solid;
    brush->solid = 0;
    if ( rider_kind == ENT_RIDER_LIVE )
        pl_trace_move( world, &start, &fin, -1, &tr, (short *) 0 );
    else
        pl_trace( world, &start, &fin, &tr );
    brush->solid = solid;
    if ( rider_kind == ENT_RIDER_LIVE &&
         ( tr.frac < 1.0f || tr.start_solid ) )
        pl_touches_add( touches, tr.hit_model );
    player->pos = tr.end_pos;
    if ( head ) player->pos.z -= PL_FEET;
    start = player->pos;
    if ( head ) start.z += PL_FEET;
    if ( rider_kind == ENT_RIDER_LIVE )
        pl_trace_move( world, &start, &start, -1, &tr, (short *) 0 );
    else
        pl_trace( world, &start, &start, &tr );
    if ( tr.all_solid ) {
        if ( rider_kind != ENT_RIDER_LIVE ) return -1;
        player->pos = saved;
        return 0;
    }
    return -1;
}

void ent_mover_damage( World *world, Player *player, Fight *fight,
                       Renderer *rdr, BspVec3 far *mins, BspVec3 far *maxs,
                       BrushModel far *brush, float damage )
{
    float dx = player->pos.x - ( mins->x + maxs->x ) * 0.5f - brush->ofs.x;
    float dy = player->pos.y - ( mins->y + maxs->y ) * 0.5f - brush->ofs.y;
    float dz = player->pos.z - ( mins->z + maxs->z ) * 0.5f - brush->ofs.z;
    float len = (float) sqrt( dx*dx + dy*dy + dz*dz );
    float kick = damage * 8.0f;

    if ( len > 0.0f ) {
        player->vel.x += dx / len * kick;
        player->vel.y += dy / len * kick;
        player->vel.z += dz / len * kick;
    }
    combat_damage_player( player, world, fight, rdr, damage );
}
