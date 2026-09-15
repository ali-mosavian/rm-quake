#ifndef __HOST_H__
#define __HOST_H__

/*
 * host.h -- one run, Quake's host.c. host_init loads the map, host_frame
 * runs the ticks a frame owes and draws it, host_shutdown reports and
 * gives the machine back.
 */

#include "renderer.h"
#include "fight.h"
#include "world.h"
#include "input.h"
#include "hud.h"
#include "ls.h"
#include "sc.h"
#include "sys_time.h"
#include "video.h"
#include "config.h"
#include "sys.h"
#include "bench.h"
#include "demo.h"
#include "dbg.h"

/* host_advance's accumulator and tick counter: variable real frame time
   turned into a constant-rate simulation. */
typedef struct {
    float accum;
    long  ticks;
    long  bench_ticks;    /* -ticks N; 0 = unbounded */
    short nwarp;          /* -warp's */
    long  warp_tick[4];
    float warp_xyz[4][3];
} HostClock;

/* Everything one run owns, in main's frame for the whole run. Hud and
   the surface cache are far-allocated: too big for DGROUP. */
typedef struct {
    RunArgs        args;
    Config         cfg;
    Video          v;
    DbgProbe       probe;
    World          world;
    Renderer       rdr;
    Camera         cam;
    Player         player;
    Fight          fight;
    PlayerCarry    carry;         /* CARRY.BIN, read before the map loads */
    short          carried;       /* -carry and a whole file */
    Input          input;
    LightStyles    ls;
    SysClock       sysclk;
    HostClock      clock;
    PhaseTimes     pt;
    BenchStats     bs;
    Demo           demo;
    Hud far       *hud;
    SurfCache far *sc;
    Mat4           mtx_prj;
    Vec3           cam_up;
    QSurf          z_dc, h_dst_dc;
} Host;

void  host_init( Host *h );
short host_running( const Host *h );
void  host_frame( Host *h );
void  host_shutdown( Host *h );

#endif
