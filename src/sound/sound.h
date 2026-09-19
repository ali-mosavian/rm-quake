#ifndef __SOUND_H__
#define __SOUND_H__

/*
 * sound.h -- the sound layer's state, apart from snd.h so world.h can
 * hold it without snd.h's chain of game headers.
 */

#include "snd_mix.h"

#define SND_COUNT 180   /* tools/mksnd.py's SOUNDS; snd.h's SND_* index them */

typedef struct {
    short  on;          /* the card is up and the samples are in */
    short  hnd;         /* the samples' EMS handle */
    short  started, under;
    unsigned short painted; /* half-buffers this map's ISR callback filled */
    unsigned long hash; /* every snd_start's (id, chan), -nosound included */
    unsigned char seen[(SND_COUNT + 7) / 8];   /* every id started, a bit each */
    SndMix mix;
} Sound;

#endif
