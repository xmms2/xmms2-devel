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




/*
 * SID-plugin using libsidplay2
 * 
 * Written by Anders Gustafsson - andersg@xmms.org
 *
 */


#include "xmms/plugin.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/transport.h"

#include "sidplay_wrapper.h"

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

/*
 * Type definitions
 */

typedef struct xmms_sid_data_St {
	struct sidplay_wrapper *wrapper;
	gchar *buffer;
	guint buffer_length;
} xmms_sid_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_sid_can_handle (const gchar *mimetype);
static gboolean xmms_sid_new (xmms_decoder_t *decoder, const gchar *mimetype);
static gboolean xmms_sid_decode_block (xmms_decoder_t *decoder);
static void xmms_sid_get_media_info (xmms_decoder_t *decoder);
static void xmms_sid_destroy (xmms_decoder_t *decoder);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER, "sid",
			"SID decoder " VERSION,
			"libsidplay2 based SID decoder");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "URL", "http://sidplay2.sourceforge.net/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");


	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_sid_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_sid_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK, xmms_sid_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_sid_destroy);

	/* Can only fastforward SIDs, not rewind */
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_SUBTUNES);

	return plugin;
}

static gboolean
xmms_sid_new (xmms_decoder_t *decoder, const gchar *mimetype)
{
	xmms_sid_data_t *data;

	g_return_val_if_fail (decoder, FALSE);
	g_return_val_if_fail (mimetype, FALSE);

	data = g_new0 (xmms_sid_data_t, 1);

	data->wrapper=sidplay_wrapper_init();

	xmms_decoder_plugin_data_set (decoder, data);
	
	return TRUE;
}

static void
xmms_sid_destroy (xmms_decoder_t *decoder)
{
	xmms_sid_data_t *data;

	data = xmms_decoder_plugin_data_get (decoder);
	g_return_if_fail (data);

	sidplay_wrapper_destroy(data->wrapper);

	if (data->buffer) {
		g_free(data->buffer);
	}

	g_free(data);
}

static void
xmms_sid_get_media_info (xmms_decoder_t *decoder)
{
/** @todo - read STIL.txt */
/*
	xmms_playlist_entry_t *entry = g_new0 (xmms_playlist_entry_t, 1);
	strncpy(entry->title,"sid..",XMMS_PL_PROPERTY);
	xmms_decoder_set_mediainfo (decoder,entry);
*/
}

static gboolean
xmms_sid_can_handle (const gchar *mimetype)
{
	g_return_val_if_fail (mimetype, FALSE);
	
	if ((g_strcasecmp (mimetype, "audio/prs.sid") == 0))
		return TRUE;

	return FALSE;
}

static gboolean
xmms_sid_decode_block (xmms_decoder_t *decoder)
{
	xmms_sid_data_t *data;
	gchar out[4096];
	xmms_transport_t *transport;
	gint len,ret;

	data = xmms_decoder_plugin_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	/* We need to load whole song from transport,
	   but that should be no problem, as SIDs generally are small */
	if (!data->buffer) {
		gint subtune, numsubtunes;
		const gchar *suburi;
		gchar *suburiend;
		transport = xmms_decoder_transport_get (decoder);
		g_return_val_if_fail (transport, FALSE);

		suburi = xmms_transport_suburi_get (transport);
		subtune = strtol (suburi, &suburiend, 0);
		if (*suburiend != 0) {
			XMMS_DBG ("Bad suburi: %s, using default subsong", 
				  suburi);
			subtune = 0;
		}

		len = 0;
		data->buffer_length = xmms_transport_size (transport);
		data->buffer = g_malloc (data->buffer_length);
		
		while (len<data->buffer_length) {
			ret = xmms_transport_read (transport, data->buffer+len,
						   data->buffer_length);

			if ( ret < 0 ) {
				g_free (data->buffer);
				data->buffer=NULL;
				return FALSE;
			}
			len += ret;
			g_assert (len >= 0);
		}
		
		ret = sidplay_wrapper_load (data->wrapper, data->buffer, 
					    data->buffer_length);
		if (ret < 0) {
			XMMS_DBG ("Load failed: %d", ret);
			return FALSE;
		}

		numsubtunes = sidplay_wrapper_subtunes (data->wrapper);
		XMMS_DBG ("subtunes: %d", numsubtunes);
		if (subtune > numsubtunes || subtune < 0) {
			XMMS_DBG ("Requested subtune %d not found, using default", subtune);
			subtune = 0;
		}

		sidplay_wrapper_set_subtune(data->wrapper,subtune);

		xmms_sid_get_media_info (decoder);

		/** @todo don't you just love hardcoded values? */
		xmms_decoder_samplerate_set (decoder, 44100); 


	}

	ret = sidplay_wrapper_play (data->wrapper, out, sizeof(out));
	if (!ret) {
		XMMS_DBG ("play err: %s", sidplay_wrapper_error(data->wrapper));
		return FALSE;
	} else {
		xmms_decoder_write (decoder, out, ret);
	}
	return TRUE;
}

