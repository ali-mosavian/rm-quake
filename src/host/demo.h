#ifndef __DEMO_H__
#define __DEMO_H__

/*
 * demo.h -- deterministic input recordings and id-demo client snapshots.
 *
 * A record is the frame's mouse and key state and its dt, then where the
 * camera ended up. -play feeds the input half back through the same code
 * path and checks the camera against the recorded half, so a replay that
 * has stopped being the same run says so once. Without the dt a replayed
 * frame runs however many ticks the replaying machine's clock gives it,
 * and the camera drifts within 15 frames.
 *
 * Recording writes to a far buffer and touches no file until a clean
 * exit: file I/O during the run was measured to change whether the live
 * input crash reproduces. A crash leaves the buffer where the log's
 * rec_buf line says, for the debugger to read.
 */

#include <stdio.h>
#include "input.h"
#include "renderer.h"
#include "world.h"
#include "fight.h"
#include "ls.h"
#include "sys.h"
#include "keybind.h"

/* far, not huge: REC_MAX * 38 + 2 has to stay under 64K */
#define REC_MAX 1600

typedef struct { short in[9]; float dt, px, py, pz, yaw; } RecEntry;
typedef struct { short count; RecEntry e[REC_MAX]; } RecBuf;
typedef struct { float time, x, y, z, pitch, yaw; } DemoCam;
typedef struct {
    float time, x, y, z, pitch, yaw;
    long items;
    short health, armor, weapon, weapon_frame;
    short shells, nails, rockets, cells;
    short armor_kind, power, weapon_model, ent_count;
} DemoFrame;
typedef struct {
    short number, type, kind, state, frame;
    float x, y, z, yaw;
} DemoEntity;

#define DEMO_ENT_BRUSH   1
#define DEMO_ENT_MONSTER 2
#define DEMO_ENT_ITEM    3
#define DEMO_ENT_LIGHT   4
#define DEMO_ENT_SOUND   5
#define DEMO_ENT_DAMAGE  6
#define DEMO_ENT_MUZZLE  7
#define DEMO_ENT_STOPSND 8
#define DEMO_ENT_PROJECTILE 9
#define REC_DEMOFRAME 52
#define REC_DEMOENTITY 26
typedef char rec_demoframe_ok[ sizeof(DemoFrame) == REC_DEMOFRAME ? 1 : -1 ];
typedef char rec_demoentity_ok[ sizeof(DemoEntity) == REC_DEMOENTITY ? 1 : -1 ];

typedef struct {
    RecBuf far *buf;          /* -record: the frames so far */
    FILE       *rf;           /* -play: the recording */
    RecEntry    rec;          /* this frame's */
    short       drift;        /* -play: divergence reported, stop checking */
    short       actions;      /* file carries logical actions, not raw keys */
    short       camera;       /* QDS1: timestamped id demo state, no input */
    short       snapshot;     /* QDS1: camera plus recorded render/client state */
    short       snapshot_applied;
    short       camera_eof;
    float       camera_time;
    DemoFrame   camera_from;
    DemoCam     camera_to;
    long        snapshot_next_pos;
    short       snapshot_item_base;
    short       snapshot_monsters, snapshot_brushes, snapshot_items;
    char        map[9];       /* QCR3/QDS1 map */
} Demo;

void  demo_open( Demo *dm, const RunArgs *args );
short demo_map( const char *name, char *map );
short demo_input( Demo *dm, Input *input, float *frame_dt );
void  demo_actions( Demo *dm, const KeyBindings KB_FAR *keys );
short demo_camera( Demo *dm, Camera *cam, Player *player, World *world,
                   Fight *fight, Renderer *rdr, LightStyles *ls, float frame_dt );
void  demo_check( Demo *dm, const Camera *cam, short frame );
void  demo_capture( Demo *dm, const Camera *cam );
void  demo_close( Demo *dm, const RunArgs *args );

#endif
