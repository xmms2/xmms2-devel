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

#include "xmms/xmms.h"
#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/util.h"
#include "xmms/magic.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <string.h>

#include "cdae.h"

/*
 * Function prototypes
 */

static gboolean xmms_cdae_can_handle (const gchar *url);
static gboolean xmms_cdae_init (xmms_transport_t *transport, const gchar *url);
static void xmms_cdae_close (xmms_transport_t *transport);
static gint xmms_cdae_read (xmms_transport_t *transport, gchar *buffer, guint len, xmms_error_t *error);
static gint xmms_cdae_size (xmms_transport_t *transport);
static gint xmms_cdae_seek (xmms_transport_t *transport, guint offset, gint whence);
/*static GList *xmms_cdae_list (const gchar *path);*/

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, "cdae",
			"CDAE transport " XMMS_VERSION,
		 	"CD Audio Transport");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_cdae_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_cdae_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_cdae_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ, xmms_cdae_read);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SIZE, xmms_cdae_size);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_cdae_seek);
//	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_LIST, xmms_cdae_list);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_SEEK);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_LOCAL);
//	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_LIST);
	

	xmms_plugin_config_value_register (plugin, "device", "/dev/cdrom", NULL, NULL);

	
	return plugin;
}

/*
 * Member functions
 */


/*static GList *
xmms_cdae_list (const gchar *path)
{
	return NULL;
}*/

static gboolean
xmms_cdae_can_handle (const gchar *url)
{
	gchar *dec;
	g_return_val_if_fail (url, FALSE);

	dec = xmms_util_decode_path (url);

	XMMS_DBG ("xmms_cdae_can_handle (%s)", dec);
	
	if ((g_strncasecmp (dec, "cdae:", 5) == 0) || (dec[0] == '/')) {
		g_free (dec);
		return TRUE;
	}

	g_free (dec);
	return FALSE;
}

static gboolean
xmms_cdae_init (xmms_transport_t *transport, const gchar *url)
{
	xmms_cdae_data_t *data;
	xmms_cdae_toc_t *toc;
	xmms_plugin_t *plugin;
	const gchar *dev;
	gint track;

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (url, FALSE);

	track = atoi (url+9);
	
	XMMS_DBG ("xmms_cdae_init (%d)", track);

	plugin = xmms_transport_plugin_get (transport);

	dev = xmms_config_value_string_get (xmms_plugin_config_lookup (plugin, "device"));

	data = g_new0 (xmms_cdae_data_t, 1);

	data->fd = open (dev, O_RDONLY | O_NONBLOCK);
	if (data->fd == -1) {
		xmms_log_error ("Couldn't open %s as cdaudio", dev);
		return FALSE;
	}

	toc = xmms_cdae_get_toc (data->fd);
	if (!toc) {
		xmms_log_error ("Unable to read toc");
		return FALSE;
	}

	if (track < toc->first_track || track > toc->last_track) {
		xmms_log_error ("Wrong wrong...");
		g_free (toc);
		return FALSE;
	}

	data->mime = "audio/pcm-data";
	data->toc = toc;
	data->pos = LBA(toc->track[track]);
	data->endpos = LBA(toc->track[track+1]);
	data->track = track;
	xmms_transport_private_data_set (transport, data);
	xmms_transport_mimetype_set (transport, (const gchar*)data->mime);
	
	return TRUE;
}

static void
xmms_cdae_close (xmms_transport_t *transport)
{
	xmms_cdae_data_t *data;
	g_return_if_fail (transport);

	data = xmms_transport_private_data_get (transport);
	if (!data)
		return;

	close (data->fd);
	g_free (data->toc);
	g_free (data);
}

static gint
xmms_cdae_read (xmms_transport_t *transport, gchar *buffer, guint len, xmms_error_t *error)
{
	xmms_cdae_data_t *data;
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	if (data->pos < data->endpos) {
		ret = xmms_cdae_read_data (data->fd, data->pos, buffer, len);
		if (ret == 0) return -1;
		data->pos += (ret/XMMS_CDAE_FRAMESIZE);
		return ret;
	} else {
		return -1;
	}
}

static gint
xmms_cdae_seek (xmms_transport_t *transport, guint offset, gint whence)
{
	xmms_cdae_data_t *data;
	gint t;

	g_return_val_if_fail (transport, -1);
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	if (whence != XMMS_TRANSPORT_SEEK_SET) return -1;

	XMMS_DBG ("at frame %d %d", data->pos, data->endpos);

	t = offset / XMMS_CDAE_FRAMESIZE;

	data->pos += t;

	XMMS_DBG ("Seeking to frame %d", data->pos);

	return t * XMMS_CDAE_FRAMESIZE;

}

static gint
xmms_cdae_size (xmms_transport_t *transport)
{
	return 0;
}

