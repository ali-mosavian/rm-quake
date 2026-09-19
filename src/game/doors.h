#ifndef __DOORS_H__
#define __DOORS_H__

#include "world.h"
#include "renderer.h"
#include "fight.h"
#include "pl_move.h"

/*
 * name: ent_move_doors
 * desc: The touch fields, then every door's state machine. A door with
 *       a targetname waits for ent_use_targets and only says its
 *       message when touched; one with a key wants the key.
 */
void ent_move_doors( World *world, Player *player, Fight *fight,
                      Renderer *rdr, PlayerTouches far *touches,
                      float dt, short rider_kind );

void ent_doors_touch( World *world, Player *player, Fight *fight,
                      Renderer *rdr, PlayerTouches far *touches );

/*
 * name: ent_link_doors
 * desc: LinkDoors: doors whose brushes touch open as one group, unless
 *       DOOR_DONT_LINK. Run once, after every door is loaded.
 */
void ent_link_doors( World *world );

/* A door's linked group sets out. Public because a pellet or a nail
   opens a secret door. */
void ent_door_fire( World *world, Player *player, Fight *fight,
                    Renderer *rdr, short grp );

/* door_go_down for one door, used when Chthon's electrode hold ends. */
void ent_door_close( World *world, Player *player, short k );

#endif
