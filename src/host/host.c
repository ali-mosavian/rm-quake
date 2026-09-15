/*
 * host.c -- see host.h. The frame is host_tick then host_render: one
 * changes the world and draws nothing, the other draws and changes none
 * of it.
 */

#include "qrcfg.h"
#include "dbg.h"
#include <stdio.h>
#include <string.h>
#include <mem.h>    /* _fmemset */
#include "host.h"
#include "mdl_ai.h"
#include "weapons.h"
#include "d_poly.h"
#include "d_faces.h"
#include "ent.h"
#include "doors.h"
#include "triggers.h"
#include "plats.h"
#include "lights.h"
#include "item.h"
#include "d_alias.h"
#include "input.h"
#include "r_bsp.h"
#include "mod_tex.h"
#include "qgl.h"
#include "view.h"
#include "screen.h"
#include "sbar.h"
#include "client.h"
#include "mdl.h"
#include "snd.h"
#include "dl.h"
#include "pal.h"
#include "ls.h"
#include "mod.h"
#include "pl_move.h"
#include "pak.h"
#include "loadscr.h"
#include "r_portal.h"

/* q_scr.bi's HOST_DT#/HOST_MAXSTEPS. */
#define HOST_DT       0.0166666f
#define HOST_MAXSTEPS 5

#define QGL_Z_OFF 0

#if QR_PROF
/* one phase, begun at t0, into its sum and its max -- once pt.n has armed
   profiling past the warm-up frames */
static void near pt_stop( Host *h, float t0, float *sum, float *max )
{
    float dt;

    if ( h->pt.n <= 0 ) return;
    dt = sys_now( &h->sysclk ) - t0;
    *sum += dt;
    if ( max && dt > *max ) *max = dt;
}
#endif

/* ------------------------------------------------------------------ */
/* the tick                                                            */

#if QR_LOG
/* What the simulation has done, every SUM_TICKS, on E9. tools/test-frames.sh
   diffs these lines, so a change that moves a monster, a mover, a trigger,
   an item or the kit fails even where no frame shows it. */
#define SUM_TICKS 60

static void near host_state_sum( Host *h, long tick )
{
    World  *world  = &h->world;
    Player *player = &h->player;
    Fight  *fight  = &h->fight;
    char  buf[168];
    long  mon = 0, brush = 0, trig = 0, item = 0;
    short i;

    for ( i = 0; i < world->mon_count; i++ ) {
        MdlEnt far *m = &world->mon[i];
        mon += (long) m->pos.x + (long) m->pos.y * 3L + (long) m->pos.z * 7L
             + m->health * 11L + m->state * 13L;
    }
    for ( i = 0; i < world->model_count; i++ )
        brush += (long) world->brush[i].ofs.x + (long) world->brush[i].ofs.y * 3L
               + (long) world->brush[i].ofs.z * 7L;
    for ( i = 0; i < world->trig_count; i++ )
        trig += world->trig[i].state * ( i + 1L );
    for ( i = 0; i < world->item_count; i++ )
        item += world->item[i].gone * ( i + 1L );

    sprintf( buf, "sum %ld pl %ld,%ld,%ld mon %ld br %ld tr %ld it %ld kit %d,%d,%d,%d,%d,%ld",
             tick, (long) player->pos.x, (long) player->pos.y, (long) player->pos.z,
             mon, brush, trig, item, fight->health, fight->shells, fight->nails,
             fight->rockets, fight->armor, fight->items );
    mark( buf );

    /* The think's own state, which sum's line cannot see: the random
       stream, every sound asked for (-nosound too), and a monster's frame,
       facing, hunt and attack clock. A kept line so sum's hash stays put. */
    mon = 0;
    for ( i = 0; i < world->mon_count; i++ ) {
        MdlEnt far *m = &world->mon[i];
        mon += m->anim_frame * ( i + 1L ) + (long) m->yaw * 5L + m->hunting * 17L
             + (long) ( m->next_attack * 10.0f ) * 19L;
    }
    sprintf( buf, "think %ld rng %lu snd %lu mon %ld", tick, world->rng.seed, world->snd.hash, mon );
    mark( buf );
}
#endif

/* changelevel: the kit to CARRY.BIN, the next map's run to NEXT.BAT,
   and the host loop ends. The next map is a new process because its
   container is a different file and nothing here unloads one.
   run.sh runs NEXT.BAT from a copy, since a batch deleted while it is
   running is "Batch file missing". */
static void near host_next_level( Fight *fight )
{
    FILE *f;

    pl_carry_save( fight );
    f = fopen( "NEXT.BAT", "w" );
    if ( f ) {
        fprintf( f, "QCPORT.EXE %s.qmp -carry%s >> run.out\n",
                 fight->next_map, fight->next_flags );
        fclose( f );
    }
    fight->state = GS_NEXT;
}

/* One tick of the state machine: dying and respawning, the area cleared,
   and the intermission's held fire -- on to the next map, or this one
   again. */
