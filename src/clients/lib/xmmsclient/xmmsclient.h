#ifndef __XMMS_CLIENT_H__
#define __XMMS_CLIENT_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xmmsc_connection_St xmmsc_connection_t;
typedef struct xmmsc_playlist_entry_St { /* no use to share them between client and core */
	gchar *url;
	guint id;
	GHashTable *properties;
} xmmsc_playlist_entry_t;

typedef struct xmmsc_file_St {
	gchar *path;
	gboolean file; /**< set to true if entry is a regular file */
} xmmsc_file_t;

xmmsc_connection_t *xmmsc_init();
gboolean xmmsc_connect (xmmsc_connection_t *, const char *);
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
void xmmsc_playlist_save (xmmsc_connection_t *c, gchar *filename);
void xmmsc_playlist_entry_free (GHashTable *entry);
void xmmsc_playback_stop (xmmsc_connection_t *c);
void xmmsc_playback_start (xmmsc_connection_t *c);
void xmmsc_playback_pause (xmmsc_connection_t *c);
void xmmsc_playback_seek_ms (xmmsc_connection_t *c, guint milliseconds);
void xmmsc_playback_seek_samples (xmmsc_connection_t *c, guint samples);
void xmmsc_playback_current_id (xmmsc_connection_t *c);
void xmmsc_playlist_list (xmmsc_connection_t *c);
void xmmsc_playlist_get_mediainfo (xmmsc_connection_t *, guint);
void xmmsc_playlist_sort (xmmsc_connection_t *c, char *property);
void xmmsc_configval_set (xmmsc_connection_t *c, gchar *key, gchar *val);
void xmmsc_file_list (xmmsc_connection_t *c, gchar *path);

void xmmsc_set_callback (xmmsc_connection_t *, gchar *, void (*)(void *,void*), void *);

void xmmsc_glib_setup_mainloop (xmmsc_connection_t *, GMainContext *);

/* sync */
int xmmscs_playback_current_id (xmmsc_connection_t *c);
GHashTable *xmmscs_playlist_get_mediainfo (xmmsc_connection_t *c, guint id);


#ifdef __cplusplus
}
#endif

#endif
