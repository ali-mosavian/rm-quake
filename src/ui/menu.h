#ifndef __MENU_H__
#define __MENU_H__

#include "cmd.h"
#include "keybind.h"
#include "pak.h"
#include "qgltypes.h"
#include "savegame.h"

#ifdef __BORLANDC__
#define MENU_FAR far
#else
#define MENU_FAR
#endif

typedef enum {
    MENU_CLOSED,
    MENU_MAIN,
    MENU_SINGLE,
    MENU_OPTIONS,
    MENU_KEYS,
    MENU_LOAD,
    MENU_SAVE,
    MENU_HELP
} MenuPage;

typedef enum {
    MENU_ACTION_NONE,
    MENU_ACTION_NEW,
    MENU_ACTION_CONSOLE,
    MENU_ACTION_DEFAULTS,
    MENU_ACTION_LOAD,
    MENU_ACTION_SAVE,
    MENU_ACTION_QUIT
} MenuAction;

typedef struct {
    QSurf atlas;
    QSurf quit_dialog;
    short page, cursor, bind_grab, help_page;
    short quit_active, quit_message;
    char notice[40];
    char slots[SG_SLOTS][SG_COMMENT];
    unsigned char loadable[SG_SLOTS];
} Menu;

void menu_init( Menu MENU_FAR *menu );
short menu_open( Menu MENU_FAR *menu, Pak *pak, MenuPage page );
short menu_quit( Menu MENU_FAR *menu, Pak *pak );
void menu_close( Menu MENU_FAR *menu );
short menu_is_open( const Menu MENU_FAR *menu );
MenuAction menu_key( Menu MENU_FAR *menu, short key,
                     KeyBindings KB_FAR *keys, CmdState far *cmd, Pak *pak );
void menu_draw( Menu MENU_FAR *menu, const KeyBindings KB_FAR *keys,
                const CmdState far *cmd, float now, QSurf dst, short w, short h );

#endif
