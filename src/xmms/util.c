#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

gchar *xmms_util_decode_path (const gchar *path){
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