static void near host_state( Host *h, short fire )
{
    World    *world  = &h->world;
    Player   *player = &h->player;
    Camera   *cam    = &h->cam;
    Fight    *fight  = &h->fight;
    Renderer *rdr    = &h->rdr;
    short scr_x_res  = h->v.scr_x_res;
    short i, ndead, nmon;

    switch ( fight->state ) {
    case GS_PLAY:
        if ( fight->health <= 0 ) {
            fight->state = GS_DEAD;
            /* PlayerDie: GibPlayer's gib or udeath under -40, else
               DeathSound's h2odeath under water or one of five,
               rint(random() * 4 + 1) */
            if ( fight->health < -40 )
                i = (short) ( rng_int( &world->rng ) & 1 ? SND_GIB : SND_UDEATH );
            else if ( player->water_level == 3 )
                i = SND_H2ODEATH;
            else if ( ( ndead = (short) ( (float) rng_int( &world->rng ) / RNG_MAX * 4.0f + 0.5f ) ) == 0 )
                i = SND_DEATH;
            else
                i = (short) ( SND_DEATH2 + ndead - 1 );
            snd_start( &h->world.snd, player, SND_ENT_PLAYER, CHAN_VOICE, i, &player->pos, ATTN_NONE );
            fight->state_until = rdr->anim_time + PL_DEATH_PAUSE;
            break;
        }
        ndead = nmon = 0;
        for ( i = 0; i < world->mon_count; i++ ) {
            if ( world->mon[i].cruc ) continue;
            nmon++;
            if ( world->mon[i].state == MDL_ST_DEAD ) ndead++;
        }
        if ( nmon > 0 && ndead == nmon ) fight->state = GS_WON;
        break;

    case GS_DEAD:
        if ( rdr->anim_time >= fight->state_until ) {
            pl_respawn( player, fight, rdr );
            fight->state = GS_PLAY;
        }
        break;

    case GS_EXIT:
        /* IntermissionThink: the tally stays up for exittime whatever
           the player does, then a held button goes on. A changelevel
           naming no map, as dm3ish's, plays this level again; nothing
           checks that a named one is shipped. */
        if ( fire && rdr->anim_time >= fight->exit_time + PL_INTER_HOLD ) {
            if ( fight->next_map[0] ) {
                host_next_level( fight );
            } else {
                pl_game_reset( world, player, cam, fight, rdr, scr_x_res );
                ent_reset( world, fight );
                fight->state = GS_PLAY;
            }
        }
        break;

    case GS_NEXT:
        /* already leaving: the host loop ends at the top of the next
           frame, and a frame may run more than one tick -- without
           this, a fire press in the second of them would put the run
           back into GS_PLAY with NEXT.BAT already written. */
        return;

    default:
        /* GS_WON: a fresh press, not the one still held from the shot
           that killed the last monster. */
        if ( fire && !fight->fire_prev ) {
            if ( fight->state == GS_WON ) {
                pl_game_reset( world, player, cam, fight, rdr, scr_x_res );
                ent_reset( world, fight );
            }
            fight->state = GS_PLAY;
        }
        break;
    }
    fight->fire_prev = fire;
}

/* One simulation step. dt is a parameter so a caller can hand it a
   different step without the routine knowing. */
static void near host_tick( Host *h, float dt )
{
    World    *world  = &h->world;
    Player   *player = &h->player;
    Camera   *cam    = &h->cam;
    Renderer *rdr    = &h->rdr;
    Input    *input  = &h->input;
    Fight    *fight  = &h->fight;
    short scr_x_res  = h->v.scr_x_res;
    short scr_y_res  = h->v.scr_y_res;
    /* fire is mouse 1 or ctrl; mouse 1 also walks forward, the
       original's binding too. Read once: outside GS_PLAY it is what
       starts the next thing rather than what shoots. */
    short fire = (short) ( input->mouse.left || input->keyboard.k[KEY_CTRL] );

    /* id decays after drawing; this is the same place, before the next */
    dl_decay( rdr, dt );

    in_handle_toggles( input, rdr, cam, player, h->hud );
    v_update_camera( cam, player, world, input, fight, rdr, dt, scr_x_res, scr_y_res );

    ent_check_teleport( player, world, rdr->anim_time, scr_x_res );
    ent_fog_tick( world, player, rdr->anim_time );

    /* Doors before triggers: a button's target is a door, and a door
       fired this tick should start moving on it. */
    ent_move_plats( world, player, dt );
    ent_move_doors( world, player, fight, rdr, dt );
    ent_move_trigs( world, player, cam, fight, rdr, dt, scr_x_res, scr_y_res );
    ent_move_trains( world, player, dt );

    /* after the movers: a plat can carry an item's floor out from under
       the player */
    pl_items_touch( world, player, fight, rdr );

    /* after the player has moved: FindTarget sees where they are now */
    if ( fight->state == GS_PLAY ) mdl_tick( world, player, fight, rdr );

    /* The shot goes after the monsters' think so it hits them where this
       frame drew them. A dead player fires nothing, but what is already
       in the air keeps flying. */
    if ( fight->state == GS_PLAY ) {
        pl_select_weapon( input, fight );
        if ( fire ) pl_fire( world, player, cam, fight, rdr );
    }
    host_view_load( world, fight->weapon );
    pl_traps_tick( world, player, fight, rdr );
    pl_spikes_tick( world, player, fight, rdr, dt );

    host_state( h, fire );

    /* where each mover ended up, so the draw order can place it */
    ent_place_models( world );

    rdr->anim_time += dt;

    /* light styles: fixed 10 Hz off the same clock, not framerate */
    if ( rdr->no_styles ) {
        ls_hold( &h->ls );
    } else {
        ent_lights_sync( world, &h->ls );
        ls_animate( &h->ls, rdr->anim_time );
    }

    dl_relink( rdr, player, fight );
}

/*
 * Spends a frame's worth of real time on whole simulation steps. Every
 * step is HOST_DT and the remainder carries: physics sees a constant
 * rate, and only how many steps a frame runs varies. A long step
 * overshoots a wall a short one stops against.
 */
