/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_log.h>

#include <gio/gio.h>

typedef struct {
	GInputStream *handle;
} xmms_gvfs_data_t;

static const struct {
	const gchar *mlib;
	const gchar *gvfs;
	xmmsv_type_t type;
} attr_map[] = {
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_MIME,        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, XMMSV_TYPE_STRING },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD,        G_FILE_ATTRIBUTE_TIME_MODIFIED,         XMMSV_TYPE_INT32  },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE,        G_FILE_ATTRIBUTE_STANDARD_SIZE,         XMMSV_TYPE_INT32  },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_DESCRIPTION, G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION,  XMMSV_TYPE_STRING }
};

static const struct {
	const gchar *scheme;
	const gint priority;
} scheme_priorities[] = {
};

static const gchar *query_attributes = G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                                       G_FILE_ATTRIBUTE_TIME_MODIFIED ","
                                       G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                       G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION;

static gboolean xmms_gvfs_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_gvfs_init (xmms_xform_t *xform);
static void xmms_gvfs_destroy (xmms_xform_t *xform);
static gint xmms_gvfs_read (xmms_xform_t *xform, gpointer buffer, gint len,
                            xmms_error_t *error);
static gint64 xmms_gvfs_seek (xmms_xform_t *xform, gint64 offset,
                              xmms_xform_seek_mode_t whence,
                              xmms_error_t *error);
static gboolean xmms_gvfs_browse (xmms_xform_t *xform, const gchar *url,
                                  xmms_error_t *error);

XMMS_XFORM_PLUGIN_DEFINE ("gvfs",
                          "gvfs transport",
                          XMMS_VERSION,
                          "Transport for glibs virtual filesystem",
                          xmms_gvfs_plugin_setup);

static gboolean
xmms_gvfs_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	GVfs *vfs;
	const gchar * const *schemes, * const *i;
	gint j;
	xmms_xform_methods_t methods;

	vfs = g_vfs_get_default ();
	if (!g_vfs_is_active (vfs)) {
		xmms_log_info ("GVfs not active - disabling gvfs transport");
		return FALSE;
	}

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_gvfs_init;
	methods.destroy = xmms_gvfs_destroy;
	methods.read = xmms_gvfs_read;
	methods.seek = xmms_gvfs_seek;
	methods.browse = xmms_gvfs_browse;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url",
	                              XMMS_STREAM_TYPE_URL,
	                              "file://*",
	                              XMMS_STREAM_TYPE_END);

	schemes = g_vfs_get_supported_uri_schemes (vfs);
	for (i = schemes; *i; i++) {
		gchar *tmp = g_strconcat (*i, "://*", NULL);
		gint priority = XMMS_STREAM_TYPE_PRIORITY_FALLBACK;

		for (j = 0; j < G_N_ELEMENTS (scheme_priorities); j++) {
			if (g_ascii_strcasecmp (scheme_priorities[j].scheme, *i) == 0) {
				priority = scheme_priorities[j].priority;
			}
		}

		xmms_xform_plugin_indata_add (xform_plugin,
		                              XMMS_STREAM_TYPE_PRIORITY,
		                              priority,
		                              XMMS_STREAM_TYPE_MIMETYPE,
		                              "application/x-url",
		                              XMMS_STREAM_TYPE_URL,
		                              tmp,
		                              XMMS_STREAM_TYPE_END);
		g_free (tmp);
	}

	return TRUE;
}

