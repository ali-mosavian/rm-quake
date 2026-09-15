#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ed.h"
#include "edict.h"

#ifndef __TURBOC__
#define _fmemset memset
#endif

#define MAXCLIENTS  1
#define KEY_MAX     256     /* ED_ParseEdict's keyname */
#define ANGLE_MAX   32      /* its anglehack temp */

enum { EV_VOID, EV_STRING, EV_FLOAT, EV_VECTOR, EV_ENTITY, EV_FIELD, EV_FUNCTION };
#define DEF_SAVEGLOBAL  0x8000

static QVal fval( float f ) { QVal v; v.f = f; return v; }
static QVal ival( sint32 i ) { QVal v; v.i = i; return v; }

sint16 ed_init( EdWorld *w, Qvm *m, sint16 max, float QFAR *freetime, uint8 QFAR *isfree )
{
    static char *names[12] = {
        "classname", "spawnflags", "model", "takedamage", "modelindex", "colormap",
        "skin", "frame", "origin", "angles", "nextthink", "solid",
    };
    sint16 *fld[12];
    sint16 i;

    fld[0] = &w->f_classname;  fld[1] = &w->f_spawnflags; fld[2] = &w->f_model;
    fld[3] = &w->f_takedamage; fld[4] = &w->f_modelindex; fld[5] = &w->f_colormap;
    fld[6] = &w->f_skin;       fld[7] = &w->f_frame;      fld[8] = &w->f_origin;
    fld[9] = &w->f_angles;     fld[10] = &w->f_nextthink; fld[11] = &w->f_solid;
    for ( i = 0; i < 12; i++ )
        if ( ( *fld[i] = qvm_field( m, names[i] ) ) < 0 ) return ED_ERR_FIELDS;
    if ( max < MAXCLIENTS + 2 || m->nedict < max ) return ED_ERR_FIELDS;

    w->m = m;
    w->num = MAXCLIENTS + 1;
    w->max = max;
    w->time = 0.0;
    w->skill = 0;
    w->freetime = freetime;
    w->isfree = isfree;
    w->hook = 0;
    _fmemset( freetime, 0, (uint16) max * sizeof *freetime );
    _fmemset( isfree, 0, (uint16) max );
    return 0;
}

static sint16 ed_clear( EdWorld *w, sint16 n )
{
    w->isfree[n] = 0;
    return qvm_ent_zero( w->m, qvm_entval( w->m, n ) );
}

sint16 ed_alloc( EdWorld *w )
{
    sint16 i;

    for ( i = MAXCLIENTS + 1; i < w->num; i++ ) {
        /* the first couple of seconds free and allocate a lot, so id relaxes the reuse */
        if ( w->isfree[i] && ( w->freetime[i] < 2.0f || w->time - w->freetime[i] > 0.5 ) )
            return ed_clear( w, i ) ? -2 : i;
    }
    if ( i == w->max ) return -1;
    w->num++;
    return ed_clear( w, i ) ? -2 : i;
}

void ed_free( EdWorld *w, sint16 n )
{
    Qvm *m = w->m;
    sint32 e = qvm_entval( m, n );
    sint16 i;

    w->isfree[n] = 1;
    qvm_ent_set( m, e, w->f_model, ival( 0 ) );
    qvm_ent_set( m, e, w->f_takedamage, fval( 0.0f ) );
    qvm_ent_set( m, e, w->f_modelindex, fval( 0.0f ) );
    qvm_ent_set( m, e, w->f_colormap, fval( 0.0f ) );
    qvm_ent_set( m, e, w->f_skin, fval( 0.0f ) );
    qvm_ent_set( m, e, w->f_frame, fval( 0.0f ) );
    for ( i = 0; i < 3; i++ ) {
        qvm_ent_set( m, e, (sint16) ( w->f_origin + i ), fval( 0.0f ) );
        qvm_ent_set( m, e, (sint16) ( w->f_angles + i ), fval( 0.0f ) );
    }
    qvm_ent_set( m, e, w->f_nextthink, fval( -1.0f ) );
    qvm_ent_set( m, e, w->f_solid, fval( 0.0f ) );
    w->freetime[n] = (float) w->time;
}

