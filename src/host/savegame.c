/* savegame.c -- single-player snapshots behind id's twelve save slots. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <mem.h>

#include "savegame.h"
#include "qgl.h"
#include "dl.h"
#include "gib.h"
#include "d_sprite.h"
#include "d_beam.h"
#include "item.h"
#include "mdl.h"
#include "mdl_ai.h"

#define SG_MAGIC   0x31565351L
#define SG_VERSION 10
#define SG_VERSION_ITEMS 2
#define SG_VERSION_SPR3 5
#define SG_VERSION_ANIM 6
#define SG_VERSION_SPAWN 7
#define SG_VERSION_ZWAIT 8
#define SG_VERSION_TRAIN_DMG 9
#define SG_VERSION_LIGHTNING 10
#define SG_VERSION_OLD 1
#define SG_DOOR_BYTES 102
#define SG_GIB_MAGIC 0x31424947L
#define SG_SPR_MAGIC 0x31525053L
#define SG_SPR2_MAGIC 0x32525053L
#define SG_SPR3_MAGIC 0x33525053L
#define SG_SPAWN_MAGIC 0x314E5053L
#define SG_V4_BUBBLES 32
typedef char sg_door_prefix_ok[ offsetof(DoorEnt, dmg) == SG_DOOR_BYTES ? 1 : -1 ];

typedef struct {
    long magic;
    short version;
    char map[9];
    char comment[SG_COMMENT];
    short skill;
    short model_count, tele_count, plat_count, door_count;
    short trig_count, light_count, item_count, mon_count;
} SaveHead;

/* Optional trailer: version-1 saves from before gibs end at the monster
   block and still load with an empty pool. */
typedef struct { long magic; short count; } SaveGibs;
typedef char rec_savegibs_ok[ sizeof(SaveGibs) == 6 ? 1 : -1 ];
typedef struct { long magic; short count; } SaveSprites;
typedef struct {
    long magic;
    short explosions, bubbles, spawners;
} SaveSprites2;
typedef struct {
    long magic;
    short explosions, bubbles, spawners, emitters;
} SaveSprites3;
typedef struct {
    long magic;
    PlayerCarry carry;
} SaveSpawn;

typedef struct {
    Rng rng;
    short mon_teles, mon_jumps, light_clock;
    BspVec3 fog_org[ENT_FOGS];
    float fog_at[ENT_FOGS];
} SaveWorld;

static void near savegame_zombie_wait_v7( MdlEnt far *ent )
{
    mdl_zombie_wait_migrate( ent );
}

static short near savegame_monsters( World *world, short version );
static short near savegame_write_block( short file, long src, long bytes );
static short near savegame_read_block( short file, long dst, long bytes );

static void near savegame_train_damage_v8( World *world, short far *damage )
{
    short i;

    for ( i = 0; i < world->plat_count; i++ ) {
        if ( world->plat[i].kind != ENT_PLAT_KIND_TRAIN ) continue;
        world->plat[i].time.block_at = 0.0f;
        world->plat[i].mode.dmg = damage[i] > 0 ? damage[i] : 2;
    }
}

static void near savegame_exit_static( TrigEnt *saved, TrigEnt far *asset )
{
    if ( saved->kind != ENT_TRIG_EXIT || asset->kind != ENT_TRIG_EXIT ) return;
    saved->target = asset->target;
    saved->kill = asset->kill;
    saved->snd = asset->snd;
    saved->delay = asset->delay;
}

/* Door action and damage are map data, not mutable game state. */
static void near savegame_door_static( DoorEnt far *door, short action,
                                       float hold, short dmg )
{
    door->action.nolink = action;
    door->hold = hold;
    door->dmg = dmg;
}

static short near savegame_read_doors( short file, World *world )
{
    short i, action, dmg;
    float hold;

    for ( i = 0; i < world->door_count; i++ ) {
        action = world->door[i].action.nolink;
        hold = world->door[i].hold;
        dmg = world->door[i].dmg;
        if ( !savegame_read_block( file, (long) &world->door[i],
                                   SG_DOOR_BYTES ) ) return 0;
        savegame_door_static( &world->door[i], action, hold, dmg );
    }
    return -1;
}

static short near savegame_write_doors( short file, World *world )
{
    short i;

    for ( i = 0; i < world->door_count; i++ )
        if ( !savegame_write_block( file, (long) &world->door[i],
                                    SG_DOOR_BYTES ) ) return 0;
    return -1;
}

