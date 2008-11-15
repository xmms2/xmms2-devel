/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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
#include "xmms/xmms_ipc.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_object.h"

struct xmms_xform_object_St {
	xmms_object_t obj;
};

struct xmms_xform_St {
	xmms_object_t obj;
	struct xmms_xform_St *prev;

	const xmms_xform_plugin_t *plugin;
	xmms_medialib_entry_t entry;

	gboolean inited;

	void *priv;

	xmms_stream_type_t *out_type;

	GList *goal_hints;

	gboolean eos;
	gboolean error;

	char *buffer;
	gint buffered;
	gint buffersize;

	gboolean metadata_collected;

	gboolean metadata_changed;
	GHashTable *metadata;

	GHashTable *privdata;
	GQueue *hotspots;

	GList *browse_list;
	GTree *browse_dict;
	gint browse_index;

	/** used for line reading */
	struct {
		gchar buf[XMMS_XFORM_MAX_LINE_SIZE];
		gchar *bufend;
	} lr;
};

typedef struct xmms_xform_hotspot_St {
	guint pos;
	gchar *key;
	xmms_object_cmd_value_t *obj;
} xmms_xform_hotspot_t;

#define READ_CHUNK 4096

struct xmms_xform_plugin_St {
	xmms_plugin_t plugin;

	xmms_xform_methods_t methods;

	GList *in_types;
};

xmms_xform_t *xmms_xform_find (xmms_xform_t *prev, xmms_medialib_entry_t entry,
                               GList *goal_hints);
const char *xmms_xform_shortname (xmms_xform_t *xform);
static xmms_xform_t *add_effects (xmms_xform_t *last,
                                  xmms_medialib_entry_t entry,
                                  GList *goal_formats);
static xmms_xform_t *xmms_xform_new_effect (xmms_xform_t* last,
                                            xmms_medialib_entry_t entry,
                                            GList *goal_formats,
                                            const gchar *name);
static void xmms_xform_destroy (xmms_object_t *object);
static void effect_callbacks_init (void);

void
xmms_xform_browse_add_entry_property_str (xmms_xform_t *xform,
                                          const gchar *key,
                                          const gchar *value)
{
	xmms_object_cmd_value_t *val = xmms_object_cmd_value_str_new (value);
	xmms_xform_browse_add_entry_property (xform, key, val);
}


void
xmms_xform_browse_add_entry_property_int (xmms_xform_t *xform,
                                          const gchar *key,
                                          gint value)
{
	xmms_object_cmd_value_t *val = xmms_object_cmd_value_int_new (value);
	xmms_xform_browse_add_entry_property (xform, key, val);
}

void
xmms_xform_browse_add_symlink_args (xmms_xform_t *xform, const gchar *basename,
                                    const gchar *url, gint nargs, gchar **args)
{
	GString *s;
	gchar *eurl;
	gchar bname[32];
	gint i;

	if (!basename) {
		g_snprintf (bname, sizeof (bname), "%d", xform->browse_index++);
		basename = bname;
	}

	xmms_xform_browse_add_entry (xform, basename, 0);
	eurl = xmms_medialib_url_encode (url);
	s = g_string_new (eurl);

	for (i = 0; i < nargs; i++) {
		g_string_append_c (s, i == 0 ? '?' : '&');
		g_string_append (s, args[i]);
	}

	xmms_xform_browse_add_entry_property_str (xform, "realpath", s->str);

	g_free (eurl);
	g_string_free (s, TRUE);
}

void
xmms_xform_browse_add_symlink (xmms_xform_t *xform, const gchar *basename,
                               const gchar *url)
{
	xmms_xform_browse_add_symlink_args (xform, basename, url, 0, NULL);
}

void
xmms_xform_browse_add_entry_property (xmms_xform_t *xform, const gchar *key,
                                      xmms_object_cmd_value_t *val)
{
	g_return_if_fail (xform);
	g_return_if_fail (xform->browse_dict);
	g_return_if_fail (key);
	g_return_if_fail (val);

	g_tree_insert (xform->browse_dict, g_strdup (key), val);
}

void
xmms_xform_browse_add_entry (xmms_xform_t *xform, const gchar *filename,
                             guint32 flags)
{
	xmms_object_cmd_value_t *val;
	const gchar *url;
	gchar *efile, *eurl, *t;
	gint l, isdir;

	g_return_if_fail (filename);

	t = strchr (filename, '/');
	g_return_if_fail (!t); /* filenames can't contain '/', can they? */

	url = xmms_xform_get_url (xform);
	g_return_if_fail (url);

	xform->browse_dict = g_tree_new_full ((GCompareDataFunc) strcmp, NULL,
	                                      g_free,
	                                      (GDestroyNotify)xmms_object_cmd_value_unref);

	eurl = xmms_medialib_url_encode (url);
	efile = xmms_medialib_url_encode (filename);

	/* can't use g_build_filename as we need to preserve
	   slashes stuff like file:/// */
	l = strlen (url);
	if (l && url[l - 1] == '/') {
		t = g_strdup_printf ("%s%s", eurl, efile);
	} else {
		t = g_strdup_printf ("%s/%s", eurl, efile);
	}

	isdir = !!(flags & XMMS_XFORM_BROWSE_FLAG_DIR);
	xmms_xform_browse_add_entry_property_str (xform, "path", t);
	xmms_xform_browse_add_entry_property_int (xform, "isdir", isdir);

	val = xmms_object_cmd_value_dict_new (xform->browse_dict);
	xform->browse_list = g_list_prepend (xform->browse_list, val);

	g_free (t);
	g_free (efile);
	g_free (eurl);
}

