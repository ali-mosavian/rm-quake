/*
 * combat.c -- combat.qc's T_Damage: what a hit does to the player and
 * to a monster.
 */

#include <math.h>
#include <string.h>

#include "combat.h"
#include "gib.h"
#include "part.h"
#include "d_sprite.h"
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

static short combat_player_health( short health, short take )
{
    long left = (long) health - take;
    return (short) ( left < -99L ? -99L : left );
}

static void near combat_pain_start( MdlEnt far *ent, short set, float now );

#define ZPAIN_IGNORE  0
#define ZPAIN_FALL    1
#define ZPAIN_EXTEND  2
#define ZPAIN_FAST    3

static short combat_monster_gibs( const MdlEnt far *ent,
                                  const MonsterFacts far *facts )
{
    return (short) ( ent->kind == MDL_KIND_ZOMBIE || ent->health < facts->gib_below );
}

static short combat_radius_target( const MdlEnt far *ent )
{
    return (short) ( MDL_IS_SOLID( ent ) && !ent->cruc );
}

static short combat_demon_jump_immune( const MdlEnt far *ent )
{
    return (short) ( ent->state == MDL_ST_LEAP &&
                     ( ent->vel.x != 0.0f || ent->vel.y != 0.0f || ent->vel.z != 0.0f ) );
}

static short combat_zombie_pain_action( const MdlEnt far *ent, short take,
                                        float now )
{
    if ( ent->state == MDL_ST_PAIN && ent->anim_set == MDL_SET_PAIN5 ) return ZPAIN_IGNORE;
    if ( take >= ZOMBIE_FALL_DMG ) return ZPAIN_FALL;
    if ( ent->state == MDL_ST_PAIN ) return ZPAIN_EXTEND;
    if ( now < ent->pain_finished ) return ZPAIN_FALL;
    return ZPAIN_FAST;
}

