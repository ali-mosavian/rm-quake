/*
 * combat.c -- combat.qc's T_Damage: what a hit does to the player and
 * to a monster.
 */

#include <math.h>

#include "combat.h"
#include "snd.h"
#include "mdl_ai.h"
#include "item.h"
#include "pl_trace.h"
#include "dl.h"
#include "doors.h"
#include "triggers.h"
#include "vshift.h"

static short combat_take( float dmg, float armor_type, short armor,
                          short *saved )
{
    volatile float prod = armor_type * dmg;
    short save = (short) ceil( prod );
    if ( save > armor ) save = armor;
    *saved = save;
    return (short) ceil( dmg - save );
}

short combat_selftest( void )
{
    short save, save2;
    short take = combat_take( 3.1f, 0.3f, 100, &save );
    short take2 = combat_take( 10.0f, 0.8f, 100, &save2 );
    return (short) ( save == 1 && take == 3 && save2 == 8 && take2 == 2 );
}

static short combat_push_target( short kind, short trig_kind )
{
    return (short) ( kind == 2 ||
                     ( kind == 3 && trig_kind == ENT_TRIG_SHOOT_BUTTON ) );
}

static void combat_brush_points( World *world, short kind, short index,
                                 BspVec3 *center, BspVec3 *target,
                                 short *trace_model )
{
    short model = kind == 2 ? world->door[index].model : world->trig[index].model;

    center->x = ( world->models[model].mins.x + world->models[model].maxs.x ) * 0.5f + world->brush[model].ofs.x;
    center->y = ( world->models[model].mins.y + world->models[model].maxs.y ) * 0.5f + world->brush[model].ofs.y;
    center->z = ( world->models[model].mins.z + world->models[model].maxs.z ) * 0.5f + world->brush[model].ofs.z;
    *target = *center;
    if ( !combat_push_target( kind, kind == 3 ? world->trig[index].kind : 0 ) ) {
        *target = world->brush[model].ofs;
        model = 0;
    }
    *trace_model = model;
}

short combat_brush_selftest( void )
{
    World world;
    Submodel models[2];
    BrushModel brushes[2];
    TrigEnt trig;
    BspVec3 center, target;
    short model;

    world.models = models; world.brush = brushes; world.trig = &trig;
    models[1].mins.x = models[1].mins.y = models[1].mins.z = 2.0f;
    models[1].maxs.x = models[1].maxs.y = models[1].maxs.z = 6.0f;
    brushes[1].ofs.x = 10.0f; brushes[1].ofs.y = 20.0f; brushes[1].ofs.z = 30.0f;
    trig.model = 1; trig.kind = ENT_TRIG_SHOOT;
    combat_brush_points( &world, 3, 0, &center, &target, &model );
    if ( model != 0 || target.x != 10.0f || target.y != 20.0f || target.z != 30.0f ) return 0;
    trig.kind = ENT_TRIG_SHOOT_BUTTON;
    combat_brush_points( &world, 3, 0, &center, &target, &model );
    return (short) ( model == 1 && target.x == 14.0f &&
                     target.y == 24.0f && target.z == 34.0f );
}

void combat_damage_player( Player *player, World *world, Fight *fight, Renderer *rdr, float dmg )
{
    Rng   *rng = &world->rng;
    Sound *snd = &world->snd;
    short save, take;

    fight->face_until = rdr->anim_time + 0.2f;

    /* T_Damage: the armor takes ceil(type * damage), and its last point
       takes the type with it. QuakeC stores the product as a float32 --
       0.8 * 10 is 8.00000012 at 80 bits, 8.0 stored -- and a cast chops
       (F_FTOL@), so ceil() it is */
    take = combat_take( dmg, fight->armor_type, fight->armor, &save );
    if ( save >= fight->armor ) { save = fight->armor; fight->armor_type = 0.0f; }
    fight->armor = (short) ( fight->armor - save );
    v_damage_shift( fight, save, take );

    /* then the pentagram stops the rest -- the armor above still wore --
       and protect3 says so no more than two seconds apart */
    if ( rdr->anim_time < fight->pent_until ) {
        if ( rdr->anim_time >= fight->pent_at ) {
            fight->pent_at = rdr->anim_time + 2.0f;
            snd_play( snd, player, SND_PENT_HIT, &player->pos );
        }
        return;
    }
    fight->health = (short) ( fight->health - take );

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
                  MdlEnt far *ent, float dmg )
{
    const MonsterFacts far *f = &mon_facts[ ent->kind ];
    float r;
    short take = (short) ceil( dmg );

    if ( ent->cruc ) return;    /* never walkmonster_start: takedamage stays DAMAGE_NO */
    ent->health = (short) ( ent->health - take );
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
        if ( take < ZOMBIE_PAIN_MIN ) return;
        if ( rdr->anim_time < ent->pain_finished ) return;
        ent->pain_finished = rdr->anim_time +
            ( take >= ZOMBIE_FALL_DMG ? ZOMBIE_FALL_TIME : ZOMBIE_FLINCH );
        r = rng_float( &world->rng );
        if ( take < ZOMBIE_FALL_DMG && r >= 0.25f && r < 0.75f )
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
    if ( f->flinch_roll > 0.0f && rng_float( &world->rng ) * f->flinch_roll > take ) return;

    ent->state = MDL_ST_PAIN;
    ent->anim_frame = 0;
}

