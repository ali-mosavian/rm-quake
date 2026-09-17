#ifndef __DEMO_H__
#define __DEMO_H__

/*
 * demo.h -- deterministic input recordings and attract-camera tracks.
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
#include "sys.h"
#include "keybind.h"

/* far, not huge: REC_MAX * 38 + 2 has to stay under 64K */
#define REC_MAX 1600

typedef struct { short in[9]; float dt, px, py, pz, yaw; } RecEntry;
typedef struct { short count; RecEntry e[REC_MAX]; } RecBuf;
typedef struct { float time, x, y, z, pitch, yaw; } DemoCam;

typedef struct {
    RecBuf far *buf;          /* -record: the frames so far */
    FILE       *rf;           /* -play: the recording */
    RecEntry    rec;          /* this frame's */
    short       drift;        /* -play: divergence reported, stop checking */
    short       actions;      /* file carries logical actions, not raw keys */
    short       camera;       /* QDC1: timestamped id demo camera, no input */
    short       camera_eof;
    float       camera_time;
    DemoCam     camera_from, camera_to;
    char        map[9];       /* QCR3/QDC1 map */
} Demo;

void  demo_open( Demo *dm, const RunArgs *args );
short demo_map( const char *name, char *map );
short demo_input( Demo *dm, Input *input, float *frame_dt );
void  demo_actions( Demo *dm, const KeyBindings KB_FAR *keys );
short demo_camera( Demo *dm, Camera *cam, Player *player, float frame_dt );
void  demo_check( Demo *dm, const Camera *cam, short frame );
void  demo_capture( Demo *dm, const Camera *cam );
void  demo_close( Demo *dm, const RunArgs *args );

#endif
