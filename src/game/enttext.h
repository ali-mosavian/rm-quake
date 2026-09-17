#ifndef __ENTTEXT_H__
#define __ENTTEXT_H__

#ifdef __BORLANDC__
#define ENTTEXT_FAR far
#else
#define ENTTEXT_FAR
#endif

void ent_copy_map( char *dst, const char ENTTEXT_FAR *src );

#endif