static short near savegame_door_loader_selftest( World *world )
{
    const char *name = "sgdoor.tmp";
    DoorEnt far *copy;
    FILE *create;
    short file, count, action, dmg, sentinel = 0x5a37, got = 0, ok = 0;
    float hold;
    long bytes;

    if ( world->door_count < 1 ) return -1;
    copy = (DoorEnt far *) qglMemAlloc( sizeof(DoorEnt) );
    if ( !copy ) return 0;
    _fmemcpy( copy, &world->door[0], sizeof(DoorEnt) );
    action = world->door[0].action.nolink;
    hold = world->door[0].hold;
    dmg = world->door[0].dmg;
    create = fopen( name, "wb" );
    if ( !create ) goto done;
    fclose( create );
    file = qglFileOpen( name );
    if ( !file ) goto done;
    world->door[0].action.nolink = 1;
    world->door[0].hold = 17.0f;
    world->door[0].dmg = (short) ( dmg + 1 );
    count = world->door_count;
    world->door_count = 1;
    ok = (short) ( savegame_write_doors( file, world ) &&
                   savegame_write_block( file, (long) (void far *) &sentinel,
                                         sizeof(sentinel) ) );
    world->door_count = count;
    world->door[0].action.nolink = action;
    world->door[0].hold = hold;
    world->door[0].dmg = dmg;
    qglFileClose( file );
    if ( !ok ) goto done;
    create = fopen( name, "rb" );
    if ( !create ) { ok = 0; goto done; }
    fseek( create, 0L, SEEK_END );
    bytes = ftell( create );
    fclose( create );
    if ( bytes != SG_DOOR_BYTES + (long) sizeof(sentinel) ) { ok = 0; goto done; }
    file = qglFileOpen( name );
    if ( !file ) { ok = 0; goto done; }
    count = world->door_count;
    world->door_count = 1;
    ok = savegame_read_doors( file, world );
    if ( ok ) ok = (short) ( qglFileRead( file, (long) (void far *) &got,
                                          sizeof(got) ) == sizeof(got) &&
                              got == sentinel );
    world->door_count = count;
    qglFileClose( file );
    ok = (short) ( ok && world->door[0].action.nolink == action &&
                   world->door[0].hold == hold && world->door[0].dmg == dmg );
done:
    _fmemcpy( &world->door[0], copy, sizeof(DoorEnt) );
    qglMemFree( (long) copy );
    remove( name );
    return ok;
}

short savegame_selftest( World *world )
{
    MdlEnt ent;
    PlatEnt plat;
    MdlEnt far *saved_mon = world->mon;
    PlatEnt far *saved_plat = world->plat;
    short saved_count = world->mon_count;
    short saved_plat_count = world->plat_count;
    short damage = 75;
    short ok;
    TrigEnt saved_trig, asset_trig;

    memset( &ent, 0, sizeof(ent) );
    ent.kind = MDL_KIND_ZOMBIE;
    ent.state = MDL_ST_PAIN;
    ent.anim_set = MDL_SET_PAIN5;
    ent.anim_frame = 10;
    ent.next_think = 2.0f;
    ent.pain_finished = 7.0f;
    world->mon = &ent;
    world->mon_count = 1;
    ok = savegame_monsters( world, 7 );
    if ( !ok || ent.next_think != 7.0f || ent.pain_finished != 0.0f ) {
        ok = 0;
        goto done;
    }
    ent.next_think = 8.0f;
    ent.pain_finished = 12.0f;
    ok = savegame_monsters( world, SG_VERSION );
    ok = (short) ( ok && ent.next_think == 8.0f && ent.pain_finished == 12.0f );
done:
    world->mon = saved_mon;
    world->mon_count = saved_count;
    if ( ok ) {
        memset( &plat, 0, sizeof(plat) );
        plat.kind = ENT_PLAT_KIND_TRAIN;
        plat.time.block_at = 8.0f;
        world->plat = &plat;
        world->plat_count = 1;
        savegame_train_damage_v8( world, &damage );
        ok = (short) ( plat.time.block_at == 0.0f && plat.mode.dmg == 75 );
    }
    world->plat = saved_plat;
    world->plat_count = saved_plat_count;
    if ( ok ) {
        memset( &saved_trig, 0, sizeof(saved_trig) );
        memset( &asset_trig, 0, sizeof(asset_trig) );
        saved_trig.kind = asset_trig.kind = ENT_TRIG_EXIT;
        saved_trig.state = ENT_TRIG_DONE;
        asset_trig.target = 7;
        asset_trig.kill = 8;
        asset_trig.snd = ENT_EXIT_NO_INTERMISSION;
        asset_trig.delay = 1.5f;
        savegame_exit_static( &saved_trig, &asset_trig );
        ok = (short) ( saved_trig.state == ENT_TRIG_DONE && saved_trig.target == 7 &&
                       saved_trig.kill == 8 && saved_trig.snd == ENT_EXIT_NO_INTERMISSION &&
                       saved_trig.delay == 1.5f );
    }
    if ( ok ) ok = savegame_door_loader_selftest( world );
    return ok;
}

