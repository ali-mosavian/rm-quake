/*
 * mdl_ai.c -- the monsters' own tick: ai.qc and sv_move.c, ported as
 * they stand. Movement distance and displayed frame are one state --
 * while army_runN is on screen, ai_run(N)'s distance is what moves that
 * think -- so the animation always matches what the monster is doing.
 *
 * The "path finding" is SV_NewChaseDir's eight compass points. There is
 * no graph and no search, because id shipped none.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>

#include "mdl_ai.h"
#include "snd.h"
#include "mdl.h"
#include "pl_trace.h"
#include "ent.h"
#include "triggers.h"
#include "weapons.h"   /* the ray-box test, the pellet spread, the projectiles */
#include "dl.h"
#include "combat.h"
#include "item.h"
#include "d_beam.h"

#define DEG2RAD 0.017453293f

/* ai_run(N)'s own per-frame distances, straight out of each .qc. */
static const short far army_run_dist[8]     = { 11, 15, 10, 10, 8, 15, 10, 8 };
static const short far knight_run_dist[8]   = { 16, 20, 13, 7, 16, 20, 14, 6 };
static const short far dog_run_dist[12]     = { 16, 32, 32, 20, 64, 32, 16, 32, 32, 20, 64, 32 };
static const short far ogre_run_dist[8]     = { 9, 12, 8, 22, 16, 4, 13, 24 };
static const short far demon_run_dist[6]    = { 20, 15, 36, 20, 15, 36 };
static const short far zombie_run_dist[18]  = { 1, 1, 0, 1, 2, 3, 4, 4, 2, 0, 0, 0, 2, 4, 6, 7, 3, 8 };
static const short far wizard_run_dist[1]   = { (short) WIZARD_FLY_DIST };
static const short far shambler_run_dist[6] = { 20, 24, 20, 20, 24, 20 };

static const short far army_walk_dist[24]   = { 1, 1, 1, 1, 2, 3, 4, 4, 2, 2, 2, 1,
                                                0, 1, 1, 1, 3, 3, 3, 3, 2, 1, 1, 1 };
static const short far knight_walk_dist[14] = { 3, 2, 3, 4, 3, 3, 3, 4, 3, 3, 2, 3, 4, 3 };
static const short far dog_walk_dist[1]     = { 8 };
static const short far ogre_walk_dist[16]   = { 3, 2, 2, 2, 2, 5, 3, 2, 3, 1, 2, 3, 3, 3, 3, 4 };
static const short far demon_walk_dist[8]   = { 8, 6, 6, 7, 4, 6, 10, 10 };
static const short far zombie_walk_dist[19] = { 0, 2, 3, 2, 1, 0, 0, 0, 0, 0, 2, 2, 1, 0, 0, 0, 0, 0, 0 };
static const short far wizard_walk_dist[1]  = { 8 };
static const short far shambler_walk_dist[12] = { 10, 9, 9, 5, 6, 12, 8, 3, 13, 9, 7, 7 };

static const short far knight_atk_dist[10]  = { 0, 7, 4, 0, 3, 4, 1, 3, 1, 5 };
static const short far knight_runatk_dist[11] = { 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10 };
static const short far dog_atk_dist[8]       = { 10, 10, 10, 10, 10, 10, 10, 10 };
static const short far ogre_swing_dist[14]   = { 11, 1, 4, 13, 19, 10, 10, 10, 10, 10, 10, 3, 8, 9 };
static const short far ogre_smash_dist[14]   = { 6, 0, 0, 1, 4, 14, 14, 20, 23, 10, 12, 0, 4, 12 };
static const short far demon_atk_dist[15]    = { 4, 0, 0, 1, 2, 1, 6, 8, 4, 2, 0, 5, 8, 4, 4 };
static const short far sham_smash_dist[12]   = { 2, 6, 6, 5, 4, 1, 0, 0, 0, 0, 5, 4 };
static const short far sham_swingr_dist[9]   = { 1, 8, 14, 7, 3, 6, 16, 3, 11 };
static const short far sham_swingl_dist[9]   = { 5, 3, 7, 3, 7, 9, 15, 4, 8 };

