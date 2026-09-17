#ifndef __VSHIFT_H__
#define __VSHIFT_H__

#include "fight.h"
#include "renderer.h"

#define V_SHIFT_CONTENTS 0
#define V_SHIFT_DAMAGE   1
#define V_SHIFT_BONUS    2
#define V_SHIFT_POWERUP  3
#define V_SHIFT_COUNT    4

typedef struct ViewShift {
    short red, green, blue, pct;
} ViewShift;

void v_damage_shift( Fight *fight, short armor, short blood );
void v_decay_shifts( Fight *fight, float dt );
void v_shifts( ViewShift shifts[V_SHIFT_COUNT], const Fight *fight,
               const Player *player, float now );
void v_shift_palette( PalRgb *out, const PalRgb *base, short n,
                      const ViewShift shifts[V_SHIFT_COUNT] );

#endif
