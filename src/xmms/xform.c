/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

/**
 * @file
 * xforms
 */

#include <string.h>

#include "xmmspriv/xmms_plugin.h"
#include "xmmspriv/xmms_xform.h"
#include "xmmspriv/xmms_streamtype.h"
#include "xmmspriv/xmms_medialib.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_object.h"


struct xmms_xform_St {
	xmms_object_t obj;
	struct xmms_xform_St *prev;

	const xmms_xform_plugin_t *plugin;
	xmms_medialib_entry_t entry;

	void *priv;

	xmms_stream_type_t *out_type;

	GList *goal_hints;

	gboolean eos;
	gboolean error;

	char *buffer;
	gint buffered;
	gint buffersize;

	GHashTable *metadata;
};

#define READ_CHUNK 4096




struct xmms_xform_plugin_St {
	xmms_plugin_t plugin;

	xmms_xform_methods_t methods;

	GList *in_types;
};

const char *xmms_xform_shortname (xmms_xform_t *xform);

static void
xmms_xform_destroy (xmms_object_t *object)
{
	xmms_xform_t *xform = (xmms_xform_t *)object;

	XMMS_DBG ("Freeing xform '%s'", xmms_xform_shortname (xform));

	if (xform->plugin) {
		xform->plugin->methods.destroy (xform);
	}

	g_hash_table_destroy (xform->metadata);

	g_free (xform->buffer);

	xmms_object_unref (xform->out_type);

	if (xform->prev) {
		xmms_object_unref (xform->prev);
	}

}


xmms_xform_t *
xmms_xform_new (xmms_xform_plugin_t *plugin, xmms_xform_t *prev, xmms_medialib_entry_t entry, GList *goal_hints)
{
	xmms_xform_t *xform;

	xform = xmms_object_new (xmms_xform_t, xmms_xform_destroy);

	xform->plugin = plugin;
	xform->entry = entry;
	xform->goal_hints = goal_hints;

	if (prev) {
		xmms_object_ref (prev);
		xform->prev = prev;
	}

	xform->metadata = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                         g_free, xmms_object_cmd_value_free);

	if (plugin) {
		if (!plugin->methods.init (xform)) {
			if (prev) {
				xmms_object_unref (prev);
			}
			return NULL;
		}
		g_return_val_if_fail (xform->out_type, NULL);
	}

	xform->buffer = g_malloc (READ_CHUNK);
	xform->buffersize = READ_CHUNK;


	return xform;
}

xmms_medialib_entry_t
xmms_xform_entry_get (xmms_xform_t *xform)
{
	return xform->entry;
}

gpointer
xmms_xform_private_data_get (xmms_xform_t *xform)
{
	return xform->priv;
}

void
xmms_xform_private_data_set (xmms_xform_t *xform, gpointer data)
{
	xform->priv = data;
}

void
xmms_xform_outdata_type_add (xmms_xform_t *xform, ...)
{
	va_list ap;
	va_start (ap, xform);
	xform->out_type = xmms_stream_type_parse (ap);
	va_end (ap);
}

void
xmms_xform_outdata_type_set (xmms_xform_t *xform, xmms_stream_type_t *type)
{
	xmms_object_ref (type);
	xform->out_type = type;
}

void
xmms_xform_outdata_type_copy (xmms_xform_t *xform)
{
	xmms_object_ref (xform->prev->out_type);
	xform->out_type = xform->prev->out_type;
}

const char *
xmms_xform_indata_get_str (xmms_xform_t *xform, xmms_stream_type_key_t key)
{
	return xmms_stream_type_get_str (xform->prev->out_type, key);
}

gint
xmms_xform_indata_get_int (xmms_xform_t *xform, xmms_stream_type_key_t key)
{
	return xmms_stream_type_get_int (xform->prev->out_type, key);
}

xmms_stream_type_t *
xmms_xform_outtype_get (xmms_xform_t *xform)
{
	return xform->out_type;
}

xmms_stream_type_t *
xmms_xform_intype_get (xmms_xform_t *xform)
{
	return xmms_xform_outtype_get (xform->prev);
}



const char *
xmms_xform_outtype_get_str (xmms_xform_t *xform, xmms_stream_type_key_t key)
{
	return xmms_stream_type_get_str (xform->out_type, key);
}

