#ifndef __SAVEGAME_H__
#define __SAVEGAME_H__

#include "world.h"
#include "renderer.h"
#include "fight.h"

#define SG_SLOTS 12
#define SG_COMMENT 40

short savegame_slot( const char *name );
void savegame_scan( char comments[SG_SLOTS][SG_COMMENT], unsigned char loadable[SG_SLOTS] );
short savegame_header( short slot, char *map, short *skill, char *error );
short savegame_save( short slot, const char *map, World *world, Player *player,
                     Camera *cam, Fight *fight, Renderer *rdr,
                     short mouse_x, short mouse_y, char *error );
short savegame_load( short slot, const char *map, World *world, Player *player,
                     Camera *cam, Fight *fight, Renderer *rdr,
                     short *mouse_x, short *mouse_y, char *error );

#endif
