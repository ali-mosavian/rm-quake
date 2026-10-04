/*
 * bench.c -- see bench.h.
 */

#include "qrcfg.h"
#include <stdio.h>
#include <string.h>
#include "dbg.h"
#include "qgl.h"
#include "bench.h"
#include "r_portal.h"   /* r_portal_stats -- the flood totals */

void bench_begin( BenchStats *bs )
{
    memset( bs, 0, sizeof(*bs) );
    bs->mem_far_largest = qglMemAvail( QGL_MEM_LARGEST );
}

void bench_frame_time( BenchStats *bs, float raw_dt )
{
    if ( bs->frame <= BENCH_WARMUP || raw_dt <= 0.0f ) return;
    if ( bs->ft_n == 0 ) {
        bs->ft_min = raw_dt;
        bs->ft_max = raw_dt;
    } else {
        if ( raw_dt < bs->ft_min ) bs->ft_min = raw_dt;
        if ( raw_dt > bs->ft_max ) bs->ft_max = raw_dt;
    }
    bs->ft_sum += raw_dt;
    bs->ft_n++;
}

/* Read before scr_count_frame resets the renderer's per-frame counters. */
void bench_frame_counts( BenchStats *bs, const Renderer *rdr, const PhaseTimes *pt )
{
    bs->poly_sum += rdr->polys;
#if QR_PROF
    if ( pt->n > 0 ) bs->pt_poly_sum += rdr->polys;
#else
    (void) pt;
#endif
    bs->tri_sum += rdr->tris;
    bs->mdl_sum += rdr->mdl_drawn;
}

/* buf must hold 96 bytes */
void bench_mark_times( const BenchStats *bs, char *buf )
{
    float mean;
    if ( bs->ft_n <= 0 ) return;
    mean = bs->ft_sum / bs->ft_n;
    sprintf( buf, "ft_mean=%ld.%02dms fps_mean=%ld.%02d ft_min=%ldms ft_max=%ldms n=%ld",
             (long) ( mean * 1000.0f ),
             (int) ( ( mean * 1000.0f - (long) ( mean * 1000.0f ) ) * 100 ),
             (long) ( bs->ft_n / bs->ft_sum ),
             (int) ( ( (float) bs->ft_n / bs->ft_sum - (long) ( (float) bs->ft_n / bs->ft_sum ) ) * 100 ),
             (long) ( bs->ft_min * 1000.0f ), (long) ( bs->ft_max * 1000.0f ), bs->ft_n );
    mark( buf );
}

static void near bench_ms( FILE *bf, const char *key, float s )
{
    fprintf( bf, "%s %ld.%03ld\n", key, (long) ( s * 1000 ), (long) ( s * 1000000 ) % 1000 );
}

#if QR_PROF
/* ms a frame over the frames profiled, which are ft_n's own. pt_other is
   what the brackets do not reach -- the backbuffer clear, the input read,
   the HUD's counters, and any pass added without one. */
static void near bench_write_phases( FILE *bf, const PhaseTimes *pt )
{
    float ph[9];
    char *nm[9];
    short i;

    ph[0] = pt->frame_sum;   nm[0] = "frame";
    ph[1] = pt->tick_sum;    nm[1] = "tick";
    ph[2] = pt->cull_sum;    nm[2] = "cull";
    ph[3] = pt->draw_sum;    nm[3] = "draw";
    ph[4] = pt->alias_sum;   nm[4] = "alias";
    ph[5] = pt->hud_sum;     nm[5] = "hud";
    ph[6] = pt->sound_sum;   nm[6] = "sound";
    ph[7] = pt->present_sum; nm[7] = "present";
    ph[8] = pt->frame_sum - ( pt->tick_sum + pt->cull_sum + pt->draw_sum +
                              pt->alias_sum + pt->hud_sum + pt->sound_sum +
                              pt->present_sum );
    nm[8] = "other";
    for ( i = 0; i < 9; i++ ) {
        float ms = ph[i] / pt->n * 1000.0f;
        short neg = (short) ( ms < 0.0f );
        long  us = (long) ( ( neg ? -ms : ms ) * 1000.0f + 0.5f );
        fprintf( bf, "pt_%s_mean %s%ld.%03ld\n", nm[i], neg ? "-" : "", us / 1000, us % 1000 );
    }
}