typedef struct {
    float anim_time;
    DynLight dlights[DL_MAX];
    short dl_tick;
    unsigned short dl_seed;
    short mouse_x, mouse_y;
} SaveRender;

static void near savegame_name( short slot, const char *ext, char *name )
{
    sprintf( name, "s%d.%s", (int) slot, ext );
}

short savegame_slot( const char *name )
{
    short slot;
    if ( !name || ( name[0] != 's' && name[0] != 'S' ) || !name[1] ) return -1;
    slot = (short) atoi( name + 1 );
    if ( slot < 0 || slot >= SG_SLOTS ) return -1;
    if ( slot < 10 && name[2] ) return -1;
    if ( slot >= 10 && name[3] ) return -1;
    return slot;
}

static short near savegame_read_head( short slot, SaveHead *head )
{
    char name[16];
    FILE *file;
    savegame_name( slot, "sav", name );
    file = fopen( name, "rb" );
    if ( !file ) return 0;
    if ( fread( head, sizeof(*head), 1, file ) != 1 ) head->magic = 0;
    fclose( file );
    if ( head->magic != SG_MAGIC ||
         ( head->version < SG_VERSION_OLD || head->version > SG_VERSION ) ) return 0;
    head->map[8] = 0;
    head->comment[SG_COMMENT - 1] = 0;
    return -1;
}

void savegame_scan( char comments[SG_SLOTS][SG_COMMENT], unsigned char loadable[SG_SLOTS] )
{
    SaveHead head;
    short i;
    for ( i = 0; i < SG_SLOTS; i++ ) {
        strcpy( comments[i], "--- UNUSED SLOT ---" );
        loadable[i] = 0;
        if ( !savegame_read_head( i, &head ) ) continue;
        strcpy( comments[i], head.comment );
        loadable[i] = 1;
    }
}

short savegame_header( short slot, char *map, short *skill, char *error )
{
    SaveHead head;
    if ( slot < 0 || slot >= SG_SLOTS || !savegame_read_head( slot, &head ) ) {
        strcpy( error, "Save slot is empty or invalid" );
        return 0;
    }
    strcpy( map, head.map );
    *skill = head.skill;
    return -1;
}

static short near savegame_write_block( short file, long src, long bytes )
{
    return (short) ( qglFileWrite( file, src, bytes ) == bytes );
}

static short near savegame_read_block( short file, long dst, long bytes )
{
    return (short) ( qglFileRead( file, dst, bytes ) == bytes );
}

static void near savegame_comment( const char *map, const World *world,
                                   const Fight *fight, char *comment )
{
    short i = 0;
    if ( world->title )
        while ( i < 21 && world->title[i] ) comment[i++] = world->title[i];
    if ( !i ) while ( i < 8 && map[i] ) comment[i] = map[i], i++;
    while ( i < 22 ) comment[i++] = ' ';
    sprintf( comment + i, "kills:%3d/%3d", (int) fight->kills, (int) world->mon_count );
    comment[SG_COMMENT - 1] = 0;
}