short combat_selftest( void )
{
    MdlEnt ent;
    short save, save2;
    short take = combat_take( 3.1f, 0.3f, 100, &save );
    short take2 = combat_take( 10.0f, 0.8f, 100, &save2 );
    memset( &ent, 0, sizeof(ent) );
    ent.next_think = 99.0f;
    combat_pain_start( &ent, MDL_SET_PAIN2, 7.0f );
    if ( save != 1 || take != 3 || save2 != 8 || take2 != 2 ||
         combat_player_health( 1, 300 ) != -99 ||
         ent.state != MDL_ST_PAIN || ent.anim_set != MDL_SET_PAIN2 ||
         ent.anim_frame != 0 || ent.next_think < 7.09f || ent.next_think > 7.11f ) return 0;

    ent.kind = MDL_KIND_ZOMBIE; ent.health = 1;
    if ( !combat_monster_gibs( &ent, &mon_facts[MDL_KIND_ZOMBIE] ) ) return 0;
    ent.state = MDL_ST_PAIN; ent.anim_set = MDL_SET_PAIN5; ent.anim_frame = 10;
    if ( combat_radius_target( &ent ) ||
         combat_zombie_pain_action( &ent, ZOMBIE_FALL_DMG, 1.0f ) != ZPAIN_IGNORE ) return 0;
    ent.anim_frame = 11;
    if ( !combat_radius_target( &ent ) ) return 0;
    ent.anim_set = MDL_SET_PAIN; ent.pain_finished = 2.0f;
    if ( combat_zombie_pain_action( &ent, ZOMBIE_PAIN_MIN, 1.0f ) != ZPAIN_EXTEND ) return 0;

    memset( &ent, 0, sizeof(ent) );
    ent.kind = MDL_KIND_DEMON; ent.state = MDL_ST_LEAP;
    if ( combat_demon_jump_immune( &ent ) ) return 0;
    ent.vel.x = 1.0f;
    if ( !combat_demon_jump_immune( &ent ) ) return 0;
    ent.vel.x = 0.0f; ent.anim_frame = 11;
    return (short) !combat_demon_jump_immune( &ent );
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

    if ( fight->health <= 0 ) return; /* Killed switches the player to DAMAGE_NO */

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
    fight->health = combat_player_health( fight->health, take );

    /* PainSound: drowning under water, a burn in slime or lava -- both
       every hit -- else one of six grunts a half second apart at most,
       rint(random() * 5 + 1) so the first and last half as often */
    if ( fight->health <= 0 ) return;
    if ( rdr->anim_time < fight->invis_until ) return;   /* player_pain: the eyes have no pain frames */
    if ( player->water_level == 3 && player->water_type == CONTENTS_WATER ) {
        d_sprite_bubbles( rdr, 1 );
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

static void near combat_pain_start( MdlEnt far *ent, short set, float now )
{
    mdl_anim_start( ent, MDL_ST_PAIN, set );
    ent->next_think = now + 0.1f;
}

void combat_damage_monster( World *world, Player *player, Fight *fight, Renderer *rdr,
                  MdlEnt far *ent, float dmg )
{
    const MonsterFacts far *f = &mon_facts[ ent->kind ];
    float r;
    short take = (short) ceil( dmg );
    short set;

    if ( ent->cruc ) return;    /* never walkmonster_start: takedamage stays DAMAGE_NO */
    ent->health = (short) ( ent->health - take );
    if ( ent->health <= 0 ) {
        ent->next_think = rdr->anim_time + 0.1f;
        if ( combat_monster_gibs( ent, f ) ) {
            if ( ent->kind == MDL_KIND_ZOMBIE )
                mdl_say( player, &world->snd, ent, 3 );
            else
                mdl_voice( player, &world->snd, ent, CHAN_VOICE, SND_UDEATH, ATTN_NORM );
            gib_spawn_monster( world, rdr, ent, rdr->anim_time );
            ent->state = MDL_ST_GIBBED;
            ent->anim_set = MDL_SET_STAND;
            ent->anim_frame = 0;
        } else {
            set = MDL_SET_DEATH;
            if ( mdl_set_count( &world->mdl[ent->kind], MDL_SET_DEATH2 ) &&
                 rng_float( &world->rng ) >= 0.5f ) set = MDL_SET_DEATH2;
            mdl_anim_start( ent, MDL_ST_DEAD, set );
            mdl_say( player, &world->snd, ent, 3 );
        }
        fight->kills++;
        return;
    }
    ent->hunting = -1;
    ent->ideal_yaw = mdl_vectoyaw( player->pos.x - ent->pos.x,
                                    player->pos.y - ent->pos.y );

    if ( ent->kind == MDL_KIND_ZOMBIE ) {
        /* zombie_pain uses the active set as its inpain state. A second
           hit during a fast flinch arms the three-second knockdown window. */
        ent->health = ZOMBIE_HEALTH;
        if ( take < ZOMBIE_PAIN_MIN ) return;
        set = combat_zombie_pain_action( ent, take, rdr->anim_time );
        if ( set == ZPAIN_IGNORE ) return;
        if ( set == ZPAIN_FALL ) {
            ent->pain_finished = 0.0f;
            combat_pain_start( ent, MDL_SET_PAIN5, rdr->anim_time );
            mdl_say( player, &world->snd, ent, 2 );
            return;
        }
        if ( set == ZPAIN_EXTEND ) {
            ent->pain_finished = rdr->anim_time + ZOMBIE_FALL_TIME;
            return;
        }
        r = rng_float( &world->rng );
        set = r < 0.25f ? MDL_SET_PAIN :
              ( r < 0.5f ? MDL_SET_PAIN2 : ( r < 0.75f ? MDL_SET_PAIN3 : MDL_SET_PAIN4 ) );
        combat_pain_start( ent, set, rdr->anim_time );
        if ( set == MDL_SET_PAIN2 || set == MDL_SET_PAIN3 )
            mdl_voice( player, &world->snd, ent, CHAN_VOICE, SND_Z_PAIN1, ATTN_NORM );
        else
            mdl_say( player, &world->snd, ent, 2 );
        return;
    }

    switch ( ent->kind ) {
    case MDL_KIND_ARMY:
        if ( rdr->anim_time < ent->pain_finished ) return;
        r = rng_float( &world->rng );
        set = r < 0.2f ? MDL_SET_PAIN : ( r < 0.6f ? MDL_SET_PAIN2 : MDL_SET_PAIN3 );
        ent->pain_finished = rdr->anim_time + ( set == MDL_SET_PAIN ? MDL_PAIN_SHORT : MDL_PAIN_LONG );
        if ( set == MDL_SET_PAIN ) mdl_say( player, &world->snd, ent, 2 );
        else mdl_voice( player, &world->snd, ent, CHAN_VOICE, f->pain_long_snd, ATTN_NORM );
        break;
    case MDL_KIND_KNIGHT:
        if ( rdr->anim_time < ent->pain_finished ) return;
        set = rng_float( &world->rng ) < 0.85f ? MDL_SET_PAIN : MDL_SET_PAIN2;
        ent->pain_finished = rdr->anim_time + KNIGHT_PAIN;
        mdl_say( player, &world->snd, ent, 2 );
        break;
    case MDL_KIND_DOG:
        set = rng_float( &world->rng ) > 0.5f ? MDL_SET_PAIN : MDL_SET_PAIN2;
        mdl_say( player, &world->snd, ent, 2 );
        break;
    case MDL_KIND_OGRE:
        if ( rdr->anim_time < ent->pain_finished ) return;
        r = rng_float( &world->rng );
        set = r < 0.25f ? MDL_SET_PAIN : ( r < 0.5f ? MDL_SET_PAIN2 :
              ( r < 0.75f ? MDL_SET_PAIN3 : ( r < 0.88f ? MDL_SET_PAIN4 : MDL_SET_PAIN5 ) ) );
        ent->pain_finished = rdr->anim_time + ( set >= MDL_SET_PAIN4 ? 2.0f : 1.0f );
        mdl_say( player, &world->snd, ent, 2 );
        break;
    case MDL_KIND_DEMON:
        if ( combat_demon_jump_immune( ent ) ||
             rdr->anim_time < ent->pain_finished ) return;
        ent->pain_finished = rdr->anim_time + 1.0f;
        mdl_say( player, &world->snd, ent, 2 );
        if ( rng_float( &world->rng ) * 200.0f > take ) return;
        set = MDL_SET_PAIN;
        break;
    case MDL_KIND_WIZARD:
        mdl_say( player, &world->snd, ent, 2 );
        if ( rng_float( &world->rng ) * 70.0f > take ) return;
        set = MDL_SET_PAIN;
        break;
    default:
        mdl_say( player, &world->snd, ent, 2 );
        if ( rng_float( &world->rng ) * SHAMBLER_PAIN_ROLL > take ||
             rdr->anim_time < ent->pain_finished ) return;
        ent->pain_finished = rdr->anim_time + SHAMBLER_PAIN;
        set = MDL_SET_PAIN;
        break;
    }
    combat_pain_start( ent, set, rdr->anim_time );
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
    part_explosion( rdr, &visual );
    d_sprite_explosion( rdr, &visual );
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
            if ( !combat_radius_target( e ) ) continue;
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
