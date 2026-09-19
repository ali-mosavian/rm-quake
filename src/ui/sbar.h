#ifndef __SBAR_H__
#define __SBAR_H__

#include "pak.h"
#include "qgltypes.h"
#include "fight.h"
#include "world.h"

/* One EMS atlas: the intermission/finale pictures, three source bars, a
   320x48 work band, then the WAD pictures. Its 512-byte rows divide an EMS
   page and need no scanline table. */
#define SBARC_W          320
#define SBARC_H           48
#define SBARC_EMS_W      512
#define SBARC_EMS_H      320
#define SBARC_INTER_X      0
#define SBARC_INTER_Y      0
#define SBARC_COMPLETE_X 160
#define SBARC_COMPLETE_Y   0
#define SBARC_FINALE_X   160
#define SBARC_FINALE_Y    24
#define SBARC_BAR_X      160
#define SBARC_SBAR_Y      48
#define SBARC_IBAR_Y      72
#define SBARC_SCORE_Y     96
#define SBARC_WORK_X     160
#define SBARC_WORK_Y     120
#define SBARC_CELL_Y     168
#define SBARC_CELL_COLS   21
#define SBARC_WEAPON_Y   240
#define SBARC_WEAPON_COLS 21
#define SBARC_ITEM_Y     272
#define SBARC_ITEM_X     336
#define SBARC_SYMBOL_X    48
#define SBARC_SYMBOL_Y   216
#define SBARC_CHARS_Y    288

void scr_sbar_load( Pak *pak );
void scr_sbar_shutdown( void );
void scr_sbar_draw( World *world, Fight *fight, float now, short showscores,
                    short lines, QSurf dst, short w, short h );
void scr_sbar_intermission( World *world, Fight *fight, QSurf dst, short w, short h );
void scr_sbar_finale( QSurf dst, short w );

#endif
