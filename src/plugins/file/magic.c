#include <glib.h>
#include <string.h>
#include "xmms/util.h"

typedef struct xmms_mimemap_St {
	gchar *ext;
	gchar *mime;
} xmms_mimemap_t;

const xmms_mimemap_t mimemap[] = {
	{ "mp3", "audio/mpeg" },
	{ "ogg", "application/ogg" },
	{ "wav", "audio/wave" },
	{ NULL, NULL }
};

const gchar *
xmms_magic_mime_from_file (const gchar *file)
{
	gchar *p;
	gint i = 0;

	g_return_val_if_fail (file, NULL);

	p = strrchr (file, '.');
	g_return_val_if_fail (p, NULL);

	*p++;

	while (mimemap[i].ext) {
		if (g_strcasecmp (mimemap[i].ext, p) == 0) {
			XMMS_DBG ("Mimetype acording to ext: %s", mimemap[i].mime);
			return mimemap[i].mime;
		}
		i++;
	}

	return NULL;

}

