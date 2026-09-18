/*
 * client.c -- see client.h.
 */

#include "client.h"
#include "mdl_ai.h"
#include "item.h"
#include "qgl.h"
#include <stdio.h>

void pl_weapon_flash( Fight *fight, float now )
{
    static short bits[6] = { PL_IT_SHOTGUN, PL_IT_SSG, PL_IT_NAILGUN,
                              PL_IT_SNG, PL_IT_GL, PL_IT_RL };
    short i;

    for ( i = 0; i < 6; i++ )
        fight->weapon_at[i] = fight->items & bits[i] ? now : -1.0f;
}

void pl_reset_player( Player *player, Fight *fight, Renderer *rdr )
{
    fight->health     = PL_HEALTH;
    fight->shells     = PL_SHELLS;
    fight->armor      = 0;
    fight->armor_type = 0.0f;
    fight->secrets    = 0;
    fight->items      = PL_IT_SHOTGUN | PL_IT_AXE;
    fight->weapon     = PL_IT_SHOTGUN;
    pl_weapon_flash( fight, rdr->anim_time );
    fight->face_until = 0.0f;
    fight->nails      = 0;
    fight->rockets    = 0;
    fight->cells      = 0;
    fight->quad_until = 0.0f;
    fight->suit_until = 0.0f;
    fight->pent_until = 0.0f;
    fight->invis_until = 0.0f;
    fight->invis_snd_at = 0.0f;
    fight->next_fire  = 0.0f;
    fight->show_hostile = 0.0f;
    fight->rot_at     = 0.0f;
    player->pos = fight->spawn;
    player->vel.x = player->vel.y = player->vel.z = 0.0f;
    player->fall_vz = 0.0f;
    player->water_dir.x = player->water_dir.y = player->water_dir.z = 0.0f;
    player->water_until = 0.0f;
}

void pl_respawn( Player *player, Fight *fight, Renderer *rdr )
{
    fight->deaths++;
    pl_reset_player( player, fight, rdr );
}

/* SetChangeParms: keys and powerups are the level's own and do not travel. */
void pl_carry_capture( PlayerCarry *c, const Fight *fight )
{
    c->items   = fight->items & ~( (long) ( PL_IT_KEY1 | PL_IT_KEY2 ) );
    c->health  = fight->health;
    if ( c->health > PL_HEALTH )    c->health = PL_HEALTH;
    if ( c->health < PL_CARRY_MIN ) c->health = PL_CARRY_MIN;
    c->armor      = fight->armor;
    c->armor_type = fight->armor_type;
    c->shells     = fight->shells;
    if ( c->shells < PL_SHELLS ) c->shells = PL_SHELLS;
    c->nails   = fight->nails;
    c->rockets = fight->rockets;
    c->cells   = fight->cells;
    c->weapon  = fight->weapon;
    c->skill   = fight->skill;
    c->serverflags = fight->serverflags;
}

/* DecodeLevelParms: -carry reads it back over pl_reset_player's kit.
   A short or missing file leaves the starting kit alone rather than
   filling the player with whatever the read did not overwrite. */
short pl_carry_read( PlayerCarry *c )
{
    FILE *f = fopen( "CARRY.BIN", "rb" );
    short ok;

    if ( !f ) return 0;
    ok = (short) ( fread( c, sizeof(*c), 1, f ) == 1 );
    fclose( f );
    return ok;
}

void pl_carry_apply( Fight *fight, const PlayerCarry *c )
{
    fight->items      = c->items;
    fight->health     = c->health;
    fight->armor      = c->armor;
    fight->armor_type = c->armor_type;
    fight->shells     = c->shells;
    fight->nails      = c->nails;
    fight->rockets    = c->rockets;
    fight->cells      = c->cells;
    fight->weapon     = c->weapon;
}

/* execute_changelevel's intermission: the view from the map's
   info_intermission, at its mangle, the player held there in noclip.
   Quake's pitch is positive down, ours up. */
void pl_intermission( Player *player, Camera *cam, Fight *fight,
                       short scr_x_res, short scr_y_res )
{
    float yaw;

    player->no_clip = -1;
    player->pos = fight->inter;
    player->vel.x = player->vel.y = player->vel.z = 0.0f;
    player->fall_vz = 0.0f;
    player->water_until = 0.0f;
    cam->pos.x = fight->inter.x;
    cam->pos.z = fight->inter.y;
    cam->pos.y = fight->inter.z;

    yaw = 360.0f - fight->inter_yaw;
    if ( yaw >= 360.0f ) yaw -= 360.0f;
    qglMousePos( (short) ( ( scr_x_res - 1 ) * yaw / 360.0f ),
                 (short) ( scr_y_res * ( 90.0f + fight->inter_pitch ) / 180.0f - 2.0f ) );
}

void pl_game_reset( World *world, Player *player, Camera *cam, Fight *fight,
                     Renderer *rdr, short scr_x_res )
{
    short i;

    for ( i = 0; i < world->mon_count; i++ ) {
        world->mon[i].pos = world->mon[i].spawn;
        mdl_spawn( world, &world->mon[i] );
    }
    /* the map's own pickups back; a backpack a soldier dropped is not
       one of them, so the count goes back to what ents.bin held */
    world->item_count = (short) ( world->item_max - world->mon_count );
    for ( i = 0; i < world->item_count; i++ ) world->item[i].gone = 0;
    for ( i = 0; i < PL_NAILS_MAX; i++ ) fight->nail[i].alive = 0;

    pl_reset_player( player, fight, rdr );
    /* on foot, facing the map's way: the camera reads its angle from
       the mouse, so the mouse is what has to move */
    player->no_clip = 0;
    fight->serverflags = world->serverflags;
    fight->level_start = rdr->anim_time;
    qglMousePos( (short) ( ( scr_x_res - 1 ) * cam->start_angle / 360.0f ), 110 );
}
