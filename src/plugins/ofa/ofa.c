/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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

/**
 * @file
 * Ofa
 */

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"

#include <glib.h>

#include "ofa1/ofa.h"

typedef enum xmms_ofa_thread_state_E {
	XMMS_OFA_WAIT = 0,
	XMMS_OFA_CALCULATE,
	XMMS_OFA_DONE,
	XMMS_OFA_ABORT,
} xmms_ofa_thread_state_t;

typedef struct xmms_ofa_data_St {
	unsigned char *buf;
	int bytes_to_read;
	int pos;

	gboolean run_ofa;
	gboolean done;

	GMutex *mutex;
	GCond *cond;
	GThread *thread;
	xmms_ofa_thread_state_t thread_state;

	char *fp;
} xmms_ofa_data_t;

static gboolean xmms_ofa_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_ofa_init (xmms_xform_t *xform);
static void xmms_ofa_destroy (xmms_xform_t *xform);
static gint xmms_ofa_read (xmms_xform_t *xform, xmms_sample_t *buf,
                                  gint len, xmms_error_t *error);
static gint64 xmms_ofa_seek (xmms_xform_t *xform, gint64 samples,
                                    xmms_xform_seek_mode_t whence,
                                    xmms_error_t *error);

static gpointer xmms_ofa_thread (gpointer arg);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN ("ofa",
                   "Open Fingerprint Architecture",
                   XMMS_VERSION,
                   "Open Fingerprint calculator",
                   xmms_ofa_plugin_setup);

static gboolean
xmms_ofa_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_ofa_init;
	methods.destroy = xmms_ofa_destroy;
	methods.read = xmms_ofa_read;
	methods.seek = xmms_ofa_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/pcm",
	                              XMMS_STREAM_TYPE_FMT_FORMAT,
	                              XMMS_SAMPLE_FORMAT_S16,
	                              XMMS_STREAM_TYPE_FMT_CHANNELS,
	                              2,
	                              XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                              44100,
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}

static gboolean
xmms_ofa_init (xmms_xform_t *xform)
{
	xmms_ofa_data_t *data;
	xmms_medialib_entry_t entry;
	xmms_medialib_session_t *session;
	GError *error = NULL;
	char *fp;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_ofa_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	data->mutex = g_mutex_new ();
	data->cond = g_cond_new ();
	data->thread = g_thread_create (xmms_ofa_thread, data, TRUE, &error);
	if (!data->thread) {
		g_mutex_free (data->mutex);
		g_free (data);
		return FALSE;
	}

	g_thread_set_priority (data->thread, G_THREAD_PRIORITY_LOW);

	data->bytes_to_read = 44100 * 135 * 4;
	data->buf = g_malloc (data->bytes_to_read);
	entry = xmms_xform_entry_get (xform);

	session = xmms_medialib_begin ();
	fp = xmms_medialib_entry_property_get_str (session, entry, "ofa_fingerprint");
	if (fp) {
		XMMS_DBG ("Entry already has ofa_fingerprint, not recalculating");
		/* keep it! */
		xmms_xform_metadata_set_str (xform, "ofa_fingerprint", fp);
		g_free (fp);
	} else {
		data->run_ofa = TRUE;
	}
	xmms_medialib_end (session);

	xmms_xform_private_data_set (xform, data);

	xmms_xform_outdata_type_copy (xform);

	return TRUE;
}

static void
xmms_ofa_destroy (xmms_xform_t *xform)
{
	xmms_ofa_data_t *data;
	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);

	g_mutex_lock (data->mutex);
	data->thread_state = XMMS_OFA_ABORT;
	g_cond_signal (data->cond);
	g_mutex_unlock (data->mutex);

	g_thread_join (data->thread);
	g_cond_free (data->cond);
	g_mutex_free (data->mutex);

	if (data->fp)
		g_free (data->fp);
	g_free (data->buf);
	g_free (data);
}

static gint
xmms_ofa_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                      xmms_error_t *error)
{
	xmms_ofa_data_t *data;
	gint read;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	read = xmms_xform_read (xform, buf, len, error);

	if (data->run_ofa && read > 0 && data->pos < data->bytes_to_read) {
		int l = MIN (data->bytes_to_read - data->pos, read);
		memcpy (data->buf + data->pos, buf, l);
		data->pos += l;
		if (data->pos == data->bytes_to_read) {
			g_mutex_lock (data->mutex);
			data->thread_state = XMMS_OFA_CALCULATE;
			g_cond_signal (data->cond);
			g_mutex_unlock (data->mutex);
			data->run_ofa = FALSE;
		}
	} else if (data->pos == data->bytes_to_read){
		if (!data->done) {
			g_mutex_lock (data->mutex);
			if (data->thread_state == XMMS_OFA_DONE) {
				xmms_xform_metadata_set_str (xform,
				                             "ofa_fingerprint",
				                             data->fp);
				data->done = TRUE;
			}
			g_mutex_unlock (data->mutex);
		}
	}

	return read;
}

static gint64
xmms_ofa_seek (xmms_xform_t *xform, gint64 samples,
                      xmms_xform_seek_mode_t whence, xmms_error_t *error)
{
	xmms_ofa_data_t *data;

	data = xmms_xform_private_data_get (xform);

	if (data->run_ofa) {
		xmms_log_info ("Seeking requested, disabling ofa calculation!");
		data->run_ofa = FALSE;
	}
	return xmms_xform_seek (xform, samples, whence, error);
}

static gpointer
xmms_ofa_thread (gpointer arg)
{
	xmms_ofa_data_t *data = (xmms_ofa_data_t *)arg;
	const char *fp;

	g_mutex_lock (data->mutex);
	while (data->thread_state == XMMS_OFA_WAIT) {
		g_cond_wait (data->cond, data->mutex);
	}
	if (data->thread_state == XMMS_OFA_ABORT) {
		g_mutex_unlock (data->mutex);
		return NULL;
	}
	g_mutex_unlock (data->mutex);

	XMMS_DBG ("Calculating fingerprint... (will consume CPU)");

	fp = ofa_create_print (data->buf,
#if G_BYTE_ORDER == G_BIG_ENDIAN
	                       OFA_BIG_ENDIAN,
#else
	                       OFA_LITTLE_ENDIAN,
#endif
	                       data->bytes_to_read/2,
	                       44100,
	                       1);

	g_mutex_lock (data->mutex);
	data->thread_state = XMMS_OFA_DONE;
	data->fp = g_strdup (fp);
	g_mutex_unlock (data->mutex);

	XMMS_DBG ("Fingerprint calculated: %s", fp);
	return NULL;
}
