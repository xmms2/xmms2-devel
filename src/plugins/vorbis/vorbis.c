/** @file
 *  OGG / Vorbis decoder for XMMS2
 *
 *  This uses a lot of decoder_example.c's code from vorbis_tools
 * 
 */
#include "xmms/plugin.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/output.h"
#include "xmms/transport.h"

#include <math.h>
#include <vorbis/codec.h>

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * Type definitions
 */


typedef struct xmms_vorbis_data_St {
	ogg_sync_state oy;
	ogg_page og;
	ogg_packet op;
	ogg_stream_state os;
	vorbis_info vi;
	vorbis_comment vc;
	vorbis_block vb;
	vorbis_dsp_state vd;

	gint convsize;
	gint inited;
} xmms_vorbis_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_vorbis_can_handle (const gchar *mimetype);
static gboolean xmms_vorbis_new (xmms_decoder_t *decoder, const gchar *mimetype);
static gboolean xmms_vorbis_decode_block (xmms_decoder_t *decoder);
static void xmms_vorbis_get_media_info (xmms_decoder_t *decoder);
static void xmms_vorbis_destroy (xmms_decoder_t *decoder);
static gboolean xmms_vorbis_init (xmms_decoder_t *decoder);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER, "vorbis",
			"Vorbis decoder " VERSION,
			"Xiph's OGG/Vorbis decoder");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "URL", "http://www.xiph.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_vorbis_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_vorbis_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK, xmms_vorbis_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_vorbis_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_vorbis_init);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_REWIND);

	return plugin;
}

static void
xmms_vorbis_destroy (xmms_decoder_t *decoder)
{
	xmms_vorbis_data_t *data;

	g_return_if_fail (decoder);

	data = xmms_decoder_plugin_data_get (decoder);
	g_return_if_fail (data);

	ogg_stream_clear (&data->os);
	vorbis_block_clear (&data->vb);
	vorbis_dsp_clear (&data->vd);
	vorbis_comment_clear (&data->vc);
	vorbis_info_clear (&data->vi);
	ogg_sync_clear (&data->oy);

	g_free (data);

}

static void
xmms_vorbis_get_media_info (xmms_decoder_t *decoder)
{
	xmms_transport_t *transport;
	xmms_playlist_entry_t *entry;
	xmms_vorbis_data_t *data;
	gchar **ptr;

	g_return_if_fail (decoder);

	data = xmms_decoder_plugin_data_get (decoder);
	g_return_if_fail (data);

	transport = xmms_decoder_transport_get (decoder);
	g_return_if_fail (transport);

	ptr = data->vc.user_comments;
	entry = xmms_playlist_entry_new (NULL);

	if (data->vi.bitrate_nominal) {
		gint duration;
		gint fsize = xmms_transport_size (xmms_decoder_transport_get (decoder)) * 8;

		XMMS_DBG ("nominal bitrate %d", data->vi.bitrate_nominal);

		if (!fsize) {
			xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_DURATION, "-1");
		} else {
			gchar *tmp;
			duration = fsize / data->vi.bitrate_nominal;
			tmp = g_strdup_printf ("%d", duration);

			xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_DURATION, tmp);
			g_free (tmp);
		}
	}

	if (ptr) {
		while (*ptr) {
			gchar **s;

			s = g_strsplit (*ptr, "=", 2);
			if (s && s[0] && s[1]) {
				XMMS_DBG ("Vorbis comment: %s=%s", s[0], g_locale_from_utf8 (s[1], -1, NULL, NULL, NULL));
				xmms_playlist_entry_set_prop (entry, s[0], s[1]);
			}

			g_strfreev (s);
			
			++ptr;
		}
	}
	

	xmms_playlist_entry_set_prop (entry, "vendor", data->vc.vendor);

	xmms_core_set_mediainfo (entry);

}

static gboolean
xmms_vorbis_can_handle (const gchar *mimetype)
{
	g_return_val_if_fail (mimetype, FALSE);
	
	if ((g_strcasecmp (mimetype, "application/ogg") == 0))
		return TRUE;

	return FALSE;

}

static gboolean
xmms_vorbis_new (xmms_decoder_t *decoder, const gchar *mimetype)
{
	xmms_vorbis_data_t *data;

	g_return_val_if_fail (decoder, FALSE);
	g_return_val_if_fail (mimetype, FALSE);

	data = g_new0 (xmms_vorbis_data_t, 1);
	ogg_sync_init (&data->oy);
	data->inited = 0;

	xmms_decoder_plugin_data_set (decoder, data);
	
	return TRUE;
}

