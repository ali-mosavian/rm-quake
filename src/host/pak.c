/*
 * pak.c -- see pak.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qgl.h"    /* qglFileOpen/qglFileSeek/qglFileRead/qglMemAlloc */
#include "pak.h"
#include "sys.h"    /* sys_error */

typedef struct {
    long  sig, ver;
    short nmap;
    long  mapofs, sum;
} QpakHead;

static void pak_fatal( char *flname, char *why )
{
    char msg[128];
    sprintf( msg, "%s: %s", flname, why );
    sys_error( msg );
}

static short pak_head( short fh, QpakHead *head )
{
    if ( qglFileRead( fh, (long) (void far *) head, (long) sizeof(*head) )
         != (long) sizeof(*head) ) return 0;
    return head->sig == QPAK_SIG && head->ver == QPAK_VER &&
           head->nmap >= 1 && head->nmap <= QPAK_MAX_MAPS;
}

static short pak_map( short fh, QpakHead *head, char *map, QpakMap *out )
{
    char want[QPAK_MAP_NAME + 1];
    short i, n = (short) strlen( map );

    if ( n > 4 && !stricmp( map + n - 4, ".qmp" ) ) n -= 4;
    if ( n < 1 || n > QPAK_MAP_NAME ) return 0;
    memcpy( want, map, n );
    want[n] = 0;
    for ( i = 0; i < head->nmap; i++ ) {
        if ( !qglFileSeek( fh, head->mapofs + (long) i * QPAK_MAP_ENT ) ||
             qglFileRead( fh, (long) (void far *) out, QPAK_MAP_ENT ) != QPAK_MAP_ENT )
            return 0;
        if ( !strncmp( out->name, want, n ) &&
             ( n == QPAK_MAP_NAME || out->name[n] == 0 ) ) return -1;
    }
    return 0;
}

static short pak_directory( short fh, QpakMap *map, QmapEnt far *dir )
{
    if ( map->n < 1 || map->n > QMAP_MAX || map->dirofs < 64L ||
         !qglFileSeek( fh, map->dirofs ) ) return 0;
    return qglFileRead( fh, (long) dir, (long) map->n * QMAP_ENT )
           == (long) map->n * QMAP_ENT;
}

void pak_open( Pak *pak, char *map )
{
    QpakHead head;
    QpakMap  entry;

    memset( pak, 0, sizeof(*pak) );
    if ( !( pak->fh = qglFileOpen( QPAK_FILE ) ) ) pak_fatal( QPAK_FILE, "would not open" );
    if ( !pak_head( pak->fh, &head ) ) pak_fatal( QPAK_FILE, "not a QPAK this build reads" );
    if ( !pak_map( pak->fh, &head, map, &entry ) ) pak_fatal( map, "no such map in QPAK" );
    if ( !pak_directory( pak->fh, &entry, pak->dir ) ) pak_fatal( map, "directory malformed" );

    pak->n = entry.n;
}

void pak_close( Pak *pak )
{
    if ( pak->fh ) qglFileClose( pak->fh );
    pak->fh = 0;
    pak->n = 0;
}

short pak_open_member( char *member, short *out_fh, long *out_offset,
                       long *out_bytes )
{
    QpakHead head;
    QpakMap map;
    QmapEnt entry;
    short fh, i, j;

    *out_fh = 0;
    if ( !( fh = qglFileOpen( QPAK_FILE ) ) ) return 0;
    if ( !pak_head( fh, &head ) ) { qglFileClose( fh ); return 0; }
    for ( i = 0; i < head.nmap; i++ ) {
        if ( !qglFileSeek( fh, head.mapofs + (long) i * QPAK_MAP_ENT ) ||
             qglFileRead( fh, (long) (void far *) &map, QPAK_MAP_ENT ) != QPAK_MAP_ENT ||
             map.n < 1 || map.n > QMAP_MAX || !qglFileSeek( fh, map.dirofs ) ) {
            qglFileClose( fh );
            return 0;
        }
        for ( j = 0; j < map.n; j++ ) {
            if ( qglFileRead( fh, (long) (void far *) &entry, QMAP_ENT ) != QMAP_ENT ) {
                qglFileClose( fh );
                return 0;
            }
            if ( strncmp( entry.name, member, QMAP_NAME ) ) continue;
            if ( !qglFileSeek( fh, entry.ofs ) ) { qglFileClose( fh ); return 0; }
            *out_fh = fh;
            if ( out_offset ) *out_offset = entry.ofs;
            if ( out_bytes ) *out_bytes = entry.size;
            return -1;
        }
    }
    qglFileClose( fh );
    return 0;
}

short pak_seek( Pak *pak, char *member, long *out_bytes )
{
    short i;

    if ( !pak->fh ) pak_fatal( member, "no map container is open" );
    for ( i = 0; i < pak->n; i++ ) {
        /* the name is NUL padded to the field on disk, so strncmp over
           the whole field is exact and needs no length */
        if ( strncmp( pak->dir[i].name, member, QMAP_NAME ) == 0 ) {
            if ( !qglFileSeek( pak->fh, pak->dir[i].ofs ) )
                pak_fatal( member, "seek failed" );
            if ( out_bytes ) *out_bytes = pak->dir[i].size;
            return pak->fh;
        }
    }
    pak_fatal( member, "no such member" );
    return 0;
}

short pak_seek_part( Pak *pak, char *member, long offset, long bytes )
{
    short i;

    if ( !pak->fh ) pak_fatal( member, "no map container is open" );
    for ( i = 0; i < pak->n; i++ ) {
        if ( strncmp( pak->dir[i].name, member, QMAP_NAME ) != 0 ) continue;
        if ( offset < 0 || bytes < 0 || offset > pak->dir[i].size ||
             bytes > pak->dir[i].size - offset )
            pak_fatal( member, "range out of bounds" );
        if ( !qglFileSeek( pak->fh, pak->dir[i].ofs + offset ) )
            pak_fatal( member, "seek failed" );
        return pak->fh;
    }
    pak_fatal( member, "no such member" );
    return 0;
}

short pak_has( const Pak *pak, char *member )
{
    short i;

    for ( i = 0; i < pak->n; i++ )
        if ( strncmp( pak->dir[i].name, member, QMAP_NAME ) == 0 ) return -1;
    return 0;
}

unsigned char far *pak_load( Pak *pak, char *member, long bytes )
{
    unsigned char far *p;
    short fh = pak_seek( pak, member, 0 );

    p = (unsigned char far *) qglMemAlloc( bytes );
    if ( !p ) pak_fatal( member, "out of memory" );

    if ( qglFileRead( fh, (long) p, bytes ) != bytes ) {
        qglMemFree( (long) p );
        pak_fatal( member, "short read" );
    }
    return p;
}

unsigned char far *pak_load_whole( Pak *pak, char *member, long *out_bytes )
{
    long n;
    unsigned char far *p;

    pak_seek( pak, member, &n );
    p = pak_load( pak, member, n );        /* seeks again; the directory is in memory */
    if ( out_bytes ) *out_bytes = n;
    return p;
}
