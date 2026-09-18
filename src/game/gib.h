#ifndef __GIB_H__
#define __GIB_H__

#include "world.h"
#include "renderer.h"
#include "gibtypes.h"
#include "pl_move.h"
#include "fight.h"

void gib_spawn_monster( World *world, Renderer *rdr, MdlEnt far *ent, float now );
void gib_spawn_player( World *world, Renderer *rdr, Player *player,
                       Fight *fight, float now );
void gib_tick( World *world, Renderer *rdr, float dt );
void gib_clear( Renderer *rdr );
GibEnt far *gib_map( Renderer *rdr, short slot );

#endif
