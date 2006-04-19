/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 */

#include "xmms/xmms_defs.h"
#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"

#include <glib.h>

/*
 * Type definitions
 */

typedef struct xmms_nulstripper_data_St {
	guint offset;
} xmms_nulstripper_data_t;


/*
 * Function prototypes
 */

gboolean xmms_nulstripper_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_nulstripper_read (xmms_xform_t *xform, xmms_sample_t *buf,
                                   gint len, xmms_error_t *err);
static void xmms_nulstripper_destroy (xmms_xform_t *decoder);
static gboolean xmms_nulstripper_init (xmms_xform_t *decoder);
static gint64 xmms_nulstripper_seek (xmms_xform_t *xform, gint64 bytes,
                                     xmms_xform_seek_mode_t whence,
                                     xmms_error_t *err);

static guint find_offset (xmms_xform_t *xform);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN ("nulstripper",
                   "nulstripper",
                   XMMS_VERSION,
                   "Strips leading NUL bytes",
                   xmms_nulstripper_plugin_setup);

gboolean
xmms_nulstripper_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_nulstripper_init;
	methods.destroy = xmms_nulstripper_destroy;
	methods.read = xmms_nulstripper_read;
	methods.seek = xmms_nulstripper_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-nul-padded",
	                              NULL);

	xmms_magic_add ("NUL padded", "application/x-nul-padded",
	                "0 byte 0x0", NULL);

	return TRUE;
}

static void
xmms_nulstripper_destroy (xmms_xform_t *xform)
{
	xmms_nulstripper_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	g_free (data);
}

static gboolean
xmms_nulstripper_init (xmms_xform_t *xform)
{
	xmms_nulstripper_data_t *data;
	guint o;

	o = find_offset (xform);
	if (!o) {
		return FALSE;
	}

	data = g_new (xmms_nulstripper_data_t, 1);
	data->offset = o;

	xmms_xform_private_data_set (xform, data);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/octet-stream",
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

static gint
xmms_nulstripper_read (xmms_xform_t *xform,
                       xmms_sample_t *buf, gint len, xmms_error_t *err)
{
	return xmms_xform_read (xform, buf, len, err);
}

static gint64
xmms_nulstripper_seek (xmms_xform_t *xform, gint64 bytes,
                       xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_nulstripper_data_t *data;

	g_return_val_if_fail (xform, 0);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, 0);

	if (whence == XMMS_XFORM_SEEK_SET) {
		bytes += data->offset;
	}

	return xmms_xform_seek (xform, bytes, whence, err);
}

static guint
find_offset (xmms_xform_t *xform)
{
	xmms_error_t err;
	guint8 buf[4096];
	gboolean done = FALSE;
	guint offset = 0;
	gint nbytes, i;

	do {
		xmms_error_reset (&err);

		nbytes = xmms_xform_read (xform, buf, sizeof (buf), &err);

		/* check for failures */
		if (nbytes < 1) {
			return 0;
		}

		/* find first non-nul character */
		for (i = 0; i < nbytes; i++) {
			if (buf[i] != '\0') {
				done = TRUE;
				break;
			}
		}

		offset += i;
	} while (!done);

	/* skip over the NULs */
	xmms_error_reset (&err);
	xmms_xform_seek (xform, offset, XMMS_XFORM_SEEK_SET, &err);

	return offset;
}
