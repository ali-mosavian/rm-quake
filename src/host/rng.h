#ifndef __RNG_H__
#define __RNG_H__

/*
 * rng.h -- the game's one random stream. Borland's rand() (CRTL RAND.C):
 * the same LCG from the same seed, so a run rolls the dice it rolled
 * through libc. dl.c's flicker keeps its own.
 */

#define RNG_MAX 0x7FFF

typedef struct {
    unsigned long seed;
} Rng;

void  rng_init( Rng *rng );
short rng_int( Rng *rng );     /* 0..RNG_MAX */
float rng_float( Rng *rng );   /* [0,1) */

#endif