static short near savegame_write( short file, SaveHead *head, World *world,
                                  Player *player, Camera *cam, Fight *fight,
                                  const PlayerCarry *spawn_carry, Renderer *rdr,
                                  short mouse_x, short mouse_y )
{
    SaveWorld sw;
    SaveRender sr;
    SaveGibs sg;
    SaveSprites3 ss;
    SaveSpawn sp;
    GibEnt far *gib;
    SpriteExplosion far *sprite;
    SpriteBubble far *bubble;
    SpriteBubbleSpawner far *spawner;
    SpriteBubbleEmitter far *emitter;

    sw.rng = world->rng;
    sw.mon_teles = world->mon_teles;
    sw.mon_jumps = world->mon_jumps;
    sw.light_clock = world->light_clock;
    memcpy( sw.fog_org, world->fog_org, sizeof(sw.fog_org) );
    memcpy( sw.fog_at, world->fog_at, sizeof(sw.fog_at) );
    sr.anim_time = rdr->anim_time;
    dl_export( rdr, sr.dlights );
    sr.dl_tick = rdr->dl_tick;
    sr.dl_seed = rdr->dl_seed;
    sr.mouse_x = mouse_x;
    sr.mouse_y = mouse_y;

    /* The Thunderbolt sound/recoil clocks are transient tail fields, kept
       out of the file so every pre-v10 block after Fight stays aligned. */
    if ( !savegame_write_block( file, (long) (void far *) head, sizeof(*head) ) ||
         !savegame_write_block( file, (long) (void far *) player, sizeof(*player) ) ||
         !savegame_write_block( file, (long) (void far *) cam, sizeof(*cam) ) ||
         !savegame_write_block( file, (long) (void far *) fight,
                                offsetof(Fight, lg_hit_at) ) ||
         !savegame_write_block( file, (long) (void far *) &sw, sizeof(sw) ) ||
         !savegame_write_block( file, (long) (void far *) &sr, sizeof(sr) ) ) return 0;
    if ( !( savegame_write_block( file, (long) world->brush,
                                  (long) world->model_count * sizeof(BrushModel) ) &&
           savegame_write_block( file, (long) world->tele,
                                 (long) world->tele_count * sizeof(Teleporter) ) &&
           savegame_write_block( file, (long) world->plat,
                                 (long) world->plat_count * sizeof(PlatEnt) ) &&
           savegame_write_doors( file, world ) &&
           savegame_write_block( file, (long) world->trig,
                                 (long) world->trig_count * sizeof(TrigEnt) ) &&
           savegame_write_block( file, (long) world->light,
                                 (long) world->light_count * sizeof(LightEnt) ) &&
           savegame_write_block( file, (long) world->item,
                                 (long) world->item_count * sizeof(ItemEnt) ) &&
           savegame_write_block( file, (long) world->mon,
                                 (long) world->mon_count * sizeof(MdlEnt) ) ) ) return 0;
    sg.magic = SG_GIB_MAGIC;
    sg.count = rdr->gib_count;
    if ( !savegame_write_block( file, (long) (void far *) &sg, sizeof(sg) ) ) return 0;
    if ( sg.count ) {
        gib = gib_map( rdr, 2 );
        if ( !gib || !savegame_write_block( file, (long) gib,
                                  (long) sg.count * sizeof(GibEnt) ) ) return 0;
    }
    ss.magic = SG_SPR3_MAGIC;
    ss.explosions = SPR_EXPLOSIONS;
    ss.bubbles = SPR_BUBBLES;
    ss.spawners = SPR_SPAWNERS;
    ss.emitters = SPR_EMITTERS;
    if ( !savegame_write_block( file, (long) (void far *) &ss, sizeof(ss) ) ) return 0;
    sprite = d_sprite_map( rdr, 2 );
    if ( !sprite || !savegame_write_block( file, (long) sprite,
                         (long) SPR_EXPLOSIONS * sizeof(SpriteExplosion) ) ) return 0;
    bubble = d_bubble_map( rdr, 2 );
    if ( !bubble || !savegame_write_block( file, (long) bubble,
                              (long) SPR_BUBBLES * sizeof(SpriteBubble) ) ) return 0;
    spawner = d_spawner_map( rdr, 2 );
    if ( !spawner || !savegame_write_block( file, (long) spawner,
                         (long) SPR_SPAWNERS * sizeof(SpriteBubbleSpawner) ) ) return 0;
    emitter = d_emitter_map( rdr, 2 );
    if ( !emitter || !savegame_write_block( file, (long) emitter,
                         (long) SPR_EMITTERS * sizeof(SpriteBubbleEmitter) ) ) return 0;
    sp.magic = SG_SPAWN_MAGIC;
    sp.carry = *spawn_carry;
    return savegame_write_block( file, (long) (void far *) &sp, sizeof(sp) );
}

short savegame_save( short slot, const char *map, World *world, Player *player,
                     Camera *cam, Fight *fight, const PlayerCarry *spawn_carry,
                     Renderer *rdr,
                     short mouse_x, short mouse_y, char *error )
{
    SaveHead head;
    char name[16], temp[16], old[16];
    FILE *create;
    short file;
    short ok;

    if ( slot < 0 || slot >= SG_SLOTS ) { strcpy( error, "Use save s0 through s11" ); return 0; }
    memset( &head, 0, sizeof(head) );
    head.magic = SG_MAGIC;
    head.version = SG_VERSION;
    strncpy( head.map, map, 8 );
    head.skill = fight->skill;
    head.model_count = world->model_count;
    head.tele_count = world->tele_count;
    head.plat_count = world->plat_count;
    head.door_count = world->door_count;
    head.trig_count = world->trig_count;
    head.light_count = world->light_count;
    head.item_count = world->item_count;
    head.mon_count = world->mon_count;
    savegame_comment( map, world, fight, head.comment );

    savegame_name( slot, "sav", name );
    savegame_name( slot, "tmp", temp );
    savegame_name( slot, "old", old );
    create = fopen( temp, "wb" );
    if ( !create ) { strcpy( error, "Could not create save file" ); return 0; }
    fclose( create );
    file = qglFileOpen( temp );
    if ( !file ) { remove( temp ); strcpy( error, "Could not open save file" ); return 0; }
    ok = savegame_write( file, &head, world, player, cam, fight, spawn_carry,
                         rdr, mouse_x, mouse_y );
    qglFileClose( file );
    if ( !ok ) { remove( temp ); strcpy( error, "Could not write save file" ); return 0; }
    remove( old );
    rename( name, old );
    if ( rename( temp, name ) ) {
        rename( old, name );
        remove( temp );
        strcpy( error, "Could not replace save file" );
        return 0;
    }
    remove( old );
    return -1;
}

