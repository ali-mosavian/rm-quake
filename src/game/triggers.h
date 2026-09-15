#ifndef __TRIGGERS_H__
#define __TRIGGERS_H__

#include "world.h"
#include "renderer.h"
#include "fight.h"

/*
 * name: ent_use_targets
 * desc: SUB_UseTargets. Fires everything whose name is id: a door's
 *       group goes, a waiting train starts, a counter counts down, a
 *       relay passes it on. id 0 is none.
 */
void ent_use_targets( World *world, Player *player, Fight *fight,
                       Renderer *rdr, short id );

/*
 * name: ent_move_trigs
 * desc: The touches, and every button's travel. A button fires when it
 *       ARRIVES, not when it is pressed, as button_wait does.
 */
void ent_move_trigs( World *world, Player *player, Camera *cam, Fight *fight,
                      Renderer *rdr, float dt, short scr_x_res, short scr_y_res );

/* A trigger's target fires. Public because a pellet or a nail fires a
   shootable trigger. */
void ent_trig_fire( World *world, Player *player, Fight *fight,
                     Renderer *rdr, short k );

/*
 * name: ent_say
 * desc: centerprint: msg is shown for ENT_MSG_TIME. An empty one is
 *       not a message and clears nothing.
 */
void ent_say( Fight *fight, Renderer *rdr, char far *msg );
void ent_talk( Player *player, Sound *snd, Fight *fight, Renderer *rdr, char far *msg );

/* misc_fireball: its first ball 0..5 s from now. ent_fireballs_sent
   counts every emitter's balls, for the run summary. */
void  ent_fireball_spawn( World *world, TrigEnt far *t );
short ent_fireballs_sent( World *world );

/*
 * name: ent_monster_jump
 * desc: trigger_monsterjump_touch on monster e, called when it moved:
 *       a walker in one takes its push, and one on the ground its throw
 *       up. World.mon_jumps counts the throws that flew.
 */
void  ent_monster_jump( World *world, MdlEnt far *e );

#endif
