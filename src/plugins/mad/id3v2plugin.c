/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 */




/** @file
 * ID3v2 stuff
 */


#include "xmms/xmms_defs.h"
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

static gint xmms_id3v2_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static void xmms_id3v2_destroy (xmms_xform_t *decoder);
static gboolean xmms_id3v2_init (xmms_xform_t *decoder);
/*static gboolean xmms_id3v2_seek (xmms_xform_t *decoder, guint bytes, gint whence);*/

/*
 * Plugin header
 */

xmms_plugin_api_version_t XMMS_PLUGIN_API_VERSION = XMMS_XFORM_API_VERSION;
xmms_plugin_type_t XMMS_PLUGIN_TYPE = XMMS_PLUGIN_TYPE_XFORM;

gboolean
xmms_xform_plugin_get (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT(methods);
	methods.init = xmms_id3v2_init;
	methods.destroy = xmms_id3v2_destroy;
	methods.read = xmms_id3v2_read;
	/*
	  methods.seek
	*/

	xmms_xform_plugin_setup (xform_plugin,
	                         "id3v2",
	                         "ID3v2 handler " XMMS_VERSION,
	                         "ID3v2 parser",
	                         &methods);

	/*
	  xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	  xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	  xmms_plugin_info_add (plugin, "License", "GPL");
	  
	  xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	  xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_REWIND);
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
	guchar *buf;
	gint res;

	xmms_error_reset (&err);

	if (xmms_xform_read (xform, hbuf, 10, &err) != 10) {
		XMMS_DBG ("Couldn't read id3v2 header...");
		return FALSE;
	}

	data = g_new0 (xmms_id3v2_data_t, 1);
	xmms_xform_private_data_set (xform, data);
	
	if (!xmms_mad_id3v2_header (hbuf, &head)) {
		XMMS_DBG ("Couldn't parse id3v2 header!?");
		return FALSE;
	}

	data->len = head.len;

	buf = g_malloc (head.len);

	res = xmms_xform_read (xform, buf, head.len, &err);
	if (res != head.len) {
		XMMS_DBG ("Couldn't read id3v2 %d bytes of id3-data data (%d)", head.len, res);
		return FALSE;
	}

	xmms_mad_id3v2_parse (xform, buf, &head);

	g_free (buf);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/octet-stream",
	                             XMMS_STREAM_TYPE_END);
	return TRUE;
}

static gint
xmms_id3v2_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err)
{
	return xmms_xform_read (xform, buf, len, err);
}

#if 0
static gboolean
xmms_id3v2_seek (xmms_xform_t *xform, guint bytes, gint whence)
{
	/*
	  if (whence == set) {
	    bytes += data->len;
	  }
	 */
	return FALSE;
}
#endif