static short near savegame_counts_match( const SaveHead *head, const World *world )
{
    return (short) ( head->model_count == world->model_count &&
                     head->tele_count == world->tele_count &&
                     head->plat_count == world->plat_count &&
                     head->door_count == world->door_count &&
                     head->trig_count == world->trig_count &&
                     head->light_count == world->light_count &&
                     head->item_count >= 0 && head->item_count <= world->item_max &&
                     head->mon_count == world->mon_count );
}

static short near savegame_read_items( short file, ItemEnt far *item,
                                       short count, short version )
{
    short i;
    if ( version >= SG_VERSION_ITEMS )
        return savegame_read_block( file, (long) item,
                                    (long) count * sizeof(ItemEnt) );
    for ( i = 0; i < count; i++ ) {
        item[i].rot_at = 0.0f;
        if ( !savegame_read_block( file, (long) &item[i],
                                   (long) offsetof(ItemEnt, rot_at) ) ) return 0;
    }
    return -1;
}

static short near savegame_read_fight( short file, Fight *fight, short version )
{
    (void) version;
    fight->lg_hit_at = 0.0f;
    fight->punch_pitch = 0.0f;
    fight->lg_sustained = 0;
    return savegame_read_block( file, (long) (void far *) fight,
                                offsetof(Fight, lg_hit_at) );
}

static unsigned char near savegame_anim_set( const MdlEnt far *ent,
                                              const MdlState *model )
{
    if ( ent->cruc && mdl_set_count( model, MDL_SET_CRUC ) ) return MDL_SET_CRUC;
    switch ( ent->state ) {
    case MDL_ST_RUN:    return MDL_SET_RUN;
    case MDL_ST_DEAD:   return MDL_SET_DEATH;
    case MDL_ST_PAIN:   return MDL_SET_PAIN;
    case MDL_ST_ATTACK: return MDL_SET_ATK;
    case MDL_ST_LEAP:   return mdl_set_count( model, MDL_SET_LEAP ) ? MDL_SET_LEAP : MDL_SET_RUN;
    }
    return MDL_SET_STAND;
}

static void near savegame_old_anim( MdlEnt far *ent, short count )
{
    if ( ent->kind == MDL_KIND_SHAMBLER && ent->state == MDL_ST_ATTACK ) {
        if ( ent->anim_frame >= 8 ) ent->anim_frame -= 2;
        else if ( ent->anim_frame >= 6 ) ent->anim_frame = 6;
    }
    if ( ent->anim_frame >= count ) {
        if ( ent->state == MDL_ST_STAND || ent->state == MDL_ST_RUN )
            ent->anim_frame = (short) ( ent->anim_frame % count );
        else
            ent->anim_frame = (short) ( count - 1 );
    }
}

static short near savegame_monsters( World *world, short version )
{
    short i;

    for ( i = 0; i < world->mon_count; i++ ) {
        MdlEnt far *ent = &world->mon[i];
        MdlState *model;
        if ( ent->kind < 0 || ent->kind >= MDL_KINDS ) return 0;
        model = &world->mdl[ent->kind];
        if ( version < SG_VERSION_ANIM ) {
            ent->anim_set = savegame_anim_set( ent, model );
            if ( ent->kind == MDL_KIND_ZOMBIE && ent->state == MDL_ST_DEAD ) {
                ent->state = MDL_ST_GIBBED;
                ent->anim_set = MDL_SET_STAND;
                ent->anim_frame = 0;
                continue;
            }
            if ( mdl_set_count( model, ent->anim_set ) )
                savegame_old_anim( ent, mdl_set_count( model, ent->anim_set ) );
        }
        if ( version < SG_VERSION_ZWAIT ) savegame_zombie_wait_v7( ent );
        if ( ent->state == MDL_ST_DEMO || ent->state == MDL_ST_GIBBED ) continue;
        if ( ent->anim_set >= MDL_NSETS || !mdl_set_count( model, ent->anim_set ) ||
             ent->anim_frame < 0 || ent->anim_frame >= mdl_set_count( model, ent->anim_set ) ) return 0;
    }
    return -1;
}