static void near host_advance( Host *h, float real_dt )
{
    HostClock *clock  = &h->clock;
    Player    *player = &h->player;
    short steps = 0, w;
#if QR_PROF
    float t0 = sys_now( &h->sysclk );
#endif

    clock->accum += real_dt;

    while ( clock->accum >= HOST_DT && steps < HOST_MAXSTEPS ) {
        /* Stop ON the tick budget, not past it: a slow frame running two
           or three steps would otherwise end the run at 902 rather than
           900, with the camera wherever those extra steps carried it. */
        if ( clock->bench_ticks > 0 && clock->ticks >= clock->bench_ticks ) {
            break;
        }
        for ( w = 0; w < clock->nwarp; w++ ) {
            if ( clock->warp_tick[w] != clock->ticks ) continue;
            player->pos.x = clock->warp_xyz[w][0];
            player->pos.y = clock->warp_xyz[w][1];
            player->pos.z = clock->warp_xyz[w][2];
            player->vel.x = player->vel.y = player->vel.z = 0.0f;
            player->fall_vz = 0.0f;
        }
        host_tick( h, HOST_DT );
        clock->accum -= HOST_DT;
        clock->ticks++;
        steps++;
#if QR_LOG
        if ( clock->ticks % SUM_TICKS == 0 )
            host_state_sum( h, (long) clock->ticks );
#endif
    }

    /* Still behind after the cap: give up on the backlog rather than
       carry it into the next frame, where it would only grow. */
    if ( clock->accum > HOST_DT ) clock->accum = 0.0f;

#if QR_PROF
    pt_stop( h, t0, &h->pt.tick_sum, &h->pt.tick_max );
#endif
}

/* ------------------------------------------------------------------ */
/* the render                                                          */

typedef struct {
    Mat4      mtx_fin;
    DiskPlane frustum[6];
    float     xresh, yresh;
} RenderView;

/* the view, the frustum and the BSP walk that fills the draw order */
static void near host_render_walk( Host *h, RenderView *rv )
{
    Camera *cam = &h->cam;
    Mat4 mtx_mdl;
    Vec3 cam_pos_b;
#if QR_PROF
    float tw;
#endif

    qglM4LookAt( &mtx_mdl, &cam->pos, &cam->look_at, &h->cam_up );
    qglM4Conc( &rv->mtx_fin, &mtx_mdl, &h->mtx_prj );
    r_set_frustum( rv->frustum, &rv->mtx_fin );

    /*
     * Birdseye. Deliberate, and it looks like a bug: the frustum above
     * was taken from the PLAYER camera, and the view matrix is now
     * rebuilt from a fixed overhead one, so the mode shows what the PVS
     * and the frustum throw away. Do not move r_set_frustum below this.
     */
    if ( !cam->fps_view ) {
        cam_pos_b.x = 351.0f;
        cam_pos_b.y = 2119.0f;
        cam_pos_b.z = -552.0f;

        cam->look_at.x = cam_pos_b.x + 1.991367e-8f;
        cam->look_at.y = cam_pos_b.y - 1.0f;
        cam->look_at.z = cam_pos_b.z + 1.570986e-2f;
    } else {
        cam_pos_b = cam->pos;
    }

    qglM4LookAt( &mtx_mdl, &cam_pos_b, &cam->look_at, &h->cam_up );
    qglM4Conc( &rv->mtx_fin, &mtx_mdl, &h->mtx_prj );

#if QR_PROF
    tw = sys_now( &h->sysclk );
#endif
    r_draw_world( &h->world, &h->rdr, rv->frustum, 0, &cam->pos, &rv->mtx_fin,
                   rv->xresh, rv->yresh, h->cfg.z_near );
#if QR_PROF
    pt_stop( h, tw, &h->pt.walk_sum, 0 );
    h->rdr.ord_sum += h->rdr.ord_count;
#endif
}

static void near host_render_faces( Host *h, RenderView *rv )
{
    World    *world = &h->world;
    Renderer *rdr   = &h->rdr;
    DrawParams dp;
#if QR_PROF
    float t0;
#endif

    dp.h_dst_dc    = h->h_dst_dc;
    dp.tex_ofs_ptr = world_tex_ofs_ptr( world );
    dp.turb_ptr    = (long) (void far *) d_turb_table();
    dp.xresh       = rv->xresh;
    dp.yresh       = rv->yresh;
    dp.z_near      = h->cfg.z_near;
    dp.z_far       = h->cfg.z_far;
    dp.anim_time   = rdr->anim_time;
    dp.frame_stamp = rdr->frame_stamp;
    dp.ord_count   = (short) rdr->ord_count;
    /* whether the atlas loaded at all; rdr->lightmap is the 'L' toggle */
    dp.use_lm      = (short) ( world->light_atlas != 0 );
    dp.lightmap    = rdr->lightmap;
    dp.backface    = rdr->backface;
    dp.rend_mode   = rdr->rend_mode;
    dp.use_mips    = rdr->use_mips;
    dp.z_avail     = (short) ( h->z_dc != 0 );
    dp.x_res       = h->v.x_res;
    dp.y_res       = h->v.y_res;
#if QR_PROF
    dp.prof        = (short) ( h->pt.n > 0 );
    t0 = sys_now( &h->sysclk );
#else
    dp.prof        = 0;
#endif
    d_draw_faces( world, rdr, h->sc, &h->ls, &dp, &rv->mtx_fin, &h->cam.pos, &h->sysclk );

    rdr->polys = (short) ( rdr->polys + dp.polys );
    rdr->tris  = (short) ( rdr->tris + dp.tris );

#if QR_PROF
    if ( h->pt.n > 0 ) {
        h->pt.build_sum  += dp.build_us  / 1000000.0f;
        h->pt.raster_sum += dp.raster_us / 1000000.0f;
    }
    pt_stop( h, t0, &h->pt.draw_sum, &h->pt.draw_max );
#endif
}

/* the pickups, the models, the missiles and the weapon in hand, depth
   tested against the world already drawn */
