/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
 * 
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 * 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *                   
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */





#include <glib.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#include "xmms/object.h"
#include "xmms/playlist.h"
#include "xmms/playlist_entry.h"
#include "xmms/util.h"
#include "xmms/signal_xmms.h"
#include "internal/transport_int.h"
#include "internal/decoder_int.h"

/*
 * Type definitions.
 */

/** @defgroup PlaylistEntry PlaylistEntry
  * @ingroup Playlist
  * @{
  */
	
static void xmms_playlist_entry_destroy (xmms_object_t *object);

struct xmms_playlist_entry_St {
	xmms_object_t object;
	gchar *url;
	gchar *mimetype;
	
	guint id;
	
	guint ref;

	GHashTable *properties;
};

/*
 * Public functions
 */

/**
  * Allocate a new #xmms_playlist_entry_t
  * @param url create the playlist_entry with a set url, it could also be set NULL
  */

xmms_playlist_entry_t *
xmms_playlist_entry_new (gchar *url)
{
	xmms_playlist_entry_t *ret;

	ret = xmms_object_new (xmms_playlist_entry_t, xmms_playlist_entry_destroy);
	ret->url = g_strdup (url);
	ret->properties = g_hash_table_new (g_str_hash, g_str_equal);
	ret->id = 0;

	return ret;
}

/**
  * Retrieve the ID number for this playlist entry.
  */

guint
xmms_playlist_entry_id_get (xmms_playlist_entry_t *entry)
{
	g_return_val_if_fail (entry, 0);
	return entry->id;
}

/**
  * Set the ID number for this playlist entry.
  */

void
xmms_playlist_entry_id_set (xmms_playlist_entry_t *entry, guint id)
{
	g_return_if_fail (entry);
	entry->id = id;
}

static void
xmms_playlist_entry_clone_foreach (gpointer key, gpointer value, gpointer udata)
{
	xmms_playlist_entry_t *new = (xmms_playlist_entry_t *) udata;

	xmms_playlist_entry_property_set (new, (gchar *)key, (gchar *)value);
}

/** 
  * Make a copy of all properties in entry to new
  */

void
xmms_playlist_entry_property_copy (xmms_playlist_entry_t *entry, 
		                   xmms_playlist_entry_t *newentry)
{
	g_return_if_fail (entry);
	g_return_if_fail (newentry);

	g_hash_table_foreach (entry->properties, xmms_playlist_entry_clone_foreach, (gpointer) newentry);

}

/**
 * Associates a value/key pair with a entry.
 * 
 * It copies the value/key and you'll need to call #xmms_playlist_entry_free
 * to free all strings.
 */

void
xmms_playlist_entry_property_set (xmms_playlist_entry_t *entry, gchar *key, gchar *value)
{
	g_return_if_fail (entry);
	g_return_if_fail (key);
	g_return_if_fail (value);

	g_hash_table_insert (entry->properties, g_ascii_strdown (key, strlen (key)), g_strdup (value));
	
}

/**
  * Set the URL for this playlist entry
  */

void
xmms_playlist_entry_url_set (xmms_playlist_entry_t *entry, gchar *url)
{
	g_return_if_fail (entry);
	g_return_if_fail (url);

	entry->url = g_strdup (url);
}

/**
  * Set the URL for this playlist entry
  */

const gchar *
xmms_playlist_entry_url_get (const xmms_playlist_entry_t *entry)
{
	g_return_val_if_fail (entry, NULL);

	return entry->url;
}

/**
  * Call #func for each property in the playlist entry.
  */

void
xmms_playlist_entry_property_foreach (xmms_playlist_entry_t *entry,
				      GHFunc func,
				      gpointer userdata)
{

	g_return_if_fail (entry);
	g_return_if_fail (func);

	g_hash_table_foreach (entry->properties, func, userdata);

}

/**
  * Get a value for a property in the playlist entry as a string.
  */

const gchar *
xmms_playlist_entry_property_get (const xmms_playlist_entry_t *entry, gchar *key)
{
	g_return_val_if_fail (entry, NULL);
	g_return_val_if_fail (key, NULL);

	return g_hash_table_lookup (entry->properties, key);
}

/**
  * Get a value for a property in the playlist entry as a int
  */