static gboolean
xmms_gvfs_init (xmms_xform_t *xform)
{
	xmms_gvfs_data_t *data;
	GFile *file;
	GFileInfo *info;
	GFileInputStream *handle;
	GError *error = NULL;
	const gchar *url;

	url = xmms_xform_indata_get_str (xform, XMMS_STREAM_TYPE_URL);
	g_return_val_if_fail (url, FALSE);

	/* This is an ugly hack to handle files with
	   chars needing url encoding */
	if (!g_ascii_strncasecmp (url, "file://", 7)) {
		file = g_file_new_for_path (url+7);
	} else {
		file = g_file_new_for_uri (url);
	}
	handle = g_file_read (file, NULL, &error);
	g_object_unref (file);

	if (!handle) {
		xmms_log_error ("Failed to upen url %s for reading: %s",
		                url, error->message);
		return FALSE;
	}

	data = g_new (xmms_gvfs_data_t, 1);
	data->handle = G_INPUT_STREAM (handle);
	xmms_xform_private_data_set (xform, data);

	info = g_file_input_stream_query_info (handle, (char *)query_attributes,
	                                       NULL, &error);

	if (!info) {
		xmms_log_info ("failed to query information for %s", url);
	} else {
		int i;

		for (i = 0; i < G_N_ELEMENTS (attr_map); i++) {
			if (!g_file_info_has_attribute (info, attr_map[i].gvfs)) {
				continue;
			}

			switch (attr_map[i].type) {
				case XMMSV_TYPE_STRING: {
					gchar *attr = g_file_info_get_attribute_as_string (info,
					                                                   attr_map[i].gvfs);
					xmms_xform_metadata_set_str (xform, attr_map[i].mlib, attr);
					g_free (attr);
					break;
				}
				case XMMSV_TYPE_INT32: {
					/* right now the xform metadata api only handles strings
					 * and 32 bit ints. however the gvfs api returns uint64 for
					 * the numeric attributes we're interested in and we just
					 * pass that to the xform and pray that it doesn't overflow
					 * as we know it's unsafe.
					 */

					gint64 attr = g_file_info_get_attribute_uint64 (info,
					                                                attr_map[i].gvfs);
					xmms_xform_metadata_set_int (xform, attr_map[i].mlib, attr);
					break;
				}
				default:
					g_assert_not_reached ();
			}
		}

		g_object_unref (info);
	}

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/octet-stream",
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

static void
xmms_gvfs_destroy (xmms_xform_t *xform)
{
	xmms_gvfs_data_t *data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	g_object_unref (data->handle);
}

static gint
xmms_gvfs_read (xmms_xform_t *xform, gpointer buffer, gint len,
                xmms_error_t *error)
{
	gint ret;
	GError *err = NULL;
	xmms_gvfs_data_t *data = xmms_xform_private_data_get (xform);

	g_return_val_if_fail (data, -1);
	g_return_val_if_fail (!g_input_stream_is_closed (data->handle), -1);

	ret = g_input_stream_read (data->handle, buffer, len, NULL, &err);

	if (ret < 0) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, err->message);
	}

	return ret;
}

static gint64
xmms_gvfs_seek (xmms_xform_t *xform, gint64 offset,
                xmms_xform_seek_mode_t whence, xmms_error_t *error)
{
	GSeekType type;
	GError *err = NULL;
	xmms_gvfs_data_t *data = xmms_xform_private_data_get (xform);

	g_return_val_if_fail (data, -1);
	g_return_val_if_fail (!g_input_stream_is_closed (data->handle), -1);

	switch (whence) {
		case XMMS_XFORM_SEEK_CUR:
			type = G_SEEK_CUR;
			break;
		case XMMS_XFORM_SEEK_SET:
			type = G_SEEK_SET;
			break;
		case XMMS_XFORM_SEEK_END:
			type = G_SEEK_END;
			break;
	}

	if (g_seekable_seek (G_SEEKABLE (data->handle), offset, type, NULL, &err)) {
		return g_seekable_tell (G_SEEKABLE (data->handle));
	}

	xmms_error_set (error, XMMS_ERROR_GENERIC, err->message);
	return -1;
}

static gboolean
xmms_gvfs_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error)
{
	GError *err = NULL;
	GFile *file;
	GFileInfo *info;
	GFileEnumerator *enumerator;

	/* Same hack as in _init */
	if (!g_ascii_strncasecmp (url, "file://", 7)) {
		file = g_file_new_for_path (url+7);
	} else {
		file = g_file_new_for_uri (url);
	}
	enumerator = g_file_enumerate_children (file,
	                                        G_FILE_ATTRIBUTE_STANDARD_NAME ","
	                                        G_FILE_ATTRIBUTE_STANDARD_TYPE ","
	                                        G_FILE_ATTRIBUTE_STANDARD_SIZE,
	                                        G_FILE_QUERY_INFO_NONE,
	                                        NULL,
	                                        &err);

	g_object_unref (file);

	if (!enumerator) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, err->message);
		return FALSE;
	}

	while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL))) {
		guint32 child_type, flags = 0;
		guint64 child_size;
		const gchar *child_name;

		child_name = g_file_info_get_attribute_byte_string (info,
		                                                    G_FILE_ATTRIBUTE_STANDARD_NAME);
		child_type = g_file_info_get_attribute_uint32 (info,
		                                               G_FILE_ATTRIBUTE_STANDARD_TYPE);
		child_size = g_file_info_get_attribute_uint64 (info,
		                                               G_FILE_ATTRIBUTE_STANDARD_SIZE);

		if (child_type & G_FILE_TYPE_DIRECTORY) {
			flags |= XMMS_XFORM_BROWSE_FLAG_DIR;
		}

		xmms_xform_browse_add_entry (xform, child_name, flags);

		if (~child_type & G_FILE_TYPE_DIRECTORY) {
			xmms_xform_browse_add_entry_property_int (xform,
			                                          XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE,
			                                          child_size);
		}

		g_object_unref (info);
	}

	g_file_enumerator_close (enumerator, NULL, NULL);

	return TRUE;
}
