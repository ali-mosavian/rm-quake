#ifndef __MOVER_H__
#define __MOVER_H__

#include "world.h"
#include "pl_move.h"

#define ENT_RIDER_DEAD 0
#define ENT_RIDER_LIVE 1
#define ENT_RIDER_HEAD 2

short ent_mover_supported( Player *player, BspVec3 far *mins,
                           BspVec3 far *maxs, BrushModel far *brush,
                           short head );
short ent_mover_intersects( World *world, Player *player,
                            BspVec3 far *mins, BspVec3 far *maxs,
                            BrushModel far *brush, short head );
short ent_mover_push( World *world, Player *player, BspVec3 far *mins,
                      BspVec3 far *maxs, BrushModel far *brush,
                      BspVec3 *was, short riding, short rider_kind,
                      PlayerTouches far *touches );
void ent_mover_damage( World *world, Player *player, Fight *fight,
                       Renderer *rdr, BspVec3 far *mins, BspVec3 far *maxs,
                       BrushModel far *brush, float damage );

#endif