static gint
xmms_browse_list_sortfunc (gconstpointer a, gconstpointer b)
{
	xmms_object_cmd_value_t *val1, *val2, *tmp1, *tmp2;

	val1 = (xmms_object_cmd_value_t *) a;
	val2 = (xmms_object_cmd_value_t *) b;

	g_return_val_if_fail (val1->type == XMMSV_TYPE_DICT, 0);
	g_return_val_if_fail (val2->type == XMMSV_TYPE_DICT, 0);

	tmp1 = g_tree_lookup (val1->value.dict, "intsort");
	tmp2 = g_tree_lookup (val2->value.dict, "intsort");

	if (tmp1 && tmp2) {
		g_return_val_if_fail (tmp1->type == XMMSV_TYPE_INT32, 0);
		g_return_val_if_fail (tmp2->type == XMMSV_TYPE_INT32, 0);
		return tmp1->value.int32 > tmp2->value.int32;
	}

	tmp1 = g_tree_lookup (val1->value.dict, "path");
	tmp2 = g_tree_lookup (val2->value.dict, "path");

	g_return_val_if_fail (!!tmp1, 0);
	g_return_val_if_fail (!!tmp2, 0);

	g_return_val_if_fail (tmp1->type == XMMSV_TYPE_STRING, 0);
	g_return_val_if_fail (tmp2->type == XMMSV_TYPE_STRING, 0);

	return g_utf8_collate (tmp1->value.string, tmp2->value.string);
}

GList *
xmms_xform_browse_method (xmms_xform_t *xform, const gchar *url,
                          xmms_error_t *error)
{
	GList *list = NULL;

	if (xform->plugin->methods.browse) {
		if (!xform->plugin->methods.browse (xform, url, error)) {
			return NULL;
		}
		list = xform->browse_list;
		xform->browse_list = NULL;
		list = g_list_sort (list, xmms_browse_list_sortfunc);
	} else {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Couldn't handle that URL");
	}

	return list;
}

GList *
xmms_xform_browse (xmms_xform_object_t *obj, const gchar *url,
                   xmms_error_t *error)
{
	GList *list = NULL;
	gchar *durl;
	xmms_xform_t *xform = NULL;
	xmms_xform_t *xform2 = NULL;

	xform = xmms_xform_new (NULL, NULL, 0, NULL);

	durl = g_strdup (url);
	xmms_medialib_decode_url (durl);
	XMMS_DBG ("url = %s", durl);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/x-url",
	                             XMMS_STREAM_TYPE_URL,
	                             durl,
	                             XMMS_STREAM_TYPE_END);

	xform2 = xmms_xform_find (xform, 0, NULL);
	if (xform2) {
		XMMS_DBG ("found xform %s", xmms_xform_shortname (xform2));
	} else {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Couldn't handle that URL");
		xmms_object_unref (xform);
		g_free (durl);
		return NULL;
	}

	list = xmms_xform_browse_method (xform2, durl, error);

	xmms_object_unref (xform);
	xmms_object_unref (xform2);

	g_free (durl);

	return list;
}

XMMS_CMD_DEFINE (browse, xmms_xform_browse, xmms_xform_object_t *,
                 LIST, STRING, NONE);

static void
xmms_xform_object_destroy (xmms_object_t *obj)
{
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_XFORM);
}

xmms_xform_object_t *
xmms_xform_object_init (void)
{
	xmms_xform_object_t *obj;

	obj = xmms_object_new (xmms_xform_object_t, xmms_xform_object_destroy);

	xmms_ipc_object_register (XMMS_IPC_OBJECT_XFORM, XMMS_OBJECT (obj));

	xmms_object_cmd_add (XMMS_OBJECT (obj), XMMS_IPC_CMD_BROWSE,
	                     XMMS_CMD_FUNC (browse));

	effect_callbacks_init ();

	return obj;
}

static void
xmms_xform_destroy (xmms_object_t *object)
{
	xmms_xform_t *xform = (xmms_xform_t *)object;

	XMMS_DBG ("Freeing xform '%s'", xmms_xform_shortname (xform));

	/* The 'destroy' method is not mandatory */
	if (xform->plugin && xform->plugin->methods.destroy && xform->inited) {
		xform->plugin->methods.destroy (xform);
	}

	g_hash_table_destroy (xform->metadata);

	g_hash_table_destroy (xform->privdata);
	g_queue_free (xform->hotspots);

	g_free (xform->buffer);

	xmms_object_unref (xform->out_type);
	xmms_object_unref (xform->plugin);

	if (xform->prev) {
		xmms_object_unref (xform->prev);
	}

}

