#ifndef __PLAYLIST_H_
#define __PLAYLIST_H_

#include <glib.h>
#include <db.h>

#define XMMS_PL_PROPERTY 128

#define XMMS_PL_APPEND 1
#define XMMS_PL_PREPEND 2
#define XMMS_PL_INSERT 3

#define XMMS_MAX_URI_LEN 1024
#define XMMS_MEDIA_DATA_LEN 1024

typedef struct xmms_playlist_St {
	gchar *path;
	DB *dbp;
} xmms_playlist_t;

typedef struct xmms_playlist_entry_St {
	gchar uri[XMMS_MAX_URI_LEN];
	gint id;

	gint type; /* XMMS_MEDIA_TYPE_XXX */

	/* this contains data that the plugin decides,
	 * like bitrate, uri things like that */
	gchar mediadata[XMMS_MEDIA_DATA_LEN];

	/* ID3-like properties */
	gchar artist[XMMS_PL_PROPERTY];
	gchar album[XMMS_PL_PROPERTY];
	gchar title[XMMS_PL_PROPERTY];
	gchar comment[XMMS_PL_PROPERTY];
	gint genre;
	gint year;
	gint tracknr;

} xmms_playlist_entry_t;


void xmms_playlist_db3_err (const char *errpfx, char *msg);
gboolean xmms_playlist_add (xmms_playlist_t *playlist, xmms_playlist_entry_t *file, gint options);
xmms_playlist_t * xmms_playlist_open_create (gchar *path);

#endif
