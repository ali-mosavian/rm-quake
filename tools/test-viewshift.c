#include <assert.h>
#include <string.h>

#include "vshift.h"

int main( void )
{
    Fight fight;
    Player player;
    ViewShift shifts[V_SHIFT_COUNT];
    PalRgb base[1], out[1];

    memset( &fight, 0, sizeof(fight) );
    memset( &player, 0, sizeof(player) );
    player.water_level = 3;
    player.water_type = CONTENTS_WATER;
    fight.bonus_pct = 50.0f;
    fight.pent_until = 2.0f;
    v_shifts( shifts, &fight, &player, 1.0f );
    assert( shifts[V_SHIFT_CONTENTS].pct == 128 );
    assert( shifts[V_SHIFT_CONTENTS].red == 130 && shifts[V_SHIFT_CONTENTS].green == 80 );
    assert( shifts[V_SHIFT_BONUS].pct == 50 );
    assert( shifts[V_SHIFT_POWERUP].red == 255 && shifts[V_SHIFT_POWERUP].green == 255 );

    v_damage_shift( &fight, 8, 2 );
    assert( fight.damage_pct == 30.0f );
    assert( fight.damage_red == 200 && fight.damage_green == 100 && fight.damage_blue == 100 );
    v_decay_shifts( &fight, 0.1f );
    assert( fight.damage_pct == 15.0f && fight.bonus_pct == 40.0f );

    base[0].red = 100; base[0].green = 100; base[0].blue = 100;
    memset( shifts, 0, sizeof(shifts) );
    shifts[0].red = 200; shifts[0].green = 0; shifts[0].blue = 0; shifts[0].pct = 128;
    v_shift_palette( out, base, 1, shifts );
    assert( out[0].red == 150 && out[0].green == 50 && out[0].blue == 50 );
    return 0;
}