gint
xmms_xform_outtype_get_int (xmms_xform_t *xform, xmms_stream_type_key_t key)
{
	return xmms_stream_type_get_int (xform->out_type, key);
}


void
xmms_xform_metadata_set_int (xmms_xform_t *xform, const char *key, int val)
{
	XMMS_DBG ("Setting '%s' to %d", key, val);
	g_hash_table_insert (xform->metadata, g_strdup (key), xmms_object_cmd_value_int_new (val));
}

void
xmms_xform_metadata_set_str (xmms_xform_t *xform, const char *key, const char *val)
{
	XMMS_DBG ("Setting '%s' to '%s'", key, val);
	g_hash_table_insert (xform->metadata, g_strdup (key), xmms_object_cmd_value_str_new (val));
}

static const xmms_object_cmd_value_t *
xmms_xform_metadata_get_val (xmms_xform_t *xform, const char *key)
{
	xmms_object_cmd_value_t *val;

	for (; xform; xform = xform->prev) {
		val = g_hash_table_lookup (xform->metadata, key);
		if (val)
			return val;
	}
	return NULL;
}

gint32
xmms_xform_metadata_get_int (xmms_xform_t *xform, const char *key)
{
	const xmms_object_cmd_value_t *val;

	val = xmms_xform_metadata_get_val (xform, key);
	if (val && val->type == XMMS_OBJECT_CMD_ARG_INT32) {
		return val->value.int32;
	} else {
		return -1;
	}
}

const gchar *
xmms_xform_metadata_get_str (xmms_xform_t *xform, const char *key)
{
	const xmms_object_cmd_value_t *val;

	val = xmms_xform_metadata_get_val (xform, key);
	if (val && val->type == XMMS_OBJECT_CMD_ARG_STRING) {
		return val->value.string;
	} else {
		return NULL;
	}
}

typedef struct {
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry;
	guint32 source;
} metadata_festate_t;

void
add_metadatum (gpointer _key, gpointer _value, gpointer user_data)
{
	xmms_object_cmd_value_t *value = (xmms_object_cmd_value_t *)_value;
	gchar *key = (gchar *)_key;
	metadata_festate_t *st = (metadata_festate_t *)user_data;
	
	if (value->type == XMMS_OBJECT_CMD_ARG_STRING) {
		xmms_medialib_entry_property_set_str_source (st->session, st->entry, key, value->value.string, st->source);
		XMMS_DBG ("setting property %s='%s'", key, value->value.string);
	} else if (value->type == XMMS_OBJECT_CMD_ARG_INT32) {
		xmms_medialib_entry_property_set_int_source (st->session, st->entry, key, value->value.int32, st->source);
		XMMS_DBG ("setting property %s=%d", key, value->value.int32);
	} else {
		XMMS_DBG ("Unknown type?!?");
	}
}

void
xmms_xform_metadata_collect (xmms_xform_t *start)
{
	metadata_festate_t info;
	xmms_xform_t *xform;
	gchar *src;

	info.entry = start->entry;
	info.session = xmms_medialib_begin_write ();

	for (xform = start; xform; xform = xform->prev) {
		XMMS_DBG ("Collecting medadata from %s", xmms_xform_shortname (xform));
		src = g_strdup_printf ("plugins/%s", xmms_xform_shortname (xform));
		info.source = xmms_medialib_source_to_id (info.session, src);
		g_hash_table_foreach (xform->metadata, add_metadatum, &info);
		g_free (src);
	}
	xmms_medialib_end (info.session);
}

const char *
xmms_xform_shortname (xmms_xform_t *xform)
{
	return xmms_plugin_shortname_get ((xmms_plugin_t *)xform->plugin);
}

gint
xmms_xform_this_peek (xmms_xform_t *xform, gpointer buf, gint siz, xmms_error_t *err)
{

	while (xform->buffered < siz) {
		gint res;

		if (xform->buffered + READ_CHUNK > xform->buffersize) {
			xform->buffersize *= 2;
			xform->buffer = g_realloc (xform->buffer, xform->buffersize);
		}

		res = xform->plugin->methods.read (xform, &xform->buffer[xform->buffered], READ_CHUNK, err);
		if (res == 0) {
			xform->eos = TRUE;
			break;
		} else if (res == -1) {
			xform->error = TRUE;
			return -1;
		} else {
			xform->buffered += res;
		}
	}

	/* might have eosed */
	siz = MIN (siz, xform->buffered);
	memcpy (buf, xform->buffer, siz);
	return siz;
}