static void near host_render_models( Host *h, RenderView *rv )
{
    World    *world = &h->world;
    Renderer *rdr   = &h->rdr;
    float     z_near = h->cfg.z_near;
    QSurf     dst   = h->h_dst_dc;
#if QR_PROF
    float t0 = sys_now( &h->sysclk );
#endif

    d_draw_items( world, rdr, &h->player, rv->frustum, &rv->mtx_fin,
                   rv->xresh, rv->yresh, z_near, dst );
    rdr->mdl_drawn = d_draw_models( world, rdr, rv->frustum, &rv->mtx_fin,
                                     rv->xresh, rv->yresh, z_near, dst );
    d_draw_spikes( &h->fight, &rv->mtx_fin, rv->xresh, rv->yresh, z_near, dst );
    /* the weapon in hand LAST, with depth off inside d_draw_view: it is
       not in the world and nothing may hide it */
    rdr->mdl_drawn = (short) ( rdr->mdl_drawn +
        d_draw_view( world, rdr, &h->cam, &h->player, &h->fight, &rv->mtx_fin,
                      rv->xresh, rv->yresh, z_near, dst ) );

    /* Portal outlines while the depth test is still on, so a portal
       behind a wall is hidden by it. */
    if ( h->hud->portal_wire ) r_portal_outline( world, rdr, dst, &rv->mtx_fin,
                                                  rv->xresh, rv->yresh, z_near );
#if QR_PROF
    pt_stop( h, t0, &h->pt.alias_sum, &h->pt.alias_max );
#endif
}

/* Quake's bar, then the stats overlay that may cover its edge. Under
   -comp host_present draws these onto the composite instead. */
static void near host_render_overlay( Host *h )
{
#if QR_PROF
    float t0 = sys_now( &h->sysclk );
#endif
    if ( !h->v.comp ) {
        scr_sbar_draw( &h->fight, h->h_dst_dc, h->v.x_res, h->v.y_res );
        scr_draw_hud( &h->world, &h->rdr, &h->cam, &h->player, h->sc, h->hud,
                      h->h_dst_dc, h->v.x_res, h->v.y_res );
        scr_draw_msg( h->hud, &h->world, &h->fight, &h->rdr, h->h_dst_dc,
                      h->v.x_res, h->v.y_res );
    }
#if QR_PROF
    pt_stop( h, t0, &h->pt.hud_sum, &h->pt.hud_max );
#endif
}

/* One frame's drawing. Reads the world, changes none of it. */
static void near host_render( Host *h )
{
    RenderView rv;
#if QR_PROF
    float t0 = sys_now( &h->sysclk );
#endif

    rv.xresh = (float) h->v.x_res / 2.0f;
    rv.yresh = (float) h->v.y_res / 2.0f;
    host_render_walk( h, &rv );
#if QR_PROF
    pt_stop( h, t0, &h->pt.cull_sum, &h->pt.cull_max );
#endif

    /* Clear to the far plane: depth is 1/z, so zero is infinitely far.
       The DESTINATION surface: qglSfZClear follows surf->zsf, and a
       depth surface's own zsf is null -- passing z_dc once cleared
       nothing, and every brush entity failed its depth test. */
    if ( h->z_dc != 0 ) qglSfZClear( h->h_dst_dc, 0 );

    /* -nodraw stops here: the walk has run and filled the draw order */
    if ( h->args.no_draw ) return;

    host_render_faces( h, &rv );
    host_render_models( h, &rv );

    /* depth off for the overlay, which is 2D */
    if ( h->z_dc != 0 ) qglSfZMode( h->h_dst_dc, QGL_Z_OFF );

    host_render_overlay( h );
}

/* ------------------------------------------------------------------ */
/* init                                                                */

static void near host_video( Host *h )
{
    Video *v = &h->v;

    v_init_ugl();
    mark( "v_init_ugl ok" );
    {   /* qglNew's `typ` is the dispatch-table OFFSET, not the SURF_
           kind: qglNew( SURF_EMS, ... ) indexed the conventional entry
           and every EMS surface came back null, noticed three loaders
           later. qglSfNew converts; this is the smallest question that
           distinguishes the two. */
        char  gb[64];
        QSurf probe = qglSfNew( 64, 4, QGL_SURF_EMS );

        sprintf( gb, "ems frame=%04X probe=%ld", qglGemFrame(), probe );
        mark( gb );
        if ( !probe ) sys_error( "EMS is up but qglSfNew(SURF_EMS) returned null" );
        qglSfFree( probe );
    }

    v->c_fmt      = 0;      /* UGL.8BIT% */
    v->scr_x_res  = h->cfg.scr_x_res;
    v->scr_y_res  = h->cfg.scr_y_res;
    v->x_res      = h->cfg.x_res;
    v->y_res      = h->cfg.y_res;
    v->comp       = h->args.comp;
    v->view_x     = h->cfg.view_x;
    v->view_y     = h->cfg.view_y;
    v->view_scale = h->cfg.view_scale;

    v_init( v, 0L );
    mark( "v_init ok" );
    {
        char buf[48];
        long raw = (long) (void far *) v->h_comp_dc;
        sprintf( buf, "h_comp_dc seg=%ld ofs=%ld", raw >> 16, raw & 0xFFFFL );
        mark( buf );
    }

    d_init_turb();
    mark( "d_init_turb ok" );
}

