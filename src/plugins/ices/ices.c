/**
 * @file
 * ICES2 based effect
 */

#include "xmms/xmms.h"
#include "xmms/plugin.h"
#include "xmms/effect.h"
#include "xmms/util.h"
#include "xmms/config.h"
#include "xmms/object.h"
#include "xmms/ringbuf.h"
#include "xmms/playlist.h"
#include "xmms/playlist_entry.h"
#include "xmms/core.h"
#include "xmms/signal_xmms.h"

#include <math.h>
#include <glib.h>
#include <stdlib.h>

#include <shout/shout.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "encode.h"

typedef struct xmms_ices_data_St {
	shout_t *shout;
	vorbis_comment vc;
	encoder_state *enc;
	gint serial;

	xmms_ringbuf_t *buf;
	gint buf_size;
	gboolean write_buf;
	GThread *thread;
} xmms_ices_data_t;

static void xmms_ices_init (xmms_effect_t *effect);
static void xmms_ices_deinit (xmms_effect_t *effect);
static void xmms_ices_samplerate_set (xmms_effect_t *effect, guint rate);
static void xmms_ices_process (xmms_effect_t *effect, gchar *buf, guint len);

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_EFFECT, "ices",
					    "ICES2 Shoutplugin " XMMS_VERSION,
					    "ICES2 Shoutplugin");


	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "INFO", "http://www.icecast.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	xmms_plugin_info_add (plugin, "License", "GPL");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_ices_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DEINIT, xmms_ices_deinit);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET, xmms_ices_samplerate_set);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_PROCESS, xmms_ices_process);

	return plugin;
}

static gboolean
xmms_ices_reconnect (xmms_ices_data_t *data)
{
	if (shout_open (data->shout) == SHOUTERR_SUCCESS) {

		XMMS_DBG ("Connected to %s:%d/%s", 
				shout_get_host (data->shout),
				shout_get_port (data->shout),
				shout_get_mount (data->shout));

	} else {
		XMMS_DBG ("Couldn't connect to shoutserver");
		return FALSE;
	}
		
	return TRUE;
}

static gpointer
xmms_ices_thread (xmms_effect_t *effect)
{
	xmms_ices_data_t *data;
	GMutex *mutex;

	g_return_val_if_fail (effect, NULL);
	data = xmms_effect_plugin_data_get (effect);
	g_return_val_if_fail (data, NULL);

	mutex = g_mutex_new ();
		
	data->write_buf = TRUE;

	xmms_ringbuf_wait_used (data->buf, data->buf_size-4096, mutex);

	XMMS_DBG ("Buffering done, starting to encode");

	if (!xmms_ices_reconnect (data)) {
		data->write_buf = FALSE;
		return NULL;
	}

	while (42) {
		gchar d[4096];
		ogg_page og;
		gint ret;

		ret = xmms_ringbuf_read (data->buf, d, 4096);
		if (ret == 0) {
			xmms_ringbuf_wait_used (data->buf, data->buf_size-4096, mutex);
			continue;
		}

		if (!data->enc) {
			data->write_buf = FALSE;
			data->thread = NULL;
			return NULL;
		}

		encode_data (data->enc, d, ret, 0);

		while (encode_dataout (data->enc, &og) > 0) {
			if (shout_send_raw (data->shout, og.header, og.header_len) < 0) {
				if (!xmms_ices_reconnect (data)) {
					data->write_buf = FALSE;
					return NULL;
				}
			} else if (shout_send_raw (data->shout, og.body, og.body_len) < 0) {
				if (!xmms_ices_reconnect (data)) {
					data->write_buf = FALSE;
					return NULL;
				}

			}
		}
			
	}

	return NULL;
}

