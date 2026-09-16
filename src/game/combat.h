#ifndef __COMBAT_H__
#define __COMBAT_H__

#include "world.h"
#include "renderer.h"
#include "pl_move.h"
#include "fight.h"

/* combat.h -- combat.qc's T_Damage, on either side. */

/*
 * name: combat_damage_player
 * desc: T_Damage on the player: the pentagram, then the armor's share,
 *       then the health.
 */
void combat_damage_player( Player *player, World *world, Fight *fight, Renderer *rdr, float dmg );

/*
 * name: combat_damage_monster
 * desc: T_Damage on a monster: its death at 0 -- and a soldier's
 *       backpack -- or its pain, gated by pain_finished, and the player
 *       is now its enemy.
 */
void combat_damage_monster( World *world, Player *player, Fight *fight, Renderer *rdr,
                  MdlEnt far *ent, float dmg );

/*
 * name: combat_radius
 * desc: T_RadiusDamage from org: the player, then every standing monster
 *       unless a monster threw it. By the player, the player takes half.
 */
#define COMBAT_BY_WORLD  (-1)
#define COMBAT_BY_PLAYER (-2)
#define COMBAT_NO_IGNORE (-1)
void combat_radius( World *world, Player *player, Fight *fight, Renderer *rdr,
                    BspVec3 org, float dmg, short attacker, short ignore );
void combat_damage_box( World *world, Player *player, Fight *fight,
                        Renderer *rdr, short i, float dmg );
short combat_selftest( void );
short combat_brush_selftest( void );

#endif
