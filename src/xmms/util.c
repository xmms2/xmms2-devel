/**
 * @file Misc utils for various functions in XMMS.
 */

#include "xmms/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>

/** Returns TRUE if the char is regular. ie, not to be encoded. */
#define _REGULARCHAR(a) ((a>=65 && a<=90) || (a>=97 && a<=122)) || (isdigit (a))

/**
 * Decodes a path encoded string. We use ordinary URL encoding from
 * HTTP/1.1 standard.
 *
 * @returns a allocated string that needs to be freed with g_free.
 */

gchar *
xmms_util_decode_path (const gchar *path)
{
	gchar *qstr;
	gchar tmp[3];
	gint c1, c2;

	c1 = c2 = 0;

	qstr = (gchar *)g_malloc0 (strlen (path) + 1);

	tmp[2] = '\0';
	while (path[c1] != '\0'){
		if (path[c1] == '%'){
			gint l;
			tmp[0] = path[c1+1];
			tmp[1] = path[c1+2];
			l = strtol(tmp,NULL,16);
			if (l!=0){
				qstr[c2] = (gchar)l;
				c1+=2;
			} else {
				qstr[c2] = path[c1];
			}
		} else if (path[c1] == '+') {
			qstr[c2] = ' ';
		} else {
			qstr[c2] = path[c1];
		}
		c1++;
		c2++;
	}
	qstr[c2] = path[c1];

	return qstr;
}

/**
 * Encodes a string with URL endcoding.
 *
 * @returns a allocated string that needs to be freed with g_free
 */

gchar *
xmms_util_encode_path (gchar *path) 
{
	gchar *out, *outreal;
	gint i;
	gint len;

	len = strlen (path);
	outreal = out = (gchar *)g_malloc0 (len * 3 + 1);

	for ( i = 0; i < len; i++) {
		if (path[i] == '/' || 
			_REGULARCHAR ((gint) path[i]) || 
			path[i] == '_' ||
			path[i] == '-' ){
			*(out++) = path[i];
		} else if (path[i] == ' '){
			*(out++) = '+';
		} else {
			g_snprintf (out, 4, "%%%02x", (guchar) path[i]);
			out += 3;
		}
	}

	return outreal;
}