/* ED_ParseEpair on value s, which it may overwrite */
static sint16 parse_epair( EdWorld *w, sint32 e, const QDef *key, char *s )
{
    Qvm *m = w->m;
    QDef def;
    float v[3];
    sint32 str, num;
    sint16 i, fn;

    switch ( key->type & ~DEF_SAVEGLOBAL ) {
    case EV_STRING:
        ed_unescape( s, s, ED_TOKEN );          /* in place: it never grows */
        if ( !( str = qvm_str_add( m, s ) ) ) return ED_ERR_STRING;
        qvm_ent_set( m, e, (sint16) key->ofs, ival( str ) );
        break;
    case EV_FLOAT:
        qvm_ent_set( m, e, (sint16) key->ofs, fval( (float) atof( s ) ) );
        break;
    case EV_VECTOR:
        if ( ed_vector( s, v ) ) return ED_ERR_VALUE;
        for ( i = 0; i < 3; i++ ) qvm_ent_set( m, e, (sint16) ( key->ofs + i ), fval( v[i] ) );
        break;
    case EV_ENTITY:
        num = atol( s );
        if ( num < 0 || num >= w->max ) return ED_ERR_ENTITY;
        qvm_ent_set( m, e, (sint16) key->ofs, ival( qvm_entval( m, num ) ) );
        break;
    case EV_FIELD:
        /* "" names fielddef 0, whose name is "" and offset 0 */
        if ( s[0] ) { if ( qvm_field_def( m, s, &def ) ) return ED_ERR_VALUE; }
        else def.ofs = 0;
        qvm_ent_set( m, e, (sint16) key->ofs, m->glob[def.ofs] );    /* id's G_INT(def->ofs), a global */
        break;
    case EV_FUNCTION:
        /* "" names function 0, as the field case's fielddef 0 */
        if ( !( fn = qvm_func( m, s ) ) && s[0] ) return ED_ERR_VALUE;
        qvm_ent_set( m, e, (sint16) key->ofs, ival( fn ) );
        break;
    }
    return 0;
}

sint16 ed_parse_edict( EdWorld *w, const char QFAR **data, sint16 n )
{
    Qvm *m = w->m;
    sint32 e = qvm_entval( m, n );
    const char QFAR *p = *data;
    char tok[ED_TOKEN], keyname[KEY_MAX], temp[ANGLE_MAX];
    QDef key;
    sint16 anglehack, init = 0, len, err;

    if ( n != 0 && qvm_ent_zero( m, e ) ) return ED_ERR_EMS;   /* the world keeps what the server set */
    for ( ;; ) {
        p = ed_token( p, tok, sizeof tok );
        if ( tok[0] == '}' ) break;
        if ( !p ) return ED_ERR_EOF;

        /* QuakeEd's scalar angle, and light's field: both compared before the trim */
        anglehack = !strcmp( tok, "angle" );
        if ( anglehack ) strcpy( tok, "angles" );
        if ( !strcmp( tok, "light" ) ) strcpy( tok, "light_lev" );
        if ( strlen( tok ) >= KEY_MAX ) return ED_ERR_LONG;
        strcpy( keyname, tok );
        for ( len = (sint16) strlen( keyname ); len && keyname[len - 1] == ' '; ) keyname[--len] = 0;

        p = ed_token( p, tok, sizeof tok );
        if ( !p ) return ED_ERR_EOF;
        if ( tok[0] == '}' ) return ED_ERR_BRACE;
        init = 1;
        if ( keyname[0] == '_' ) continue;      /* utility comments */
        if ( qvm_field_def( m, keyname, &key ) ) continue;
        if ( anglehack ) {
            if ( strlen( tok ) >= ANGLE_MAX ) return ED_ERR_LONG;
            strcpy( temp, tok );
            sprintf( tok, "0 %s 0", temp );
        }
        if ( ( err = parse_epair( w, e, &key, tok ) ) != 0 ) return err;
    }
    if ( !init ) w->isfree[n] = 1;
    *data = p;
    return 0;
}

static sint16 inhibited( EdWorld *w, sint32 e )
{
    long flags = (long) qvm_ent_get( w->m, e, w->f_spawnflags ).f;

    return ( w->skill == 0 && ( flags & 256 ) ) || ( w->skill == 1 && ( flags & 512 ) )
        || ( w->skill >= 2 && ( flags & 1024 ) );
}

sint16 ed_load( EdWorld *w, const char QFAR *text )
{
    Qvm *m = w->m;
    char tok[2];                                /* only the first byte is asked; ed_token still steps the whole token */
    sint16 n = 0, first = 1, err, fn;
    sint32 e, classname;

    m->glob[QG_TIME].f = (float) w->time;
    for ( ;; ) {
        text = ed_token( text, tok, sizeof tok );
        if ( !text ) break;
        if ( tok[0] != '{' ) return ED_ERR_BRACE;
        if ( !first && ( n = ed_alloc( w ) ) < 0 ) return n == -1 ? ED_ERR_NOEDICTS : ED_ERR_EMS;
        first = 0;
        if ( ( err = ed_parse_edict( w, &text, n ) ) != 0 ) return err;
        if ( w->hook ) w->hook( w, ED_PARSED, n );

        e = qvm_entval( m, n );
        if ( inhibited( w, e ) ) { ed_free( w, n ); continue; }
        if ( !( classname = qvm_ent_get( m, e, w->f_classname ).i ) ) { ed_free( w, n ); continue; }
        if ( classname == QVM_STR_EMPTY ) return ED_ERR_NULLFN;
        if ( !( fn = qvm_func_str( m, classname ) ) ) { ed_free( w, n ); continue; }

        m->glob[QG_SELF].i = e;
        if ( w->hook ) w->hook( w, ED_SPAWNING, n );
        qvm_run( m, fn );
        if ( m->trap ) return ED_ERR_TRAP;
        if ( w->hook ) w->hook( w, ED_SPAWNED, n );
    }
    return 0;
}