/* Positive is ai_painforward; negative is ai_pain. */
static const signed char far army_pain_dist[6]    = { 0, 0, 0, 0, 0, -1 };
static const signed char far army_painb_dist[14]  = { 0, 13, 9, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0 };
static const signed char far army_painc_dist[13]  = { 0, -1, 0, 0, 1, 1, 0, -1, 4, 3, 6, 8, 0 };
static const signed char far knight_painb_dist[11]= { 0, 3, 0, 0, 2, 4, 2, 5, 5, 0, 0 };
static const signed char far dog_painb_dist[16]   = { 0, 0, -4, -12, -12, -2, 0, -4, 0, -10, 0, 0, 0, 0, 0, 0 };
static const signed char far ogre_paind_dist[16]  = { 0, -10, -9, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const signed char far ogre_paine_dist[15]  = { 0, -10, -9, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const signed char far zombie_paina_dist[12]= { 0, 3, 1, -1, -3, -1, 0, 0, 0, 0, 0, 0 };
static const signed char far zombie_painb_dist[28]= { 0, -2, -8, -6, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 };
static const signed char far zombie_painc_dist[18]= { 0, 0, -3, -1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0 };
static const signed char far zombie_paind_dist[13]= { 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0 };
static const signed char far zombie_paine_dist[30]= { 0, -8, -5, -3, -1, -2, -1, -1, -2, 0, 0, 0, 0, 0, 0,
                                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, 1, -1, 0, 0 };

/* One think's context, on mdl_think's stack; the handlers take it by near
   pointer (SS == DS). */
typedef struct {
    World      *world;
    Player     *player;
    Fight      *fight;
    Renderer   *rdr;
    MdlEnt far *ent;
    MdlState   *m;
    short       can_chase;
} Think;

typedef short (near *AiCheck)( Think *t );   /* -1: the think is over */
typedef void  (near *AiStep)( Think *t );

/* What a kind does that its code does not: each .qc's numbers. */
typedef struct {
    const short far *run;          /* ai_run(N)'s distances, a frame each */
    short            nrun;
    const short far *walk;         /* ai_walk(N)'s, separate from the run */
    short            nwalk;
    float            walk_period, walk_chance;   /* the idle roll on the walk, */
    short            walk_snd;
    float            run_period, run_chance;     /* and on the run */
    short            run_snd;
    AiStep           idle;         /* in place of the two rolls, or 0 */
    AiCheck          check_attack;
    AiStep           attack_frame; /* what the attack set's frames do, or 0 */
    short            pain_lies;    /* zombie_paine: lies on the last pain frame till it may rise */
    float            leap_touch;   /* a leap bites past this speed, */
    float            leap_dmg, leap_scale;       /* for leap_dmg + random() * leap_scale */
} MonsterInfo;

static void  near idle_ogre( Think *t );
static void  near idle_zombie( Think *t );
static void  near idle_wizard( Think *t );
static short near ai_check_knight( Think *t );
static short near ai_check_dog( Think *t );
static short near ai_check_ogre( Think *t );
static short near ai_check_demon( Think *t );
static short near ai_check_shambler( Think *t );
static short near ai_check_zombie( Think *t );
static short near ai_check_wizard( Think *t );
static short near ai_check_army( Think *t );
static void  near atk_knight( Think *t );
static void  near atk_army( Think *t );
static void  near atk_dog( Think *t );
static void  near atk_demon( Think *t );
static void  near atk_ogre( Think *t );
static void  near atk_zombie( Think *t );
static void  near atk_wizard( Think *t );
static void  near atk_shambler( Think *t );

/* MDL_KIND_* order */
static const MonsterInfo far mon_info[] = {
    { army_run_dist,     8,  army_walk_dist,     24, 2.4f, 0.2f, SND_ARMY_IDLE,   0.0f, 0.0f, 0,             0,           ai_check_army,     atk_army,    0, DOG_LEAP_SPEED,   DOG_LEAP_DMG,   DOG_LEAP_DMG },
    { knight_run_dist,   8,  knight_walk_dist,   14, 1.4f, 0.2f, SND_KNIGHT_IDLE, 0.0f, 0.0f, 0,             0,           ai_check_knight,   atk_knight,  0, DOG_LEAP_SPEED,   DOG_LEAP_DMG,   DOG_LEAP_DMG },
    { dog_run_dist,      12, dog_walk_dist,       1, 0.8f, 0.2f, SND_DOG_IDLE,    0.0f, 0.0f, 0,             0,           ai_check_dog,      atk_dog,     0, DOG_LEAP_SPEED,   DOG_LEAP_DMG,   DOG_LEAP_DMG },
    { ogre_run_dist,     8,  ogre_walk_dist,     16, 0.0f, 0.0f, 0,               0.0f, 0.0f, 0,             idle_ogre,   ai_check_ogre,     atk_ogre,    0, DOG_LEAP_SPEED,   DOG_LEAP_DMG,   DOG_LEAP_DMG },
    { demon_run_dist,    6,  demon_walk_dist,     8, 0.8f, 0.2f, SND_DEMON_IDLE,  0.0f, 0.0f, 0,             0,           ai_check_demon,    atk_demon,   0, DEMON_LEAP_TOUCH, DEMON_LEAP_DMG, 10.0f },
    { zombie_run_dist,   18, zombie_walk_dist,   19, 0.0f, 0.0f, 0,               0.0f, 0.0f, 0,             idle_zombie, ai_check_zombie,   atk_zombie,  1, DOG_LEAP_SPEED,   DOG_LEAP_DMG,   DOG_LEAP_DMG },
    { wizard_run_dist,   1,  wizard_walk_dist,    1, 0.0f, 0.0f, 0,               0.0f, 0.0f, 0,             idle_wizard, ai_check_wizard,   atk_wizard,  0, DOG_LEAP_SPEED,   DOG_LEAP_DMG,   DOG_LEAP_DMG },
    { shambler_run_dist, 6,  shambler_walk_dist, 12, 1.2f, 0.2f, SND_SHAM_IDLE,   0.6f, 0.2f, SND_SHAM_IDLE, 0,           ai_check_shambler, atk_shambler,0, DOG_LEAP_SPEED,   DOG_LEAP_DMG,   DOG_LEAP_DMG },
};
/* a missing row would zero-fill, and its check_attack would call _TEXT:0000 */
typedef char mon_rows_ok[ sizeof mon_info / sizeof mon_info[0] == MDL_KINDS ? 1 : -1 ];

/* MDL_KIND_* order. The zombie's death sound is its gib already, so no
   health is under its gib_below. */
const MonsterFacts far mon_facts[] = {
    { MDL_HEALTH,      0, SND_MON + 0 * 4,  -35,          0.0f,          0.0f,               SND_ARMY_PAIN2, 0, ENT_ITEM_SHELLS,  5 },
    { KNIGHT_HEALTH,   0, SND_MON + 1 * 4,  -40,          KNIGHT_PAIN,   0.0f,               0,              0, 0,                0 },
    { DOG_HEALTH,      0, SND_MON + 2 * 4,  -35,          0.0f,          0.0f,               0,              0, 0,                0 },
    { OGRE_HEALTH,     0, SND_MON + 3 * 4,  -80,          KNIGHT_PAIN,   0.0f,               0,              0, ENT_ITEM_ROCKETS, 2 },
    { DEMON_HEALTH,    0, SND_MON + 4 * 4,  -80,          KNIGHT_PAIN,   200.0f,             0,              0, 0,                0 },
    { ZOMBIE_HEALTH,   0, SND_MON2 + 0 * 4, -32767 - 1,   0.0f,          0.0f,               0,              1, 0,                0 },
    { WIZARD_HEALTH,   1, SND_MON2 + 1 * 4, -40,          0.0f,          0.0f,               0,              0, 0,                0 },
    { SHAMBLER_HEALTH, 0, SND_MON2 + 2 * 4, -60,          SHAMBLER_PAIN, SHAMBLER_PAIN_ROLL, 0,              0, 0,                0 },
};
typedef char mon_facts_ok[ sizeof mon_facts / sizeof mon_facts[0] == MDL_KINDS ? 1 : -1 ];

void mdl_anim_start( MdlEnt far *ent, short state, short set )
{
    ent->state = state;
    ent->anim_set = (unsigned char) set;
    ent->anim_frame = 0;
    ent->leap_touch = 0;
}

static float mdl_anglemod( float v )
{
    while ( v >= 360.0f ) v -= 360.0f;
    while ( v < 0.0f )    v += 360.0f;
    return v;
}

/* PF_vectoyaw, including its (int) truncation toward zero. */
float mdl_vectoyaw( float dx, float dy )
{
    float yaw;

    if ( dx == 0.0f && dy == 0.0f ) return 0.0f;
    yaw = (float) ( atan2( dy, dx ) * 57.29577951 );
    yaw = (float) (long) yaw;
    if ( yaw < 0.0f ) yaw += 360.0f;
    return yaw;
}

/* PF_changeyaw: up to MDL_YAW_SPEED degrees per think. */
static void mdl_change_yaw( MdlEnt far *ent )
{
    float cur = mdl_anglemod( ent->yaw ), ideal = ent->ideal_yaw, mv;

    if ( cur == ideal ) return;
    mv = ideal - cur;
    if ( ideal > cur ) { if ( mv >=  180.0f ) mv -= 360.0f; }
    else               { if ( mv <= -180.0f ) mv += 360.0f; }

    if ( mv > 0.0f ) { if ( mv >  MDL_YAW_SPEED ) mv =  MDL_YAW_SPEED; }
    else             { if ( mv < -MDL_YAW_SPEED ) mv = -MDL_YAW_SPEED; }

    ent->yaw = mdl_anglemod( cur + mv );
}

/* A monster's own four, by kind: sight, attack, pain, death. */
void mdl_voice( Player *player, Sound *snd, MdlEnt far *ent, short chan, short id, short attn )
{
    BspVec3 org = ent->pos;

    /* the record's place in its array is its entity number */
    snd_start( snd, player, (short) ( SND_ENT_MON + FP_OFF( ent ) / sizeof(MdlEnt) ), chan, id, &org, attn );
}

void mdl_say( Player *player, Sound *snd, MdlEnt far *ent, short which )
{
    short id = (short) ( mon_facts[ ent->kind ].voice + which );

    mdl_voice( player, snd, ent, which == 1 ? CHAN_WEAPON : CHAN_VOICE, id, ATTN_NORM );
}

/* The idle noises each .qc makes on its walk, run or stand frames, as a
   roll once a cycle: the cycle's frames at 10 Hz and id's chance. */
static short mdl_idle_roll( Player *player, Rng *rng, Sound *snd, Renderer *rdr, MdlEnt far *ent,
                             float period, float chance, short id )
{
    ent->idle_at = rdr->anim_time + period;
    if ( rng_float( rng ) >= chance ) return 0;
    mdl_voice( player, snd, ent, CHAN_VOICE, id, ATTN_IDLE );
    return -1;
}

/* ogre_walk3's ogidle and walk6's ogdrag, stand5's ogidle, run1's ogidle2 */
static void near idle_ogre( Think *t )
{
    Player *player = t->player;
    Rng *rng = &t->world->rng;
    Sound *snd = &t->world->snd;
    Renderer *rdr = t->rdr;
    MdlEnt far *ent = t->ent;
    short walk = (short) ( ent->state == MDL_ST_RUN && !ent->hunting );
    short run  = (short) ( ent->state == MDL_ST_RUN && ent->hunting );

    if ( walk && !mdl_idle_roll( player, rng, snd, rdr, ent, 1.6f, 0.2f, SND_OGRE_IDLE ) )
        mdl_idle_roll( player, rng, snd, rdr, ent, 1.6f, 0.1f, SND_OGRE_DRAG );
    if ( ent->state == MDL_ST_STAND ) mdl_idle_roll( player, rng, snd, rdr, ent, 0.9f, 0.2f, SND_OGRE_IDLE );
    if ( run ) mdl_idle_roll( player, rng, snd, rdr, ent, 0.8f, 0.2f, SND_OGRE_IDLE2 );
}

/* walk12 and walk19's z_idle; run18's z_idle, then z_idle1 */
static void near idle_zombie( Think *t )
{
    Player *player = t->player;
    Rng *rng = &t->world->rng;
    Sound *snd = &t->world->snd;
    Renderer *rdr = t->rdr;
    MdlEnt far *ent = t->ent;
    short walk = (short) ( ent->state == MDL_ST_RUN && !ent->hunting );
    short run  = (short) ( ent->state == MDL_ST_RUN && ent->hunting );

    if ( walk ) mdl_idle_roll( player, rng, snd, rdr, ent, 0.95f, 0.2f, SND_MON2 );
    if ( run && !mdl_idle_roll( player, rng, snd, rdr, ent, 1.8f, 0.2f, SND_MON2 ) )
        mdl_idle_roll( player, rng, snd, rdr, ent, 1.8f, 0.2f, SND_Z_IDLE1 );
}

/* wiz_idlesound: waitmin two seconds apart, widle1 or widle2 a tenth each */
static void near idle_wizard( Think *t )
{
    Player *player = t->player;
    Rng *rng = &t->world->rng;
    Sound *snd = &t->world->snd;
    MdlEnt far *ent = t->ent;
    float wr;

    ent->idle_at = t->rdr->anim_time + 2.0f;
    wr = rng_float( rng ) * 5.0f;
    if ( wr > 4.5f ) mdl_voice( player, snd, ent, CHAN_VOICE, SND_WIZ_IDLE1, ATTN_IDLE );
    if ( wr < 0.5f ) mdl_voice( player, snd, ent, CHAN_VOICE, SND_WIZ_IDLE1 + 1, ATTN_IDLE );
}

/* The idle noises each .qc makes on its walk or run frames: a roll once a cycle */
static void near mdl_idle( Think *t )
{
    const MonsterInfo far *inf = &mon_info[ t->ent->kind ];
    MdlEnt far *ent = t->ent;
    short walk = (short) ( ent->state == MDL_ST_RUN && !ent->hunting );
    short run  = (short) ( ent->state == MDL_ST_RUN && ent->hunting );

    if ( t->rdr->anim_time < ent->idle_at ) return;
    if ( inf->idle ) { inf->idle( t ); return; }
    if ( walk && inf->walk_period > 0.0f )
        mdl_idle_roll( t->player, &t->world->rng, &t->world->snd, t->rdr, ent, inf->walk_period, inf->walk_chance, inf->walk_snd );
    if ( run && inf->run_period > 0.0f )
        mdl_idle_roll( t->player, &t->world->rng, &t->world->snd, t->rdr, ent, inf->run_period, inf->run_chance, inf->run_snd );
}

/* SV_movestep's FL_FLY case: a straight trace, held 30..40 above the
   player while hunting. */
static short mdl_flystep( World *world, Player *player, MdlEnt far *ent,
                           float dx, float dy )
{
    BspVec3 start, fin;
    TraceResult tr;
    float dz;
    short try_n;

    for ( try_n = 0; try_n < 2; try_n++ ) {
        start = ent->pos;
        fin.x = ent->pos.x + dx; fin.y = ent->pos.y + dy; fin.z = ent->pos.z;
        if ( try_n == 0 && ent->hunting ) {
            dz = ent->pos.z - player->pos.z;
            if ( dz > WIZARD_FLY_HI ) fin.z -= WIZARD_FLY_STEP;
            if ( dz < WIZARD_FLY_LO ) fin.z += WIZARD_FLY_STEP;
        }
        pl_trace( world, &start, &fin, &tr );
        if ( tr.frac >= 1.0f && !tr.start_solid && !tr.all_solid ) {
            ent->pos = tr.end_pos;
            return -1;
        }
    }
    return 0;
}

/*
 * SV_movestep's vertical dance: one hull-1 trace from (new xy, z +
 * STEPSIZE) down to (new xy, z - STEPSIZE), so a step down finds its
 * floor and a step up climbs one. Nothing within the band is an edge,
 * and the step is refused. SV_CheckBottom's four-corner support test is
 * not ported, so a monster can teeter over a corner id would refuse.
 */
static short mdl_movestep( World *world, Player *player, MdlEnt far *ent,
                            float dx, float dy )
{
    BspVec3 start, fin;
    TraceResult tr;

    if ( mon_facts[ ent->kind ].flyer )
        return mdl_flystep( world, player, ent, dx, dy );

    start.x = ent->pos.x + dx;
    start.y = ent->pos.y + dy;
    start.z = ent->pos.z + MDL_STEPSIZE;
    fin = start;
    fin.z = ent->pos.z - MDL_STEPSIZE;

    pl_trace( world, &start, &fin, &tr );
    if ( tr.all_solid ) return 0;

    if ( tr.start_solid ) {
        start.z = ent->pos.z;
        pl_trace( world, &start, &fin, &tr );
        if ( tr.all_solid || tr.start_solid ) return 0;
    }
    if ( tr.frac > 0.999f ) return 0;

    ent->pos = tr.end_pos;
    return -1;
}

/*
 * SV_StepDirection, oddity included: the step is reverted (position
 * only, not the turn) when the monster is not yet within 45 degrees of
 * the heading, and TRUE is still returned. That is why one turns in
 * place for a think or two before it walks.
 */
static short mdl_step_dir( World *world, Player *player, MdlEnt far *ent,
                            float yaw, float dist )
{
    BspVec3 old;
    float rad, delta;

    ent->ideal_yaw = yaw;
    mdl_change_yaw( ent );

    rad = yaw * DEG2RAD;
    old = ent->pos;
    if ( !mdl_movestep( world, player, ent,
                         (float) cos( rad ) * dist, (float) sin( rad ) * dist ) )
        return 0;

    delta = mdl_anglemod( ent->yaw - ent->ideal_yaw );
    if ( delta > 45.0f && delta < 315.0f ) ent->pos = old;
    return -1;
}

/* SV_NewChaseDir: the diagonal, the two cardinals (order coin-flipped
   as id's own (rand()&3)&1 does), the old heading, a randomised sweep
   of all eight, then reverse. */
static void mdl_new_chase_dir( World *world, Player *player, MdlEnt far *ent,
                                BspVec3 *goal, float dist )
{
    float deltax, deltay, d1, d2, tdir, tmp, olddir, turnaround;

    olddir = mdl_anglemod( (float) (long) ( ent->ideal_yaw / 45.0f ) * 45.0f );
    turnaround = mdl_anglemod( olddir - 180.0f );

    deltax = goal->x - ent->pos.x;
    deltay = goal->y - ent->pos.y;

    if      ( deltax >  10.0f ) d1 = 0.0f;
    else if ( deltax < -10.0f ) d1 = 180.0f;
    else                        d1 = -1.0f;

    if      ( deltay < -10.0f ) d2 = 270.0f;
    else if ( deltay >  10.0f ) d2 = 90.0f;
    else                        d2 = -1.0f;

    if ( d1 != -1.0f && d2 != -1.0f ) {
        if ( d1 == 0.0f ) tdir = ( d2 == 90.0f ) ? 45.0f : 315.0f;
        else              tdir = ( d2 == 90.0f ) ? 135.0f : 215.0f;
        if ( tdir != turnaround &&
             mdl_step_dir( world, player, ent, tdir, dist ) ) return;
    }

    if ( ( (long) ( rng_float( &world->rng ) * 4.0f ) & 1 ) || fabs( deltay ) > fabs( deltax ) ) {
        tmp = d1; d1 = d2; d2 = tmp;
    }
    if ( d1 != -1.0f && d1 != turnaround &&
         mdl_step_dir( world, player, ent, d1, dist ) ) return;
    if ( d2 != -1.0f && d2 != turnaround &&
         mdl_step_dir( world, player, ent, d2, dist ) ) return;

    if ( olddir != -1.0f && mdl_step_dir( world, player, ent, olddir, dist ) ) return;

    if ( (long) ( rng_float( &world->rng ) * 2.0f ) ) {
        for ( tdir = 0.0f; tdir <= 315.0f; tdir += 45.0f )
            if ( tdir != turnaround &&
                 mdl_step_dir( world, player, ent, tdir, dist ) ) return;
    } else {
        for ( tdir = 315.0f; tdir >= 0.0f; tdir -= 45.0f )
            if ( tdir != turnaround &&
                 mdl_step_dir( world, player, ent, tdir, dist ) ) return;
    }

    if ( mdl_step_dir( world, player, ent, turnaround, dist ) ) return;

    ent->ideal_yaw = olddir;    /* can't move */
}

/* SV_MoveToGoal: a 1-in-4 chance to skip the direct step. */
static void mdl_move_to_goal( World *world, Player *player, MdlEnt far *ent,
                               BspVec3 *goal, float dist )
{
    short skip = (short) ( (long) ( rng_float( &world->rng ) * 4.0f ) == 1 );
    short stepped = 0;

    if ( !skip ) stepped = mdl_step_dir( world, player, ent, ent->ideal_yaw, dist );
    if ( skip || !stepped )
        mdl_new_chase_dir( world, player, ent, goal, dist );
}

static short mdl_visible( World *world, BspVec3 *eye, BspVec3 *peye )
{
    TraceResult tr;

    pl_trace_point( world, eye, peye, &tr );
    return (short) ( tr.frac == 1.0f && !tr.all_solid &&
                     !( tr.in_open && tr.in_water ) );
}

/*
 * FindTarget, collapsed to the one enemy this world has. RANGE_MELEE
 * skips infront -- "will become hostile even if back is turned";
 * RANGE_NEAR needs it unless the player fired within the second.
 * visible() is id's hull-0 eye-to-eye traceline. Crossing from open air
 * into water is not visible even when the line itself is unobstructed.
 */
static short mdl_find_target( World *world, Player *player, Fight *fight,
                               Renderer *rdr, MdlEnt far *ent )
{
    BspVec3 eye, peye;
    float dx, dy, dz, r, yaw_rad, dlen, dot;

    eye  = ent->pos;    eye.z  += MDL_VIEW_OFS;
    peye = player->pos; peye.z += PL_EYE;

    dx = peye.x - eye.x; dy = peye.y - eye.y; dz = peye.z - eye.z;
    r = (float) sqrt( dx*dx + dy*dy + dz*dz );
    if ( r >= MDL_RANGE_MID ) return 0;          /* RANGE_FAR */

    if ( !mdl_visible( world, &eye, &peye ) ) return 0;

    if ( r >= MDL_RANGE_NEAR || fight->show_hostile < rdr->anim_time ) {
        if ( r >= MDL_RANGE_MELEE ) {
            yaw_rad = ent->yaw * DEG2RAD;
            dlen = (float) sqrt( dx*dx + dy*dy );
            dot = dlen > 0.0f
                ? ( dx * (float) cos( yaw_rad ) + dy * (float) sin( yaw_rad ) ) / dlen
                : 1.0f;
            if ( dot <= 0.3f ) return 0;         /* not infront */
        }
    }

    ent->ideal_yaw = mdl_vectoyaw( dx, dy );     /* HuntTarget faces at once */
    return -1;
}

/* the goal is corner i, and the face towards it (t_movetarget) */
static void mdl_patrol_to( World *world, MdlEnt far *ent, short i )
{
    PathCorner far *c = &world->corner[i];

    ent->corner = i;
    ent->goal = c->org;
    ent->ideal_yaw = mdl_vectoyaw( c->org.x - ent->pos.x, c->org.y - ent->pos.y );
}

/* Not id's: a point on a circle round where the monster is NOW, so one
   that has wandered somewhere keeps drifting instead of yo-yoing back
   to its spawn. An unreachable goal times out in mdl_think. */
static void mdl_pick_goal( Rng *rng, MdlEnt far *ent )
{
    float ang  = rng_float( rng ) * 6.28318531f;
    float dist = MDL_WANDER_MIN + rng_float( rng ) * ( MDL_WANDER_MAX - MDL_WANDER_MIN );

    ent->goal.x = ent->pos.x + (float) cos( ang ) * dist;
    ent->goal.y = ent->pos.y + (float) sin( ang ) * dist;
    ent->goal.z = ent->pos.z;
}

/* ai_melee's and Demon_Melee's test: the player within range of the origin */
static short mdl_in_reach( Player *player, MdlEnt far *ent, float range )
{
    float dx = player->pos.x - ent->pos.x;
    float dy = player->pos.y - ent->pos.y;
    float dz = player->pos.z - ent->pos.z;

    return (short) ( dx*dx + dy*dy + dz*dz <= range * range );
}

/* army_fire: four pellets at the player's position 0.2 s back along
   their own velocity, each traced through the world and then against
   the player's box. */
static void mdl_fire( World *world, Player *player, Fight *fight,
                       Renderer *rdr, MdlEnt far *ent )
{
    BspVec3 org, fin, aim, dir;
    TraceResult tr;
    float l;
    short p, dmg = 0;

    /* army_fire's EF_MUZZLEFLASH; a monster's angles carry no pitch */
    aim.x = (float) cos( ent->yaw * 0.01745329f );
    aim.y = (float) sin( ent->yaw * 0.01745329f );
    aim.z = 0.0f;
    {
        BspVec3 org = ent->pos;
        dl_muzzle( rdr, (short) ( DL_KEY_MON + ( ent - world->mon ) ), &org, &aim );
    }

    aim.x = player->pos.x - player->vel.x * MDL_AIM_LAG - ent->pos.x;
    aim.y = player->pos.y - player->vel.y * MDL_AIM_LAG - ent->pos.y;
    aim.z = player->pos.z - player->vel.z * MDL_AIM_LAG - ent->pos.z;
    l = (float) sqrt( aim.x*aim.x + aim.y*aim.y + aim.z*aim.z );
    if ( l < 1.0f ) return;
    aim.x /= l; aim.y /= l; aim.z /= l;

    org.x = ent->pos.x + aim.x * 10.0f;
    org.y = ent->pos.y + aim.y * 10.0f;
    org.z = ent->pos.z + MDL_ZLO + ( MDL_ZHI - MDL_ZLO ) * 0.7f;

    for ( p = 0; p < MDL_PELLETS; p++ ) {
        pl_spread_dir( &world->rng, &aim, MDL_SPREAD, MDL_SPREAD, &dir );
        fin.x = org.x + dir.x * PL_SHOT_RANGE;
        fin.y = org.y + dir.y * PL_SHOT_RANGE;
        fin.z = org.z + dir.z * PL_SHOT_RANGE;
        pl_trace( world, &org, &fin, &tr );
        if ( mdl_ray_player( &player->pos, &org, &dir, PL_SHOT_RANGE * tr.frac ) >= 0.0f )
            dmg = (short) ( dmg + MDL_PELLET_DMG );
    }
    if ( dmg > 0 ) combat_damage_player( player, world, fight, rdr, dmg );
}

/* CastLightning: a hull-0 line from 40 up toward 16 above the player's
   origin. The endpoint is based on self.origin, not org: that 40-unit
   mismatch is in id's QC. */
static void mdl_bolt( World *world, Player *player, Fight *fight,
                       Renderer *rdr, MdlEnt far *ent )
{
    BspVec3 org, fin, d;
    TraceResult tr;
    float l;

    if ( ent->anim_frame == SHAMBLER_BOLT_A ) mdl_voice( player, &world->snd, ent, CHAN_WEAPON, SND_SHAM_BOOM, ATTN_NORM );
    org = ent->pos;
    org.z += SHAMBLER_BOLT_UP;
    d.x = player->pos.x - org.x;
    d.y = player->pos.y - org.y;
    d.z = player->pos.z + SHAMBLER_BOLT_AIM - org.z;
    l = (float) sqrt( d.x*d.x + d.y*d.y + d.z*d.z );
    if ( l < 1.0f ) return;
    d.x /= l; d.y /= l; d.z /= l;
    fin.x = ent->pos.x + d.x * SHAMBLER_BOLT_RANGE;
    fin.y = ent->pos.y + d.y * SHAMBLER_BOLT_RANGE;
    fin.z = ent->pos.z + d.z * SHAMBLER_BOLT_RANGE;
    pl_trace_point( world, &org, &fin, &tr );
    d_beam_spawn( rdr, ent->order, MDL_FX_BOLT, &org, &tr.end_pos );
    if ( mdl_ray_player( &player->pos, &org, &d,
                          SHAMBLER_BOLT_RANGE * tr.frac ) >= 0.0f )
        combat_damage_player( player, world, fight, rdr, SHAMBLER_BOLT_DMG );
}

/* dog_leap1 and demon1_jump1 begin facing on the ground. Their own
   leap2/leap4 think supplies velocity later. */
static void mdl_leap_start( MdlEnt far *ent, float dx, float dy )
{
    ent->ideal_yaw = mdl_vectoyaw( dx, dy );
    ent->yaw = ent->ideal_yaw;
    mdl_anim_start( ent, MDL_ST_LEAP, MDL_SET_LEAP );
    ent->vel.x = ent->vel.y = ent->vel.z = 0.0f;
}

/* CheckDogJump's and CheckDemonJump's height test: the player's body
   between a quarter and three quarters up the monster's */
static short mdl_leap_height( float dz )
{
    return (short) ( MDL_ZLO < dz + PL_ZLO + 0.75f * ( PL_ZHI - PL_ZLO ) &&
                     MDL_ZHI > dz + PL_ZLO + 0.25f * ( PL_ZHI - PL_ZLO ) );
}

void mdl_spawn( World *world, MdlEnt far *ent )
{
    BspVec3 start, fin;
    TraceResult tr;

    ent->ideal_yaw = ent->yaw;
    ent->next_think = 0.0f;
    mdl_anim_start( ent, MDL_ST_STAND, ent->cruc ? MDL_SET_CRUC : MDL_SET_STAND );
    ent->goal = ent->pos;
    ent->wander_ticks = 0;
    ent->hunting = 0;
    ent->next_attack = 0.0f;
    ent->flash_until = 0.0f;
    ent->pain_finished = 0.0f;
    ent->leap_touch = 0;
    ent->vel.x = ent->vel.y = ent->vel.z = 0.0f;
    ent->idle_at = 0.0f;
    ent->water = 0;

    /* walkmonster_start_go: a target is a path_corner, and th_walk at once */
    ent->corner = ent->patrol;
    if ( ent->corner >= 0 && ent->corner < world->corner_count ) {
        mdl_patrol_to( world, ent, ent->corner );
        mdl_anim_start( ent, MDL_ST_RUN, MDL_SET_WALK );
    }

    ent->health = mon_facts[ ent->kind ].health;

    /* staggered: a row of monsters spawned in one tick would otherwise
       all pick their first wander goal on the same think */
    ent->stand_until = rng_float( &world->rng ) * MDL_STAND_MAX;

    /* droptofloor. The map puts a monster at its own origin, which can
       be well above the floor, and nothing here has gravity: without
       this it stands in mid-air for ever, since mdl_movestep's own
       +-STEPSIZE band only finds a floor already within 18 units. */
    /* From a copy: pl_trace's start is near, and `&ent->pos` handed to it
       lost its segment -- the trace started from DGROUP, and a different
       monster stood somewhere else in every build. */
    start = ent->pos;
    fin = ent->pos;
    fin.z -= 256.0f;
    pl_trace( world, &start, &fin, &tr );
    /* flymonster_start_go: a flyer stays where the map put it */
    if ( !ent->cruc && !mon_facts[ ent->kind ].flyer && tr.frac < 1.0f && !tr.all_solid )
        ent->pos = tr.end_pos;
    ent->spawn = ent->pos;
}

/* zombie_cruc1..6: nailed at the map origin. Every frame waits 0.1 to
   0.2 seconds; the first one rolls its static moan. */
static void near ai_cruc( Think *t )
{
    MdlEnt far *ent = t->ent;

    if ( ent->idle_at == 0.0f )
        ent->idle_at = -1.0f;
    else
        ent->anim_frame = (short) ( ( ent->anim_frame + 1 ) % mdl_set_count( t->m, MDL_SET_CRUC ) );
    if ( ent->anim_frame == 0 && rng_float( &t->world->rng ) < 0.1f )
        mdl_voice( t->player, &t->world->snd, ent, CHAN_VOICE, SND_CRUC_IDLE, ATTN_STATIC );
    ent->next_think = t->rdr->anim_time + 0.1f;
    if ( ent->anim_frame ) ent->next_think += rng_float( &t->world->rng ) * 0.1f;
}

static short near mdl_zombie_can_stand( World *world, Player *player,
                                        short player_alive, MdlEnt far *ent,
                                        MdlEnt far *mon, short mon_count,
                                        short trace_world )
{
    short i;

    if ( player_alive &&
         fabs( ent->pos.x - player->pos.x ) < MDL_HALF + PL_HALF &&
         fabs( ent->pos.y - player->pos.y ) < MDL_HALF + PL_HALF &&
         ent->pos.z + MDL_ZHI > player->pos.z + PL_ZLO &&
         ent->pos.z + MDL_ZLO < player->pos.z + PL_ZHI ) return 0;
    for ( i = 0; i < mon_count; i++ ) {
        MdlEnt far *other = &mon[i];
        if ( other == ent || !MDL_IS_SOLID( other ) ) continue;
        if ( fabs( ent->pos.x - other->pos.x ) < MDL_HALF + MDL_MON_HALF( other->kind ) &&
             fabs( ent->pos.y - other->pos.y ) < MDL_HALF + MDL_MON_HALF( other->kind ) &&
             ent->pos.z + MDL_ZHI > other->pos.z + MDL_ZLO &&
             ent->pos.z + MDL_ZLO < other->pos.z + MDL_MON_ZHI( other->kind ) ) return 0;
    }
    return trace_world ? mdl_movestep( world, player, ent, 0.0f, 0.0f ) : -1;
}

static short near mdl_pain_dist( const MdlEnt far *ent )
{
    const signed char far *dist = 0;
    short count = 0;

    switch ( ent->kind ) {
    case MDL_KIND_ARMY:
        if ( ent->anim_set == MDL_SET_PAIN ) { dist = army_pain_dist; count = 6; }
        else if ( ent->anim_set == MDL_SET_PAIN2 ) { dist = army_painb_dist; count = 14; }
        else if ( ent->anim_set == MDL_SET_PAIN3 ) { dist = army_painc_dist; count = 13; }
        break;
    case MDL_KIND_KNIGHT:
        if ( ent->anim_set == MDL_SET_PAIN2 ) { dist = knight_painb_dist; count = 11; }
        break;
    case MDL_KIND_DOG:
        if ( ent->anim_set == MDL_SET_PAIN2 ) { dist = dog_painb_dist; count = 16; }
        break;
    case MDL_KIND_OGRE:
        if ( ent->anim_set == MDL_SET_PAIN4 ) { dist = ogre_paind_dist; count = 16; }
        else if ( ent->anim_set == MDL_SET_PAIN5 ) { dist = ogre_paine_dist; count = 15; }
        break;
    case MDL_KIND_ZOMBIE:
        if ( ent->anim_set == MDL_SET_PAIN ) { dist = zombie_paina_dist; count = 12; }
        else if ( ent->anim_set == MDL_SET_PAIN2 ) { dist = zombie_painb_dist; count = 28; }
        else if ( ent->anim_set == MDL_SET_PAIN3 ) { dist = zombie_painc_dist; count = 18; }
        else if ( ent->anim_set == MDL_SET_PAIN4 ) { dist = zombie_paind_dist; count = 13; }
        else if ( ent->anim_set == MDL_SET_PAIN5 ) { dist = zombie_paine_dist; count = 30; }
        break;
    }
    return dist && ent->anim_frame < count ? dist[ent->anim_frame] : 0;
}

static float near mdl_pain_heading( const MdlEnt far *ent, short dist )
{
    return dist > 0 ? ent->ideal_yaw : ent->yaw;
}

/* SV_CheckWaterTransition: a splash crossing a liquid's surface, sky
   counting as one as id's test has it */
static void near ai_water( Think *t )
{
    MdlEnt far *ent = t->ent;
    BspVec3 goal;
    short in;

    goal = ent->pos;
    in = (short) ( pl_point_contents( &goal, t->world ) <= CONTENTS_WATER ? 1 : -1 );
    if ( ent->water && ent->water != in ) mdl_voice( t->player, &t->world->snd, ent, CHAN_AUTO, SND_H2OHIT, ATTN_NORM );
    ent->water = in;
}

/* Hit: the flinch, then the hunt. */
static void near ai_pain( Think *t )
{
    MdlEnt far *ent = t->ent;
    Renderer *rdr = t->rdr;
    short lies = (short) ( mon_info[ ent->kind ].pain_lies && ent->anim_set == MDL_SET_PAIN5 );
    short count = mdl_set_count( t->m, ent->anim_set );
    short dist;

    /* zombie_paine11 lies non-solid for five seconds. paine12 restores
       solidity only when walkmove(0, 0) says the standing box fits. */
    if ( lies && ent->anim_frame == 10 ) {
        if ( ent->pain_finished == 0.0f ) ent->pain_finished = rdr->anim_time + ZOMBIE_DOWN_WAIT;
        if ( rdr->anim_time < ent->pain_finished ) return;
        if ( !mdl_zombie_can_stand( t->world, t->player, t->fight->health > 0,
                                    ent, t->world->mon, t->world->mon_count, -1 ) ) {
            ent->pain_finished = rdr->anim_time + ZOMBIE_DOWN_WAIT;
            return;
        }
    }
    ent->anim_frame++;
    if ( ent->anim_frame >= count ) {
        mdl_anim_start( ent, MDL_ST_RUN, MDL_SET_RUN );
        return;
    }
    dist = mdl_pain_dist( ent );
    if ( dist ) {
        float yaw = mdl_pain_heading( ent, dist ) * DEG2RAD;
        mdl_movestep( t->world, t->player, ent,
                      (float) cos( yaw ) * dist, (float) sin( yaw ) * dist );
    }
    if ( lies && ent->anim_frame == 9 )
        mdl_voice( t->player, &t->world->snd, ent, CHAN_BODY, SND_Z_FALL, ATTN_NORM );
    if ( lies && ent->anim_frame == 10 ) ent->pain_finished = rdr->anim_time + ZOMBIE_DOWN_WAIT;
}

static short near mdl_attack_dist( const MdlEnt far *ent )
{
    const short far *dist = 0;
    short count = 0;

    switch ( ent->kind ) {
    case MDL_KIND_KNIGHT:
        if ( ent->anim_set == MDL_SET_ATK ) { dist = knight_atk_dist; count = 10; }
        else if ( ent->anim_set == MDL_SET_ATK2 ) { dist = knight_runatk_dist; count = 11; }
        break;
    case MDL_KIND_DOG:
        if ( ent->anim_set == MDL_SET_ATK ) { dist = dog_atk_dist; count = 8; }
        break;
    case MDL_KIND_OGRE:
        if ( ent->anim_set == MDL_SET_ATK2 ) { dist = ogre_swing_dist; count = 14; }
        else if ( ent->anim_set == MDL_SET_ATK3 ) { dist = ogre_smash_dist; count = 14; }
        break;
    case MDL_KIND_DEMON:
        if ( ent->anim_set == MDL_SET_ATK ) { dist = demon_atk_dist; count = 15; }
        break;
    case MDL_KIND_SHAMBLER:
        if ( ent->anim_set == MDL_SET_ATK2 ) { dist = sham_smash_dist; count = 12; }
        else if ( ent->anim_set == MDL_SET_ATK3 ) { dist = sham_swingr_dist; count = 9; }
        else if ( ent->anim_set == MDL_SET_ATK4 ) { dist = sham_swingl_dist; count = 9; }
        break;
    }
    return dist && ent->anim_frame < count ? dist[ent->anim_frame] : 0;
}

static float near mdl_side_heading( const MdlEnt far *ent, const Player *player )
{
    float yaw = ent->yaw * DEG2RAD;
    float dx = player->pos.x - ent->pos.x - (float) sin( yaw ) * 30.0f;
    float dy = player->pos.y - ent->pos.y + (float) cos( yaw ) * 30.0f;
    return mdl_vectoyaw( dx, dy );
}

static void near mdl_attack_step( Think *t )
{
    Player *player = t->player;
    MdlEnt far *ent = t->ent;
    const MonsterInfo far *inf = &mon_info[ent->kind];
    short dist;

    ent->ideal_yaw = mdl_vectoyaw( player->pos.x - ent->pos.x,
                                    player->pos.y - ent->pos.y );
    mdl_change_yaw( ent );
    dist = mdl_attack_dist( ent );
    if ( ent->kind == MDL_KIND_KNIGHT && ent->anim_set == MDL_SET_ATK2 &&
         ent->anim_frame >= 1 && ent->anim_frame <= 9 ) {
        float heading = mdl_side_heading( ent, player ) * DEG2RAD;
        mdl_movestep( t->world, player, ent,
                      (float) cos( heading ) * 20.0f,
                      (float) sin( heading ) * 20.0f );
    } else if ( dist )
        mdl_movestep( t->world, player, ent,
                      (float) cos( ent->yaw * DEG2RAD ) * dist,
                      (float) sin( ent->yaw * DEG2RAD ) * dist );
    if ( inf->attack_frame ) inf->attack_frame( t );
    if ( ent->kind == MDL_KIND_SHAMBLER && ent->anim_set == MDL_SET_ATK && ent->anim_frame == 2 )
        ent->next_think += 0.2f;
}

static void near mdl_attack_start( Think *t, short set )
{
    mdl_anim_start( t->ent, MDL_ST_ATTACK, set );
    mdl_attack_step( t );
}

/* The attack set, facing the player: the knight's and the demon's
   charge the frame's distance and strike on their own frames. The
   ogre's grenade, the zombie's gib and the wizard's spikes leave on
   their own frames. */
/* the sword on knight_atk's frames, within reach */
static void near atk_army( Think *t )
{
    if ( t->ent->anim_frame != 4 ) return;
    t->ent->flash_until = t->rdr->anim_time + MDL_FLASH;
    mdl_say( t->player, &t->world->snd, t->ent, 1 );
    mdl_fire( t->world, t->player, t->fight, t->rdr, t->ent );
}

static void near atk_knight( Think *t )
{
    World *world = t->world;
    Player *player = t->player;
    MdlEnt far *ent = t->ent;
    float dmg;

    if ( ( ( ent->anim_set == MDL_SET_ATK && ent->anim_frame >= KNIGHT_ATK_FIRST &&
             ent->anim_frame <= KNIGHT_ATK_LAST ) ||
           ( ent->anim_set == MDL_SET_ATK2 && ent->anim_frame >= 4 && ent->anim_frame <= 8 ) ) &&
         mdl_in_reach( player, ent, KNIGHT_MELEE_RANGE ) ) {
        dmg = ( rng_float( &world->rng ) + rng_float( &world->rng ) + rng_float( &world->rng ) ) * KNIGHT_MELEE_DMG;
        if ( dmg > 0 ) combat_damage_player( player, world, t->fight, t->rdr, dmg );
    }
}

static void near atk_dog( Think *t )
{
    float dmg;

    if ( t->ent->anim_frame != 3 || !mdl_in_reach( t->player, t->ent, DOG_BITE_RANGE ) ) return;
    dmg = ( rng_float( &t->world->rng ) + rng_float( &t->world->rng ) +
            rng_float( &t->world->rng ) ) * DOG_BITE_DMG;
    if ( dmg > 0.0f ) combat_damage_player( t->player, t->world, t->fight, t->rdr, dmg );
}

/* Demon_Melee on attacka's two claw frames */
static void near atk_demon( Think *t )
{
    World *world = t->world;
    Player *player = t->player;
    MdlEnt far *ent = t->ent;

    if ( ( ent->anim_frame == DEMON_CLAW_A || ent->anim_frame == DEMON_CLAW_B ) &&
         mdl_in_reach( player, ent, DEMON_CLAW_RANGE ) )
        combat_damage_player( player, world, t->fight, t->rdr,
                    DEMON_CLAW_BASE + rng_float( &world->rng ) * DEMON_CLAW_DMG );
}

static void near atk_ogre( Think *t )
{
    MdlEnt far *ent = t->ent;
    float dmg;

    if ( ent->anim_set == MDL_SET_ATK && ent->anim_frame == OGRE_GREN_FRAME )
        mdl_grenade( t->player, &t->world->snd, t->fight, t->rdr, t->ent );
    if ( ent->anim_set == MDL_SET_ATK3 && ent->anim_frame == 10 )
        ent->next_think += rng_float( &t->world->rng ) * 0.2f;
    if ( ( ent->anim_set == MDL_SET_ATK2 && ent->anim_frame >= 4 && ent->anim_frame <= 10 ) ||
         ( ent->anim_set == MDL_SET_ATK3 && ent->anim_frame >= 5 && ent->anim_frame <= 10 ) ) {
        if ( !mdl_in_reach( t->player, ent, OGRE_SAW_RANGE ) ) return;
        dmg = ( rng_float( &t->world->rng ) + rng_float( &t->world->rng ) +
                rng_float( &t->world->rng ) ) * OGRE_SAW_DMG;
        if ( dmg > 0.0f ) combat_damage_player( t->player, t->world, t->fight, t->rdr, dmg );
    }
}

static void near atk_zombie( Think *t )
{
    if ( t->ent->anim_frame == mdl_set_count( t->m, t->ent->anim_set ) - 1 )
        mdl_gib( t->player, &t->world->snd, t->fight, t->rdr, t->ent );
}

static short near mdl_wizard_fire_frame( short frame )
{
    return (short) ( frame == WIZARD_FIRE_A || frame == WIZARD_FIRE_B );
}

static void near atk_wizard( Think *t )
{
    if ( mdl_wizard_fire_frame( t->ent->anim_frame ) )
        mdl_spike( t->world, t->player, t->fight, t->rdr, t->ent );
}

static void near atk_shambler( Think *t )
{
    MdlEnt far *ent = t->ent;
    short f = t->ent->anim_frame;
    float dmg;

    if ( ent->anim_set == MDL_SET_ATK &&
         ( f == SHAMBLER_BOLT_A || f == SHAMBLER_BOLT_B || f == SHAMBLER_BOLT_C ||
           ( f == SHAMBLER_BOLT_C + 1 && t->fight->skill == 3 ) ) )
        mdl_bolt( t->world, t->player, t->fight, t->rdr, t->ent );
    if ( !( ( ent->anim_set == MDL_SET_ATK2 && f == 9 ) ||
            ( ( ent->anim_set == MDL_SET_ATK3 || ent->anim_set == MDL_SET_ATK4 ) && f == 6 ) ) ) return;
    if ( !mdl_in_reach( t->player, ent, 100.0f ) ) return;
    dmg = ( rng_float( &t->world->rng ) + rng_float( &t->world->rng ) +
            rng_float( &t->world->rng ) ) *
          ( ent->anim_set == MDL_SET_ATK2 ? SHAMBLER_SMASH_DMG : 20.0f );
    if ( dmg > 0.0f ) {
        combat_damage_player( t->player, t->world, t->fight, t->rdr, dmg );
        mdl_voice( t->player, &t->world->snd, ent, CHAN_VOICE, SND_SHAM_SMACK, ATTN_NORM );
    }
}

static void near ai_attack( Think *t )
{
    MdlEnt far *ent = t->ent;
    short next;

    ent->anim_frame++;
    if ( ent->anim_frame < mdl_set_count( t->m, ent->anim_set ) ) {
        mdl_attack_step( t );
        return;
    }
    if ( ent->kind == MDL_KIND_SHAMBLER &&
         ( ent->anim_set == MDL_SET_ATK3 || ent->anim_set == MDL_SET_ATK4 ) &&
         rng_float( &t->world->rng ) < 0.5f ) {
        next = ent->anim_set == MDL_SET_ATK3 ? MDL_SET_ATK4 : MDL_SET_ATK3;
        mdl_attack_start( t, next );
    } else {
        mdl_anim_start( ent, MDL_ST_RUN, MDL_SET_RUN );
    }
}

static short near mdl_leap_phase( MdlEnt far *ent )
{
    short launch = ent->kind == MDL_KIND_DOG ? 1 : 3;
    if ( ent->anim_frame < launch ) ent->anim_frame++;
    return (short) ( ent->anim_frame >= launch );
}

static void near mdl_leap_advance( MdlEnt far *ent, const MdlState *m,
                                    short launched )
{
    if ( launched ) return;
    if ( ent->kind == MDL_KIND_DOG && ent->anim_set == MDL_SET_LEAP ) {
        if ( ent->anim_frame < mdl_set_count( m, MDL_SET_LEAP ) - 1 ) ent->anim_frame++;
    } else if ( ent->kind == MDL_KIND_DEMON && ent->anim_set == MDL_SET_LEAP ) {
        if ( ent->anim_frame < 9 ) ent->anim_frame++;
    } else {
        ent->anim_frame = (short) ( ( ent->anim_frame + 1 ) % mdl_set_count( m, ent->anim_set ) );
    }
}

/* In the air: gravity on the velocity, a trace along it, and
   Dog_JumpTouch. A stop while falling is the ground; one while
   rising is a wall, and the leaper drops down it. */
static void near ai_leap( Think *t )
{
    World *world = t->world;
    Player *player = t->player;
    MdlEnt far *ent = t->ent;
    BspVec3 goal, fin;
    TraceResult tr;
    float dx, dy, dz, thr;
    float fwd, up;
    short launched = 0;
    const MonsterInfo far *inf = &mon_info[ ent->kind ];

    if ( ent->kind == MDL_KIND_DEMON && ent->anim_frame >= 10 &&
         ent->vel.x == 0.0f && ent->vel.y == 0.0f && ent->vel.z == 0.0f ) {
        if ( ent->anim_frame < 11 ) ent->anim_frame++;
        else mdl_anim_start( ent, MDL_ST_RUN, MDL_SET_RUN );
        return;
    }
    if ( ent->leap_touch != MDL_THROWN &&
         ent->vel.x == 0.0f && ent->vel.y == 0.0f && ent->vel.z == 0.0f ) {
        ent->ideal_yaw = mdl_vectoyaw( player->pos.x - ent->pos.x,
                                       player->pos.y - ent->pos.y );
        mdl_change_yaw( ent );
        if ( !mdl_leap_phase( ent ) ) return;
        fwd = ent->kind == MDL_KIND_DOG ? DOG_LEAP_SPEED : DEMON_LEAP_SPEED;
        up = ent->kind == MDL_KIND_DOG ? DOG_LEAP_UP : DEMON_LEAP_UP;
        ent->pos.z += 1.0f;
        ent->vel.x = (float) cos( ent->yaw * DEG2RAD ) * fwd;
        ent->vel.y = (float) sin( ent->yaw * DEG2RAD ) * fwd;
        ent->vel.z = up;
        t->fight->leaps++;
        launched = -1;
    }
    ent->vel.z -= t->fight->gravity * 0.1f;
    fin.x = ent->pos.x + ent->vel.x * 0.1f;
    fin.y = ent->pos.y + ent->vel.y * 0.1f;
    fin.z = ent->pos.z + ent->vel.z * 0.1f;
    goal = ent->pos;
    pl_trace( world, &goal, &fin, &tr );
    ent->pos = tr.end_pos;
    /* a trigger_monsterjump throw counts once it rises; -1 keeps the bite off */
    if ( ent->leap_touch == MDL_THROWN && ent->pos.z > goal.z ) {
        world->mon_jumps++;
        ent->leap_touch = -1;
    }
    if ( !ent->leap_touch ) {
        dx = player->pos.x - ent->pos.x;
        dy = player->pos.y - ent->pos.y;
        dz = player->pos.z - ent->pos.z;
        if ( fabs( dx ) < MDL_HALF + PL_HALF && fabs( dy ) < MDL_HALF + PL_HALF &&
             fabs( dz ) < MDL_ZHI - PL_ZLO ) {
            thr = inf->leap_touch;
            if ( ent->vel.x * ent->vel.x + ent->vel.y * ent->vel.y +
                 ent->vel.z * ent->vel.z > thr * thr ) {
                combat_damage_player( player, world, t->fight, t->rdr,
                    inf->leap_dmg + rng_float( &world->rng ) * inf->leap_scale );
                ent->leap_touch = -1;
            }
        }
    }
    if ( tr.frac < 1.0f ) {
        if ( ent->vel.z <= 0.0f ) {
            if ( ent->kind == MDL_KIND_DEMON && ent->anim_set == MDL_SET_LEAP ) {
                ent->vel.x = ent->vel.y = ent->vel.z = 0.0f;
                ent->leap_touch = -1;
                ent->anim_frame = 10;
            } else {
                mdl_anim_start( ent, MDL_ST_RUN, MDL_SET_RUN );
            }
            return;
        }
        ent->vel.x = ent->vel.y = 0.0f;
    }
    mdl_leap_advance( ent, t->m, launched );
}

static void near ai_stand( Think *t )
{
    World *world = t->world;
    Renderer *rdr = t->rdr;
    MdlEnt far *ent = t->ent;

    if ( t->can_chase && rdr->anim_time >= t->fight->invis_until &&
         mdl_find_target( world, t->player, t->fight, rdr, ent ) ) {
        ent->hunting = -1;
        mdl_say( t->player, &world->snd, ent, 0 );
        mdl_anim_start( ent, MDL_ST_RUN, MDL_SET_RUN );
        ent->wander_ticks = 0;
    } else if ( rdr->anim_time >= ent->stand_until ) {
        if ( ent->corner >= 0 ) {
            mdl_patrol_to( world, ent, ent->corner );
        } else {
            mdl_pick_goal( &world->rng, ent );
            ent->ideal_yaw = mdl_vectoyaw( ent->goal.x - ent->pos.x,
                                            ent->goal.y - ent->pos.y );
        }
        mdl_anim_start( ent, MDL_ST_RUN, MDL_SET_WALK );
        ent->wander_ticks = 0;
    } else {
        ent->anim_frame = (short) ( ( ent->anim_frame + 1 ) % mdl_set_count( t->m, ent->anim_set ) );
    }
}

/* CheckAttack for a monster with th_melee only: in RANGE_MELEE
   with a clear line, the sword instead of a step. -1: the think is over. */
static short near ai_check_knight( Think *t )
{
    World *world = t->world;
    Player *player = t->player;
    MdlEnt far *ent = t->ent;
    float dx, dy, dz;

    if ( mdl_find_target( world, player, t->fight, t->rdr, ent ) ) {
        dx = player->pos.x - ent->pos.x;
        dy = player->pos.y - ent->pos.y;
        dz = player->pos.z - ent->pos.z;
        if ( dx*dx + dy*dy + dz*dz < MDL_RANGE_MELEE * MDL_RANGE_MELEE ) {
            /* knight_attack chooses the motion only by range. The run
               attack's independent random chooses its sword sound. */
            if ( dx*dx + dy*dy + dz*dz >= 80.0f * 80.0f ) {
                mdl_attack_start( t, MDL_SET_ATK2 );
                if ( rng_float( &world->rng ) > 0.5f )
                    mdl_voice( player, &world->snd, ent, CHAN_WEAPON, SND_SWORD2, ATTN_NORM );
                else
                    mdl_say( player, &world->snd, ent, 1 );
            } else {
                mdl_say( player, &world->snd, ent, 1 );
                mdl_attack_start( t, MDL_SET_ATK );
            }
            return -1;
        }
    }
    return 0;
}

/* DogCheckAttack: dog_bite on the run within range, once an
   attack cycle -- no attack set fits the page -- else the leap,
   CheckDogJump's 80 to 150 level with the body at its height. */
static short near ai_check_dog( Think *t )
{
    World *world = t->world;
    Player *player = t->player;
    Fight *fight = t->fight;
    Renderer *rdr = t->rdr;
    MdlEnt far *ent = t->ent;
    float dx, dy, dz;

    if ( rdr->anim_time >= ent->next_attack &&
         mdl_find_target( world, player, fight, rdr, ent ) ) {
        dx = player->pos.x - ent->pos.x;
        dy = player->pos.y - ent->pos.y;
        dz = player->pos.z - ent->pos.z;
        if ( dx*dx + dy*dy + dz*dz < DOG_BITE_RANGE * DOG_BITE_RANGE ) {
            ent->next_attack = rdr->anim_time + DOG_BITE_RATE;
            mdl_say( player, &world->snd, ent, 1 );
            mdl_attack_start( t, MDL_SET_ATK );
            return -1;
        } else if ( dx*dx + dy*dy > DOG_LEAP_MIN * DOG_LEAP_MIN &&
                    dx*dx + dy*dy < DOG_LEAP_MAX * DOG_LEAP_MAX &&
                    mdl_leap_height( dz ) ) {
            mdl_leap_start( ent, dx, dy );
            ent->next_attack = rdr->anim_time + DOG_BITE_RATE;
            mdl_say( player, &world->snd, ent, 1 );
            return -1;
        }
    }
    return 0;
}

/* CheckAttack with a th_melee: the chainsaw on the run in
   RANGE_MELEE -- swing5..11's (r+r+r)*4 every other think -- and
   past it the grenade. */
static short near ai_check_ogre( Think *t )
{
    World *world = t->world;
    Player *player = t->player;
    Fight *fight = t->fight;
    Renderer *rdr = t->rdr;
    MdlEnt far *ent = t->ent;
    float dx, dy, dz, d2, chance;

    if ( mdl_find_target( world, player, fight, rdr, ent ) ) {
        dx = player->pos.x - ent->pos.x;
        dy = player->pos.y - ent->pos.y;
        dz = player->pos.z - ent->pos.z;
        d2 = dx*dx + dy*dy + dz*dz;
        if ( d2 < MDL_RANGE_MELEE * MDL_RANGE_MELEE ) {
            if ( rdr->anim_time >= ent->next_attack ) {
                ent->next_attack = rdr->anim_time + OGRE_SWING;
                mdl_say( player, &world->snd, ent, 1 );
                mdl_attack_start( t, rng_float( &world->rng ) > 0.5f ? MDL_SET_ATK3 : MDL_SET_ATK2 );
                return -1;
            }
        } else if ( rdr->anim_time >= ent->next_attack ) {
            chance = MDL_ATK_MID;
            if ( d2 < MDL_RANGE_NEAR * MDL_RANGE_NEAR ) chance = MDL_ATK_NEAR_MELEE;
            if ( d2 >= MDL_RANGE_MID * MDL_RANGE_MID )  chance = 0.0f;
            if ( rng_float( &world->rng ) < chance ) {
                /* ogre_nail1 says nothing; OgreFireGrenade's throw does */
                ent->next_attack = rdr->anim_time + 2.0f * rng_float( &world->rng );
                mdl_attack_start( t, MDL_SET_ATK );
                return -1;
            }
        }
    }
    return 0;
}

/* DemonCheckAttack: the claws in RANGE_MELEE, else the leap. */
static short near ai_check_demon( Think *t )
{
    World *world = t->world;
    Player *player = t->player;
    Renderer *rdr = t->rdr;
    MdlEnt far *ent = t->ent;
    float dx, dy, dz, d2;

    if ( mdl_find_target( world, player, t->fight, rdr, ent ) ) {
        dx = player->pos.x - ent->pos.x;
        dy = player->pos.y - ent->pos.y;
        dz = player->pos.z - ent->pos.z;
        if ( dx*dx + dy*dy + dz*dz < MDL_RANGE_MELEE * MDL_RANGE_MELEE ) {
            mdl_attack_start( t, MDL_SET_ATK );
            mdl_say( player, &world->snd, ent, 1 );
            return -1;
        } else if ( rdr->anim_time >= ent->next_attack ) {
            d2 = dx*dx + dy*dy;
            if ( d2 > DEMON_LEAP_MIN * DEMON_LEAP_MIN &&
                 ( d2 < DEMON_LEAP_MAX * DEMON_LEAP_MAX || rng_float( &world->rng ) < 0.1f ) &&
                 mdl_leap_height( dz ) ) {
                mdl_leap_start( ent, dx, dy );
                ent->next_attack = rdr->anim_time + 2.0f * rng_float( &world->rng );
                mdl_voice( player, &world->snd, ent, CHAN_VOICE, SND_DJUMP, ATTN_NORM );
                return -1;
            }
        }
    }
    return 0;
}

/* ShamCheckAttack: the smash in RANGE_MELEE, else the lightning. */
static short near ai_check_shambler( Think *t )
{
    World *world = t->world;
    Player *player = t->player;
    Fight *fight = t->fight;
    Renderer *rdr = t->rdr;
    MdlEnt far *ent = t->ent;
    float dx, dy, dz, d2, chance;

    if ( mdl_find_target( world, player, fight, rdr, ent ) ) {
        dx = player->pos.x - ent->pos.x;
        dy = player->pos.y - ent->pos.y;
        dz = player->pos.z - ent->pos.z;
        d2 = dx*dx + dy*dy + dz*dz;
        if ( d2 < MDL_RANGE_MELEE * MDL_RANGE_MELEE ) {
            if ( rdr->anim_time >= ent->next_attack ) {
                ent->next_attack = rdr->anim_time + SHAMBLER_SMASH;
                /* sham_melee: the smash's melee1 past 0.6 or unhurt,
                   swingr's melee2 past 0.3, swingl's melee1 */
                chance = rng_float( &world->rng );
                if ( chance > 0.6f || ent->health == SHAMBLER_HEALTH )
                    mdl_attack_start( t, MDL_SET_ATK2 );
                else if ( chance > 0.3f )
                    mdl_attack_start( t, MDL_SET_ATK3 );
                else
                    mdl_attack_start( t, MDL_SET_ATK4 );
                mdl_voice( player, &world->snd, ent, CHAN_VOICE,
                           ent->anim_set == MDL_SET_ATK3 ? SND_SHAM_MELEE2 : SND_SHAM_MELEE,
                           ATTN_NORM );
                return -1;
            }
        } else if ( rdr->anim_time >= ent->next_attack &&
                    d2 < SHAMBLER_BOLT_RANGE * SHAMBLER_BOLT_RANGE ) {
            ent->next_attack = rdr->anim_time + SHAMBLER_ATK_WAIT + 2.0f * rng_float( &world->rng );
            mdl_attack_start( t, MDL_SET_ATK );
            mdl_say( player, &world->snd, ent, 1 );
            return -1;
        }
    }
    return 0;
}

/* CheckAttack with a th_missile alone, and WizardCheckAttack:
   a clear line, ready, and a chance by range. */
static short near ai_check_missile( Think *t, float near_p, float mid_p, float wait,
                                    short say, short set )
{
    World *world = t->world;
    Player *player = t->player;
    Renderer *rdr = t->rdr;
    MdlEnt far *ent = t->ent;
    float dx, dy, dz, d2, chance;

    if ( rdr->anim_time >= ent->next_attack &&
         mdl_find_target( world, player, t->fight, rdr, ent ) ) {
        dx = player->pos.x - ent->pos.x;
        dy = player->pos.y - ent->pos.y;
        dz = player->pos.z - ent->pos.z;
        d2 = dx*dx + dy*dy + dz*dz;
        if ( d2 < MDL_RANGE_MELEE * MDL_RANGE_MELEE )
            chance = MDL_ATK_MELEE;
        else if ( d2 < MDL_RANGE_NEAR * MDL_RANGE_NEAR )
            chance = near_p;
        else if ( d2 < MDL_RANGE_MID * MDL_RANGE_MID )
            chance = mid_p;
        else
            chance = 0.0f;
        if ( rng_float( &world->rng ) < chance ) {
            ent->next_attack = rdr->anim_time +
                ( wait > 0.0f ? wait : 2.0f * rng_float( &world->rng ) );
            if ( set < 0 ) {
                float r = rng_float( &world->rng );
                set = r < 0.3f ? MDL_SET_ATK : ( r < 0.6f ? MDL_SET_ATK2 : MDL_SET_ATK3 );
            }
            mdl_attack_start( t, set );
            /* Wiz_StartFast's wattack; the zombie's z_shot1 is the throw's */
            if ( say ) mdl_say( player, &world->snd, ent, 1 );
            return -1;
        }
    }
    return 0;
}

static short near ai_check_zombie( Think *t )
{
    return ai_check_missile( t, ZOMBIE_ATK_NEAR, ZOMBIE_ATK_MID, 0.0f, 0, -1 );
}

/* Wiz_StartFast's wait is fixed, and it says wattack */
static short near ai_check_wizard( Think *t )
{
    return ai_check_missile( t, WIZARD_ATK_NEAR, WIZARD_ATK_MID, WIZARD_ATK_WAIT, -1,
                             MDL_SET_ATK );
}

/* SoldierCheckAttack: a clear line, attack_finished passed, and
   a chance by range each think; then army_fire and 1 + random(). */
static short near ai_check_army( Think *t )
{
    World *world = t->world;
    Player *player = t->player;
    Fight *fight = t->fight;
    Renderer *rdr = t->rdr;
    MdlEnt far *ent = t->ent;
    float dx, dy, dz, d2, chance;

    if ( rdr->anim_time >= ent->next_attack &&
         mdl_find_target( world, player, fight, rdr, ent ) ) {
        dx = player->pos.x - ent->pos.x;
        dy = player->pos.y - ent->pos.y;
        dz = player->pos.z - ent->pos.z;
        d2 = dx*dx + dy*dy + dz*dz;
        if ( d2 < MDL_RANGE_MELEE * MDL_RANGE_MELEE )     chance = MDL_ATK_MELEE;
        else if ( d2 < MDL_RANGE_NEAR * MDL_RANGE_NEAR )  chance = MDL_ATK_NEAR;
        else if ( d2 < MDL_RANGE_MID * MDL_RANGE_MID )    chance = MDL_ATK_MID;
        else                                              chance = 0.0f;
        if ( rng_float( &world->rng ) < chance ) {
            ent->next_attack = rdr->anim_time + 1.0f + rng_float( &world->rng );
            mdl_attack_start( t, MDL_SET_ATK );
            return -1;
        }
    }
    return 0;
}

/* ai_walk: FindTarget every frame of the walk too; HuntTarget
   then starts the run cycle and the step is skipped */
static short near ai_walk( Think *t )
{
    MdlEnt far *ent = t->ent;

    if ( t->can_chase && t->rdr->anim_time >= t->fight->invis_until &&
         mdl_find_target( t->world, t->player, t->fight, t->rdr, ent ) ) {
        ent->hunting = -1;
        mdl_say( t->player, &t->world->snd, ent, 0 );
        mdl_anim_start( ent, MDL_ST_RUN, MDL_SET_RUN );
        return -1;
    }
    return 0;
}

/* t_movetarget: at the corner, its wait standing, then the next; a
   corner with no target is stood at for good. */
static void near ai_patrol_arrive( Think *t )
{
    MdlEnt far *ent = t->ent;
    float dx, dy;

    dx = ent->pos.x - ent->goal.x;
    dy = ent->pos.y - ent->goal.y;
    if ( dx*dx + dy*dy < MDL_WANDER_ARRIVE * MDL_WANDER_ARRIVE ) {
        PathCorner far *c = &t->world->corner[ ent->corner ];
        if ( c->nxt < 0 ) {
            ent->corner = -1;
            mdl_anim_start( ent, MDL_ST_STAND, MDL_SET_STAND );
            ent->stand_until = 1.0e9f;
        } else if ( c->wait > 0.0f ) {
            ent->corner = c->nxt;
            mdl_anim_start( ent, MDL_ST_STAND, MDL_SET_STAND );
            ent->stand_until = t->rdr->anim_time + c->wait;
        } else {
            mdl_patrol_to( t->world, ent, c->nxt );
        }
    }
}

/* The wander rests on arrival, or gives up -- the same compass
   search a real chase can also fail to route around. A hunt does
   neither, which is why this is gated on hunting. */
static void near ai_wander_arrive( Think *t )
{
    MdlEnt far *ent = t->ent;
    float dx, dy;

    ent->wander_ticks++;
    dx = ent->pos.x - ent->goal.x;
    dy = ent->pos.y - ent->goal.y;
    if ( dx*dx + dy*dy < MDL_WANDER_ARRIVE * MDL_WANDER_ARRIVE ||
         ent->wander_ticks >= MDL_WANDER_MAXTICKS ) {
        mdl_anim_start( ent, MDL_ST_STAND, MDL_SET_STAND );
        ent->stand_until = t->rdr->anim_time + MDL_STAND_MIN +
                            rng_float( &t->world->rng ) * ( MDL_STAND_MAX - MDL_STAND_MIN );
    }
}

/* ai_run and ai_walk: the frame's distance toward the player or the goal */
static void near ai_run( Think *t )
{
    const MonsterInfo far *inf = &mon_info[ t->ent->kind ];
    MdlEnt far *ent = t->ent;
    BspVec3 goal;
    float dist;

    if ( ent->anim_set == MDL_SET_WALK )
        dist = inf->walk[ ent->anim_frame % inf->nwalk ];
    else
        dist = inf->run[ ent->anim_frame % inf->nrun ];

    goal = t->player->pos;
    if ( ent->hunting ) {
        if ( inf->check_attack( t ) ) return;
    } else {
        if ( ai_walk( t ) ) return;
        goal = ent->goal;
    }

    mdl_move_to_goal( t->world, t->player, ent, &goal, dist );
    ent->anim_frame = (short) ( ( ent->anim_frame + 1 ) % mdl_set_count( t->m, ent->anim_set ) );

    if ( !ent->hunting && ent->corner >= 0 ) {
        ai_patrol_arrive( t );
        return;
    }
    if ( !ent->hunting ) ai_wander_arrive( t );
}

void mdl_think( World *world, Player *player, Fight *fight, Renderer *rdr,
                 MdlEnt far *ent, MdlState *m, short can_chase )
{
    Think t;

    if ( !m->loaded ) return;
    if ( rdr->anim_time < ent->next_think ) return;
    ent->next_think = rdr->anim_time + 0.1f;

    /* Dead: the death frames once, then a corpse. */
    if ( ent->state == MDL_ST_GIBBED ) return;
    if ( ent->state == MDL_ST_DEAD ) {
        if ( ent->anim_frame < mdl_set_count( m, ent->anim_set ) - 1 ) {
            ent->anim_frame++;
            if ( ent->anim_frame == 2 && mon_facts[ent->kind].drop_amount )
                pl_backpack_add( world, ent, mon_facts[ent->kind].drop_kind,
                                 mon_facts[ent->kind].drop_amount, rdr->anim_time );
        }
        return;
    }

    t.world = world;
    t.player = player;
    t.fight = fight;
    t.rdr = rdr;
    t.ent = ent;
    t.m = m;
    t.can_chase = can_chase;

    if ( ent->cruc ) { ai_cruc( &t ); return; }
    ai_water( &t );
    switch ( ent->state ) {
    case MDL_ST_PAIN:   ai_pain( &t );   return;
    case MDL_ST_ATTACK: ai_attack( &t ); return;
    case MDL_ST_LEAP:   ai_leap( &t );   return;
    }
    mdl_idle( &t );
    if ( ent->state == MDL_ST_STAND ) ai_stand( &t );
    else                              ai_run( &t );
}

/* What the AI actually did this run, for a headless assertion: a frame
   cannot show a monster hunting, and a still one cannot be told from a
   working one that had nobody to chase. */
void mdl_ai_stats( World *world, short *hunting, short *moved )
{
    short i, h = 0, mv = 0;

    for ( i = 0; i < world->mon_count; i++ ) {
        MdlEnt far *e = &world->mon[i];
        float dx = e->pos.x - e->spawn.x, dy = e->pos.y - e->spawn.y;
        if ( e->hunting ) h++;
        if ( dx*dx + dy*dy > 32.0f * 32.0f ) mv++;
    }
    *hunting = h;
    *moved = mv;
}

short mdl_ai_visible_selftest( World *world, Player *player )
{
    MdlEnt far *best = 0;
    BspVec3 eye, peye;
    TraceResult tr;
    float dx, dy, dz, d, best_d = 1.0e30f;
    short i;

    for ( i = 0; i < world->mon_count; i++ ) {
        MdlEnt far *e = &world->mon[i];
        if ( e->cruc || MDL_IS_DEAD( e ) ) continue;
        if ( e->kind == MDL_KIND_DEMON ) { best = e; break; }
        dx = e->pos.x - player->pos.x; dy = e->pos.y - player->pos.y; dz = e->pos.z - player->pos.z;
        d = dx*dx + dy*dy + dz*dz;
        if ( d < best_d ) { best = e; best_d = d; }
    }
    if ( !best ) return -1;
    eye = best->pos; eye.z += MDL_VIEW_OFS;
    peye = player->pos; peye.z += PL_EYE;
    i = mdl_visible( world, &eye, &peye );
    pl_trace( world, &eye, &peye, &tr );
    return (short) ( i | ( ( tr.frac == 1.0f && !tr.all_solid ) ? 2 : 0 ) );
}

short mdl_anim_selftest( void )
{
    MdlEnt ent, mons[2];
    MdlState model;
    Player player;
    unsigned char counts[MDL_NSETS];
    short i, launched;
    float heading;

    memset( &ent, 0, sizeof(ent) );
    memset( &model, 0, sizeof(model) );
    memset( &player, 0, sizeof(player) );
    memset( counts, 0, sizeof(counts) );

    model.zhi = 123.0f;
    model.vpages = 12345;
    model.page_frames = 23456;
    for ( i = 0; i < MDL_NSETS; i++ ) counts[i] = (unsigned char) ( ( i * 7 + 3 ) & 31 );
    mdl_set_counts( &model, counts );
    for ( i = 0; i < MDL_NSETS; i++ )
        if ( mdl_set_count( &model, i ) != counts[i] ) return 0;
    if ( model.zhi != 123.0f || model.vpages != 12345 || model.page_frames != 23456 ) return 0;

    memset( counts, 0, sizeof(counts) );
    counts[MDL_SET_LEAP] = 16;
    mdl_set_counts( &model, counts );

    ent.kind = MDL_KIND_DOG; ent.anim_set = MDL_SET_LEAP;
    launched = mdl_leap_phase( &ent );
    mdl_leap_advance( &ent, &model, launched );
    if ( !launched || ent.anim_frame != 1 ) return 0;

    ent.kind = MDL_KIND_DEMON; ent.anim_frame = 0;
    if ( mdl_leap_phase( &ent ) || ent.anim_frame != 1 ) return 0;
    if ( mdl_leap_phase( &ent ) || ent.anim_frame != 2 ) return 0;
    launched = mdl_leap_phase( &ent );
    mdl_leap_advance( &ent, &model, launched );
    if ( !launched || ent.anim_frame != 3 ) return 0;

    ent.kind = MDL_KIND_KNIGHT; ent.yaw = 0.0f;
    player.pos.x = 100.0f;
    heading = mdl_side_heading( &ent, &player );
    if ( heading < 16.0f || heading > 17.0f ) return 0;

    ent.kind = MDL_KIND_OGRE; ent.anim_set = MDL_SET_ATK2; ent.anim_frame = 4;
    if ( mdl_attack_dist( &ent ) != 19 ) return 0;
    ent.anim_set = MDL_SET_ATK3; ent.anim_frame = 5;
    if ( mdl_attack_dist( &ent ) != 14 ) return 0;
    ent.kind = MDL_KIND_SHAMBLER; ent.anim_set = MDL_SET_ATK3; ent.anim_frame = 6;
    if ( mdl_attack_dist( &ent ) != 16 ) return 0;

    ent.kind = MDL_KIND_OGRE; ent.anim_set = MDL_SET_PAIN4; ent.anim_frame = 1;
    if ( mdl_pain_dist( &ent ) != -10 ) return 0;
    ent.kind = MDL_KIND_ARMY; ent.anim_set = MDL_SET_PAIN2; ent.anim_frame = 1;
    if ( mdl_pain_dist( &ent ) != 13 ) return 0;
    ent.yaw = 0.0f; ent.ideal_yaw = 90.0f;
    if ( mdl_pain_heading( &ent, 1 ) != 90.0f || mdl_pain_heading( &ent, -1 ) != 0.0f ) return 0;

    if ( !mdl_wizard_fire_frame( 3 ) || !mdl_wizard_fire_frame( 8 ) ||
         mdl_wizard_fire_frame( 2 ) || mdl_wizard_fire_frame( 7 ) ) return 0;
    ent.kind = MDL_KIND_ZOMBIE; ent.state = MDL_ST_PAIN;
    ent.anim_set = MDL_SET_PAIN5; ent.anim_frame = 10;
    if ( MDL_IS_SOLID( &ent ) || MDL_MON_HALF( MDL_KIND_DOG ) != 32.0f ||
         MDL_MON_ZHI( MDL_KIND_SHAMBLER ) != 64.0f ||
         ZOMBIE_DOWN_WAIT < 5.09f || ZOMBIE_DOWN_WAIT > 5.11f ) return 0;
    ent.anim_frame = 11;
    if ( !MDL_IS_SOLID( &ent ) ) return 0;

    memset( mons, 0, sizeof(mons) );
    mons[0].kind = MDL_KIND_ZOMBIE;
    mons[1].kind = MDL_KIND_DOG; mons[1].state = MDL_ST_RUN;
    mons[1].pos.x = 47.0f;
    if ( mdl_zombie_can_stand( 0, &player, 0, &mons[0], mons, 2, 0 ) ) return 0;
    mons[1].pos.x = 48.0f;
    return (short) ( mdl_zombie_can_stand( 0, &player, 0, &mons[0], mons, 2, 0 ) != 0 );
}

void mdl_tick( World *world, Player *player, Fight *fight, Renderer *rdr,
               short live_ai )
{
    short i;
    BspVec3 was;
    MdlState *m;

    for ( i = 0; i < world->mon_count; i++ ) {
        if ( ( rdr->no_ai || !live_ai ) && !MDL_IS_DEAD( &world->mon[i] ) ) continue;
        m = mdl_of( world, world->mon[i].kind );
        was = world->mon[i].pos;
        if ( m ) mdl_think( world, player, fight, rdr, &world->mon[i], m, -1 );
        if ( !MDL_IS_DEAD( &world->mon[i] ) && !world->mon[i].cruc ) {
            ent_monster_teleport( world, rdr, rdr->anim_time, &world->mon[i] );
            /* SV_LinkEdict's touches: only a monster that moved */
            if ( world->mon[i].pos.x != was.x || world->mon[i].pos.y != was.y ||
                 world->mon[i].pos.z != was.z )
                ent_monster_jump( world, &world->mon[i] );
        }
    }
}
