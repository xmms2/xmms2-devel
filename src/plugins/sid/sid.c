/*
 * SID-plugin using libsidplay2
 */


#include "xmms/plugin.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/output.h"
#include "xmms/transport.h"

#include "sidplay_wrapper.h"

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

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

	xmms_plugin_method_add (plugin, XMMS_METHOD_CAN_HANDLE, xmms_sid_can_handle);
	xmms_plugin_method_add (plugin, XMMS_METHOD_NEW, xmms_sid_new);
	xmms_plugin_method_add (plugin, XMMS_METHOD_DECODE_BLOCK, xmms_sid_decode_block);
	xmms_plugin_method_add (plugin, XMMS_METHOD_DESTROY, xmms_sid_destroy);

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
/* FIXME - read STIL.txt
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
	xmms_output_t *output;
	gchar out[4096];
	xmms_transport_t *transport;
	gint len,ret;

	data = xmms_decoder_plugin_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	/* We need to load whole song from transport,
	   but that should be no problem, as SIDs generally are small */
	if (!data->buffer) {
		transport = xmms_decoder_transport_get (decoder);
		g_return_val_if_fail (transport, FALSE);
		
		len = data->buffer_length = xmms_transport_size (transport);
		data->buffer = g_malloc (data->buffer_length);
		
		while (len) {
			ret = xmms_transport_read (transport, data->buffer, len);
			if ( ret < 0 ) {
				g_free (data->buffer);
				data->buffer=NULL;
				return FALSE;
			}
			len -= ret;
			g_assert (len >= 0);
		}
		
		ret = sidplay_wrapper_load (data->wrapper, data->buffer, 
					    data->buffer_length);
		if (ret < 0) {
			XMMS_DBG ("Load failed: %d", ret);
			return FALSE;
		}

		xmms_sid_get_media_info (decoder);

	}

	output = xmms_decoder_output_get (decoder);
	g_return_val_if_fail (output, FALSE);

	ret = sidplay_wrapper_play (data->wrapper, out, sizeof(out));
	if (!ret) {
		XMMS_DBG ("play err: %s", sidplay_wrapper_error(data->wrapper));
		return FALSE;
	} else {
		xmms_output_write (output, out, ret);
	}
	return TRUE;
}