static gboolean
xmms_vorbis_init (xmms_decoder_t *decoder)
{
	xmms_vorbis_data_t *data;
	xmms_transport_t *transport;
	gchar *buffer;
	gint ret;
	gint i;

	g_return_val_if_fail (decoder, FALSE);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	data = xmms_decoder_plugin_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	buffer = ogg_sync_buffer (&data->oy, 4096);
	ret = xmms_transport_read (transport, buffer, 4096);
	XMMS_DBG ("Hmm : Got %d bytes", ret);
	ogg_sync_wrote (&data->oy, ret);
	
	if (ogg_sync_pageout (&data->oy, &data->og)!=1) {
		if (ret < 4096) {
			return FALSE;
		}

		XMMS_DBG ("This can't be a Oggstream. Are you lying?");

		return FALSE;
	}

	ogg_stream_init (&data->os, ogg_page_serialno (&data->og));
	vorbis_info_init (&data->vi);
	vorbis_comment_init (&data->vc);

	if (ogg_stream_pagein (&data->os, &data->og) < 0) {
		XMMS_DBG ("Error reading first page of Ogg stream");
		return FALSE;
	}

	if (ogg_stream_packetout (&data->os, &data->op) != 1) {
		XMMS_DBG ("Error reading first header!");
		return FALSE;
	}
	
	if (vorbis_synthesis_headerin (&data->vi, NULL, &data->op) < 0) {
		XMMS_DBG ("This is OGG but no vorbis inside?!");
		return FALSE;
	}

	i=0;
	while (i < 2) {
		while (i < 2) {
			int result = ogg_sync_pageout (&data->oy, &data->og);
			if (result == 0) break;

			if (result == 1) {
				ogg_stream_pagein (&data->os, &data->og);

				while (i < 2) {
					result = ogg_stream_packetout (&data->os, &data->op);
					if (result == 0) break;
					if (result < 0) {
						XMMS_DBG ("Corrupted datastream");
						return FALSE;
					}

					XMMS_DBG ("synthesis_headerin");
					vorbis_synthesis_headerin (&data->vi, &data->vc, &data->op);
					i++;
				}
			}
		}

		buffer = ogg_sync_buffer (&data->oy, 4096);
		ret = xmms_transport_read (transport, buffer, 4096);
		XMMS_DBG ("Got %d bytes", ret);

		if (ret == 0 && i < 2) {
			XMMS_DBG ("EOF before the whole header...");
			return FALSE;
		}

		ogg_sync_wrote (&data->oy, ret);
	}

	data->convsize = 4096/data->vi.channels;
	
	vorbis_synthesis_init (&data->vd, &data->vi);
	vorbis_block_init (&data->vd, &data->vb);

	xmms_vorbis_get_media_info (decoder);

	XMMS_DBG ("vorbis_init ok!");

	data->inited = 1;
	return TRUE;
}

static gboolean
xmms_vorbis_decode_block (xmms_decoder_t *decoder)
{
	xmms_vorbis_data_t *data;
	xmms_transport_t *transport;
	xmms_output_t *output;
	gchar *buffer;
	gint res;
	gint ret;

	data = xmms_decoder_plugin_data_get (decoder);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	output = xmms_decoder_output_get (decoder);
	g_return_val_if_fail (output, FALSE);

	buffer = ogg_sync_buffer (&data->oy, 4096);
	ret = xmms_transport_read (transport, buffer, 4096);
	ogg_sync_wrote (&data->oy, ret);
	if (ret == 0) 
		return FALSE;

	res = ogg_sync_pageout (&data->oy, &data->og);
	if (res == 0) return TRUE; /* need more data */

	if (res < 0) {
		XMMS_DBG ("Corrupted data, but continuing anyway");
		return TRUE;
	}

	ogg_stream_pagein (&data->os, &data->og);
	while (1) {
		float **pcm;
		int samples;
		res = ogg_stream_packetout (&data->os, &data->op);
		ogg_int16_t convbuffer[4096];

		if (res == 0) break;

		if (vorbis_synthesis (&data->vb, &data->op) == 0)
			vorbis_synthesis_blockin (&data->vd, &data->vb);


		while ((samples = vorbis_synthesis_pcmout (&data->vd, &pcm)) > 0) {
			int i,j;
			int bout = (samples<data->convsize?samples:data->convsize);

			for (i = 0; i < data->vi.channels; i++) {
				ogg_int16_t *ptr = convbuffer+i;
				float *mono = pcm[i];
				for (j = 0; j < bout; j++) {
					int val = mono[j]*32767.f;
					
					/* clipping */
					if (val > 32767) {
						val = 32767;
					}

					if (val < -32767) {
						val = -32767;
					}

					*ptr = val;
					ptr += data->vi.channels;

				}
			}

			xmms_output_write (output, convbuffer, (2*data->vi.channels)*bout);
			vorbis_synthesis_read (&data->vd, bout);
		}

		if (ogg_page_eos (&data->og)) {
			return FALSE;
		}

	}
	
	return TRUE;
}

