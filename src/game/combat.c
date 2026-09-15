/*
 * combat.c -- combat.qc's T_Damage: what a hit does to the player and
 * to a monster.
 */

#include <math.h>

#include "combat.h"
#include "snd.h"
#include "mdl_ai.h"
#include "item.h"

void combat_damage_player( Player *player, World *world, Fight *fight, Renderer *rdr, short dmg )
{
    Rng   *rng = &world->rng;
    Sound *snd = &world->snd;
    short save;
    float prod;

    /* T_Damage: the armor takes ceil(type * damage), and its last point
       takes the type with it. QuakeC stores the product as a float32 --
       0.8 * 10 is 8.00000012 at 80 bits, 8.0 stored -- and a cast chops
       (F_FTOL@), so ceil() it is */
    prod = fight->armor_type * dmg;
    save = (short) ceil( prod );
    if ( save >= fight->armor ) { save = fight->armor; fight->armor_type = 0.0f; }
    fight->armor = (short) ( fight->armor - save );

    /* then the pentagram stops the rest -- the armor above still wore --
       and protect3 says so no more than two seconds apart */
    if ( rdr->anim_time < fight->pent_until ) {
        if ( rdr->anim_time >= fight->pent_at ) {
            fight->pent_at = rdr->anim_time + 2.0f;
            snd_play( snd, player, SND_PENT_HIT, &player->pos );
        }
        return;
    }
    fight->health = (short) ( fight->health - ( dmg - save ) );

    /* PainSound: drowning under water, a burn in slime or lava -- both
       every hit -- else one of six grunts a half second apart at most,
       rint(random() * 5 + 1) so the first and last half as often */
    if ( fight->health <= 0 ) return;
    if ( rdr->anim_time < fight->invis_until ) return;   /* player_pain: the eyes have no pain frames */
    if ( player->water_level == 3 && player->water_type == CONTENTS_WATER ) {
        snd_self( snd, player, CHAN_VOICE, (short) ( SND_DROWN1 + ( rng_int( rng ) & 1 ) ) );
        return;
    }
    if ( player->water_level > 0 && player->water_type != CONTENTS_WATER ) {
        snd_self( snd, player, CHAN_VOICE, (short) ( SND_BURN1 + ( rng_int( rng ) & 1 ) ) );
        return;
    }
    if ( rdr->anim_time < fight->pain_at ) return;
    fight->pain_at = rdr->anim_time + PL_PAIN_GAP;
    save = (short) ( rng_float( rng ) * 5.0f + 0.5f );
    snd_self( snd, player, CHAN_VOICE, (short) ( save < 3 ? SND_PAIN1 + save : SND_PAIN4 + save - 3 ) );
}

void combat_damage_monster( World *world, Player *player, Fight *fight, Renderer *rdr,
                  MdlEnt far *ent, short dmg )
{
    const MonsterFacts far *f = &mon_facts[ ent->kind ];
    float r;

    if ( ent->cruc ) return;    /* never walkmonster_start: takedamage stays DAMAGE_NO */
    ent->health = (short) ( ent->health - dmg );
    if ( ent->health <= 0 ) {
        ent->state = MDL_ST_DEAD;
        ent->anim_frame = 0;
        if ( ent->health < f->gib_below )
            mdl_voice( player, &world->snd, ent, CHAN_VOICE, SND_UDEATH, ATTN_NORM );
        else
            mdl_say( player, &world->snd, ent, 3 );
        fight->kills++;
        if ( f->backpack )
            pl_item_add( world, ENT_ITEM_SHELLS, ENT_BACKPACK, &ent->pos );
        return;
    }
    ent->hunting = -1;
    ent->ideal_yaw = mdl_vectoyaw( player->pos.x - ent->pos.x,
                                    player->pos.y - ent->pos.y );

    if ( f->pain_resets ) {
        /* zombie_pain: z_pain at every hit, the health always back, so
           only one hit of 60 kills; under 9 ignored, 25 or more drops it
           for three seconds. Half the flinches are painb and painc, whose
           frames say z_pain1. */
        ent->health = ZOMBIE_HEALTH;
        mdl_say( player, &world->snd, ent, 2 );
        if ( dmg < ZOMBIE_PAIN_MIN ) return;
        if ( rdr->anim_time < ent->pain_finished ) return;
        ent->pain_finished = rdr->anim_time +
            ( dmg >= ZOMBIE_FALL_DMG ? ZOMBIE_FALL_TIME : ZOMBIE_FLINCH );
        r = rng_float( &world->rng );
        if ( dmg < ZOMBIE_FALL_DMG && r >= 0.25f && r < 0.75f )
            mdl_voice( player, &world->snd, ent, CHAN_VOICE, SND_Z_PAIN1, ATTN_NORM );
        ent->state = MDL_ST_PAIN;
        ent->anim_frame = 0;
        return;
    }

    if ( rdr->anim_time < ent->pain_finished ) return;
    if ( f->pain_time > 0.0f ) {
        ent->pain_finished = rdr->anim_time + f->pain_time;
    } else {
        r = rng_float( &world->rng );
        ent->pain_finished = rdr->anim_time +
            ( r < MDL_PAIN_SHORT_P ? MDL_PAIN_SHORT : MDL_PAIN_LONG );
        /* army_pain: pain1 with the short flinch, pain2 with the others */
        if ( f->pain_long_snd && r >= MDL_PAIN_SHORT_P ) {
            mdl_voice( player, &world->snd, ent, CHAN_VOICE, f->pain_long_snd, ATTN_NORM );
            ent->state = MDL_ST_PAIN;
            ent->anim_frame = 0;
            return;
        }
    }
    mdl_say( player, &world->snd, ent, 2 );
    /* demon1_pain and sham_pain: a hit under the roll does not flinch;
       only they draw it */
    if ( f->flinch_roll > 0.0f && rng_float( &world->rng ) * f->flinch_roll > dmg ) return;

    ent->state = MDL_ST_PAIN;
    ent->anim_frame = 0;
}

/* T_RadiusDamage: dmg less half the distance to the body's centre, the
   player's then every standing monster's -- CanDamage's line not asked.
   An attacker caught in its own blast takes half (head == attacker). */
void combat_radius( World *world, Player *player, Fight *fight, Renderer *rdr,
                    BspVec3 org, float dmg, short attacker )
{
    float dx, dy, dz, pts;
    short m;

    dx = player->pos.x - org.x;
    dy = player->pos.y - org.y;
    dz = player->pos.z + ( PL_ZLO + PL_ZHI ) * 0.5f - org.z;
    pts = dmg - 0.5f * (float) sqrt( dx*dx + dy*dy + dz*dz );
    if ( attacker == COMBAT_BY_PLAYER ) pts *= 0.5f;
    if ( pts > 0.0f ) combat_damage_player( player, world, fight, rdr, (short) pts );
    if ( attacker == COMBAT_BY_MONSTER ) return;   /* port: a monster's blast spares every monster */

    for ( m = 0; m < world->mon_count; m++ ) {
        MdlEnt far *e = &world->mon[m];
        if ( e->state == MDL_ST_DEAD ) continue;
        dx = e->pos.x - org.x;
        dy = e->pos.y - org.y;
        dz = e->pos.z + ( MDL_ZLO + MDL_ZHI ) * 0.5f - org.z;
        pts = dmg - 0.5f * (float) sqrt( dx*dx + dy*dy + dz*dz );
        if ( pts > 0.0f ) combat_damage_monster( world, player, fight, rdr, e, (short) pts );
    }
}
