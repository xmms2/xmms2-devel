#ifndef __XMMS_CORE_H__
#define __XMMS_CORE_H__

#include "xmms/object.h"
#include "xmms/output.h"
#include "xmms/decoder.h"

typedef struct xmms_core_St {
	xmms_object_t object;

	xmms_output_t *output;
	xmms_decoder_t *decoder;

	xmms_playlist_t *playlist;
	xmms_playlist_entry_t *curr_song;
} xmms_core_t;

extern xmms_core_t *core;


void xmms_core_output_set (xmms_output_t *output);

void xmms_core_playtime_set (guint time);
void xmms_core_play_next ();
void xmms_core_playlist_adduri (gchar *nuri);
void xmms_core_playlist_jump (guint id);
void xmms_core_playlist_remove (guint id);
void xmms_core_playlist_shuffle ();
xmms_playlist_t * xmms_core_get_playlist ();

void xmms_core_quit ();

void xmms_core_set_mediainfo (xmms_playlist_entry_t *entry);
void xmms_core_set_playlist (xmms_playlist_t *playlist);
gboolean xmms_core_get_mediainfo (xmms_playlist_entry_t *entry);
xmms_playlist_entry_t *xmms_core_playlist_entry_mediainfo (guint id);
gchar *xmms_core_get_uri ();
gint xmms_core_get_id ();

void xmms_core_init ();
void xmms_core_start ();

#endif
