/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"

#include "backend.h"

#include <glib.h>

#include <pulse/pulseaudio.h>


static struct {
	xmms_sample_format_t xmms_fmt;
	pa_sample_format_t pulse_fmt;
} xmms_pulse_formats[] = {
	{XMMS_SAMPLE_FORMAT_U8, PA_SAMPLE_U8},
#if G_BYTE_ORDER == G_LITTLE_ENDIAN /* Yes, there is PA_SAMPLE_xxNE,
                                       but they does only work
                                       if you can be sure that
                                       WORDS_BIGENDIAN is correctly
                                       defined */
	{XMMS_SAMPLE_FORMAT_S16, PA_SAMPLE_S16LE},
	{XMMS_SAMPLE_FORMAT_FLOAT, PA_SAMPLE_FLOAT32LE},
#else
	{XMMS_SAMPLE_FORMAT_S16, PA_SAMPLE_S16BE},
	{XMMS_SAMPLE_FORMAT_FLOAT, PA_SAMPLE_FLOAT32BE},
#endif
};

struct xmms_pulse {
	pa_threaded_mainloop *mainloop;
	pa_context *context;
	pa_stream *stream;
	pa_sample_spec sample_spec;
	pa_channel_map channel_map;
	int operation_success;
	int volume;
};

static gboolean check_pulse_health (xmms_pulse *p, int *rerror)
{
	if (!p->context || pa_context_get_state (p->context) != PA_CONTEXT_READY ||
	    !p->stream || pa_stream_get_state (p->stream) != PA_STREAM_READY) {
		if ((p->context &&
		     pa_context_get_state (p->context) == PA_CONTEXT_FAILED) ||
		    (p->stream &&
		     pa_stream_get_state (p->stream) == PA_STREAM_FAILED)) {
			if (rerror)
				*(rerror) = pa_context_errno (p->context);
		} else if (rerror)
			*(rerror) = PA_ERR_BADSTATE;
		return FALSE;
	}
	return TRUE;
}

/*
 * Callbacks to handle updates from the Pulse daemon.
 */
static void signal_mainloop (void *userdata)
{
	xmms_pulse *p = userdata;
	assert (p);

	pa_threaded_mainloop_signal (p->mainloop, 0);
}

static void context_state_cb (pa_context *c, void *userdata)
{
	assert (c);

	switch (pa_context_get_state (c)) {
	case PA_CONTEXT_READY:
	case PA_CONTEXT_TERMINATED:
	case PA_CONTEXT_FAILED:
		signal_mainloop (userdata);

	case PA_CONTEXT_UNCONNECTED:
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
		break;
	}
}

static void stream_state_cb (pa_stream *s, void * userdata)
{
	assert (s);

	switch (pa_stream_get_state (s)) {
	case PA_STREAM_READY:
	case PA_STREAM_FAILED:
	case PA_STREAM_TERMINATED:
		signal_mainloop (userdata);

	case PA_STREAM_UNCONNECTED:
	case PA_STREAM_CREATING:
		break;
	}
}

static void stream_latency_update_cb (pa_stream *s, void *userdata)
{
	signal_mainloop (userdata);
}

static void stream_request_cb (pa_stream *s, size_t length, void *userdata)
{
	signal_mainloop (userdata);
}

static void drain_result_cb (pa_stream *s, int success, void *userdata)
{
	xmms_pulse *p = userdata;
	assert (s);
	assert (p);

	p->operation_success = success;
	signal_mainloop (userdata);
}


/*
 * Public API.
 */
