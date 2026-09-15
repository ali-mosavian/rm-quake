/* idref.c -- id's own entity-text code, from WinQuake (GPL): COM_Parse (common.c), the loop of
   ED_NewString and the vector case of ED_ParseEpair (pr_edict.c). Verbatim but for the buffers the
   hunk used to give, so ed_test.c compares ed.c against id's code and not against a reading of it. */
#include <stdlib.h>
#include <string.h>

char com_token[1024];

char *COM_Parse (char *data)
{
	int             c;
	int             len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}

// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':')
			break;
	} while (c>32);

	com_token[len] = 0;
	return data;
}

/* ED_NewString's loop, into new (Hunk_Alloc's memory is zeroed) */
void idref_newstring (char *string, char *new)
{
	char	*new_p;
	int		i,l;

	l = strlen(string) + 1;
	memset (new, 0, l + 1);
	new_p = new;

	for (i=0 ; i< l ; i++)
	{
		if (string[i] == '\\' && i < l-1)
		{
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		}
		else
			*new_p++ = string[i];
	}
}

/* ED_ParseEpair's ev_vector; string zeroed so a short value reads zeros, not the stack */
void idref_vector (char *s, float *d)
{
	int		i;
	char	string[128];
	char	*v, *w;

	memset (string, 0, sizeof string);
	strcpy (string, s);
	v = string;
	w = string;
	for (i=0 ; i<3 ; i++)
	{
		while (*v && *v != ' ')
			v++;
		*v = 0;
		d[i] = atof (w);
		w = v = v+1;
	}
}
