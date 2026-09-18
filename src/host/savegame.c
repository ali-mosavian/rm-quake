/* savegame.c -- single-player snapshots behind id's twelve save slots. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "savegame.h"
#include "qgl.h"
#include "dl.h"
#include "gib.h"
#include "item.h"

#define SG_MAGIC   0x31565351L
#define SG_VERSION 2
#define SG_VERSION_OLD 1
#define SG_GIB_MAGIC 0x31424947L

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

typedef struct {
    Rng rng;
    short mon_teles, mon_jumps, light_clock;
    BspVec3 fog_org[ENT_FOGS];
    float fog_at[ENT_FOGS];
} SaveWorld;

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
         ( head->version != SG_VERSION && head->version != SG_VERSION_OLD ) ) return 0;
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
                                  Renderer *rdr, short mouse_x, short mouse_y )
{
    SaveWorld sw;
    SaveRender sr;
    SaveGibs sg;
    GibEnt far *gib;

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

    if ( !savegame_write_block( file, (long) (void far *) head, sizeof(*head) ) ||
         !savegame_write_block( file, (long) (void far *) player, sizeof(*player) ) ||
         !savegame_write_block( file, (long) (void far *) cam, sizeof(*cam) ) ||
         !savegame_write_block( file, (long) (void far *) fight, sizeof(*fight) ) ||
         !savegame_write_block( file, (long) (void far *) &sw, sizeof(sw) ) ||
         !savegame_write_block( file, (long) (void far *) &sr, sizeof(sr) ) ) return 0;
    if ( !( savegame_write_block( file, (long) world->brush,
                                  (long) world->model_count * sizeof(BrushModel) ) &&
           savegame_write_block( file, (long) world->tele,
                                 (long) world->tele_count * sizeof(Teleporter) ) &&
           savegame_write_block( file, (long) world->plat,
                                 (long) world->plat_count * sizeof(PlatEnt) ) &&
           savegame_write_block( file, (long) world->door,
                                 (long) world->door_count * sizeof(DoorEnt) ) &&
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
    if ( !sg.count ) return -1;
    gib = gib_map( rdr, 2 );
    return gib && savegame_write_block( file, (long) gib,
                                        (long) sg.count * sizeof(GibEnt) );
}

short savegame_save( short slot, const char *map, World *world, Player *player,
                     Camera *cam, Fight *fight, Renderer *rdr,
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
    ok = savegame_write( file, &head, world, player, cam, fight, rdr, mouse_x, mouse_y );
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
    if ( version == SG_VERSION )
        return savegame_read_block( file, (long) item,
                                    (long) count * sizeof(ItemEnt) );
    for ( i = 0; i < count; i++ ) {
        item[i].rot_at = 0.0f;
        if ( !savegame_read_block( file, (long) &item[i],
                                   (long) offsetof(ItemEnt, rot_at) ) ) return 0;
    }
    return -1;
}

static float near savegame_warn_v1( float until, float warned, float now )
{
    if ( until <= now ) return 0.0f;
    return warned == until ? now + 1.0f : -until;
}

short savegame_load( short slot, const char *map, World *world, Player *player,
                     Camera *cam, Fight *fight, Renderer *rdr,
                     short *mouse_x, short *mouse_y, char *error )
{
    SaveHead head;
    SaveWorld sw;
    SaveRender sr;
    SaveGibs sg;
    char name[16];
    short file;
    short i, ok, gib_count = 0;
    long got;
    GibEnt far *gib = 0;

    savegame_name( slot, "sav", name );
    file = qglFileOpen( name );
    if ( !file || !savegame_read_block( file, (long) (void far *) &head, sizeof(head) ) ) {
        if ( file ) qglFileClose( file );
        strcpy( error, "Could not read save file" );
        return 0;
    }
    head.map[8] = 0;
    if ( head.magic != SG_MAGIC ||
         ( head.version != SG_VERSION && head.version != SG_VERSION_OLD ) ||
         stricmp( head.map, map ) ||
         !savegame_counts_match( &head, world ) ) {
        qglFileClose( file );
        strcpy( error, "Save file does not match this map" );
        return 0;
    }
    ok = (short) ( savegame_read_block( file, (long) (void far *) player, sizeof(*player) ) &&
                   savegame_read_block( file, (long) (void far *) cam, sizeof(*cam) ) &&
                   savegame_read_block( file, (long) (void far *) fight, sizeof(*fight) ) &&
                   savegame_read_block( file, (long) (void far *) &sw, sizeof(sw) ) &&
                   savegame_read_block( file, (long) (void far *) &sr, sizeof(sr) ) &&
                   savegame_read_block( file, (long) world->brush,
                                      (long) world->model_count * sizeof(BrushModel) ) &&
                   savegame_read_block( file, (long) world->tele,
                                      (long) world->tele_count * sizeof(Teleporter) ) &&
                   savegame_read_block( file, (long) world->plat,
                                      (long) world->plat_count * sizeof(PlatEnt) ) &&
                   savegame_read_block( file, (long) world->door,
                                      (long) world->door_count * sizeof(DoorEnt) ) &&
                   savegame_read_block( file, (long) world->trig,
                                      (long) world->trig_count * sizeof(TrigEnt) ) &&
                   savegame_read_block( file, (long) world->light,
                                      (long) world->light_count * sizeof(LightEnt) ) );
    if ( ok ) ok = savegame_read_items( file, world->item,
                                        head.item_count, head.version );
    if ( ok ) ok = savegame_read_block( file, (long) world->mon,
                                        (long) world->mon_count * sizeof(MdlEnt) );
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
    qglFileClose( file );
    if ( !ok ) { strcpy( error, "Save file is truncated" ); return 0; }

    world->rng = sw.rng;
    world->mon_teles = sw.mon_teles;
    world->mon_jumps = sw.mon_jumps;
    world->light_clock = sw.light_clock;
    memcpy( world->fog_org, sw.fog_org, sizeof(sw.fog_org) );
    memcpy( world->fog_at, sw.fog_at, sizeof(sw.fog_at) );
    world->item_count = head.item_count;
    rdr->anim_time = sr.anim_time;
    if ( head.version == SG_VERSION_OLD ) {
        /* v1 had one player-wide mega timer. Preserve that behavior for
           the old snapshot by handing it to one collected mega. */
        if ( fight->rot_at > 0.0f ) for ( i = 0; i < world->item_count; i++ ) {
            ItemEnt far *it = &world->item[i];
            if ( it->gone && it->kind == ENT_ITEM_HEALTH && it->amount == ENT_ITEM_MEGA ) {
                it->rot_at = fight->rot_at;
                break;
            }
        }
        fight->quad_warn = savegame_warn_v1( fight->quad_until, fight->quad_warn, sr.anim_time );
        fight->suit_warn = savegame_warn_v1( fight->suit_until, fight->suit_warn, sr.anim_time );
        fight->pent_warn = savegame_warn_v1( fight->pent_until, fight->pent_warn, sr.anim_time );
        fight->invis_warn = savegame_warn_v1( fight->invis_until, fight->invis_warn, sr.anim_time );
    }
    dl_import( rdr, sr.dlights );
    rdr->dl_tick = sr.dl_tick;
    rdr->dl_seed = sr.dl_seed;
    rdr->gib_count = gib_count;
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
