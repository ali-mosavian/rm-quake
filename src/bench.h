#ifndef __BENCH_H__
#define __BENCH_H__

/*
 * bench.h -- what a bounded run measured, and bench.txt.
 *
 * The frame-time figures skip the first frames: they carry the tail of
 * the load and the first surface builds, which no later frame repeats.
 */

#include "renderer.h"
#include "sc.h"

typedef struct {
    short frame;              /* frames drawn */
    long  poly_sum, tri_sum, mdl_sum;
    long  pt_poly_sum;        /* polys over the profiled frames alone */
    float ft_min, ft_max, ft_sum;
    long  ft_n;               /* frames timed: frame > BENCH_WARMUP */
    long  mem_far_largest;    /* the largest DOS block once the map is in */
} BenchStats;

#define BENCH_WARMUP 3

void bench_begin( BenchStats *bs );
void bench_frame_time( BenchStats *bs, float raw_dt );
void bench_frame_counts( BenchStats *bs, const Renderer *rdr, const PhaseTimes *pt );
void bench_mark_times( const BenchStats *bs, char *buf );
void bench_write( const BenchStats *bs, const PhaseTimes *pt, const Renderer *rdr,
                  SurfCache far *sc );

#endif