static void near host_selftests( void )
{
    char     buf[96];
    long     before;
    SysClock clk;
    float    n0, n1;

    sprintf( buf, "ls_selftest %d", ls_selftest() );
    mark( buf );

    /* LARGEST first: a failing 48h merges the free blocks, and TOTAL
       counts each unmerged one 16 bytes short. */
    before = ( qglMemAvail( QGL_MEM_LARGEST ), qglMemAvail( QGL_MEM_TOTAL ) );
    sprintf( buf, "sc_selftest %d", sc_selftest() );
    mark( buf );
    qglMemAvail( QGL_MEM_LARGEST );
    sprintf( buf, "sc_selftest_leak %ld", before - qglMemAvail( QGL_MEM_TOTAL ) );
    mark( buf );

    sprintf( buf, "dl_selftest %d", dl_selftest() );
    mark( buf );
    sprintf( buf, "ent_lights_selftest %d", ent_lights_selftest() );
    mark( buf );
    sprintf( buf, "item_selftest %d", item_selftest() );
    mark( buf );

    /* sys_time's calibration: tick_hz near 144 on this emulator, not the
       1000 it falls back to uncalibrated -- that fallback once ran the
       whole game 7x too slow. */
    qglTmrInit( 1000 );
    sys_time_init( &clk );
    n0 = sys_now( &clk );
    n1 = sys_now( &clk );
    sprintf( buf, "sys_time tick_hz=%ld.%02d now0=%ld now1=%ld",
             (long) clk.tick_hz, (int) ( (clk.tick_hz - (long) clk.tick_hz) * 100 ),
             (long) (n0 * 1000), (long) (n1 * 1000) );
    mark( buf );
}

/* the map and its textures, two loading-bar stages; the palette comes
   back for host_load to install once the loading screen is done */
static PalRgb far * near host_load_world( Host *h, LoadScreen *ldr )
{
    World *world = &h->world;
    QSurf  vdc   = h->v.h_video_dc;
    MapCounts counts;
    PalRgb far *tex_pal;
    char  buf[160];
    short k;

    ld_stage( ldr, vdc, h->hud, "loading map" );
    /* before the load: r_load_portals reads it to decide whether to take
       the table at all */
    h->rdr.portal = (short) ( h->args.no_portal ? 0 : -1 );
    mod_load_world( world, &h->rdr, &h->cam, &h->fight, h->args.map_name, &counts );
    ld_step( ldr, vdc, h->hud );

    sprintf( buf, "mod_load_world ok faces=%d leaves=%d models=%d tele=%d plat=%d item=%d mon=%d",
             world->face_count, world->leaf_count, world->model_count,
             world->tele_count, world->plat_count, world->item_count, world->mon_count );
    mark( buf );
    /* which models arrived: the model behind a monster record is a
       separate member that may not have. A line a kind: appended into
       one buffer, eight kinds overran it. */
    for ( k = 0; k < MDL_KINDS; k++ ) {
        if ( !world->mdl[k].loaded ) continue;
        sprintf( buf, "model %d:%dv/%dt/%df", (int) k, world->mdl[k].nvert,
                 world->mdl[k].ntri, world->mdl[k].nframe );
        mark( buf );
    }

    sprintf( buf, "spawn=%ld,%ld,%ld angle=%ld",
             (long) h->cam.pos.x, (long) h->cam.pos.y, (long) h->cam.pos.z,
             (long) h->cam.start_angle );
    mark( buf );

    ld_stage( ldr, vdc, h->hud, "loading textures" );
    tex_pal = mod_load_textures( world, &counts );
    ld_step( ldr, vdc, h->hud );

    sprintf( buf, "mod_load_textures ok textures=%ld pal=%ld tex_raw=%ld tex_shaded=%ld",
             counts.textures, (long) (void far *) tex_pal,
             (long) world->tex_raw, (long) world->tex_shaded );
    mark( buf );
    return tex_pal;
}

/* the colormap, the sound, the surface cache, the styles and the input:
   four loading-bar stages */
static void near host_load_rest( Host *h, LoadScreen *ldr )
{
    QSurf vdc = h->v.h_video_dc;
    char  buf[96];

    /* a contiguous 16K EMS page, wanted before other map data (mod.h) */
    ld_stage( ldr, vdc, h->hud, "loading colormap" );
    mod_load_colormap( &h->world );
    mark( "mod_load_colormap ok" );

    /* after the map: its ambient_* points were recorded by the ent loader */
    snd_init( &h->world.snd, &h->world.pak, h->args.no_sound );
    {   short st, lp, un;
        snd_stats( &h->world.snd, &st, &lp, &un );
        sprintf( buf, "snd loops=%d", (int) lp );
        mark( buf );
    }
    if ( h->args.sndsum ) {
        sprintf( buf, "snd_sum=%08lX", snd_sum( &h->world.snd ) );
        mark( buf );
    }
    ld_step( ldr, vdc, h->hud );

    ld_stage( ldr, vdc, h->hud, "surface cache" );
    h->sc = (SurfCache far *) qglMemAlloc( (long) sizeof(SurfCache) );
    if ( !h->sc || !sc_init( h->sc, h->world.face_count ) ) {
        short step; long want;
        sc_fail( &step, &want );
        sprintf( buf, "sc_init FAILED step=%d want=%ld largest=%ld total=%ld",
                 (int) step, want, qglMemAvail( QGL_MEM_LARGEST ),
                 qglMemAvail( QGL_MEM_TOTAL ) );
    } else {
        sprintf( buf, "sc_init ok largest=%ld total=%ld",
                 qglMemAvail( QGL_MEM_LARGEST ), qglMemAvail( QGL_MEM_TOTAL ) );
    }
    mark( buf );
    ld_step( ldr, vdc, h->hud );

    ld_stage( ldr, vdc, h->hud, "light styles" );
    ls_init( &h->ls );
    ld_step( ldr, vdc, h->hud );

    ld_stage( ldr, vdc, h->hud, "input" );
    in_init( &h->input, vdc );
    ld_step( ldr, vdc, h->hud );
}

