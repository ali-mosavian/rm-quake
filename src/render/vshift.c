/* id's cshifts: palette-space effects layered in contents, damage, bonus,
   powerup order. They describe a frame and never alter game state except
   their two explicitly decaying flash strengths. */

#include "vshift.h"

static void v_shift_set( ViewShift *shift, short red, short green, short blue, short pct )
{
    shift->red = red; shift->green = green; shift->blue = blue; shift->pct = pct;
}

void v_damage_shift( Fight *fight, short armor, short blood )
{
    float count = (float) ( armor + blood ) * 0.5f;

    if ( count < 10.0f ) count = 10.0f;
    fight->damage_pct += 3.0f * count;
    if ( fight->damage_pct > 150.0f ) fight->damage_pct = 150.0f;
    if ( armor > blood ) {
        fight->damage_red = 200; fight->damage_green = 100; fight->damage_blue = 100;
    } else if ( armor ) {
        fight->damage_red = 220; fight->damage_green = 50; fight->damage_blue = 50;
    } else {
        fight->damage_red = 255; fight->damage_green = 0; fight->damage_blue = 0;
    }
}

void v_decay_shifts( Fight *fight, float dt )
{
    fight->damage_pct -= dt * 150.0f;
    if ( fight->damage_pct < 0.0f ) fight->damage_pct = 0.0f;
    fight->bonus_pct -= dt * 100.0f;
    if ( fight->bonus_pct < 0.0f ) fight->bonus_pct = 0.0f;
}

void v_shifts( ViewShift shifts[V_SHIFT_COUNT], const Fight *fight,
               const Player *player, float now )
{
    v_shift_set( &shifts[V_SHIFT_CONTENTS], 130, 80, 50, 0 );
    if ( player->water_level == 3 ) {
        if ( player->water_type == CONTENTS_LAVA )
            v_shift_set( &shifts[V_SHIFT_CONTENTS], 255, 80, 0, 150 );
        else if ( player->water_type == CONTENTS_SLIME )
            v_shift_set( &shifts[V_SHIFT_CONTENTS], 0, 25, 5, 150 );
        else
            v_shift_set( &shifts[V_SHIFT_CONTENTS], 130, 80, 50, 128 );
    }
    v_shift_set( &shifts[V_SHIFT_DAMAGE], fight->damage_red, fight->damage_green,
                 fight->damage_blue, (short) fight->damage_pct );
    v_shift_set( &shifts[V_SHIFT_BONUS], 215, 186, 69, (short) fight->bonus_pct );
    v_shift_set( &shifts[V_SHIFT_POWERUP], 0, 0, 0, 0 );
    if ( now < fight->quad_until )
        v_shift_set( &shifts[V_SHIFT_POWERUP], 0, 0, 255, 30 );
    else if ( now < fight->suit_until )
        v_shift_set( &shifts[V_SHIFT_POWERUP], 0, 255, 0, 20 );
    else if ( now < fight->invis_until )
        v_shift_set( &shifts[V_SHIFT_POWERUP], 100, 100, 100, 100 );
    else if ( now < fight->pent_until )
        v_shift_set( &shifts[V_SHIFT_POWERUP], 255, 255, 0, 30 );
}

void v_shift_palette( PalRgb *out, const PalRgb *base, short n,
                      const ViewShift shifts[V_SHIFT_COUNT] )
{
    short i, k;
    int red, green, blue;

    for ( i = 0; i < n; i++ ) {
        red = base[i].red; green = base[i].green; blue = base[i].blue;
        for ( k = 0; k < V_SHIFT_COUNT; k++ ) {
            red += ( shifts[k].pct * ( shifts[k].red - red ) ) >> 8;
            green += ( shifts[k].pct * ( shifts[k].green - green ) ) >> 8;
            blue += ( shifts[k].pct * ( shifts[k].blue - blue ) ) >> 8;
        }
        out[i].red = (unsigned char) red;
        out[i].green = (unsigned char) green;
        out[i].blue = (unsigned char) blue;
    }
}