static short near savegame_read_triggers( short file, World *world )
{
    TrigEnt saved;
    short i;

    for ( i = 0; i < world->trig_count; i++ ) {
        if ( !savegame_read_block( file, (long) (void far *) &saved, sizeof(saved) ) ) return 0;
        /* Targets and spawnflags come from the map, not mutable state.
           Old saves predate those exit fields and stored zeroes. */
        savegame_exit_static( &saved, &world->trig[i] );
        _fmemcpy( &world->trig[i], &saved, sizeof(saved) );
    }
    return -1;
}

static float near savegame_warn_v1( float until, float warned, float now )
{
    if ( until <= now ) return 0.0f;
    return warned == until ? now + 1.0f : -until;
}

short savegame_load( short slot, const char *map, World *world, Player *player,
                     Camera *cam, Fight *fight, PlayerCarry *spawn_carry,
                     Renderer *rdr,
                     short *mouse_x, short *mouse_y, char *error )
{
    SaveHead head;
    SaveWorld sw;
    SaveRender sr;
    SaveGibs sg;
    SaveSprites ss;
    SaveSprites2 ss2;
    SaveSprites3 ss3;
    SaveSpawn sp;
    char name[16];
    short file;
    short i, ok, gib_count = 0, sprite_live = 0, bubble_live = 0, spawn_left = 0;
    short emitters = 0;
    long got;
    GibEnt far *gib = 0;
    SpriteExplosion far *sprite = 0;
    SpriteBubble far *bubble = 0;
    SpriteBubbleSpawner far *spawner = 0;
    SpriteBubbleEmitter far *emitter = 0;
    short far *train_damage = 0;

    savegame_name( slot, "sav", name );
    file = qglFileOpen( name );
    if ( !file || !savegame_read_block( file, (long) (void far *) &head, sizeof(head) ) ) {
        if ( file ) qglFileClose( file );
        strcpy( error, "Could not read save file" );
        return 0;
    }
    head.map[8] = 0;
    if ( head.magic != SG_MAGIC ||
         ( head.version < SG_VERSION_OLD || head.version > SG_VERSION ) ||
         stricmp( head.map, map ) ||
         !savegame_counts_match( &head, world ) ) {
        qglFileClose( file );
        strcpy( error, "Save file does not match this map" );
        return 0;
    }
    if ( head.version < SG_VERSION_TRAIN_DMG ) {
        train_damage = (short far *) qglMemAlloc(
            (long) ( world->plat_count ? world->plat_count : 1 ) * sizeof(short) );
        if ( !train_damage ) {
            qglFileClose( file );
            strcpy( error, "Could not preserve train damage" );
            return 0;
        }
        for ( i = 0; i < world->plat_count; i++ )
            train_damage[i] = world->plat[i].kind == ENT_PLAT_KIND_TRAIN
                            ? world->plat[i].mode.dmg : 0;
    }
    ok = (short) ( savegame_read_block( file, (long) (void far *) player, sizeof(*player) ) &&
                   savegame_read_block( file, (long) (void far *) cam, sizeof(*cam) ) &&
                   savegame_read_fight( file, fight, head.version ) &&
                   savegame_read_block( file, (long) (void far *) &sw, sizeof(sw) ) &&
                   savegame_read_block( file, (long) (void far *) &sr, sizeof(sr) ) &&
                   savegame_read_block( file, (long) world->brush,
                                      (long) world->model_count * sizeof(BrushModel) ) &&
                   savegame_read_block( file, (long) world->tele,
                                      (long) world->tele_count * sizeof(Teleporter) ) &&
                   savegame_read_block( file, (long) world->plat,
                                      (long) world->plat_count * sizeof(PlatEnt) ) &&
                   savegame_read_doors( file, world ) &&
                   savegame_read_triggers( file, world ) &&
                   savegame_read_block( file, (long) world->light,
                                      (long) world->light_count * sizeof(LightEnt) ) );
    if ( train_damage ) {
        if ( ok ) savegame_train_damage_v8( world, train_damage );
        qglMemFree( (long) train_damage );
    }
    if ( ok ) ok = savegame_read_items( file, world->item,
                                        head.item_count, head.version );
    if ( ok ) ok = savegame_read_block( file, (long) world->mon,
                                        (long) world->mon_count * sizeof(MdlEnt) );
    if ( ok ) ok = savegame_monsters( world, head.version );
    if ( ok ) {
        got = qglFileRead( file, (long) (void far *) &sg, sizeof(sg) );
        if ( got ) {
            if ( got != sizeof(sg) || sg.magic != SG_GIB_MAGIC ||
                 sg.count < 0 || sg.count > GIB_MAX ) ok = 0;
            else if ( sg.count ) {
                gib = gib_map( rdr, 2 );
                ok = (short) ( gib && savegame_read_block( file, (long) gib,
                                             (long) sg.count * sizeof(GibEnt) ) );
                if ( ok ) gib_count = sg.count;
            }
        }
    }
    if ( ok && head.version == 3 ) {
        if ( !savegame_read_block( file, (long) (void far *) &ss, sizeof(ss) ) ||
             ss.magic != SG_SPR_MAGIC || ss.count != SPR_EXPLOSIONS ) ok = 0;
        else {
            sprite = d_sprite_map( rdr, 2 );
            ok = (short) ( sprite && savegame_read_block( file, (long) sprite,
                         (long) SPR_EXPLOSIONS * sizeof(SpriteExplosion) ) );
            if ( ok ) for ( i = 0; i < SPR_EXPLOSIONS; i++ )
                if ( sprite[i].live ) sprite_live++;
        }
    }
    if ( ok && head.version == 4 ) {
        if ( !savegame_read_block( file, (long) (void far *) &ss2, sizeof(ss2) ) ||
             ss2.magic != SG_SPR2_MAGIC || ss2.explosions != SPR_EXPLOSIONS ||
             ss2.bubbles != SG_V4_BUBBLES || ss2.spawners != SPR_SPAWNERS ) ok = 0;
        else {
            sprite = d_sprite_map( rdr, 2 );
            ok = (short) ( sprite && savegame_read_block( file, (long) sprite,
                         (long) SPR_EXPLOSIONS * sizeof(SpriteExplosion) ) );
            if ( ok ) for ( i = 0; i < SPR_EXPLOSIONS; i++ )
                if ( sprite[i].live ) sprite_live++;
            bubble = d_bubble_map( rdr, 2 );
            if ( ok ) ok = (short) ( bubble && savegame_read_block( file, (long) bubble,
                         (long) SG_V4_BUBBLES * sizeof(SpriteBubble) ) );
            if ( ok ) for ( i = 0; i < SG_V4_BUBBLES; i++ )
                if ( bubble[i].live ) bubble_live++;
            spawner = d_spawner_map( rdr, 2 );
            if ( ok ) ok = (short) ( spawner && savegame_read_block( file, (long) spawner,
                         (long) SPR_SPAWNERS * sizeof(SpriteBubbleSpawner) ) );
            if ( ok ) for ( i = 0; i < SPR_SPAWNERS; i++ ) {
                if ( spawner[i].left < 0 || spawner[i].left > SPR_BUBBLES ) ok = 0;
                else spawn_left = (short) ( spawn_left + spawner[i].left );
            }
        }
    }
    if ( ok && head.version >= SG_VERSION_SPR3 ) {
        if ( !savegame_read_block( file, (long) (void far *) &ss3, sizeof(ss3) ) ||
             ss3.magic != SG_SPR3_MAGIC || ss3.explosions != SPR_EXPLOSIONS ||
             ss3.bubbles != SPR_BUBBLES || ss3.spawners != SPR_SPAWNERS ||
             ss3.emitters != SPR_EMITTERS ) ok = 0;
        else {
            sprite = d_sprite_map( rdr, 2 );
            ok = (short) ( sprite && savegame_read_block( file, (long) sprite,
                         (long) SPR_EXPLOSIONS * sizeof(SpriteExplosion) ) );
            if ( ok ) for ( i = 0; i < SPR_EXPLOSIONS; i++ )
                if ( sprite[i].live ) sprite_live++;
            bubble = d_bubble_map( rdr, 2 );
            if ( ok ) ok = (short) ( bubble && savegame_read_block( file, (long) bubble,
                         (long) SPR_BUBBLES * sizeof(SpriteBubble) ) );
            if ( ok ) for ( i = 0; i < SPR_BUBBLES; i++ )
                if ( bubble[i].live ) bubble_live++;
            spawner = d_spawner_map( rdr, 2 );
            if ( ok ) ok = (short) ( spawner && savegame_read_block( file, (long) spawner,
                         (long) SPR_SPAWNERS * sizeof(SpriteBubbleSpawner) ) );
            if ( ok ) for ( i = 0; i < SPR_SPAWNERS; i++ ) {
                if ( spawner[i].left < 0 || spawner[i].left > SPR_BUBBLES ) ok = 0;
                else spawn_left = (short) ( spawn_left + spawner[i].left );
            }
            emitter = d_emitter_map( rdr, 2 );
            if ( ok ) ok = (short) ( emitter && savegame_read_block( file, (long) emitter,
                         (long) SPR_EMITTERS * sizeof(SpriteBubbleEmitter) ) );
            if ( ok ) for ( i = 0; i < SPR_EMITTERS; i++ )
                if ( emitter[i].live ) emitters++;
        }
    }
    if ( ok && head.version >= SG_VERSION_SPAWN ) {
        if ( !savegame_read_block( file, (long) (void far *) &sp, sizeof(sp) ) ||
             sp.magic != SG_SPAWN_MAGIC ) ok = 0;
        else *spawn_carry = sp.carry;
    }
    qglFileClose( file );
    if ( !ok ) { strcpy( error, "Save file is truncated" ); return 0; }

    if ( head.version < SG_VERSION_SPAWN ) {
        memset( spawn_carry, 0, sizeof(*spawn_carry) );
        spawn_carry->items = PL_IT_SHOTGUN | PL_IT_AXE;
        spawn_carry->health = PL_HEALTH;
        spawn_carry->shells = PL_SHELLS;
        spawn_carry->weapon = PL_IT_SHOTGUN;
        spawn_carry->skill = fight->skill;
        spawn_carry->serverflags = fight->serverflags;
    }

    world->rng = sw.rng;
    world->mon_teles = sw.mon_teles;
    world->mon_jumps = sw.mon_jumps;
    world->light_clock = sw.light_clock;
    memcpy( world->fog_org, sw.fog_org, sizeof(sw.fog_org) );
    memcpy( world->fog_at, sw.fog_at, sizeof(sw.fog_at) );
    world->item_count = head.item_count;
    mdl_rebind_items( world, fight->worldtype );
    rdr->anim_time = sr.anim_time;
    if ( head.version < SG_VERSION_SPR3 ) {
        d_sprite_air_reset( rdr, sr.anim_time );
        emitters = rdr->bubble_emitters;
    }
    if ( head.version == SG_VERSION_OLD ) {
        /* v1 had one player-wide mega timer. Preserve that behavior for
           the old snapshot by handing it to one collected mega. */
        if ( fight->lightning_at > 0.0f ) for ( i = 0; i < world->item_count; i++ ) {
            ItemEnt far *it = &world->item[i];
            if ( it->gone && it->kind == ENT_ITEM_HEALTH && it->amount == ENT_ITEM_MEGA ) {
                it->rot_at = fight->lightning_at;
                break;
            }
        }
        fight->quad_warn = savegame_warn_v1( fight->quad_until, fight->quad_warn, sr.anim_time );
        fight->suit_warn = savegame_warn_v1( fight->suit_until, fight->suit_warn, sr.anim_time );
        fight->pent_warn = savegame_warn_v1( fight->pent_until, fight->pent_warn, sr.anim_time );
        fight->invis_warn = savegame_warn_v1( fight->invis_until, fight->invis_warn, sr.anim_time );
    }
    if ( head.version < SG_VERSION_LIGHTNING ) {
        fight->lightning_at = -1.0f;
        fight->lg_hit_at = 0.0f;
        fight->punch_pitch = 0.0f;
        fight->lg_sustained = 0;
    }
    dl_import( rdr, sr.dlights );
    rdr->dl_tick = sr.dl_tick;
    rdr->dl_seed = sr.dl_seed;
    rdr->gib_count = gib_count;
    rdr->sprite_live = sprite_live;
    rdr->sprite_drawn = 0;
    rdr->bubble_live = bubble_live;
    rdr->bubble_drawn = 0;
    rdr->bubble_spawn_left = head.version >= 4 ? spawn_left : 0;
    rdr->bubble_emitters = emitters;
    d_beam_clear( rdr );
    rdr->pvs_leaf = 0;
    if ( rdr->item_lc )
        for ( i = 0; i < world->item_count; i++ ) rdr->item_lc[i].ok = 0;
    if ( rdr->mdl_lc )
        for ( i = 0; i < world->mon_count; i++ ) rdr->mdl_lc[i].ok = 0;
    fight->msg = 0;
    fight->msg_time = 0.0f;
    *mouse_x = sr.mouse_x;
    *mouse_y = sr.mouse_y;
    return -1;
}
