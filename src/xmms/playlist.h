#ifndef __PLAYLIST_H_
#define __PLAYLIST_H_

#include <glib.h>

/*
 * Public definitions
 */

#define XMMS_PL_APPEND 1
#define XMMS_PL_PREPEND 2
#define XMMS_PL_INSERT 3

/*
 * Private defintions
 */

#define XMMS_MAX_URI_LEN 1024
#define XMMS_MEDIA_DATA_LEN 1024
#define XMMS_PL_PROPERTY 128

typedef struct xmms_playlist_St {
	GSList *list;
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
	gchar genre[XMMS_PL_PROPERTY];
	gint year;
	gint tracknr;
	guint bitrate;
	guint duration; /* in seconds */

} xmms_playlist_entry_t;


/*
 * Public functions
 */

xmms_playlist_t * xmms_playlist_init ();

gboolean xmms_playlist_add (xmms_playlist_t *playlist, xmms_playlist_entry_t *file, gint options);
guint xmms_playlist_entries (xmms_playlist_t *playlist);

/*
 * Entry modifications
 */

xmms_playlist_entry_t * xmms_playlist_entry_new (gchar *uri);
xmms_playlist_entry_t * xmms_playlist_pop (xmms_playlist_t *playlist);
void xmms_playlist_entry_free (xmms_playlist_entry_t *entry);



#endif
