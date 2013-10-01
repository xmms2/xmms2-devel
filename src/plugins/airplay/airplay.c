/*
 *  XMMS2 - Airplay: Airport Express Output Plugin
 *  Copyright (C) 2005-2006 Mohsin Patel <mohsin.patel@gmail.com>
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

#include <xmms/xmms_outputplugin.h>
#include <xmms/xmms_log.h>
#include <xmms_configuration.h>

#include <glib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/param.h>
#include <errno.h>
#include "raop_client.h"

typedef enum {
	STATE_IDLE,
	STATE_CONNECT,
	STATE_RUNNING,
	STATE_DISCONNECT,
	STATE_FLUSH,
	STATE_QUIT,
} xmms_airplay_state_t;


#define AIRPLAY_IO_TIMEOUT 5

/*
 * Type definitions
 */
typedef struct xmms_airplay_data_St {
	GThread *thread;
	GMutex raop_mutex;
	gint wake_pipe[2];
	xmms_airplay_state_t state;
	gdouble volume;
} xmms_airplay_data_t;

/*
 * Function prototypes
 */
static gboolean xmms_airplay_plugin_setup (xmms_output_plugin_t *plugin);
static gboolean xmms_airplay_new (xmms_output_t *output);
static void xmms_airplay_destroy (xmms_output_t *output);
static gboolean xmms_airplay_volume_get (xmms_output_t *output,
                                         const gchar **names,
                                         guint *values,
                                         guint *num_channels);
static gboolean xmms_airplay_volume_set (xmms_output_t *output,
                                         const gchar *channel, guint volume);
static gboolean xmms_airplay_status (xmms_output_t *output,
                                     xmms_playback_status_t status);
static void xmms_airplay_flush (xmms_output_t *output);
static guint xmms_airplay_buffersize_get (xmms_output_t *output);

static gpointer xmms_airplay_thread (gpointer arg);
static int xmms_airplay_stream_cb (void *arg, guchar *buf, int len);

/*
 * Plugin header
 */

XMMS_OUTPUT_PLUGIN_DEFINE ("airplay", "AirPlay Output", XMMS_VERSION,
                           "Airport Express output plugin",
                           xmms_airplay_plugin_setup);

static gboolean
xmms_airplay_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);

	methods.new = xmms_airplay_new;
	methods.destroy = xmms_airplay_destroy;

	methods.volume_get = xmms_airplay_volume_get;
	methods.volume_set = xmms_airplay_volume_set;

	methods.latency_get = xmms_airplay_buffersize_get;
	methods.status = xmms_airplay_status;
	methods.flush = xmms_airplay_flush;

	xmms_output_plugin_methods_set (plugin, &methods);

	xmms_output_plugin_config_property_register (plugin,
	                                             "airport_address",
	                                             RAOP_DEFAULT_IP, NULL, NULL);
	return TRUE;
}

/*
 * Member functions
 */


/**
 * Creates data for new plugin.
 */
