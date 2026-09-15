#ifndef __BSPENTS_H__
#define __BSPENTS_H__

/* bspents -- a .bsp's entities lump as a NUL-terminated text the caller frees, 0 when unreadable */
char *bsp_entities( const char *path );

#endif