xmms_pulse *
xmms_pulse_backend_new (const char *server, const char *name,
                        int *rerror)
{
	xmms_pulse *p;
	int error = PA_ERR_INTERNAL;

	if (server && !*server) {
		if (rerror)
			*rerror = PA_ERR_INVALID;
		return NULL;
	}

	p = g_new0 (xmms_pulse, 1);
	if (!p)
		return NULL;

	p->volume = 100;

	p->mainloop = pa_threaded_mainloop_new ();
	if (!p->mainloop)
		goto fail;

	p->context = pa_context_new (pa_threaded_mainloop_get_api (p->mainloop), name);
	if (!p->context)
		goto fail;

	pa_context_set_state_callback (p->context, context_state_cb, p);

	if (pa_context_connect (p->context, server, 0, NULL) < 0) {
		error = pa_context_errno (p->context);
		goto fail;
	}

	pa_threaded_mainloop_lock (p->mainloop);

	if (pa_threaded_mainloop_start (p->mainloop) < 0)
		goto unlock_and_fail;

	/* Wait until the context is ready */
	pa_threaded_mainloop_wait (p->mainloop);

	if (pa_context_get_state (p->context) != PA_CONTEXT_READY) {
		error = pa_context_errno (p->context);
		goto unlock_and_fail;
	}

	pa_threaded_mainloop_unlock (p->mainloop);
	return p;

 unlock_and_fail:
	pa_threaded_mainloop_unlock (p->mainloop);
 fail:
	if (rerror)
		*rerror = error;
	xmms_pulse_backend_free (p);
	return NULL;
}


void xmms_pulse_backend_free (xmms_pulse *p)
{
	assert (p);

	if (p->stream)
		xmms_pulse_backend_close_stream (p);
	if (p->mainloop)
		pa_threaded_mainloop_stop (p->mainloop);
	if (p->context)
		pa_context_unref (p->context);
	if (p->mainloop)
		pa_threaded_mainloop_free (p->mainloop);

	g_free (p);
}


gboolean xmms_pulse_backend_set_stream (xmms_pulse *p, const char *stream_name,
                                        const char *sink,
                                        xmms_sample_format_t format,
                                        int samplerate, int channels,
                                        int *rerror)
{
	pa_sample_format_t pa_format = PA_SAMPLE_INVALID;
	pa_cvolume cvol;
	int error = PA_ERR_INTERNAL;
	int ret;
	int i;
	assert (p);

	/* Convert the XMMS2 sample format to the pulse format. */
	for (i = 0; i < G_N_ELEMENTS (xmms_pulse_formats); i++) {
		if (xmms_pulse_formats[i].xmms_fmt == format) {
			pa_format = xmms_pulse_formats[i].pulse_fmt;
			break;
		}
	}
	g_return_val_if_fail (pa_format != PA_SAMPLE_INVALID, FALSE);

	/* If there is an existing stream, check to see if it can do the
	 * job. */
	if (p->stream && p->sample_spec.format == pa_format &&
	    p->sample_spec.rate == samplerate &&
	    p->sample_spec.channels == channels)
		return TRUE;

	/* The existing stream needs to be shut down. */
	if (p->stream)
		xmms_pulse_backend_close_stream (p);

	pa_threaded_mainloop_lock (p->mainloop);


	/* Configure the new stream. */
	p->sample_spec.format = pa_format;
	p->sample_spec.rate = samplerate;
	p->sample_spec.channels = channels;
	pa_channel_map_init_auto (&p->channel_map, channels, PA_CHANNEL_MAP_DEFAULT);

	/* Create and set up the new stream. */
	p->stream = pa_stream_new (p->context, stream_name, &p->sample_spec, &p->channel_map);
	if (!p->stream) {
		error = pa_context_errno (p->context);
		goto unlock_and_fail;
	}

	pa_stream_set_state_callback (p->stream, stream_state_cb, p);
	pa_stream_set_write_callback (p->stream, stream_request_cb, p);
	pa_stream_set_latency_update_callback (p->stream, stream_latency_update_cb, p);

	pa_cvolume_set (&cvol, p->sample_spec.channels,
	                PA_VOLUME_NORM * p->volume / 100);

	ret = pa_stream_connect_playback (p->stream, sink, NULL,
	                                  PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE,
	                                  &cvol, NULL);

	if (ret < 0) {
		error = pa_context_errno (p->context);
		goto unlock_and_fail;
	}

	/* Wait until the stream is ready */
	while (pa_stream_get_state (p->stream) == PA_STREAM_CREATING) {
		pa_threaded_mainloop_wait (p->mainloop);
	}
	if (pa_stream_get_state (p->stream) != PA_STREAM_READY) {
		error = pa_context_errno (p->context);
		goto unlock_and_fail;
	}

	pa_threaded_mainloop_unlock (p->mainloop);
	return TRUE;

 unlock_and_fail:
	pa_threaded_mainloop_unlock (p->mainloop);
	if (rerror)
		*rerror = error;
	if (p->stream)
		pa_stream_unref (p->stream);
	p->stream = NULL;
	return FALSE;
}


