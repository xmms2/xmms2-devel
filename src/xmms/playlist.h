#ifndef __XMMS_PLAYLIST_H__
#define __XMMS_PLAYLIST_H__

#include <glib.h>

#include "xmms/object.h"
#include "xmms/playlist_entry.h"

/*
 * Public definitions
 */

typedef enum {
	XMMS_PLAYLIST_APPEND,
	XMMS_PLAYLIST_PREPEND,
	XMMS_PLAYLIST_INSERT,
	XMMS_PLAYLIST_DELETE
} xmms_playlist_actions_t;

typedef enum {
	XMMS_PLAYLIST_CHANGED_ADD,
	XMMS_PLAYLIST_CHANGED_SET_POS,
	XMMS_PLAYLIST_CHANGED_SHUFFLE,
	XMMS_PLAYLIST_CHANGED_REMOVE,
	XMMS_PLAYLIST_CHANGED_CLEAR,
	XMMS_PLAYLIST_CHANGED_MOVE
} xmms_playlist_changed_actions_t;


/*
 * Private defintions
 */

#define XMMS_MAX_URI_LEN 1024
#define XMMS_MEDIA_DATA_LEN 1024


struct xmms_playlist_St;
typedef struct xmms_playlist_St xmms_playlist_t;

typedef struct xmms_playlist_changed_msg_St {
	gint type;
	guint id;
	gpointer arg;
} xmms_playlist_changed_msg_t;

/*
 * Public functions
 */

xmms_playlist_t * xmms_playlist_init ();

gboolean xmms_playlist_add (xmms_playlist_t *playlist, xmms_playlist_entry_t *file, gint options);
guint xmms_playlist_entries_total (xmms_playlist_t *playlist);
guint xmms_playlist_entries_left (xmms_playlist_t *playlist);
gboolean xmms_playlist_set_current_position (xmms_playlist_t *playlist, guint id);
gint xmms_playlist_get_current_position (xmms_playlist_t *playlist);
xmms_playlist_entry_t * xmms_playlist_get_byid (xmms_playlist_t *playlist, guint id);
xmms_playlist_entry_t * xmms_playlist_get_next_entry (xmms_playlist_t *playlist);
xmms_playlist_entry_t * xmms_playlist_get_prev_entry (xmms_playlist_t *playlist);
xmms_playlist_entry_t * xmms_playlist_get_current_entry (xmms_playlist_t *playlist);
GList * xmms_playlist_list (xmms_playlist_t *playlist);
void xmms_playlist_wait (xmms_playlist_t *playlist);
void xmms_playlist_sort (xmms_playlist_t *playlist, gchar *property);
void xmms_playlist_shuffle (xmms_playlist_t *playlist);
void xmms_playlist_clear (xmms_playlist_t *playlist);
gboolean xmms_playlist_id_remove (xmms_playlist_t *playlist, guint id);

xmms_playlist_entry_t *xmms_playlist_entry_alloc ();

/*
 * Entry modifications
 */


#endif
