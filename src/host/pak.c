/*
 * pak.c -- see pak.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qgl.h"    /* qglFileOpen/qglFileSeek/qglFileRead/qglMemAlloc */
#include "pak.h"
#include "sys.h"    /* sys_error */

static void pak_fatal( char *flname, char *why )
{
    char msg[128];
    sprintf( msg, "%s: %s", flname, why );
    sys_error( msg );
}

void pak_open( Pak *pak, char *qmp )
{
    /* the header, in the writer's own field order */
    struct {
        long  sig, ver;
        short ndir;
        long  dirofs, sum;
        char  name[QMAP_NAME];
    } head;

    if ( !( pak->fh = qglFileOpen( qmp ) ) ) pak_fatal( qmp, "would not open" );

    if ( qglFileRead( pak->fh, (long) (void far *) &head, (long) sizeof(head) )
             != (long) sizeof(head) )
        pak_fatal( qmp, "header short read" );
    if ( head.sig != QMAP_SIG ) pak_fatal( qmp, "not a map container" );
    if ( head.ver != QMAP_VER ) pak_fatal( qmp, "a map container this build does not read" );
    if ( head.ndir < 1 || head.ndir > QMAP_MAX ) pak_fatal( qmp, "directory out of range" );

    pak->n = head.ndir;
    if ( !qglFileSeek( pak->fh, head.dirofs ) ) pak_fatal( qmp, "directory seek failed" );
    if ( qglFileRead( pak->fh, (long) (void far *) pak->dir,
                       (long) pak->n * QMAP_ENT ) != (long) pak->n * QMAP_ENT )
        pak_fatal( qmp, "directory short read" );
}

void pak_close( Pak *pak )
{
    if ( pak->fh ) qglFileClose( pak->fh );
    pak->fh = 0;
    pak->n = 0;
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
