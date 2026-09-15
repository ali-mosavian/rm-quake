/*
 * rng.c -- see rng.h.
 */

#include "rng.h"

void rng_init( Rng *rng )
{
    rng->seed = 1UL;
}

short rng_int( Rng *rng )
{
    rng->seed = rng->seed * 0x015A4E35UL + 1UL;
    return (short) ( ( rng->seed >> 16 ) & 0x7FFFUL );
}

float rng_float( Rng *rng )
{
    return (float) rng_int( rng ) / ( (float) RNG_MAX + 1.0f );
}