static short combat_can_damage( World *world, BspVec3 *org, BspVec3 *target,
                                short model )
{
    static const short ofs[4][2] = { {15,15}, {-15,-15}, {-15,15}, {15,-15} };
    TraceResult tr;
    BspVec3 p;
    short i;

    pl_trace_point( world, org, target, &tr );
    if ( tr.frac == 1.0f || ( model > 0 && tr.hit_model == model ) ) return 1;
    if ( model > 0 ) return 0;
    for ( i = 0; i < 4; i++ ) {
        p = *target;
        p.x += ofs[i][0]; p.y += ofs[i][1];
        pl_trace_point( world, org, &p, &tr );
        if ( tr.frac == 1.0f ) return 1;
    }
    return 0;
}

void combat_damage_box( World *world, Player *player, Fight *fight,
                        Renderer *rdr, short i, float dmg )
{
    ItemEnt far *it = &world->item[i];
    BspVec3 visual;
    short take;

    if ( it->gone || it->kind != ENT_ITEM_EXPLOBOX ) return;
    take = (short) ceil( dmg );
    it->amount = (short) ( it->amount - take );
    if ( it->amount > 0 ) return;
    it->gone = -1;
    fight->booms++;
    visual = it->pos;
    visual.z += ENT_BOX_TOP * 0.5f;
    snd_play( &world->snd, player, SND_BOOM, &visual );
    dl_explosion( rdr, &visual );
    combat_radius( world, player, fight, rdr, it->pos, ENT_BOX_DMG,
                   COMBAT_BY_WORLD, it->order );
}

void combat_radius( World *world, Player *player, Fight *fight, Renderer *rdr,
                    BspVec3 org, float dmg, short attacker, short ignore )
{
    float dx, dy, dz, pts, cutoff = dmg + 40.0f;
    short m, i, next, kind, index, cursor = 32767, model;
    BspVec3 center, target;

    for ( ;; ) {
        next = -1; kind = -1; index = -1;
        for ( m = 0; m < world->mon_count; m++ )
            if ( world->mon[m].order < cursor && world->mon[m].order > next )
                { next = world->mon[m].order; kind = 0; index = m; }
        for ( i = 0; i < world->item_count; i++ )
            if ( world->item[i].kind == ENT_ITEM_EXPLOBOX &&
                 world->item[i].order < cursor && world->item[i].order > next )
                { next = world->item[i].order; kind = 1; index = i; }
        for ( i = 0; i < world->door_count; i++ )
            if ( world->door[i].shoot && world->door[i].state == ENT_DOOR_SHUT &&
                 world->door[i].order < cursor && world->door[i].order > next )
                { next = world->door[i].order; kind = 2; index = i; }
        for ( i = 0; i < world->trig_count; i++ )
            if ( ( world->trig[i].kind == ENT_TRIG_SHOOT ||
                   world->trig[i].kind == ENT_TRIG_SHOOT_BUTTON ) &&
                 world->trig[i].state == ENT_TRIG_READY &&
                 world->trig[i].order < cursor && world->trig[i].order > next )
                { next = world->trig[i].order; kind = 3; index = i; }
        if ( next < 0 ) break;
        cursor = next;
        if ( next == ignore ) continue;
        model = 0;
        if ( kind == 0 ) {
            MdlEnt far *e = &world->mon[index];
            if ( e->state == MDL_ST_DEAD || e->cruc ) continue;
            center = target = e->pos;
            center.z += ( e->kind == MDL_KIND_OGRE || e->kind == MDL_KIND_DEMON ||
                          e->kind == MDL_KIND_SHAMBLER ) ? 20.0f : 8.0f;
        } else if ( kind == 1 ) {
            ItemEnt far *it = &world->item[index];
            if ( it->gone ) continue;
            center = target = it->pos;
            center.z += ENT_BOX_TOP * 0.5f;
        } else {
            /* InitTrigger is MOVETYPE_NONE, so CanDamage probes its origin
               and does not collide with its own SOLID_BBOX. */
            combat_brush_points( world, kind, index, &center, &target, &model );
        }
        dx = center.x - org.x; dy = center.y - org.y; dz = center.z - org.z;
        if ( dx*dx + dy*dy + dz*dz > cutoff*cutoff ) continue;
        pts = dmg - 0.5f * (float) sqrt( dx*dx + dy*dy + dz*dz );
        if ( next == attacker ) pts *= 0.5f;
        if ( kind == 0 && world->mon[index].kind == MDL_KIND_SHAMBLER ) pts *= 0.5f;
        if ( attacker == COMBAT_BY_PLAYER && rdr->anim_time < fight->quad_until ) pts *= PL_QUAD_MUL;
        if ( pts <= 0.0f || !combat_can_damage( world, &org, &target, model ) ) continue;
        if ( kind == 0 ) combat_damage_monster( world, player, fight, rdr, &world->mon[index], pts );
        else if ( kind == 1 ) combat_damage_box( world, player, fight, rdr, index, pts );
        else if ( kind == 2 ) ent_door_fire( world, player, world->door[index].link );
        else                  ent_trig_shot( world, player, fight, rdr, index );
    }

    if ( ignore != COMBAT_BY_PLAYER ) {
        center = target = player->pos;
        center.z += ( PL_ZLO + PL_ZHI ) * 0.5f;
        dx = center.x - org.x; dy = center.y - org.y; dz = center.z - org.z;
        if ( dx*dx + dy*dy + dz*dz <= cutoff*cutoff ) {
            pts = dmg - 0.5f * (float) sqrt( dx*dx + dy*dy + dz*dz );
            if ( attacker == COMBAT_BY_PLAYER ) pts *= 0.5f;
            if ( attacker == COMBAT_BY_PLAYER && rdr->anim_time < fight->quad_until ) pts *= PL_QUAD_MUL;
            if ( pts > 0.0f && combat_can_damage( world, &org, &target, 0 ) )
                combat_damage_player( player, world, fight, rdr, pts );
        }
    }
}