void xmms_pulse_backend_close_stream (xmms_pulse *p)
{
	assert (p);

	pa_threaded_mainloop_lock (p->mainloop);

	/* We're killing it anyway, sod errors. */
	xmms_pulse_backend_drain (p, NULL);

	pa_stream_disconnect (p->stream);
	pa_stream_unref (p->stream);
	p->stream = NULL;

	pa_threaded_mainloop_unlock (p->mainloop);
}

gboolean xmms_pulse_backend_write (xmms_pulse *p, const char *data,
                                   size_t length, int *rerror)
{
	assert (p);

	if (!data || !length) {
		if (rerror)
			*rerror = PA_ERR_INVALID;
		return FALSE;
	}

	pa_threaded_mainloop_lock (p->mainloop);
	if (!check_pulse_health (p, rerror))
		goto unlock_and_fail;

	while (length > 0) {
		size_t buf_len;
		int ret;

		while (!(buf_len = pa_stream_writable_size (p->stream))) {
			pa_threaded_mainloop_wait (p->mainloop);
			if (!check_pulse_health (p, rerror))
				goto unlock_and_fail;
		}

		if (buf_len == (size_t)-1) {
			if (rerror)
				*rerror = pa_context_errno ((p)->context);
			goto unlock_and_fail;
		}
		if (buf_len > length)
			buf_len = length;

		ret = pa_stream_write (p->stream, data, buf_len, NULL, 0, PA_SEEK_RELATIVE);
		if (ret < 0) {
			if (rerror)
				*rerror = pa_context_errno ((p)->context);
			goto unlock_and_fail;
		}

		data += buf_len;
		length -= buf_len;
	}

	pa_threaded_mainloop_unlock (p->mainloop);
	return TRUE;

 unlock_and_fail:
	pa_threaded_mainloop_unlock (p->mainloop);
	return FALSE;
}


gboolean xmms_pulse_backend_drain (xmms_pulse *p, int *rerror)
{
	pa_operation *o = NULL;
	assert (p);

	if (!check_pulse_health (p, rerror))
		goto unlock_and_fail;

	o = pa_stream_drain (p->stream, drain_result_cb, p);
	if (!o) {
		if (rerror)
			*rerror = pa_context_errno ((p)->context);
		goto unlock_and_fail;
	}

	p->operation_success = 0;
	while (pa_operation_get_state (o) != PA_OPERATION_DONE) {
		pa_threaded_mainloop_wait (p->mainloop);
		if (!check_pulse_health (p, rerror))
			goto unlock_and_fail;
	}
	pa_operation_unref (o);
	o = NULL;
	if (!p->operation_success) {
		if (rerror)
			*rerror = pa_context_errno ((p)->context);
		goto unlock_and_fail;
	}

	return TRUE;

 unlock_and_fail:
	if (o) {
		pa_operation_cancel (o);
		pa_operation_unref (o);
	}

	return FALSE;
}