xmms_xform_t *
xmms_xform_new (xmms_xform_plugin_t *plugin, xmms_xform_t *prev,
                xmms_medialib_entry_t entry, GList *goal_hints)
{
	xmms_xform_t *xform;

	xform = xmms_object_new (xmms_xform_t, xmms_xform_destroy);

	xmms_object_ref (plugin);
	xform->plugin = plugin;
	xform->entry = entry;
	xform->goal_hints = goal_hints;
	xform->lr.bufend = &xform->lr.buf[0];

	if (prev) {
		xmms_object_ref (prev);
		xform->prev = prev;
	}

	xform->metadata = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                         g_free,
	                                         (GDestroyNotify)xmms_object_cmd_value_unref);

	xform->privdata = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                         g_free,
	                                         (GDestroyNotify)xmms_object_cmd_value_unref);
	xform->hotspots = g_queue_new ();

	if (plugin && entry) {
		if (!plugin->methods.init (xform)) {
			xmms_object_unref (xform);
			return NULL;
		}
		xform->inited = TRUE;
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
xmms_xform_indata_find_str (xmms_xform_t *xform, xmms_stream_type_key_t key)
{
	const gchar *r;
	r = xmms_stream_type_get_str (xform->prev->out_type, key);
	if (r) {
		return r;
	} else if (xform->prev) {
		return xmms_xform_indata_find_str (xform->prev, key);
	}
	return NULL;
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
	g_hash_table_insert (xform->metadata, g_strdup (key),
	                     xmms_object_cmd_value_int_new (val));
	xform->metadata_changed = TRUE;
}

void
xmms_xform_metadata_set_str (xmms_xform_t *xform, const char *key,
                             const char *val)
{
	const char *old;

	if (xmms_xform_metadata_get_str (xform, key, &old)) {
		if (strcmp (old, val) == 0) {
			return;
		}
	}

	g_hash_table_insert (xform->metadata, g_strdup (key),
	                     xmms_object_cmd_value_str_new (val));

	xform->metadata_changed = TRUE;
}

static const xmms_object_cmd_value_t *
xmms_xform_metadata_get_val (xmms_xform_t *xform, const char *key)
{
	xmms_object_cmd_value_t *val = NULL;

	for (; xform; xform = xform->prev) {
		val = g_hash_table_lookup (xform->metadata, key);
		if (val) {
			break;
		}
	}

	return val;
}

gboolean
xmms_xform_metadata_has_val (xmms_xform_t *xform, const gchar *key)
{
	return !!xmms_xform_metadata_get_val (xform, key);
}

gboolean
xmms_xform_metadata_get_int (xmms_xform_t *xform, const char *key,
                             gint32 *val)
{
	const xmms_object_cmd_value_t *obj;
	gboolean ret = FALSE;

	obj = xmms_xform_metadata_get_val (xform, key);
	if (obj && obj->type == XMMSV_TYPE_INT32) {
		*val = obj->value.int32;
		ret = TRUE;
	}

	return ret;
}

gboolean
xmms_xform_metadata_get_str (xmms_xform_t *xform, const char *key,
                             const gchar **val)
{
	const xmms_object_cmd_value_t *obj;
	gboolean ret = FALSE;

	obj = xmms_xform_metadata_get_val (xform, key);
	if (obj && obj->type == XMMSV_TYPE_STRING) {
		*val = obj->value.string;
		ret = TRUE;
	}

	return ret;
}

typedef struct {
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry;
	guint32 source;
} metadata_festate_t;

static void
add_metadatum (gpointer _key, gpointer _value, gpointer user_data)
{
	xmms_object_cmd_value_t *value = (xmms_object_cmd_value_t *) _value;
	gchar *key = (gchar *) _key;
	metadata_festate_t *st = (metadata_festate_t *) user_data;

	if (value->type == XMMSV_TYPE_STRING) {
		xmms_medialib_entry_property_set_str_source (st->session,
		                                             st->entry,
		                                             key,
		                                             value->value.string,
		                                             st->source);
	} else if (value->type == XMMSV_TYPE_INT32) {
		xmms_medialib_entry_property_set_int_source (st->session,
		                                             st->entry,
		                                             key,
		                                             value->value.int32,
		                                             st->source);
	} else {
		XMMS_DBG ("Unknown type?!?");
	}
}

static void
xmms_xform_metadata_collect_one (xmms_xform_t *xform, metadata_festate_t *info)
{
	gchar src[XMMS_PLUGIN_SHORTNAME_MAX_LEN + 8];

	XMMS_DBG ("Collecting metadata from %s", xmms_xform_shortname (xform));

	g_snprintf (src, sizeof (src), "plugin/%s",
	            xmms_xform_shortname (xform));

	info->source = xmms_medialib_source_to_id (info->session, src);
	g_hash_table_foreach (xform->metadata, add_metadatum, info);

	xform->metadata_changed = FALSE;
}

static void
xmms_xform_metadata_collect_r (xmms_xform_t *xform, metadata_festate_t *info,
                               GString *namestr)
{
	if (xform->prev) {
		xmms_xform_metadata_collect_r (xform->prev, info, namestr);
	}

	if (xform->plugin) {
		if (namestr->len) {
			g_string_append_c (namestr, ':');
		}
		g_string_append (namestr, xmms_xform_shortname (xform));
	}

	if (xform->metadata_changed) {
		xmms_xform_metadata_collect_one (xform, info);
	}

	xform->metadata_collected = TRUE;
}

static void
xmms_xform_metadata_collect (xmms_xform_t *start, GString *namestr, gboolean rehashing)
{
	metadata_festate_t info;
	gint times_played;
	gint last_started;
	GTimeVal now;

	info.entry = start->entry;
	info.session = xmms_medialib_begin_write ();

	times_played = xmms_medialib_entry_property_get_int (info.session,
	                                                     info.entry,
	                                                     XMMS_MEDIALIB_ENTRY_PROPERTY_TIMESPLAYED);

	/* times_played == -1 if we haven't played this entry yet. so after initial
	 * metadata collection the mlib would have timesplayed = -1 if we didn't do
	 * the following */
	if (times_played < 0) {
		times_played = 0;
	}

	last_started = xmms_medialib_entry_property_get_int (info.session,
	                                                     info.entry,
	                                                     XMMS_MEDIALIB_ENTRY_PROPERTY_LASTSTARTED);

	xmms_medialib_entry_cleanup (info.session, info.entry);

	xmms_xform_metadata_collect_r (start, &info, namestr);

	xmms_medialib_entry_property_set_str (info.session, info.entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_CHAIN,
	                                      namestr->str);

	xmms_medialib_entry_property_set_int (info.session, info.entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_TIMESPLAYED,
	                                      times_played + (rehashing ? 0 : 1));

	if (!rehashing || (rehashing && last_started)) {
		g_get_current_time (&now);

		xmms_medialib_entry_property_set_int (info.session, info.entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_LASTSTARTED,
		                                      (rehashing ? last_started : now.tv_sec));
	}

	xmms_medialib_entry_status_set (info.session, info.entry,
	                                XMMS_MEDIALIB_ENTRY_STATUS_OK);

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

static void
xmms_xform_auxdata_set_val (xmms_xform_t *xform, char *key,
                            xmms_object_cmd_value_t *val)
{
	xmms_xform_hotspot_t *hs;

	hs = g_new0 (xmms_xform_hotspot_t, 1);
	hs->pos = xform->buffered;
	hs->key = key;
	hs->obj = val;

	g_queue_push_tail (xform->hotspots, hs);
}

void
xmms_xform_auxdata_barrier (xmms_xform_t *xform)
{
	xmms_object_cmd_value_t *val = xmms_object_cmd_value_none_new ();
	xmms_xform_auxdata_set_val (xform, NULL, val);
}

void
xmms_xform_auxdata_set_int (xmms_xform_t *xform, const char *key, int intval)
{
	xmms_object_cmd_value_t *val = xmms_object_cmd_value_int_new (intval);
	xmms_xform_auxdata_set_val (xform, g_strdup (key), val);
}

void
xmms_xform_auxdata_set_str (xmms_xform_t *xform, const gchar *key,
                            const gchar *strval)
{
	xmms_object_cmd_value_t *val;
	const char *old;

	if (xmms_xform_auxdata_get_str (xform, key, &old)) {
		if (strcmp (old, strval) == 0) {
			return;
		}
	}

	val = xmms_object_cmd_value_str_new (strval);
	xmms_xform_auxdata_set_val (xform, g_strdup (key), val);
}

void
xmms_xform_auxdata_set_bin (xmms_xform_t *xform, const gchar *key,
                            gpointer data, gssize len)
{
	xmms_object_cmd_value_t *val;
	GString *bin;

	bin = g_string_new_len (data, len);
	val = xmms_object_cmd_value_bin_new (bin);
	xmms_xform_auxdata_set_val (xform, g_strdup (key), val);
}

static const xmms_object_cmd_value_t *
xmms_xform_auxdata_get_val (xmms_xform_t *xform, const gchar *key)
{
	guint i;
	xmms_xform_hotspot_t *hs;
	xmms_object_cmd_value_t *val = NULL;

	/* privdata is always got from the previous xform */
	xform = xform->prev;

	/* check if we have unhandled current (pos 0) hotspots for this key */
	for (i=0; (hs = g_queue_peek_nth (xform->hotspots, i)) != NULL; i++) {
		if (hs->pos != 0) {
			break;
		} else if (hs->key && !strcmp (key, hs->key)) {
			val = hs->obj;
		}
	}

	if (!val) {
		val = g_hash_table_lookup (xform->privdata, key);
	}

	return val;
}

gboolean
xmms_xform_auxdata_has_val (xmms_xform_t *xform, const gchar *key)
{
	return !!xmms_xform_auxdata_get_val (xform, key);
}

gboolean
xmms_xform_auxdata_get_int (xmms_xform_t *xform, const gchar *key, gint32 *val)
{
	const xmms_object_cmd_value_t *obj;

	obj = xmms_xform_auxdata_get_val (xform, key);
	if (obj && obj->type == XMMSV_TYPE_INT32) {
		*val = obj->value.int32;
		return TRUE;
	}

	return FALSE;
}

gboolean
xmms_xform_auxdata_get_str (xmms_xform_t *xform, const gchar *key,
                            const gchar **val)
{
	const xmms_object_cmd_value_t *obj;

	obj = xmms_xform_auxdata_get_val (xform, key);
	if (obj && obj->type == XMMSV_TYPE_STRING) {
		*val = obj->value.string;
		return TRUE;
	}

	return FALSE;
}

gboolean
xmms_xform_auxdata_get_bin (xmms_xform_t *xform, const gchar *key,
                            gpointer *data, gssize *datalen)
{
	const xmms_object_cmd_value_t *obj;

	obj = xmms_xform_auxdata_get_val (xform, key);
	if (obj && obj->type == XMMSV_TYPE_BIN) {
		GString *bin = obj->value.bin;

		*data = bin->str;
		*datalen = bin->len;

		return TRUE;
	}

	return FALSE;
}

const char *
xmms_xform_shortname (xmms_xform_t *xform)
{
	return (xform->plugin)
	       ? xmms_plugin_shortname_get ((xmms_plugin_t *) xform->plugin)
	       : "unknown";
}

gint
xmms_xform_this_peek (xmms_xform_t *xform, gpointer buf, gint siz,
                      xmms_error_t *err)
{
	while (xform->buffered < siz) {
		gint res;

		if (xform->buffered + READ_CHUNK > xform->buffersize) {
			xform->buffersize *= 2;
			xform->buffer = g_realloc (xform->buffer, xform->buffersize);
		}

		res = xform->plugin->methods.read (xform,
		                                   &xform->buffer[xform->buffered],
		                                   READ_CHUNK, err);

		if (res < -1) {
			XMMS_DBG ("Read method of %s returned bad value (%d) - BUG IN PLUGIN",
			          xmms_xform_shortname (xform), res);
			res = -1;
		}

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

static void
xmms_xform_hotspot_callback (gpointer data, gpointer user_data)
{
	xmms_xform_hotspot_t *hs = data;
	gint *read = user_data;

	hs->pos -= *read;
}

static gint
xmms_xform_hotspots_update (xmms_xform_t *xform)
{
	xmms_xform_hotspot_t *hs;
	gint ret = -1;

	hs = g_queue_peek_head (xform->hotspots);
	while (hs != NULL && hs->pos == 0) {
		g_queue_pop_head (xform->hotspots);
		if (hs->key) {
			g_hash_table_insert (xform->privdata, hs->key, hs->obj);
		}
		hs = g_queue_peek_head (xform->hotspots);
	}

	if (hs != NULL) {
		ret = hs->pos;
	}

	return ret;
}

gint
xmms_xform_this_read (xmms_xform_t *xform, gpointer buf, gint siz,
                      xmms_error_t *err)
{
	gint read = 0;
	gint nexths;

	if (xform->error) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "Read on errored xform");
		return -1;
	}

	/* update hotspots */
	nexths = xmms_xform_hotspots_update (xform);
	if (nexths >= 0) {
		siz = MIN (siz, nexths);
	}

	if (xform->buffered) {
		read = MIN (siz, xform->buffered);
		memcpy (buf, xform->buffer, read);
		xform->buffered -= read;

		/* buffer edited, update hotspot positions */
		g_queue_foreach (xform->hotspots, &xmms_xform_hotspot_callback, &read);

		if (xform->buffered) {
			/* unless we are _peek:ing often
			   this should be fine */
			memmove (xform->buffer, &xform->buffer[read], xform->buffered);
		}
	}

	if (xform->eos) {
		return read;
	}

	while (read < siz) {
		gint res;

		res = xform->plugin->methods.read (xform, buf + read, siz - read, err);
		if (xform->metadata_collected && xform->metadata_changed)
			xmms_xform_metadata_update (xform);

		if (res < -1) {
			XMMS_DBG ("Read method of %s returned bad value (%d) - BUG IN PLUGIN", xmms_xform_shortname (xform), res);
			res = -1;
		}

		if (res == 0) {
			xform->eos = TRUE;
			break;
		} else if (res == -1) {
			xform->error = TRUE;
			return -1;
		} else {
			if (read == 0)
				xmms_xform_hotspots_update (xform);

			if (!g_queue_is_empty (xform->hotspots)) {
				if (xform->buffered + res > xform->buffersize) {
					xform->buffersize = MAX (xform->buffersize * 2,
					                         xform->buffersize + res);
					xform->buffer = g_realloc (xform->buffer,
					                           xform->buffersize);
				}

				g_memmove (xform->buffer + xform->buffered, buf + read, res);
				xform->buffered += res;
				break;
			}
			read += res;
		}
	}

	return read;
}

gint64
xmms_xform_this_seek (xmms_xform_t *xform, gint64 offset,
                      xmms_xform_seek_mode_t whence, xmms_error_t *err)
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
		xmms_xform_hotspot_t *hs;

		xform->eos = FALSE;
		xform->buffered = 0;

		/* flush the hotspot queue on seek */
		while ((hs = g_queue_pop_head (xform->hotspots)) != NULL) {
			g_free (hs->key);
			xmms_object_cmd_value_unref (hs->obj);
			g_free (hs);
		}
	}

	return res;
}