static void near host_load( Host *h )
{
    LoadScreen  ldr;
    PalRgb far *tex_pal;
    char buf[96];

    /* what a level starts with: PlayerCombat's own, not zero */
    h->fight.health = PL_HEALTH;
    h->fight.shells = PL_SHELLS;
    h->fight.items  = PL_IT_SHOTGUN;
    h->fight.weapon = PL_IT_SHOTGUN;
    h->fight.state  = GS_PLAY;

    /* far: Hud embeds a 2 KB font and two history rings, and as a stack
       local it overran the HostClock beside it rather than faulting */
    h->hud = (Hud far *) qglMemAlloc( (long) sizeof(Hud) );
    if ( !h->hud ) sys_error( "out of far memory for Hud" );
    _fmemset( h->hud, 0, sizeof(*h->hud) );

    /* the skill picks the map's ents<N>.bin, so it is known first: the
       carried one wins over -skill, as the cvar outlives a changelevel */
    h->carried = (short) ( h->args.carry && pl_carry_read( &h->carry ) );
    h->world.skill = h->carried ? h->carry.skill : h->args.skill;
    if ( h->world.skill < 0 ) h->world.skill = 0;
    if ( h->world.skill > 3 ) h->world.skill = 3;
    h->fight.skill = h->world.skill;
    h->world.serverflags = (short) ( ( h->carried ? h->carry.serverflags : h->args.sflags ) & 15 );
    h->fight.serverflags = h->world.serverflags;

    /* The container before anything asks it for a member; marked by
       name, since a changelevel's second leg is a second run in the
       same log. */
    pak_open( &h->world.pak, h->args.map_name );
    sprintf( buf, "asset_map ok %s", h->args.map_name );
    mark( buf );

    /* the font first: the loading screen wants it, and it is not the map's */
    font_load( &h->world.pak, &h->hud->font, "font.fnt" );
    mark( "font_load ok" );
    scr_sbar_load( &h->world.pak );
    mark( "scr_sbar_load ok" );

    /* six ld_step calls follow in the two loaders: keep this in step */
    ld_begin( &ldr, h->v.h_video_dc, h->hud, 6, h->v.scr_x_res, h->v.scr_y_res );
    tex_pal = host_load_world( h, &ldr );
    host_load_rest( h, &ldr );

    /* The map's palette only now: installed during the load it would
       repaint the loading screen's ramps. scr_hud_colors best-fits the
       overlay against the live palette, so it follows. */
    if ( tex_pal ) {
        pal_install( (PalRgb far *) tex_pal );
        qglMemFree( (long) tex_pal );
    }
    scr_hud_colors( h->hud );
    mark( "scr_hud_colors ok" );
}

/* what the next map's process is told about this run: how it runs,
   never where it started -- -at and -yaw belong to this map's geometry */
static void near host_next_flags( Host *h )
{
    const RunArgs *a  = &h->args;
    char          *nf = h->fight.next_flags;

    nf[0] = '\0';
    if ( a->use_lm )   strcat( nf, " -lm" );
    if ( a->no_stats ) strcat( nf, " -nostats" );
    if ( a->no_ai )    strcat( nf, " -noai" );
    if ( a->no_mdl )   strcat( nf, " -nomdl" );
    if ( a->no_items ) strcat( nf, " -noitems" );
    if ( a->no_view )  strcat( nf, " -noview" );
    if ( a->no_sound ) strcat( nf, " -nosound" );
    if ( a->comp )     strcat( nf, " -comp" );
    if ( a->fire )     strcat( nf, " -fire" );
    if ( a->bench_ticks > 0 )
        sprintf( nf + strlen( nf ), " -ticks %ld", a->bench_ticks );
}

/* the player at the spawn or -at, the carried kit, the aim, and the
   inputs a headless run holds */
static void near host_spawn( Host *h )
{
    const RunArgs *a = &h->args;
    char  buf[160];
    short my = 110;

    /* -at X Y Z: BSP space, Z up, the space player.pos lives in */
    if ( a->at_set ) {
        BspVec3 start;
        start.x = a->at_x; start.y = a->at_y; start.z = a->at_z;
        pl_init( &h->player, &h->cam, &start );
    } else {
        pl_init( &h->player, &h->cam, 0 );
    }
    /* where a respawn puts them back: wherever this run started */
    h->fight.spawn = h->player.pos;

    /* marked: the end-of-run counters cannot say what arrived */
    if ( a->carry && !h->carried ) mark( "carry refused: CARRY.BIN missing or short" );
    /* DecodeLevelParms: back on start with a rune held, the kit starts over */
    if ( h->carried && h->world.serverflags && !stricmp( a->map_name, "start.qmp" ) )
        mark( "carry reset: start with a rune held" );
    else if ( h->carried ) {
        pl_carry_apply( &h->fight, &h->carry );
        sprintf( buf, "carry health %d shells %d nails %d rockets %d armor %d items %ld weapon %d",
                 h->fight.health, h->fight.shells, h->fight.nails, h->fight.rockets,
                 h->fight.armor, h->fight.items, h->fight.weapon );
        mark( buf );
    }
    host_next_flags( h );

    /* The camera reads its angle from the mouse, so aiming is moving the
       mouse. v_update_camera reads phi as PI*(mouse.y+2)/y_res; y 110 is
       100.8 degrees, a little below the horizon, what every headless
       shot has always used. */
    if ( a->yaw_set ) h->cam.start_angle = a->yaw;
    if ( a->pitch_set ) {
        my = (short) ( h->v.scr_y_res * a->pitch / 180.0f - 2.0f );
        if ( my < 0 ) my = 0;
        if ( my > h->v.scr_y_res - 1 ) my = (short) ( h->v.scr_y_res - 1 );
    }
    qglMousePos( (short) ( (h->v.scr_x_res - 1) * h->cam.start_angle / 360.0f ), my );

    /* no keyboard under a headless run: hold the keys v_update_camera reads */
    if ( a->walk )   h->input.keyboard.k[KEY_W] = -1;
    if ( a->jump )   h->input.keyboard.k[KEY_SPCBAR] = -1;
    if ( a->strafe ) h->input.keyboard.k[KEY_A] = -1;
    if ( a->fire )   h->input.mouse.left = -1;
}