static void
xmms_ices_init (xmms_effect_t *effect) 
{
	xmms_ices_data_t *data;
	xmms_config_value_t *val;
	const gchar *host, *port;

	shout_init ();
	
	data = g_new0 (xmms_ices_data_t, 1);
	data->shout = shout_new ();

	shout_set_format (data->shout, SHOUT_FORMAT_VORBIS);
	shout_set_protocol (data->shout, SHOUT_PROTOCOL_HTTP);

	val = xmms_plugin_config_value_register (xmms_effect_plugin_get (effect),
						 "encodingnombr",
						 "64000",
						 NULL,
						 NULL);

	val = xmms_plugin_config_value_register (xmms_effect_plugin_get (effect),
						 "host",
						 "localhost",
						 NULL,
						 NULL);
	host = xmms_config_value_string_get (val);
	shout_set_host (data->shout, xmms_config_value_string_get (val));

	val = xmms_plugin_config_value_register (xmms_effect_plugin_get (effect),
						 "port",
						 "8000",
						 NULL,
						 NULL);
	port = xmms_config_value_string_get (val);
	shout_set_port (data->shout, xmms_config_value_int_get (val));

	val = xmms_plugin_config_value_register (xmms_effect_plugin_get (effect),
						 "password",
						 "hackme",
						 NULL,
						 NULL);
	shout_set_password (data->shout, xmms_config_value_string_get (val));

	val = xmms_plugin_config_value_register (xmms_effect_plugin_get (effect),
						 "user",
						 "source",
						 NULL,
						 NULL);
	shout_set_user (data->shout, xmms_config_value_string_get (val));
	
	shout_set_agent (data->shout, "XMMS/" XMMS_VERSION);

	val = xmms_plugin_config_value_register (xmms_effect_plugin_get (effect),
						 "mount",
						 "/stream.ogg",
						 NULL,
						 NULL);
	shout_set_mount (data->shout, xmms_config_value_string_get (val));

	val = xmms_plugin_config_value_register (xmms_effect_plugin_get (effect),
						 "public",
						 "0",
						 NULL,
						 NULL);
	shout_set_public (data->shout, xmms_config_value_int_get (val));

	val = xmms_plugin_config_value_register (xmms_effect_plugin_get (effect),
						 "streamname",
						 "",
						 NULL,
						 NULL);
	shout_set_name (data->shout, xmms_config_value_string_get (val));

	val = xmms_plugin_config_value_register (xmms_effect_plugin_get (effect),
						 "streamdescription",
						 "",
						 NULL,
						 NULL);
	shout_set_description (data->shout, xmms_config_value_string_get (val));
	
	val = xmms_plugin_config_value_register (xmms_effect_plugin_get (effect),
						 "streamgenre",
						 "",
						 NULL,
						 NULL);
	shout_set_genre (data->shout, xmms_config_value_string_get (val));

	val = xmms_plugin_config_value_register (xmms_effect_plugin_get (effect),
						 "streamurl",
						 "",
						 NULL,
						 NULL);
	shout_set_url (data->shout, xmms_config_value_string_get (val));

	data->write_buf = FALSE;
	val = xmms_plugin_config_value_register (xmms_effect_plugin_get (effect),
						 "buffersize",
						 "65536",
						 NULL,
						 NULL);
	data->buf = xmms_ringbuf_new (xmms_config_value_int_get (val));
	data->buf_size = xmms_config_value_int_get (val);

	data->serial = 1;
	
	xmms_effect_plugin_data_set (effect, data);

	data->thread = g_thread_create ((GThreadFunc) xmms_ices_thread, (gpointer) effect, TRUE, NULL);
}

static void
xmms_ices_deinit (xmms_effect_t *effect) 
{
}

static void
xmms_ices_samplerate_set (xmms_effect_t *effect, guint rate)
{
	xmms_ices_data_t *data;
	xmms_config_value_t *val;
	const xmms_playlist_entry_t *entry;
	gint nombr;
	gchar *vc;
	g_return_if_fail (effect);

	data = xmms_effect_plugin_data_get (effect);
	g_return_if_fail (data);

	val = xmms_plugin_config_lookup (xmms_effect_plugin_get (effect), "encodingnombr");
	nombr = xmms_config_value_int_get (val);
	XMMS_DBG ("Inited a encoder with rate %d nombr %d", rate, nombr);

	entry = xmms_core_playlist_entry_mediainfo (xmms_core_get_id ());

	vorbis_comment_clear (&data->vc);
	vorbis_comment_init (&data->vc);

	vc = g_strdup_printf ("title=%s", 
			      xmms_playlist_entry_property_get (entry, 
				      XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE));
	vorbis_comment_add (&data->vc, vc);
	g_free (vc);

	vc = g_strdup_printf ("artist=%s",
			      xmms_playlist_entry_property_get (entry,
				      XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST));
	vorbis_comment_add (&data->vc, vc);
	g_free (vc);

	vc = g_strdup_printf ("album=%s",
			      xmms_playlist_entry_property_get (entry,
				      XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM));
	vorbis_comment_add (&data->vc, vc);
	g_free (vc);

	data->enc = encode_initialise (2, rate, 1, -1, nombr, -1, 
				       3, data->serial++, &data->vc);
	if (!data->enc)
		return;
	
}

static void
xmms_ices_process (xmms_effect_t *effect, gchar *buf, guint len)
{
	xmms_ices_data_t *data;

	g_return_if_fail (effect);

	data = xmms_effect_plugin_data_get (effect);
	g_return_if_fail (data);

	if (data->write_buf) 
		xmms_ringbuf_write (data->buf, buf, len);
}
