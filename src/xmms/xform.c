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

	gboolean metadata_changed;
	GHashTable *metadata;
};

#define READ_CHUNK 4096




struct xmms_xform_plugin_St {
	xmms_plugin_t plugin;

	xmms_xform_methods_t methods;

	GList *in_types;
};

const char *xmms_xform_shortname (xmms_xform_t *xform);

static xmms_xform_t *add_effects (xmms_xform_t *last, xmms_medialib_entry_t entry,
                                  GList *goal_formats);

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
	xform->metadata_changed = TRUE;
}

void
xmms_xform_metadata_set_str (xmms_xform_t *xform, const char *key, const char *val)
{
	const char *old;

	old = xmms_xform_metadata_get_str (xform, key);

	if (old && strcmp (old, val) == 0)
		return;

	XMMS_DBG ("Setting '%s' to '%s'", key, val);
	g_hash_table_insert (xform->metadata, g_strdup (key), xmms_object_cmd_value_str_new (val));
	xform->metadata_changed = TRUE;
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

gboolean
xmms_xform_metadata_has_val (xmms_xform_t *xform, const gchar *key)
{
	return !!xmms_xform_metadata_get_val (xform, key);
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

static void
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

static void
xmms_xform_metadata_collect_one (xmms_xform_t *xform, metadata_festate_t *info)
{
	gchar *src;

	XMMS_DBG ("Collecting medadata from %s", xmms_xform_shortname (xform));
	src = g_strdup_printf ("plugins/%s", xmms_xform_shortname (xform));
	info->source = xmms_medialib_source_to_id (info->session, src);
	g_hash_table_foreach (xform->metadata, add_metadatum, info);
	g_free (src);
	xform->metadata_changed = FALSE;
}

static void
xmms_xform_metadata_collect (xmms_xform_t *start)
{
	metadata_festate_t info;
	xmms_xform_t *xform;

	info.entry = start->entry;
	info.session = xmms_medialib_begin_write ();

	xmms_medialib_entry_cleanup (info.session, info.entry);

	for (xform = start; xform->prev; xform = xform->prev) {
		if (xform->metadata_changed)
			xmms_xform_metadata_collect_one (xform, &info);
	}
	xmms_medialib_end (info.session);
	xmms_medialib_entry_send_update (info.entry);
}

static void
xmms_xform_metadata_update (xmms_xform_t *xform)
{
	metadata_festate_t info;

	info.entry = xform->entry;
	info.session = xmms_medialib_begin_write ();

	xmms_xform_metadata_collect_one (xform, &info);

	xmms_medialib_end (info.session);
	xmms_medialib_entry_send_update (info.entry);
}

const char *
xmms_xform_shortname (xmms_xform_t *xform)
{
	return (xform->plugin)
	       ? xmms_plugin_shortname_get ((xmms_plugin_t *) xform->plugin)
	       : "unknown";
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
		if (xform->metadata_changed)
			xmms_xform_metadata_update (xform);

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
		XMMS_DBG ("Seek not implemented in '%s'", xmms_xform_shortname (xform));
		xmms_error_set (err, XMMS_ERROR_GENERIC, "Seek not implemented");
		return -1;
	}

	if (xform->buffered && whence == XMMS_XFORM_SEEK_CUR) {
		offset -= xform->buffered;
	}

	res = xform->plugin->methods.seek (xform, offset, whence, err);
	if (res != -1) {
		xform->eos = FALSE;
		xform->buffered = 0;
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
	gchar *durl, *args;
	
	xform = xmms_xform_new (NULL, NULL, entry, goal_formats);

	session = xmms_medialib_begin ();
	url = xmms_medialib_entry_property_get_str (session, entry, XMMS_MEDIALIB_ENTRY_PROPERTY_URL);
	xmms_medialib_end (session);

	durl = g_strdup (url);
	args = strchr (durl, '?');
	if (args) {
		gchar **params;
		int i;
		*args = 0;
		args++;
		xmms_medialib_decode_url (args);

		params = g_strsplit (args, "&", 0);

		for(i = 0; params && params[i]; i++) {
			gchar *v;
			v = strchr (params[i], '=');
			if (v) {
				*v = 0;
				v++;
				xmms_xform_metadata_set_str (xform, params[i], v);
			} else {
				xmms_xform_metadata_set_int (xform, params[i], 1);
			}
		}
		g_strfreev (params);
	}
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

	last = add_effects (last, entry, goal_formats);

	xmms_xform_metadata_collect (last);

	XMMS_DBG ("Goaltype found!! \\o/");

	return last;
}

xmms_config_property_t *
xmms_xform_plugin_config_property_register (xmms_xform_plugin_t *xform_plugin,
                                            const gchar *name,
                                            const gchar *default_value,
                                            xmms_object_handler_t cb,
                                            gpointer userdata)
{
	return xmms_plugin_config_property_register ((xmms_plugin_t *) xform_plugin,
	                                             name, default_value, cb, userdata);
}

xmms_config_property_t *
xmms_xform_config_lookup (xmms_xform_t *xform, const gchar *path)
{
	g_return_val_if_fail (xform->plugin, NULL);

	return xmms_plugin_config_lookup ((xmms_plugin_t *) xform->plugin, path);
}

static xmms_xform_t *
add_effects (xmms_xform_t *last, xmms_medialib_entry_t entry, GList *goal_formats)
{
	xmms_xform_t *xform;
	gint effect_no = 0;

	while (42) {
		xmms_config_property_t *cfg;
		xmms_plugin_t *plugin;
		gchar key[64];
		const gchar *name;

		g_snprintf (key, sizeof (key), "effect.order.%i", effect_no++);

		cfg = xmms_config_lookup (key);
		if (!cfg) {
			/* this is just a ugly hack to have a configvalue
			   to set */
			xmms_config_property_register (key, "", NULL, NULL);
			break;
		}

		name = xmms_config_property_get_string (cfg);

		if (!name[0])
			break;

		plugin = xmms_plugin_find (XMMS_PLUGIN_TYPE_XFORM, name);
		if (plugin) {
			xform = xmms_xform_new ((xmms_xform_plugin_t *) plugin,
			                        last, entry, goal_formats);

			xmms_object_unref (plugin);
			xmms_object_unref (last);
			last = xform;

			xmms_plugin_config_property_register (plugin, "enabled", "0",
			                                      NULL, NULL);
		}
	}

	return last;
}
