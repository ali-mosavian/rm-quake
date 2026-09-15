#ifndef __PLATS_H__
#define __PLATS_H__

#include "world.h"

short ent_plat_touched( Player *player, World *world, short p );
void  ent_move_plats( World *world, Player *player, float dt );

/*
 * name: ent_move_trains
 * desc: Every func_train one leg on, carrying its rider.
 */
void ent_move_trains( World *world, Player *player, float dt );

/*
 * name: ent_train_init
 * desc: func_train_find: the brush's mins on its first corner. One with
 *       a targetname waits there for its trigger; the rest go at once.
 */
void ent_train_init( World *world, PlatEnt far *p );

#endif
