#ifndef __ENT_H__
#define __ENT_H__

#include "renderer.h"
#include "world.h"
#include "fight.h"
#include "enttext.h"

short ent_find_node( short m, World *world );
short ent_point_leaf( BspVec3 *p, World *world );
void  ent_check_teleport( Player *player, World *world, Renderer *rdr,
                          float now, short scr_x_res );
void  ent_fog_tick( World *world, Player *player, float now );
void  ent_monster_teleport( World *world, Renderer *rdr, float now, MdlEnt far *e );
void  ent_place_models( World *world );
char far *ent_msg( World *world, short id );

short ent_box_touched( Player *player, BspVec3 far *mins, BspVec3 far *maxs, float slack );

/* sound() on a mover: submodel m's own voice channel, from the middle
   of its brush where it is now -- SV_StartSound's origin for a brush */
void  ent_brush_sound( Player *player, World *world, short m, short id );

/*
 * name: ent_reset
 * desc: Every door shut and every trigger, button and train as the map
 *       loaded. pl_game_reset's half of the world.
 */
void  ent_reset( World *world, Fight *fight );

/* q_ent.bi's EntsHead/EntsTele/EntsPlat -- ents.bin's own layout,
   mkassets.py's already-resolved entity data (targetname pairing,
   trigger/plat validation, all done offline; nothing here parses the
   map's entity TEXT, which BASIC's own 32,767-byte string cap made
   impossible for some maps in the first place). */
/* q_ent.bi's EntsHead -- the whole of what mkassets.py now
   resolves out of the entities text. cport reads the spawn, the
   teleporters, the plats and the hidden submodels; the rest are counts
   it skips over, and they are here because the records they size sit
   between the ones it does read. */
typedef struct {
    BspVec3 spawn;
    float   angle;
    BspVec3 inter;          /* info_intermission: where the exit looks from */
    float   inter_pitch;
    float   inter_yaw;
    short   nmodels;        /* stamp: must equal the map's model count, or
                                these assets are from another map */
    short   ntele;
    short   nplat;
    short   nhide;
    short   nitem;
    short   ndoor;
    short   ntrig;
    short   nmon;
    short   namb;
    short   ntrain;
    short   ncorner;
    short   worldtype;
    short   ncrate;
    float   gravity;
    short   msg_size;
    short   title;            /* byte offset in the message pool */
    short   nlight;
    BspVec3 spawn2;         /* info_player_start2, or the start where a map has none */
    float   angle2;
    short   ngate;
} EntsHead;

/* A monster record. Not read -- the file puts the monsters FIRST, ahead
   of the teleporters, so its size is what the reader has to skip. */
typedef struct {
    short   kind;
    short   order;
    BspVec3 org;
    float   angle;
    short   first;
    short   cruc;           /* SPAWN_CRUCIFIED */
} EntsMon;

typedef struct {
    short model;
    BspVec3  dest;
    float yaw;
    short name, player_only;
} EntsTele;

typedef struct {
    short model;
    float speed;
    float travel;
    short snd;          /* 1 plat1/plat2, 2 medplat */
    short low_trigger;  /* PLAT_LOW_TRIGGER */
} EntsPlat;

/* The records between the ones cport reads. ents.bin's order is
   monsters, teleporters, plats, hides, gates, items, doors, triggers,
   ambients, trains, corners, crates, lights, messages. */
typedef struct {
    short   kind, amount, target, crate, order;
    BspVec3 org;
} EntsItem;

typedef struct {
    short   model;
    BspVec3 travel;
    BspVec3 mid;
    float   speed, hold;
    short   start_open, action, targeted, secret, shoot, snd, key, msg, order;
} EntsDoor;

typedef struct {
    short   model, kind, target, name, kill, count;
    float   wait, speed;
    BspVec3 travel;
    BspVec3 org;
    short   snd, msg;
    float   delay;
    short   order;
} EntsTrig;

typedef struct {
    short   snd, vol;
    BspVec3 org;
} EntsAmb;

typedef struct {
    short model;
    float speed;
    short dmg, targeted, first;
    short snd;          /* 1 train1/train2, 0 misc/null */
} EntsTrain;

typedef struct {
    short name, style, on;
} EntsLight;

/* misc.qc: func_episodegate (boss 0) stands while a rune in bits is held,
   func_bossgate (boss 1) until all of them are */
typedef struct {
    short model, boss, bits;
} EntsGate;

/* q_ent.bi's PathCorner is the file record too. */

/* ents.bin's own records, checked the same way bsptypes.h checks the
   map's -- see the note there. */
#define REC_ENTSHEAD 90
#define REC_ENTSMON  24
#define REC_ENTSTELE 22
#define REC_ENTSPLAT 14
#define REC_ENTSITEM 22
#define REC_CRATE    92
#define REC_ENTSDOOR 52
#define REC_ENTSTRIG 54
#define REC_ENTSAMB  16
#define REC_ENTSTRAIN 14
#define REC_PATHCORNER 18
#define REC_ENTSLIGHT 6
#define REC_ENTSGATE  6
typedef char rec_entsgate_ok[ sizeof(EntsGate) == REC_ENTSGATE ? 1 : -1 ];
typedef char rec_entslight_ok[ sizeof(EntsLight) == REC_ENTSLIGHT ? 1 : -1 ];

typedef char rec_entshead_ok[ sizeof(EntsHead) == REC_ENTSHEAD ? 1 : -1 ];
typedef char rec_entsmon_ok [ sizeof(EntsMon)  == REC_ENTSMON  ? 1 : -1 ];
typedef char rec_entstele_ok[ sizeof(EntsTele) == REC_ENTSTELE ? 1 : -1 ];
typedef char rec_entsplat_ok[ sizeof(EntsPlat) == REC_ENTSPLAT ? 1 : -1 ];
typedef char rec_entsitem_ok[ sizeof(EntsItem) == REC_ENTSITEM ? 1 : -1 ];
typedef char rec_crate_ok[ sizeof(CrateModel) == REC_CRATE ? 1 : -1 ];
typedef char rec_entsdoor_ok[ sizeof(EntsDoor) == REC_ENTSDOOR ? 1 : -1 ];
typedef char rec_entstrig_ok[ sizeof(EntsTrig) == REC_ENTSTRIG ? 1 : -1 ];
typedef char rec_entsamb_ok [ sizeof(EntsAmb)  == REC_ENTSAMB  ? 1 : -1 ];
typedef char rec_entstrain_ok[ sizeof(EntsTrain) == REC_ENTSTRAIN ? 1 : -1 ];
typedef char rec_pathcorner_ok[ sizeof(PathCorner) == REC_PATHCORNER ? 1 : -1 ];

/*
 * name: ent_load_spawn
 * desc: The spawn point, from ents.bin. BSP is Z-up and the camera is
 *       Y-up, so y and z swap here.
 */
void ent_load_spawn( World *world, Camera *cam, Fight *fight );

/*
 * name: ent_load_world
 * desc: Loads every ents.bin record kind into world, and spawns the
 *       monsters and drops the items onto the floor.
 */
void ent_load_world( World *world );
void ent_load_air_bubbles( World *world, Renderer *rdr );

/* the map's trigger_secret count, for the intermission's tally */
short ent_secret_total( World *world );

#endif
