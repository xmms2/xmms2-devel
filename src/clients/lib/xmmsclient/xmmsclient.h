#ifndef __XMMS_CLIENT_H__
#define __XMMS_CLIENT_H__

#include <glib.h>

typedef struct xmmsc_connection_St xmmsc_connection_t;
typedef struct xmmsc_playlist_entry_St { /* no use to share them between client and core */
	gchar *url;
	guint id;
	GHashTable *properties;
} xmmsc_playlist_entry_t;

xmmsc_connection_t *xmmsc_init();
gboolean xmmsc_connect (xmmsc_connection_t *);
void xmmsc_deinit(xmmsc_connection_t *);

gchar *xmmsc_get_last_error (xmmsc_connection_t *c);
gchar *xmmsc_encode_path (gchar *path);
gchar *xmmsc_decode_path (const gchar *path);

void xmmsc_quit(xmmsc_connection_t *);
void xmmsc_play_next(xmmsc_connection_t *);
void xmmsc_play_prev(xmmsc_connection_t *);
void xmmsc_playlist_shuffle(xmmsc_connection_t *);
void xmmsc_playlist_jump (xmmsc_connection_t *, guint);
void xmmsc_playlist_add (xmmsc_connection_t *, char *);
void xmmsc_playlist_remove (xmmsc_connection_t *, guint);
void xmmsc_playlist_clear (xmmsc_connection_t *c);
void xmmsc_playlist_entry_free (GHashTable *entry);
void xmmsc_playback_stop (xmmsc_connection_t *c);
void xmmsc_playback_start (xmmsc_connection_t *c);
GList *xmmsc_playlist_list (xmmsc_connection_t *);
guint xmmsc_get_playing_id (xmmsc_connection_t *);
GHashTable *xmmsc_playlist_get_mediainfo (xmmsc_connection_t *, guint);

void xmmsc_set_callback (xmmsc_connection_t *, gchar *, void (*)(void *,void*), void *);

void xmmsc_glib_setup_mainloop (xmmsc_connection_t *, GMainContext *);

#endif