/* the renderer's switches, the projection, the depth buffer and the clocks */
static void near host_view_setup( Host *h )
{
    const RunArgs *a   = &h->args;
    Renderer      *rdr = &h->rdr;

    h->cam.fps_view = -1;
    h->cam_up.x = 0.0f; h->cam_up.y = 1.0f; h->cam_up.z = 0.0f;

    rdr->use_mips  = (short) ( a->no_mip ? 0 : -1 );
    rdr->no_subvis = a->no_subvis;
    rdr->no_lcache = a->no_lcache;
    rdr->no_dlight = a->no_dlight;
    rdr->no_styles = a->no_styles;
    rdr->lightmap  = a->use_lm;   /* the 'L' toggle's starting state */
    rdr->rend_mode = 0;
    rdr->backface  = (short) ( a->no_cull ? 0 : -1 );
    rdr->no_ents   = a->no_ents;
    rdr->no_items  = a->no_items;
    rdr->no_mdl    = a->no_mdl;
    rdr->no_view   = a->no_view;
    rdr->no_ai     = a->no_ai;
    h->hud->portal_wire = a->ptwire;
    /* off unless asked for; -nostats stays accepted for old recipes */
    h->hud->stats = (short) ( a->stats && !a->no_stats ? -1 : 0 );
    h->hud->bench = (short) ( a->bench_ticks > 0 ? -1 : 0 );

    /* VGA 13h's pixels are not square: the SCALED view size cancels
       against the mode's */
    {
        float aspect = ( (float) h->cfg.view_w * h->cfg.scr_y_res * 4.0f )
                      / ( (float) h->cfg.view_h * h->cfg.scr_x_res * 3.0f );
        qglM4Persp( &h->mtx_prj, h->cfg.cam_fov, aspect, h->cfg.z_near, h->cfg.z_far );
    }

    h->h_dst_dc = h->v.h_back_bdc;
    h->z_dc = 0;
    if ( !a->no_z ) {
        /* qgl binds the depth buffer to the surface it was made for */
        h->z_dc = qglSfZNew( h->h_dst_dc, QGL_SURF_EMS );
        if ( h->z_dc ) qglZScale( 65535.0f * h->cfg.z_near );
    }
    rdr->ents_last = (short) ( a->bad_order || ( h->z_dc != 0 && !a->ent_insert ) );

    sys_time_init( &h->sysclk );
    h->clock.bench_ticks = a->bench_ticks;   /* 0: unbounded, ESC only */
    h->clock.nwarp = a->nwarp;
    memcpy( h->clock.warp_tick, a->warp_tick, sizeof(h->clock.warp_tick) );
    memcpy( h->clock.warp_xyz, a->warp_xyz, sizeof(h->clock.warp_xyz) );
    /* armed per frame once the warm-up is past: the first frames build
       every surface in view and no later frame repeats that */
    h->pt.n = 0;
}

void host_init( Host *h )
{
    rng_init( &h->world.rng );
    sys_parse_args( &h->args );
    mark( "sys_parse_args ok" );
    config_load( &h->cfg, "stuff.ini" );
    mark( "config_load ok" );

    host_video( h );
    host_selftests();
    host_load( h );
    host_spawn( h );
    host_view_setup( h );

    /* the largest DOS block once the map, caches and depth are in */
    bench_begin( &h->bs );
    demo_open( &h->demo, &h->args );
}

/* ------------------------------------------------------------------ */
/* the frame                                                           */

short host_running( const Host *h )
{
    return (short) ( ( h->clock.bench_ticks == 0 || h->clock.ticks < h->clock.bench_ticks )
                     && !h->input.keyboard.k[KEY_ESC] && h->fight.state != GS_NEXT );
}

/* -comp: v_present has scaled the view into the composite and left the
   screen alone, so the overlay goes onto the composite at the mode's
   resolution and one blit carries it. A second pass over VRAM tears and
   costs frames. */
static void near host_present( Host *h )
{
    Video *v = &h->v;
#if QR_PROF
    float t0 = sys_now( &h->sysclk );
#endif

    v_present( v, h->h_dst_dc, 0 );
    if ( v->comp ) {
        scr_sbar_draw( &h->fight, v->h_comp_dc, v->scr_x_res, v->scr_y_res );
        scr_draw_hud( &h->world, &h->rdr, &h->cam, &h->player, h->sc, h->hud,
                      v->h_comp_dc, v->scr_x_res, v->scr_y_res );
        scr_draw_msg( h->hud, &h->world, &h->fight, &h->rdr, v->h_comp_dc,
                      v->scr_x_res, v->scr_y_res );
        qglDrBlit( v->h_video_dc, 0, 0, v->h_comp_dc );
    }
#if QR_PROF
    pt_stop( h, t0, &h->pt.present_sum, 0 );
#endif
}

