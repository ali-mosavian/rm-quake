#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "world.h"
#include "renderer.h"
#include "pl_move.h"
#include "fight.h"

#define PL_DEATH_A   0
#define PL_DEATH_B   1
#define PL_DEATH_C   2
#define PL_DEATH_D   3
#define PL_DEATH_E   4
#define PL_DEATH_AXE 5

#define PL_DEATH_DYING   0
#define PL_DEATH_RELEASE 1
#define PL_DEATH_READY   2

typedef struct {
    short set, frame, phase;
    float next_frame;
} PlayerDeath;

/*
 * client.h -- the player's side of the game's state, client.qc: the kit,
 * dying and respawning, the level put back, the kit carried to the next
 * map and the intermission's view. host.c's state machine drives them.
 */

/* the kit a level starts with, and the player back at the spawn */
void pl_reset_player( Player *player, Fight *fight, Renderer *rdr );
void pl_weapon_flash( Fight *fight, float now );

short pl_death_choose( short weapon, float random );
void pl_death_begin( PlayerDeath *death, Player *player, Fight *fight,
                     short set, float kick, float now );
short pl_death_tick( PlayerDeath *death, float now, short advance );
void pl_death_reset( PlayerDeath *death );
short pl_death_selftest( void );

/* the fight again: every monster back at its spawn, every pickup back,
   the player at the start. Kills and deaths keep counting. */
void pl_game_reset( World *world, Player *player, Camera *cam, Fight *fight,
                     Renderer *rdr, short scr_x_res );

/* SetChangeParms and DecodeLevelParms: the kit that travels to the next
   map. CARRY.BIN remains the command-line compatibility path. */
void  pl_carry_capture( PlayerCarry *c, const Fight *fight );
/* read before the map loads, since the skill picks its ents; 0 when
   there is none or it is short */
short pl_carry_read( PlayerCarry *c );
void  pl_carry_apply( Fight *fight, const PlayerCarry *c );

/* execute_changelevel's view: the camera at the map's info_intermission,
   at its mangle, the player held there */
void pl_intermission( Player *player, Camera *cam, Fight *fight,
                       short scr_x_res, short scr_y_res );

#endif