static gboolean
xmms_airplay_new (xmms_output_t *output)
{
	xmms_airplay_data_t *data;

	g_return_val_if_fail (output, FALSE);

	data = g_new0 (xmms_airplay_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	g_mutex_init (&data->raop_mutex);
	xmms_output_private_data_set (output, data);

	/* We only do 44.1KHz, 16-bit, stereo */
	xmms_output_format_add (output, XMMS_SAMPLE_FORMAT_S16, 2, 44100);

	if (pipe (data->wake_pipe) < 0)
		return FALSE;

	data->thread = g_thread_new ("x2 airplay", xmms_airplay_thread, (gpointer) output);

	return TRUE;
}

/**
 * Frees data for this plugin allocated in xmms_airplay_new.
 */
static void
xmms_airplay_destroy (xmms_output_t *output)
{
	xmms_airplay_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	g_mutex_lock (&data->raop_mutex);
	data->state = STATE_QUIT;
	write (data->wake_pipe[1], "X", 1);
	g_mutex_unlock (&data->raop_mutex);

	g_thread_join (data->thread);

	g_mutex_clear (&data->raop_mutex);
	g_free (data);
}

static int
xmms_airplay_stream_cb (void *arg, guchar *buf, int len)
{
	xmms_output_t *output = (xmms_output_t *)arg;

	return xmms_output_read (output, (gchar *)buf, len);
}

/**
 * The workhorse routine
 * XXX: requires more robust network i/o error handling
 * 	e.g. try re-connecting in case of failure
 */
static gpointer
xmms_airplay_thread (gpointer arg)
{
	xmms_output_t *output = (xmms_output_t *)arg;
	xmms_config_property_t *val;
	const gchar *tmp;
	xmms_airplay_data_t *data;
	raop_client_t *rc;
	fd_set rfds, wfds, efds;
	struct timeval timeout;
	int stream_fd;
	int rtsp_fd;
	int wake_fd;
	int max_fd;
	int ret;
	gdouble prev_vol = 0.0;

	data = xmms_output_private_data_get (output);
	wake_fd = data->wake_pipe[0];

	ret = raop_client_init (&rc);
	if (ret != RAOP_EOK)
		return NULL;

	g_mutex_lock (&data->raop_mutex);
	while (data->state != STATE_QUIT) {
		switch (data->state) {
		case STATE_IDLE:
			g_mutex_unlock (&data->raop_mutex);
			FD_ZERO (&rfds);
			FD_SET (wake_fd, &rfds);
			select (wake_fd+1, &rfds, NULL, NULL, NULL);
			{
				char cmd;
				read (wake_fd, &cmd, 1);
			}
			g_mutex_lock (&data->raop_mutex);
			break;
		case STATE_CONNECT:
			g_mutex_unlock (&data->raop_mutex);
			val = xmms_output_config_lookup (output, "airport_address");
			tmp = xmms_config_property_get_string (val);
			XMMS_DBG ("Connecting to %s", tmp);
			ret = raop_client_connect (rc, tmp, RAOP_DEFAULT_PORT);
			g_mutex_lock (&data->raop_mutex);
			if (ret == RAOP_EOK) {
				raop_client_set_stream_cb (rc, xmms_airplay_stream_cb, output);
				raop_client_get_volume (rc, &data->volume);
				prev_vol = data->volume;
				XMMS_DBG ("Connected!");
				data->state = STATE_RUNNING;
			} else {
				xmms_error_t error;
				data->state = STATE_IDLE;
				xmms_error_set (&error, XMMS_ERROR_GENERIC,
				                "Error connecting");
				g_mutex_unlock (&data->raop_mutex);
				xmms_output_set_error (output, &error);
				g_mutex_lock (&data->raop_mutex);
			}
			break;
		case STATE_RUNNING:
			break;
		case STATE_FLUSH:
			XMMS_DBG ("Flushing...");
			g_mutex_unlock (&data->raop_mutex);
			raop_client_flush (rc);
			g_mutex_lock (&data->raop_mutex);
			data->state = STATE_RUNNING;
			break;
		case STATE_DISCONNECT:
			XMMS_DBG ("Disconnecting...");
			g_mutex_unlock (&data->raop_mutex);
			raop_client_disconnect (rc);
			g_mutex_lock (&data->raop_mutex);
			data->state = STATE_IDLE;
			break;
		default:
			break;
		}

		if (data->state != STATE_RUNNING)
			continue;

		if (data->volume != prev_vol) {
			XMMS_DBG ("Setting volume...");
			raop_client_set_volume (rc, data->volume);
			prev_vol = data->volume;
			continue;
		}

		g_mutex_unlock (&data->raop_mutex);

		FD_ZERO (&rfds);
		FD_ZERO (&wfds);
		FD_ZERO (&efds);
		timeout.tv_sec = AIRPLAY_IO_TIMEOUT;
		timeout.tv_usec = 0;

		FD_SET (wake_fd, &rfds);
		rtsp_fd = raop_client_rtsp_sock (rc);
		stream_fd = raop_client_stream_sock (rc);
		if (raop_client_can_read (rc, rtsp_fd)) {
			FD_SET (rtsp_fd, &rfds);
		}
		if (raop_client_can_write (rc, rtsp_fd)) {
			FD_SET (rtsp_fd, &wfds);
		}
		if (raop_client_can_read (rc, stream_fd)) {
			FD_SET (stream_fd, &rfds);
		}
		if (raop_client_can_write (rc, stream_fd)) {
			FD_SET (stream_fd, &wfds);
		}
		FD_SET (rtsp_fd, &efds);
		if (stream_fd != -1)
			FD_SET (stream_fd, &efds);

		max_fd = MAX (wake_fd, MAX (rtsp_fd, stream_fd));
		ret = select (max_fd + 1, &rfds, &wfds, &efds, &timeout);
		if (ret <= 0) {
			g_mutex_lock (&data->raop_mutex);
			if (!(ret == -1 && errno == EINTR)) {
				data->state = STATE_DISCONNECT;
			}
			continue;
		}
		if (FD_ISSET (wake_fd, &rfds)) {
			char cmd;
			read (wake_fd, &cmd, 1);
			g_mutex_lock (&data->raop_mutex);
			continue;
		}

		if (FD_ISSET (rtsp_fd, &rfds))
			raop_client_handle_io (rc, rtsp_fd, G_IO_IN);
		if (FD_ISSET (rtsp_fd, &wfds))
			raop_client_handle_io (rc, rtsp_fd, G_IO_OUT);
		if (FD_ISSET (rtsp_fd, &efds)) {
			raop_client_handle_io (rc, rtsp_fd, G_IO_ERR);
			g_mutex_lock (&data->raop_mutex);
			data->state = STATE_DISCONNECT;
			g_mutex_unlock (&data->raop_mutex);
		}
		if (stream_fd != -1) {
			if (FD_ISSET (stream_fd, &rfds))
				raop_client_handle_io (rc, stream_fd, G_IO_IN);
			if (FD_ISSET (stream_fd, &wfds))
				raop_client_handle_io (rc, stream_fd, G_IO_OUT);
			if (FD_ISSET (stream_fd, &efds)) {
				raop_client_handle_io (rc, stream_fd, G_IO_ERR);
				g_mutex_lock (&data->raop_mutex);
				data->state = STATE_DISCONNECT;
				g_mutex_unlock (&data->raop_mutex);
			}
		}
		g_mutex_lock (&data->raop_mutex);
	}
	g_mutex_unlock (&data->raop_mutex);
	raop_client_destroy (rc);

	XMMS_DBG ("Airplay thread exit");
	return NULL;
}

static gboolean
xmms_airplay_status (xmms_output_t *output, xmms_playback_status_t status)
{
	xmms_airplay_data_t *data;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (output, FALSE);

	if (status == XMMS_PLAYBACK_STATUS_PLAY) {
		XMMS_DBG ("STARTING PLAYBACK!");
		g_mutex_lock (&data->raop_mutex);
		if (data->state == STATE_IDLE) {
			data->state = STATE_CONNECT;
			write (data->wake_pipe[1], "X", 1);
		}
		g_mutex_unlock (&data->raop_mutex);
	} else {
		g_mutex_lock (&data->raop_mutex);
		if (data->state == STATE_RUNNING) {
			data->state = STATE_DISCONNECT;
			write (data->wake_pipe[1], "X", 1);
		}
		g_mutex_unlock (&data->raop_mutex);
	}
	return TRUE;
}

static gboolean
xmms_airplay_volume_set (xmms_output_t *output,
                         const gchar *channel,
                         guint volume)
{
	gdouble v;
	xmms_airplay_data_t *data;
	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	v = (100.0 - volume) * RAOP_MIN_VOLUME / 100;

	g_mutex_lock (&data->raop_mutex);
	data->volume = v;
	write (data->wake_pipe[1], "X", 1);
	g_mutex_unlock (&data->raop_mutex);

	return TRUE;
}

static gboolean
xmms_airplay_volume_get (xmms_output_t *output,
                         const gchar **names, guint *values,
                         guint *num_channels)
{
	gdouble v;
	xmms_airplay_data_t *data;
	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (!*num_channels) {
		*num_channels = 1;
		return TRUE;
	}

	g_return_val_if_fail (*num_channels == 1, FALSE);
	g_return_val_if_fail (names, FALSE);
	g_return_val_if_fail (values, FALSE);

	v = data->volume;

	values[0] = (guint) (100 - (100 * v) / RAOP_MIN_VOLUME);
	names[0] = "master";
	return TRUE;
}

/**
 *  this gets called both on song change and song seek
 */
static void
xmms_airplay_flush (xmms_output_t *output)
{
	xmms_airplay_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	XMMS_DBG ("Airplay flushing requested");
	g_mutex_lock (&data->raop_mutex);
	if (data->state == STATE_RUNNING) {
		data->state = STATE_FLUSH;
		write (data->wake_pipe[1], "X", 1);
	}
	g_mutex_unlock (&data->raop_mutex);
}

static guint
xmms_airplay_buffersize_get (xmms_output_t *output)
{
	xmms_airplay_data_t *data;

	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	/* XXX: return size of RAOP write buffer? */
	return 0;
}