gint
xmms_playlist_entry_property_get_int (const xmms_playlist_entry_t *entry, gchar *key)
{
	gchar *t;
	g_return_val_if_fail (entry, -1);
	g_return_val_if_fail (key, -1);

	t = g_hash_table_lookup (entry->properties, key);

	if (t)
		return strtol (t, NULL, 10);
	else
		return 0;
}


static gboolean
xmms_playlist_entry_foreach_free (gpointer key, gpointer value, gpointer udata)
{
	g_free (key);
	g_free (value);
	return TRUE;
}

static void
xmms_playlist_entry_destroy (xmms_object_t *object)
{
	xmms_playlist_entry_t *entry = (xmms_playlist_entry_t *)object;

	if (entry->url)
		g_free (entry->url);
	
	if (entry->mimetype)
		g_free (entry->mimetype);

	g_hash_table_foreach_remove (entry->properties, xmms_playlist_entry_foreach_free, NULL);
	g_hash_table_destroy (entry->properties);
}

static gchar *wellknowns[] = {
	XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST,
	XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM,
	XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE,
	XMMS_PLAYLIST_ENTRY_PROPERTY_YEAR,
	XMMS_PLAYLIST_ENTRY_PROPERTY_TRACKNR,
	XMMS_PLAYLIST_ENTRY_PROPERTY_GENRE,
	XMMS_PLAYLIST_ENTRY_PROPERTY_BITRATE,
	XMMS_PLAYLIST_ENTRY_PROPERTY_COMMENT,
	XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION,
	XMMS_PLAYLIST_ENTRY_PROPERTY_CHANNEL,
	XMMS_PLAYLIST_ENTRY_PROPERTY_SAMPLERATE,
	NULL 
};

gboolean
xmms_playlist_entry_is_wellknown (gchar *property)
{
	gint i = 0;

	while (wellknowns[i]) {
		if (g_strcasecmp (wellknowns[i], property) == 0)
			return TRUE;

		i++;
	}

	return FALSE;
}

/**
  * Set the mimetype for this entry.
  */

void
xmms_playlist_entry_mimetype_set (xmms_playlist_entry_t *entry, const gchar *mimetype)
{

	g_return_if_fail (entry);
	g_return_if_fail (mimetype);

	entry->mimetype = g_strdup (mimetype);

}

/**
  * Get the mimetype for this entry
  */

const gchar *
xmms_playlist_entry_mimetype_get (xmms_playlist_entry_t *entry)
{
	g_return_val_if_fail (entry, NULL);

	return entry->mimetype;
}

/**
 * Tell all clients that this entry has been changed
 */

void
xmms_playlist_entry_changed (xmms_playlist_t *playlist, xmms_playlist_entry_t *entry)
{
	g_return_if_fail (playlist);
	g_return_if_fail (entry);


	XMMS_DBG ("info for %d updated", entry->id);
	
	xmms_object_emit_f (XMMS_OBJECT (playlist), 
			    XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID,
			    XMMS_OBJECT_METHOD_ARG_UINT32,
			    entry->id);

}


/**
  * Start a new decoder for this entry.
  */

xmms_decoder_t *
xmms_playlist_entry_start (xmms_playlist_entry_t *entry)
{
	xmms_transport_t *t;
	xmms_decoder_t *d;
	const gchar *mime;

	XMMS_DBG ("Starting up for %s", xmms_playlist_entry_url_get (entry));

	t = xmms_transport_new ();

	if (!t) {
		return NULL;
	}

	if (!xmms_transport_open (t, entry)) {
		xmms_transport_close (t);
		xmms_object_unref (t);
		return NULL;
	}

	xmms_transport_start (t);

	/*
	 * Waiting for the mimetype forever
	 * All transporters MUST set a mimetype, 
	 * NULL on error
	 */
	XMMS_DBG ("Waiting for mimetype");
	mime = xmms_transport_mimetype_get_wait (t);
	if (!mime) {
		xmms_transport_close (t);
		xmms_object_unref (t);
		return NULL;
	}

	XMMS_DBG ("mime-type: %s", mime);

	xmms_playlist_entry_mimetype_set (entry, mime);

	d = xmms_decoder_new ();

	if (!d) {
		xmms_transport_close (t);
		xmms_object_unref (t);
		return NULL;
	}

	if (!xmms_decoder_open (d, t)) {
		xmms_transport_close (t);
		xmms_object_unref (t);
		xmms_object_unref (d);
		return NULL;
	}

	xmms_object_unref (t);

	return d;
	
}


/** @} */