gint
xmms_xform_this_read (xmms_xform_t *xform, gpointer buf, gint siz, xmms_error_t *err)
{
	gint read = 0;

	if (xform->error) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "Read on errored xform");
		return -1;
	}

	if (xform->eos) {
		return 0;
	}

	if (xform->buffered) {
		read = MIN (siz, xform->buffered);
		memcpy (buf, xform->buffer, read);
		xform->buffered -= read;
		if (xform->buffered) {
			/* unless we are _peek:ing often
			   this should be fine */
			memmove (xform->buffer, &xform->buffer[read], xform->buffered);
		}
	}

	while (read < siz) {
		gint res;

		res = xform->plugin->methods.read (xform, buf + read, siz - read, err);
		if (res == 0) {
			xform->eos = TRUE;
			break;
		} else if (res == -1) {
			xform->error = TRUE;
			return -1;
		} else {
			read += res;
		}
	}

	return read;
}

gint64
xmms_xform_this_seek (xmms_xform_t *xform, gint64 offset, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	gint64 res;

	if (xform->error) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "Seek on errored xform");
		return -1;
	}

	if (!xform->plugin->methods.seek) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "Seek not implemented");
		return -1;
	}

	if (xform->buffered) {
		if (whence == XMMS_XFORM_SEEK_CUR) {
			offset -= xform->buffered;
		}
		xform->buffered = 0;
	}

	res = xform->plugin->methods.seek (xform, offset, whence, err);
	if (res != -1) {
		xform->eos = FALSE;
	}

	return res;
}

gint
xmms_xform_peek (xmms_xform_t *xform, gpointer buf, gint siz, xmms_error_t *err)
{
	g_return_val_if_fail (xform->prev, -1);
	return xmms_xform_this_peek (xform->prev, buf, siz, err);
}

gint
xmms_xform_read (xmms_xform_t *xform, gpointer buf, gint siz, xmms_error_t *err)
{
	g_return_val_if_fail (xform->prev, -1);
	return xmms_xform_this_read (xform->prev, buf, siz, err);
}

gint64
xmms_xform_seek (xmms_xform_t *xform, gint64 offset, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	g_return_val_if_fail (xform->prev, -1);
	return xmms_xform_this_seek (xform->prev, offset, whence, err);
}



static void
xmms_xform_plugin_destroy (xmms_object_t *obj)
{
	xmms_xform_plugin_t *plugin = (xmms_xform_plugin_t *)obj;
	xmms_stream_type_t *in_type;
	GList *n;

	for (n = plugin->in_types; n; n = g_list_next (n)) {
		in_type = (xmms_stream_type_t *)n->data;
		xmms_object_unref (in_type);
	}

	xmms_plugin_destroy ((xmms_plugin_t *)obj);
}

xmms_plugin_t *
xmms_xform_plugin_new (void)
{
	xmms_xform_plugin_t *res;

	res = xmms_object_new (xmms_xform_plugin_t, xmms_xform_plugin_destroy);

	return (xmms_plugin_t *)res;
}

void
xmms_xform_plugin_methods_set (xmms_xform_plugin_t *plugin, xmms_xform_methods_t *methods)
{

	g_return_if_fail (plugin);
	g_return_if_fail (plugin->plugin.type == XMMS_PLUGIN_TYPE_XFORM);

	XMMS_DBG ("Registering xform '%s'", xmms_plugin_shortname_get ((xmms_plugin_t *)plugin));

	memcpy (&plugin->methods, methods, sizeof (xmms_xform_methods_t));
}

gboolean
xmms_xform_plugin_verify (xmms_plugin_t *_plugin)
{
	xmms_xform_plugin_t *plugin = (xmms_xform_plugin_t *)_plugin;

	g_return_val_if_fail (plugin, FALSE);
	g_return_val_if_fail (plugin->plugin.type == XMMS_PLUGIN_TYPE_XFORM, FALSE);

	/* more checks */

	return TRUE;
}

