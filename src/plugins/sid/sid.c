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


#include "xmms/xmms_defs.h"
#include "xmms/xmms_decoderplugin.h"
#include "xmms/xmms_log.h"

#include "sidplay_wrapper.h"

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#warning "CONVERT TO SAMPLE_T"

/*
 * Type definitions
 */

typedef struct xmms_sid_data_St {
	struct sidplay_wrapper *wrapper;
	gchar *buffer;
	guint buffer_length;
	guint subtune;
} xmms_sid_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_sid_new (xmms_decoder_t *decoder);
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
							  "SID decoder " XMMS_VERSION,
							  "libsidplay2 based SID decoder");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "URL", "http://sidplay2.sourceforge.net/");  
	xmms_plugin_info_add (plugin, "URL", 
					"http://www.geocities.com/SiliconValley/Lakes/5147/resid/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");


	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, 
							xmms_sid_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK, 
							xmms_sid_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, 
							xmms_sid_destroy);

	/* Can only fastforward SIDs, not rewind */
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_SUBTUNES);

	xmms_plugin_magic_add (plugin, "sidplay infofile", "audio/prs.sid",
	                       "0 string SIDPLAY INFOFILE", NULL);
	xmms_plugin_magic_add (plugin, "psid header", "audio/prs.sid",
	                       "0 string PSID", NULL);
	xmms_plugin_magic_add (plugin, "rsid header", "audio/prs.sid",
	                       "0 string RSID", NULL);

	return plugin;
}

static gboolean
xmms_sid_new (xmms_decoder_t *decoder)
{
	xmms_sid_data_t *data;

	g_return_val_if_fail (decoder, FALSE);

	data = g_new0 (xmms_sid_data_t, 1);

	data->wrapper = sidplay_wrapper_init ();

	xmms_decoder_private_data_set (decoder, data);
	
	return TRUE;
}

static void
xmms_sid_destroy (xmms_decoder_t *decoder)
{
	xmms_sid_data_t *data;

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	sidplay_wrapper_destroy (data->wrapper);

	if (data->buffer) {
		g_free (data->buffer);
	}

	g_free (data);
}


/**
 * @todo  Enable option to use STIL for media info and
 * timedatabase for song length.
 */
static void
xmms_sid_get_media_info (xmms_decoder_t *decoder)
{
	xmms_sid_data_t *data;
	char artist[32];
	char title[32];
	gint err;
	xmms_medialib_entry *entry;
	
	entry = xmms_playlist_entry_new (NULL);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	err = sidplay_wrapper_songinfo (data->wrapper, artist, title);
	if (err > 0) {
		xmms_medialib_entry_property_set (entry, 
				XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, title);
		xmms_medialib_entry_property_set (entry, 
				XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST, artist);
		xmms_decoder_entry_mediainfo_set (decoder,entry);
	}
}

static gboolean
xmms_sid_decode_block (xmms_decoder_t *decoder)
{
	xmms_sid_data_t *data;
	gchar out[4096];
	xmms_transport_t *transport;
	gint len,ret;
	xmms_error_t error;

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	/* We need to load whole song from transport,
	   but that should be no problem, as SIDs generally are small */
	if (!data->buffer) {
		gint numsubtunes;
		/*
		const gchar *suburi;
		gchar *suburiend;
		*/
		transport = xmms_decoder_transport_get (decoder);
		g_return_val_if_fail (transport, FALSE);

		/* suburi = xmms_transport_suburl_get (transport); 
		data->subtune = strtol (suburi, &suburiend, 0);
		if (*suburiend != 0) {
			xmms_log_error ("Bad suburi: %s, using default subsong", suburi);
			data->subtune = 0;
		}
		*/
		
		len = 0;
		data->buffer_length = xmms_transport_size (transport);
		data->buffer = g_malloc (data->buffer_length);
		
		while (len<data->buffer_length) {
			ret = xmms_transport_read (transport, data->buffer + len,
									   data->buffer_length, &error);
			
			if ( ret <= 0 ) {
				g_free (data->buffer);
				data->buffer = NULL;
				return FALSE;
			}
			len += ret;
			g_assert (len >= 0);
		}
		
		ret = sidplay_wrapper_load (data->wrapper, data->buffer, 
									data->buffer_length);
		
		if (ret < 0) {
			xmms_log_error ("Load failed: %d", ret);
			return FALSE;
		}

		numsubtunes = sidplay_wrapper_subtunes (data->wrapper);
		XMMS_DBG ("subtunes: %d", numsubtunes);
		if (data->subtune > numsubtunes || data->subtune < 0) {
			xmms_log_error ("Requested subtune %d not found, using default", 
					  data->subtune);
			data->subtune = 0;
		}

		sidplay_wrapper_set_subtune (data->wrapper, data->subtune);

		xmms_sid_get_media_info (decoder);

		/** @todo don't you just love hardcoded values? */
		xmms_decoder_samplerate_set (decoder, 44100); 


	}

	ret = sidplay_wrapper_play (data->wrapper, out, sizeof (out));
	if (!ret) {
		xmms_log_error ("play err: %s", sidplay_wrapper_error (data->wrapper));
		return FALSE;
	} else {
		xmms_decoder_write (decoder, out, ret);
	}
	return TRUE;
}