static void near bench_write_profile( FILE *bf, const BenchStats *bs, const PhaseTimes *pt,
                                      const Renderer *rdr )
{
    bench_write_phases( bf, pt );
    /* build and raster are INSIDE draw, so they are printed apart */
    bench_ms( bf, "pt_build_mean", pt->build_sum / pt->n );
    bench_ms( bf, "pt_raster_mean", pt->raster_sum / pt->n );
    bench_ms( bf, "pt_frame_max", pt->frame_max );
    bench_ms( bf, "pt_walk_mean", pt->walk_sum / pt->n );
    fprintf( bf, "pt_nodes %ld\n", rdr->nd_seen / pt->n );
    fprintf( bf, "pt_leaves %ld\n", rdr->lf_seen / pt->n );
    fprintf( bf, "pt_marked %ld\n", rdr->mk_faces / pt->n );
    fprintf( bf, "pt_polys %ld\n", bs->pt_poly_sum / pt->n );
    /* d_mdl_visible, a frame: means over the profiled frames, since a raw
       total differs between two arms that rendered a different count */
    fprintf( bf, "pt_dv_calls %ld\n", rdr->dv_calls / pt->n );
    fprintf( bf, "pt_dv_fout %ld\n", rdr->dv_fout / pt->n );
    fprintf( bf, "pt_dv_desc %ld\n", rdr->dv_desc / pt->n );
    fprintf( bf, "pt_dv_vis %ld\n", rdr->dv_vis / pt->n );
    fprintf( bf, "pt_vis_leaves %d\n", rdr->vis_leaves );
    fprintf( bf, "pt_vis_nodes %d\n", rdr->vis_nodes );
    fprintf( bf, "pt_ord %ld\n", rdr->ord_sum / pt->n );
}
#endif

static void near bench_write_times( FILE *bf, const BenchStats *bs, const PhaseTimes *pt,
                                    const Renderer *rdr )
{
    long pops, projs, pushes;

    bench_ms( bf, "ft_min", bs->ft_min );
    bench_ms( bf, "ft_max", bs->ft_max );
    bench_ms( bf, "ft_mean", bs->ft_sum / bs->ft_n );
    fprintf( bf, "ft_n %ld\n", bs->ft_n );
#if QR_PROF
    /* the phase sums cover every profiled frame; say how many */
    fprintf( bf, "pt_frames %ld\n", pt->n );
#endif
    fprintf( bf, "fps_mean %ld.%02ld\n", (long) ( bs->ft_n / bs->ft_sum ),
             (long) ( ( bs->ft_n / bs->ft_sum ) * 100 ) % 100 );
#if QR_PROF
    if ( pt->n > 0 ) bench_write_profile( bf, bs, pt, rdr );
#else
    (void) pt; (void) rdr;
#endif
    /* the portal flood's own work, so a cull cost has a divisor */
    r_portal_stats( &pops, &projs, &pushes );
    fprintf( bf, "pt_pops %ld\n", pops );
    fprintf( bf, "pt_projs %ld\n", projs );
    fprintf( bf, "pt_pushes %ld\n", pushes );
}

void bench_write( const BenchStats *bs, const PhaseTimes *pt, const Renderer *rdr,
                  SurfCache far *sc )
{
    CacheStats scs;
    FILE *bf = fopen( "bench.txt", "w" );
    if ( !bf ) return;

    fprintf( bf, "frames %d\n", bs->frame );
    fprintf( bf, "polys %ld\n", bs->poly_sum );
    fprintf( bf, "tris %ld\n", bs->tri_sum );
    sc_stats( sc, &scs );
    fprintf( bf, "sc_builds %ld\n", scs.total_builds );
    fprintf( bf, "sc_evict %ld\n", scs.evict );
    fprintf( bf, "sc_live %ld\n", scs.live );
    fprintf( bf, "sc_blocks_peak %d\n", scs.blocks );
    fprintf( bf, "sc_nofresh %ld\n", scs.nofresh );
    fprintf( bf, "sc_unlit %ld\n", scs.unlit );
    fprintf( bf, "mem_far_largest %ld\n", bs->mem_far_largest );
    if ( bs->ft_n > 0 ) bench_write_times( bf, bs, pt, rdr );
    fclose( bf );
}
