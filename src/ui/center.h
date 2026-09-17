#ifndef __CENTER_H__
#define __CENTER_H__

#ifdef __BORLANDC__
#define CENTER_FAR far
#else
#define CENTER_FAR
#endif

#define CENTER_LINE 40
#define CENTER_TEXT_MAX 1023

short center_count_lines( const char CENTER_FAR *text );
const char CENTER_FAR *center_next_line( const char CENTER_FAR *text,
                                         char *line );
short center_y( short lines, short height );
short center_step( void );
short center_advance( float *remaining, float frame_dt );

#endif