gboolean xmms_pulse_backend_flush (xmms_pulse *p, int *rerror)
{
	pa_operation *o = NULL;

	pa_threaded_mainloop_lock (p->mainloop);
	if (!check_pulse_health (p, rerror))
		goto unlock_and_fail;

	o = pa_stream_flush (p->stream, drain_result_cb, p);
	if (!o) {
		if (rerror)
			*rerror = pa_context_errno ((p)->context);
		goto unlock_and_fail;
	}

	p->operation_success = 0;
	while (pa_operation_get_state (o) != PA_OPERATION_DONE) {
		pa_threaded_mainloop_wait (p->mainloop);
		if (!check_pulse_health (p, rerror))
			goto unlock_and_fail;
	}
	pa_operation_unref (o);
	o = NULL;
	if (!p->operation_success) {
		if (rerror)
			*rerror = pa_context_errno ((p)->context);
		goto unlock_and_fail;
	}

	pa_threaded_mainloop_unlock (p->mainloop);
	return 0;

 unlock_and_fail:
	if (o) {
		pa_operation_cancel (o);
		pa_operation_unref (o);
	}

	pa_threaded_mainloop_unlock (p->mainloop);
	return -1;
}


int xmms_pulse_backend_get_latency (xmms_pulse *p, int *rerror)
{
	pa_usec_t t;
	int negative, r;
	assert (p);

	pa_threaded_mainloop_lock (p->mainloop);

	while (1) {
		if (!check_pulse_health (p, rerror))
			goto unlock_and_fail;

		if (pa_stream_get_latency (p->stream, &t, &negative) >= 0)
			break;

		r = pa_context_errno (p->context);
		if (r != PA_ERR_NODATA) {
			if (rerror)
				*rerror = r;
			goto unlock_and_fail;
		}
		/* Wait until latency data is available again */
		pa_threaded_mainloop_wait (p->mainloop);
	}

	pa_threaded_mainloop_unlock (p->mainloop);

	return negative ? 0 : t;

 unlock_and_fail:
	pa_threaded_mainloop_unlock (p->mainloop);
	return -1;
}


static void volume_set_cb (pa_context *c, int success, void *udata)
{
	int *res = (int *) udata;
	*res = success;
}


int xmms_pulse_backend_volume_set (xmms_pulse *p, unsigned int vol)
{
	pa_operation *o;
	pa_cvolume cvol;
	int idx, res = 0;

	if (p == NULL) {
		return FALSE;
	}

	pa_threaded_mainloop_lock (p->mainloop);

	if (p->stream != NULL) {
		pa_cvolume_set (&cvol, p->sample_spec.channels,
		                PA_VOLUME_NORM * vol / 100);

		idx = pa_stream_get_index (p->stream);

		o = pa_context_set_sink_input_volume (p->context, idx, &cvol,
		                                      volume_set_cb, &res);
		if (o) {
			/* wait for result to land */
			while (pa_operation_get_state (o) != PA_OPERATION_DONE) {
				pa_threaded_mainloop_wait (p->mainloop);
			}

			pa_operation_unref (o);

			/* The cb set res to 1 or 0 depending on success */
			if (res) {
				p->volume = vol;
			}
		}

	}

	pa_threaded_mainloop_unlock (p->mainloop);

	return res;
}


static void volume_get_cb (pa_context *c, const pa_sink_input_info *i,
                           int eol, void *udata)
{
	unsigned int *vol = (unsigned int *) udata;
	double total = 0;
	int j;

	if (i != NULL && i->volume.channels > 0 && *vol == -1) {
		for (j = 0; j < i->volume.channels; j++) {
			total += i->volume.values[j] * 100.0 / PA_VOLUME_NORM;
		}

		*vol = (unsigned int) ceil (total / i->volume.channels);
	}
}


int xmms_pulse_backend_volume_get (xmms_pulse *p, unsigned int *vol)
{
	pa_operation *o;
	int idx;

	if (p == NULL) {
		return FALSE;
	}

	pa_threaded_mainloop_lock (p->mainloop);

	*vol = -1;

	if (p->stream != NULL) {
		idx = pa_stream_get_index (p->stream);

		o = pa_context_get_sink_input_info (p->context, idx,
		                                    volume_get_cb, vol);

		if (o) {
			while (pa_operation_get_state (o) != PA_OPERATION_DONE) {
				pa_threaded_mainloop_wait (p->mainloop);
			}

			pa_operation_unref (o);
		}
	}

	pa_threaded_mainloop_unlock (p->mainloop);

	return *vol != -1;
}
