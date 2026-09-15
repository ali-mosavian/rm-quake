#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "world.h"
#include "renderer.h"
#include "pl_move.h"
#include "fight.h"

/*
 * client.h -- the player's side of the game's state, client.qc: the kit,
 * dying and respawning, the level put back, the kit carried to the next
 * map and the intermission's view. host.c's state machine drives them.
 */

/* the kit a level starts with, and the player back at the spawn */
void pl_reset_player( Player *player, Fight *fight, Renderer *rdr );

/* one death counted, then the kit again; the world keeps whatever the
   player did to it -- Quake's own respawn */
void pl_respawn( Player *player, Fight *fight, Renderer *rdr );

/* the fight again: every monster back at its spawn, every pickup back,
   the player at the start. Kills and deaths keep counting. */
void pl_game_reset( World *world, Player *player, Camera *cam, Fight *fight,
                     Renderer *rdr, short scr_x_res );

/* SetChangeParms and DecodeLevelParms: the kit that travels to the next
   map, through CARRY.BIN. Keys and powerups stay behind. */
void  pl_carry_save( Fight *fight );
/* read before the map loads, since the skill picks its ents; 0 when
   there is none or it is short */
short pl_carry_read( PlayerCarry *c );
void  pl_carry_apply( Fight *fight, const PlayerCarry *c );

/* execute_changelevel's view: the camera at the map's info_intermission,
   at its mangle, the player held there */
void pl_intermission( Player *player, Camera *cam, Fight *fight,
                       short scr_x_res, short scr_y_res );

#endif