void
xmms_xform_plugin_indata_add (xmms_xform_plugin_t *plugin, ...)
{
	xmms_stream_type_t *t;
	va_list ap;

	va_start (ap, plugin);
	t = xmms_stream_type_parse (ap);
	va_end (ap);
	plugin->in_types = g_list_prepend (plugin->in_types, t);
}


typedef struct match_state_St {
	xmms_xform_plugin_t *match;
	xmms_stream_type_t *out_type;
} match_state_t;

static gboolean
xmms_xform_match (xmms_plugin_t *_plugin, gpointer user_data)
{
	xmms_xform_plugin_t *plugin = (xmms_xform_plugin_t *)_plugin;
	match_state_t *state = (match_state_t *)user_data;
	GList *t;

	g_assert (_plugin->type == XMMS_PLUGIN_TYPE_XFORM);
	g_assert (!state->match);

	if (!plugin->in_types) {
		XMMS_DBG ("Skipping plugin '%s'", xmms_plugin_shortname_get (_plugin));
		return TRUE;
	}

	XMMS_DBG ("Trying plugin '%s'", xmms_plugin_shortname_get (_plugin));

	for (t = plugin->in_types; t; t = g_list_next (t)) {
		if (xmms_stream_type_match (t->data, state->out_type)) {
			XMMS_DBG ("Plugin '%s' matched",  xmms_plugin_shortname_get (_plugin));
			state->match = plugin;
			return FALSE;
		}
	}
	return TRUE;
}

xmms_xform_t *
xmms_xform_find (xmms_xform_t *prev, xmms_medialib_entry_t entry, GList *goal_hints)
{
	match_state_t state;
	xmms_xform_t *xform = NULL;

	state.out_type = prev->out_type;
	state.match = NULL;

	xmms_plugin_foreach (XMMS_PLUGIN_TYPE_XFORM, xmms_xform_match, &state);
	
	if (state.match) {
		xform = xmms_xform_new (state.match, prev, entry, goal_hints);
	} else {
		XMMS_DBG ("Found no matching plugin...");
	}

	return xform;
}

gboolean
xmms_xform_iseos (xmms_xform_t *xform)
{
	return xform->eos;
}

const xmms_stream_type_t *
xmms_xform_get_out_stream_type (xmms_xform_t *xform)
{
	return xform->out_type;
}

const GList *
xmms_xform_goal_hints_get (xmms_xform_t *xform)
{
	return xform->goal_hints;
}


static gboolean
has_goalformat (xmms_xform_t *xform, GList *goal_formats)
{
	GList *n;

	for (n = goal_formats; n; n = g_list_next (n)) {
		xmms_stream_type_t *goal_type = n->data;
		if (xmms_stream_type_match (goal_type, xmms_xform_get_out_stream_type (xform))) {
			return TRUE;
		}

	}
	XMMS_DBG ("Not in one of %d goal-types", g_list_length (goal_formats));
	return FALSE;
}

xmms_xform_t *
xmms_xform_chain_setup (xmms_medialib_entry_t entry, GList *goal_formats)
{
	xmms_medialib_session_t *session;
	xmms_xform_t *xform, *last;
	const gchar *url;
	gchar *durl;
	
	xform = xmms_xform_new (NULL, NULL, entry, goal_formats);

	session = xmms_medialib_begin ();
	url = xmms_medialib_entry_property_get_str (session, entry, XMMS_MEDIALIB_ENTRY_PROPERTY_URL);
	xmms_medialib_end (session);

	durl = g_strdup (url);
	xmms_medialib_decode_url (durl);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/x-url",
	                             XMMS_STREAM_TYPE_URL,
	                             durl,
	                             XMMS_STREAM_TYPE_END);

	g_free (durl);

	last = xform;

	do {
		xform = xmms_xform_find (last, entry, goal_formats);
		if (!xform) {
			XMMS_DBG ("Couldn't set up chain!");
			xmms_object_unref (last);
			return NULL;
		}
		xmms_object_unref (last);
		last = xform;
	} while (!has_goalformat (xform, goal_formats));

	/* add a buffer */
/*
	last = xmms_ringbuf_xform_new (xform, entry);

	xmms_object_unref (xform);
*/
	/* add effect-xforms here */

	xmms_xform_metadata_collect (last);

	XMMS_DBG ("Goaltype found!! \\o/");

	return last;
}