gint
xmms_xform_peek (xmms_xform_t *xform, gpointer buf, gint siz,
                 xmms_error_t *err)
{
	g_return_val_if_fail (xform->prev, -1);
	return xmms_xform_this_peek (xform->prev, buf, siz, err);
}

gchar *
xmms_xform_read_line (xmms_xform_t *xform, gchar *line, xmms_error_t *err)
{
	gchar *p;

	g_return_val_if_fail (xform, NULL);
	g_return_val_if_fail (line, NULL);

	p = strchr (xform->lr.buf, '\n');

	if (!p) {
		gint l, r;

		l = (XMMS_XFORM_MAX_LINE_SIZE - 1) - (xform->lr.bufend - xform->lr.buf);
		if (l) {
			r = xmms_xform_read (xform, xform->lr.bufend, l, err);
			if (r < 0) {
				return NULL;
			}
			xform->lr.bufend += r;
		}
		if (xform->lr.bufend <= xform->lr.buf)
			return NULL;

		*(xform->lr.bufend) = '\0';
		p = strchr (xform->lr.buf, '\n');
		if (!p) {
			p = xform->lr.bufend;
		}
	}

	if (p > xform->lr.buf && *(p-1) == '\r') {
		*(p-1) = '\0';
	} else {
		*p = '\0';
	}

	strcpy (line, xform->lr.buf);
	memmove (xform->lr.buf, p + 1, xform->lr.bufend - p);
	xform->lr.bufend -= (p - xform->lr.buf) + 1;
	*xform->lr.bufend = '\0';

	return line;
}

