#ifndef __HOST_H__
#define __HOST_H__

#include "qrcfg.h"

/*
 * host.h -- one session, Quake's host.c. Maps are loaded and released
 * inside it; video, input and the allocator live until host_shutdown.
 */

#include "renderer.h"
#include "fight.h"
#include "client.h"
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
#include "cmd.h"
#include "console.h"
#include "keybind.h"
#include "menu.h"

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

#define ATTRACT_MAX 8
typedef struct {
    char demo[ATTRACT_MAX][13];
    char pending[13];
    short count, next, active, waiting;
} Attract;

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
    PlayerDeath    death;
    PlayerCarry    carry;         /* next-map or current level-entry kit */
    short          carried;       /* -carry and a whole file */
    Input          input;
    LightStyles    ls;
    SysClock       sysclk;
    HostClock      clock;
    PhaseTimes     pt;
    BenchStats     bs;
    Demo           demo;
    Attract        attract;
    CmdState far  *cmd;
    Console far   *console;
    KeyBindings far *keys;
    Menu far       *menu;
    short          pending_load;
    short          quit;
    long           console_frames;
    float          fov_applied;
    float          skill_cvar_seen;
    float          viewsize_applied;
    float          forward_speed;
    short          invert_mouse;
    short          crosshair_cvar, crosshair_x_cvar, crosshair_y_cvar;
    Hud far       *hud;
    SurfCache far *sc;
    Mat4           mtx_prj;
    Vec3           cam_up;
    QSurf          z_dc, h_dst_dc;
    long           map_mem_total, map_mem_largest;
    short          map_ems_pages;
#if QR_DEBUG_CAPTURE
    long           debug_capture_last;
    unsigned short debug_capture_seq;
#endif
} Host;

void  host_init( Host *h );
short host_running( const Host *h );
void  host_frame( Host *h );
short host_changelevel( Host *h );
void  host_shutdown( Host *h );

#endif
