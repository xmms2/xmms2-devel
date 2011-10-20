/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 */




/** @file
 * ID3v2 stuff
 */


#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"
#include "id3.h"

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * Type definitions
 */

typedef struct xmms_id3v2_data_St {
	guint len;
} xmms_id3v2_data_t;


/*
 * Function prototypes
 */

gboolean xmms_id3v2_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static void xmms_id3v2_destroy (xmms_xform_t *decoder);
static gboolean xmms_id3v2_init (xmms_xform_t *decoder);
static gint64 xmms_id3v2_seek (xmms_xform_t *xform, gint64 bytes, xmms_xform_seek_mode_t whence, xmms_error_t *err);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN ("id3v2",
                   "ID3v2 parser",
                   XMMS_VERSION,
                   "ID3v2 tag container handler",
                   xmms_id3v2_plugin_setup);

gboolean
xmms_id3v2_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_id3v2_init;
	methods.destroy = xmms_id3v2_destroy;
	methods.read = xmms_xform_read;
	methods.seek = xmms_id3v2_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	/*
	  xmms_plugin_info_add (plugin, "URL", "http://xmms2.org/");
	  xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	  xmms_plugin_info_add (plugin, "License", "GPL");
	*/

	/* xmms_xform_indata_constraint_add */
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/id3v2",
	                              NULL);

	xmms_magic_add ("id3 header", "application/id3v2",
	                "0 string ID3",
	                ">3 byte <0xff",
	                ">>4 byte <0xff",
	                NULL);

	return TRUE;
}

static void
xmms_id3v2_destroy (xmms_xform_t *xform)
{
	xmms_id3v2_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	g_free (data);

}

static gboolean
xmms_id3v2_init (xmms_xform_t *xform)
{
	xmms_id3v2_data_t *data;
	xmms_id3v2_header_t head;
	xmms_error_t err;
	guchar hbuf[20];
	gint filesize;
	guchar *buf;
	gint res;
	const gchar *metakey;

	xmms_error_reset (&err);

	if (xmms_xform_read (xform, hbuf, 10, &err) != 10) {
		XMMS_DBG ("Couldn't read id3v2 header...");
		return FALSE;
	}

	data = g_new0 (xmms_id3v2_data_t, 1);
	xmms_xform_private_data_set (xform, data);

	if (!xmms_id3v2_is_header (hbuf, &head)) {
		XMMS_DBG ("Couldn't parse id3v2 header!?");
		return FALSE;
	}

	/* Total data length is the length of header data plus header bytes */
	data->len = head.len + 10;

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	if (xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
		xmms_xform_metadata_set_int (xform, metakey, filesize - head.len);
	}


	buf = g_malloc (head.len);

	res = xmms_xform_read (xform, buf, head.len, &err);
	if (res != head.len) {
		XMMS_DBG ("Couldn't read id3v2 %d bytes of id3-data data (%d)", head.len, res);
		return FALSE;
	}

	xmms_id3v2_parse (xform, buf, &head);

	g_free (buf);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/octet-stream",
	                             XMMS_STREAM_TYPE_END);
	return TRUE;
}

static gint64
xmms_id3v2_seek (xmms_xform_t *xform, gint64 bytes, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_id3v2_data_t *data;
	int ret;

	g_return_val_if_fail (xform, 0);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, 0);

	if (whence == XMMS_XFORM_SEEK_SET) {
		bytes += data->len;
	}

	ret = xmms_xform_seek (xform, bytes, whence, err);

	if (ret == -1) {
		return -1;
	}

	ret -= data->len;

	return ret;
}