gint
xmms_xform_read (xmms_xform_t *xform, gpointer buf, gint siz, xmms_error_t *err)
{
	g_return_val_if_fail (xform->prev, -1);
	return xmms_xform_this_read (xform->prev, buf, siz, err);
}

gint64
xmms_xform_seek (xmms_xform_t *xform, gint64 offset,
                 xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	g_return_val_if_fail (xform->prev, -1);
	return xmms_xform_this_seek (xform->prev, offset, whence, err);
}

const gchar *
xmms_xform_get_url (xmms_xform_t *xform)
{
	const gchar *url = NULL;
	xmms_xform_t *x;
	x = xform;

	while (!url && x) {
		url = xmms_xform_indata_get_str (x, XMMS_STREAM_TYPE_URL);
		x = x->prev;
	}

	return url;
}

static void
xmms_xform_plugin_destroy (xmms_object_t *obj)
{
	xmms_xform_plugin_t *plugin = (xmms_xform_plugin_t *)obj;

	while (plugin->in_types) {
		xmms_object_unref (plugin->in_types->data);

		plugin->in_types = g_list_delete_link (plugin->in_types,
		                                       plugin->in_types);
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
xmms_xform_plugin_methods_set (xmms_xform_plugin_t *plugin,
                               xmms_xform_methods_t *methods)
{

	g_return_if_fail (plugin);
	g_return_if_fail (plugin->plugin.type == XMMS_PLUGIN_TYPE_XFORM);

	XMMS_DBG ("Registering xform '%s'",
	          xmms_plugin_shortname_get ((xmms_plugin_t *) plugin));

	memcpy (&plugin->methods, methods, sizeof (xmms_xform_methods_t));
}

gboolean
xmms_xform_plugin_verify (xmms_plugin_t *_plugin)
{
	xmms_xform_plugin_t *plugin = (xmms_xform_plugin_t *) _plugin;

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
	gchar *config_key, config_value[32];
	gint priority;

	va_start (ap, plugin);
	t = xmms_stream_type_parse (ap);
	va_end (ap);

	config_key = g_strconcat ("priority.",
	                          xmms_stream_type_get_str (t, XMMS_STREAM_TYPE_NAME),
	                          NULL);
	priority = xmms_stream_type_get_int (t, XMMS_STREAM_TYPE_PRIORITY);
	g_snprintf (config_value, sizeof (config_value), "%d", priority);
	xmms_xform_plugin_config_property_register (plugin, config_key,
	                                            config_value, NULL, NULL);
	g_free (config_key);

	plugin->in_types = g_list_prepend (plugin->in_types, t);
}

static gboolean
xmms_xform_plugin_supports (xmms_xform_plugin_t *plugin, xmms_stream_type_t *st,
                            gint *priority)
{
	GList *t;

	for (t = plugin->in_types; t; t = g_list_next (t)) {
		if (xmms_stream_type_match (t->data, st)) {
			if (priority) {
				gchar *config_key;
				xmms_config_property_t *conf_priority;

				config_key = g_strconcat ("priority.",
				                          xmms_stream_type_get_str (t->data, XMMS_STREAM_TYPE_NAME),
				                          NULL);
				conf_priority = xmms_plugin_config_lookup ((xmms_plugin_t *)plugin,
				                                           config_key);
				g_free (config_key);

				if (conf_priority) {
					*priority = xmms_config_property_get_int (conf_priority);
				} else {
					*priority = XMMS_STREAM_TYPE_PRIORITY_DEFAULT;
				}
			}
			return TRUE;
		}
	}
	return FALSE;
}

typedef struct match_state_St {
	xmms_xform_plugin_t *match;
	xmms_stream_type_t *out_type;
	gint priority;
} match_state_t;

static gboolean
xmms_xform_match (xmms_plugin_t *_plugin, gpointer user_data)
{
	xmms_xform_plugin_t *plugin = (xmms_xform_plugin_t *)_plugin;
	match_state_t *state = (match_state_t *)user_data;
	gint priority;

	g_assert (_plugin->type == XMMS_PLUGIN_TYPE_XFORM);

	if (!plugin->in_types) {
		XMMS_DBG ("Skipping plugin '%s'", xmms_plugin_shortname_get (_plugin));
		return TRUE;
	}

	XMMS_DBG ("Trying plugin '%s'", xmms_plugin_shortname_get (_plugin));
	if (xmms_xform_plugin_supports (plugin, state->out_type, &priority)) {
		XMMS_DBG ("Plugin '%s' matched (priority %d)",
		          xmms_plugin_shortname_get (_plugin), priority);
		if (priority > state->priority) {
			if (state->match) {
				XMMS_DBG ("Using plugin '%s' (priority %d) instead of '%s' (priority %d)",
				          xmms_plugin_shortname_get (_plugin), priority,
				          xmms_plugin_shortname_get ((xmms_plugin_t *)state->match),
				          state->priority);
			}

			state->match = plugin;
			state->priority = priority;
		}
	}

	return TRUE;
}

xmms_xform_t *
xmms_xform_find (xmms_xform_t *prev, xmms_medialib_entry_t entry,
                 GList *goal_hints)
{
	match_state_t state;
	xmms_xform_t *xform = NULL;

	state.out_type = prev->out_type;
	state.match = NULL;
	state.priority = -1;

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
	gboolean ret = TRUE;

	if (xform->prev) {
		ret = xform->prev->eos;
	}

	return ret;
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
	const xmms_stream_type_t *current;
	gboolean ret = FALSE;
	GList *n;

	current = xmms_xform_get_out_stream_type (xform);

	for (n = goal_formats; n; n = g_list_next (n)) {
		xmms_stream_type_t *goal_type = n->data;
		if (xmms_stream_type_match (goal_type, current)) {
			ret = TRUE;
			break;
		}

	}

	if (!ret) {
		XMMS_DBG ("Not in one of %d goal-types", g_list_length (goal_formats));
	}

	return ret;
}

static void
outdata_type_metadata_collect (xmms_xform_t *xform)
{
	gint val;
	const char *mime;
	xmms_stream_type_t *type;

	type = xform->out_type;
	mime = xmms_stream_type_get_str (type, XMMS_STREAM_TYPE_MIMETYPE);
	if (strcmp (mime, "audio/pcm") != 0) {
		return;
	}

	val = xmms_stream_type_get_int (type, XMMS_STREAM_TYPE_FMT_FORMAT);
	if (val != -1) {
		const gchar *name = xmms_sample_name_get ((xmms_sample_format_t) val);
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLE_FMT,
		                             name);
	}

	val = xmms_stream_type_get_int (type, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	if (val != -1) {
		xmms_xform_metadata_set_int (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLERATE,
		                             val);
	}

	val = xmms_stream_type_get_int (type, XMMS_STREAM_TYPE_FMT_CHANNELS);
	if (val != -1) {
		xmms_xform_metadata_set_int (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_CHANNELS,
		                             val);
	}
}

static xmms_xform_t *
chain_setup (xmms_medialib_entry_t entry, const gchar *url, GList *goal_formats)
{
	xmms_xform_t *xform, *last;
	gchar *durl, *args;

	if (!entry) {
		entry = 1; /* FIXME: this is soooo ugly, don't do this */
	}

	xform = xmms_xform_new (NULL, NULL, 0, goal_formats);

	durl = g_strdup (url);

	args = strchr (durl, '?');
	if (args) {
		gchar **params;
		gint i;
		*args = 0;
		args++;
		xmms_medialib_decode_url (args);

		params = g_strsplit (args, "&", 0);

		for (i = 0; params && params[i]; i++) {
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

	xmms_xform_outdata_type_add (xform, XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/x-url", XMMS_STREAM_TYPE_URL,
	                             durl, XMMS_STREAM_TYPE_END);

	g_free (durl);

	last = xform;

	do {
		xform = xmms_xform_find (last, entry, goal_formats);
		if (!xform) {
			xmms_log_error ("Couldn't set up chain for '%s' (%d)",
			                url, entry);
			xmms_object_unref (last);

			return NULL;
		}
		xmms_object_unref (last);
		last = xform;
	} while (!has_goalformat (xform, goal_formats));

	outdata_type_metadata_collect (last);

	return last;
}

void
chain_finalize (xmms_xform_t *xform, xmms_medialib_entry_t entry,
                const gchar *url, gboolean rehashing)
{
	GString *namestr;

	namestr = g_string_new ("");
	xmms_xform_metadata_collect (xform, namestr, rehashing);
	xmms_log_info ("Successfully setup chain for '%s' (%d) containing %s",
	               url, entry, namestr->str);

	g_string_free (namestr, TRUE);
}

gchar *
get_url_for_entry (xmms_medialib_entry_t entry)
{
	xmms_medialib_session_t *session;
	gchar *url = NULL;

	session = xmms_medialib_begin ();
	url = xmms_medialib_entry_property_get_str (session, entry,
	                                            XMMS_MEDIALIB_ENTRY_PROPERTY_URL);
	xmms_medialib_end (session);

	if (!url) {
		xmms_log_error ("Couldn't get url for entry (%d)", entry);
	}

	return url;
}

xmms_xform_t *
xmms_xform_chain_setup (xmms_medialib_entry_t entry, GList *goal_formats,
                        gboolean rehash)
{
	gchar *url;
	xmms_xform_t *xform;

	if (!(url = get_url_for_entry (entry))) {
		return NULL;
	}

	xform = xmms_xform_chain_setup_url (entry, url, goal_formats, rehash);
	g_free (url);

	return xform;
}

xmms_xform_t *
xmms_xform_chain_setup_url (xmms_medialib_entry_t entry, const gchar *url,
                            GList *goal_formats, gboolean rehash)
{
	xmms_xform_t *last;
	xmms_plugin_t *plugin;
	xmms_xform_plugin_t *xform_plugin;
	gboolean add_segment = FALSE;

	last = chain_setup (entry, url, goal_formats);
	if (!last) {
		return NULL;
	}

	/* first check that segment plugin is available in the system */
	plugin = xmms_plugin_find (XMMS_PLUGIN_TYPE_XFORM, "segment");
	xform_plugin = (xmms_xform_plugin_t *) plugin;

	/* if segment plugin input is the same as current output, include it
	 * for collecting additional duration metadata on audio entries */
	if (xform_plugin) {
		add_segment = xmms_xform_plugin_supports (xform_plugin,
		                                          last->out_type,
		                                          NULL);
		xmms_object_unref (plugin);
	}

	/* add segment plugin to the chain if it can be added */
	if (add_segment) {
		last = xmms_xform_new_effect (last, entry, goal_formats, "segment");
		if (!last) {
			return NULL;
		}
	}

	/* if not rehashing, also initialize all the effect plugins */
	if (!rehash) {
		last = add_effects (last, entry, goal_formats);
		if (!last) {
			return NULL;
		}
	}

	chain_finalize (last, entry, url, rehash);
	return last;
}

xmms_config_property_t *
xmms_xform_plugin_config_property_register (xmms_xform_plugin_t *xform_plugin,
                                            const gchar *name,
                                            const gchar *default_value,
                                            xmms_object_handler_t cb,
                                            gpointer userdata)
{
	xmms_plugin_t *plugin = (xmms_plugin_t *) xform_plugin;

	return xmms_plugin_config_property_register (plugin, name,
	                                             default_value,
	                                             cb, userdata);
}

xmms_config_property_t *
xmms_xform_config_lookup (xmms_xform_t *xform, const gchar *path)
{
	g_return_val_if_fail (xform->plugin, NULL);

	return xmms_plugin_config_lookup ((xmms_plugin_t *) xform->plugin, path);
}

static xmms_xform_t *
add_effects (xmms_xform_t *last, xmms_medialib_entry_t entry,
             GList *goal_formats)
{
	gint effect_no;

	for (effect_no = 0; TRUE; effect_no++) {
		xmms_config_property_t *cfg;
		gchar key[64];
		const gchar *name;

		g_snprintf (key, sizeof (key), "effect.order.%i", effect_no);

		cfg = xmms_config_lookup (key);
		if (!cfg) {
			break;
		}

		name = xmms_config_property_get_string (cfg);

		if (!name[0]) {
			continue;
		}

		last = xmms_xform_new_effect (last, entry, goal_formats, name);
	}

	return last;
}

static xmms_xform_t *
xmms_xform_new_effect (xmms_xform_t *last, xmms_medialib_entry_t entry,
                       GList *goal_formats, const gchar *name)
{
	xmms_plugin_t *plugin;
	xmms_xform_plugin_t *xform_plugin;
	xmms_xform_t *xform;

	plugin = xmms_plugin_find (XMMS_PLUGIN_TYPE_XFORM, name);
	if (!plugin) {
		xmms_log_error ("Couldn't find any effect named '%s'", name);
		return last;
	}

	xform_plugin = (xmms_xform_plugin_t *) plugin;
	if (!xmms_xform_plugin_supports (xform_plugin, last->out_type, NULL)) {
		xmms_log_info ("Effect '%s' doesn't support format, skipping",
		               xmms_plugin_shortname_get (plugin));
		xmms_object_unref (plugin);
		return last;
	}

	xform = xmms_xform_new (xform_plugin, last, entry, goal_formats);

	if (xform) {
		xmms_object_unref (last);
		last = xform;
	} else {
		xmms_log_info ("Effect '%s' failed to initialize, skipping",
		               xmms_plugin_shortname_get (plugin));
	}
	xmms_xform_plugin_config_property_register (xform_plugin,
	                                            "enabled", "0",
	                                            NULL, NULL);
	xmms_object_unref (plugin);
	return last;
}

static void
update_effect_properties (xmms_object_t *object, gconstpointer data,
                          gpointer userdata)
{
	gint effect_no = GPOINTER_TO_INT (userdata);
	const gchar *name = (gchar *)data;

	xmms_config_property_t *cfg;
	xmms_xform_plugin_t *xform_plugin;
	xmms_plugin_t *plugin;
	gchar key[64];

	if (name[0]) {
		plugin = xmms_plugin_find (XMMS_PLUGIN_TYPE_XFORM, name);
		if (!plugin) {
			xmms_log_error ("Couldn't find any effect named '%s'", name);
		} else {
			xform_plugin = (xmms_xform_plugin_t *) plugin;
			xmms_xform_plugin_config_property_register (xform_plugin, "enabled",
			                                            "1", NULL, NULL);
			xmms_object_unref (plugin);
		}

		/* setup new effect.order.n */
		g_snprintf (key, sizeof (key), "effect.order.%i", effect_no + 1);

		cfg = xmms_config_lookup (key);
		if (!cfg) {
			xmms_config_property_register (key, "", update_effect_properties,
			                               GINT_TO_POINTER (effect_no + 1));
		}
	}
}

static void
effect_callbacks_init (void)
{
	gint effect_no;

	xmms_config_property_t *cfg;
	xmms_xform_plugin_t *xform_plugin;
	xmms_plugin_t *plugin;
	gchar key[64];
	const gchar *name;

	for (effect_no = 0; ; effect_no++) {
		g_snprintf (key, sizeof (key), "effect.order.%i", effect_no);

		cfg = xmms_config_lookup (key);
		if (!cfg) {
			break;
		}
		xmms_config_property_callback_set (cfg, update_effect_properties,
		                                   GINT_TO_POINTER (effect_no));

		name = xmms_config_property_get_string (cfg);
		if (!name[0]) {
			continue;
		}

		plugin = xmms_plugin_find (XMMS_PLUGIN_TYPE_XFORM, name);
		if (!plugin) {
			xmms_log_error ("Couldn't find any effect named '%s'", name);
			continue;
		}

		xform_plugin = (xmms_xform_plugin_t *) plugin;
		xmms_xform_plugin_config_property_register (xform_plugin, "enabled",
		                                            "1", NULL, NULL);

		xmms_object_unref (plugin);
	}

	/* the name stored in the last present property was not "" or there was no
	   last present property */
	if ((!effect_no) || name[0]) {
			xmms_config_property_register (key, "", update_effect_properties,
			                               GINT_TO_POINTER (effect_no));
	}
}