void host_frame( Host *h )
{
    float raw_dt, frame_dt;
#if QR_PROF
    float t_frame = 0.0f, t_ph;
#endif

    frame_dt = sys_frame_time( &h->sysclk, &raw_dt );
    if ( !demo_input( &h->demo, &h->input, &frame_dt ) ) {
        h->input.keyboard.k[KEY_ESC] = -1;   /* the recording ended: stop, as the live run did */
        return;
    }

    bench_frame_time( &h->bs, raw_dt );
    /* The same predicate as ft_*, so pt_frames and ft_n are the same
       frames. t_frame is this frame's own boundary: raw_dt is the
       previous frame's length. */
    if ( h->bs.frame > BENCH_WARMUP ) {
#if QR_PROF
        /* the walk's counters are means over the profiled frames, so
           they start where pt.n does */
        if ( h->pt.n == 0 ) {
            h->rdr.nd_seen = h->rdr.lf_seen = 0;
            h->rdr.mk_faces = h->rdr.ord_sum = 0;
            h->rdr.dv_calls = h->rdr.dv_fout = h->rdr.dv_desc = h->rdr.dv_vis = 0;
        }
        h->pt.n++;
        t_frame = sys_now( &h->sysclk );
#endif
    }

    host_advance( h, frame_dt );

    /* Between the tick and the render, where nothing holds an EMS window:
       the mixer takes PAGE_SLOT. Every frame: the DMA never stops, and
       what is not repainted is played again. */
#if QR_PROF
    t_ph = sys_now( &h->sysclk );
#endif
    snd_frame( &h->world.snd, &h->world, &h->player, frame_dt );
#if QR_PROF
    pt_stop( h, t_ph, &h->pt.sound_sum, 0 );
#endif

    demo_check( &h->demo, &h->cam, h->bs.frame );
    demo_capture( &h->demo, &h->cam );

    qglDrFill( h->h_dst_dc, 0, 0, (short)(h->v.x_res - 1), (short)(h->v.y_res - 1), 0 );
    host_render( h );
    in_screenshot_key( &h->input, h->h_dst_dc, h->v.x_res, h->v.y_res );
    host_present( h );

    /* the totals before scr_count_frame resets rdr's short counters */
    bench_frame_counts( &h->bs, &h->rdr, &h->pt );
    scr_count_frame( h->hud, &h->rdr, h->sc, frame_dt );
#if QR_PROF
    pt_stop( h, t_frame, &h->pt.frame_sum, &h->pt.frame_max );
#endif
    h->bs.frame++;
}

/* ------------------------------------------------------------------ */
/* shutdown                                                            */

/* the run's summary on E9, which the gates read */
static void near host_report( Host *h )
{
    Fight *fight = &h->fight;
    char   buf[160];
    short  a, b, c;

    sprintf( buf, "frames=%d polys=%ld tris=%ld mdl=%ld pos=%ld,%ld,%ld",
             h->bs.frame, h->bs.poly_sum, h->bs.tri_sum, h->bs.mdl_sum,
             (long) h->cam.pos.x, (long) h->cam.pos.y, (long) h->cam.pos.z );
    mark( buf );

    mdl_ai_stats( &h->world, &a, &b );
    sprintf( buf, "ai hunting=%d moved=%d leaps=%d teles=%d balls=%d mjumps=%d",
             (int) a, (int) b, (int) fight->leaps, (int) h->world.mon_teles,
             (int) ent_fireballs_sent( &h->world ), (int) h->world.mon_jumps );
    mark( buf );

    snd_stats( &h->world.snd, &a, &b, &c );
    sprintf( buf, "snd started=%d loops=%d under=%d", (int) a, (int) b, (int) c );
    mark( buf );
    snd_report( &h->world.snd, buf );
    mark( buf );

    sprintf( buf, "gs_state %d map %s next %s secrets %d/%d skill %d",
             (int) fight->state, h->args.map_name, fight->next_map,
             (int) fight->secrets, (int) fight->secret_total, (int) fight->skill );
    mark( buf );
    sprintf( buf, "sflags %d", (int) fight->serverflags );
    mark( buf );

    sprintf( buf, "fight=health %d shells %d nails %d rockets %d armor %d items %ld took %d kills %d booms %d deaths %d cells %d",
             fight->health, fight->shells, fight->nails, fight->rockets,
             fight->armor, fight->items, item_taken( &h->world ), fight->kills,
             fight->booms, fight->deaths, fight->cells );
    mark( buf );

    bench_mark_times( &h->bs, buf );
}

void host_shutdown( Host *h )
{
    Video *v = &h->v;

    demo_close( &h->demo, &h->args );
    host_report( h );

    /* under -comp the composited frame is in h_comp_dc */
    if ( v->comp )
        scr_screenshot( "bench.bmp", v->h_comp_dc, v->scr_x_res, v->scr_y_res );
    else
        scr_screenshot( "bench.bmp", h->h_dst_dc, v->x_res, v->y_res );
    mark( "scr_screenshot ok" );
    bench_write( &h->bs, &h->pt, &h->rdr, h->sc );
    if ( h->args.mdlsum ) mdl_report_sums( &h->world );

    if ( v->h_back_bdc ) qglSfFree( v->h_back_bdc );
    /* The ISRs out FIRST: INT 8 and INT 9 point into this program, and
       DOS does not restore a vector at exit. A changelevel's second run
       once chained its timer to a dead handler and hung in
       sys_time_init. */
    snd_shutdown( &h->world.snd );
    qglTmrShutdown();
    qglKbdShutdown();
    qglMouseShutdown();
    qglVgaShutdown();
    qglMemShutdown();
    mark( "restored" );

    dbg_probe_report( &h->probe );

    {
        FILE *f = fopen( "cport.txt", "w" );
        if ( f ) {
            /* turb_table()[0] is sin 0: the table built and its far
               pointer reads back */
            fprintf( f, "OK vdc=%ld bdc=%ld turb0_is_zero=%d\n",
                     v->h_video_dc, v->h_back_bdc, ( d_turb_table()[0] == 0.0f ) );
            fclose( f );
        }
    }
}
