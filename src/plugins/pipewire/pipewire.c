/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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

#include <xmms/xmms_outputplugin.h>
#include <xmms/xmms_log.h>

#include <glib.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/utils/result.h>

/*
 * PipeWire is a pull driven audio system: it asks the client for data
 * from a realtime graph via the stream's process callback. XMMS2 already
 * provides a ring buffer that the decoding pipeline fills, so this plugin
 * uses the event driven (status) output model, just like the jack plugin:
 * a stream is connected up front with a fixed format, and the process
 * callback pulls converted samples out of XMMS2 with #xmms_output_read.
 * The XMMS2 converter takes care of adapting arbitrary input formats to
 * the single format we advertise.
 *
 * The stream is connected without a fixed sample rate or channel count so
 * that PipeWire negotiates the sink's native values. They are reported back
 * through the param_changed callback and are then advertised to XMMS2, which
 * lets the converter resample and remix directly to the sink layout and
 * avoids a second conversion step inside PipeWire.
 *
 * The stream is connected inactive and only activated while XMMS2 is
 * playing. An active stream keeps its node running, which keeps the sink
 * (and the underlying device) from going idle and suspending, so pushing
 * silence while paused or stopped would keep the audio hardware powered up
 * for as long as the daemon runs.
 */

/*
 * Defines
 */

#define XMMS_PIPEWIRE_DEFAULT_NAME "XMMS2"

/* We render float samples to the graph; the rate and channel count are
 * negotiated with the sink (see above). These are only used if negotiation
 * does not report values in time. */
#define XMMS_PIPEWIRE_FALLBACK_RATE     48000
#define XMMS_PIPEWIRE_FALLBACK_CHANNELS 2

/* How long to wait for the sink format to be negotiated, in seconds. */
#define XMMS_PIPEWIRE_NEGOTIATE_TIMEOUT 5

/*
 * Type definitions
 */

typedef struct {
	struct pw_thread_loop *loop;
	struct pw_stream *stream;

	gint stride;

	/* Sample rate and channel count negotiated with the sink; samplerate
	 * is 0 until param_changed reports the format. Guarded by the thread
	 * loop while negotiating. */
	gint samplerate;
	gint channels;
	gboolean error;

	gboolean running;

	guint volume;
} xmms_pipewire_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_pipewire_plugin_setup (xmms_output_plugin_t *plugin);
static gboolean xmms_pipewire_new (xmms_output_t *output);
static void xmms_pipewire_destroy (xmms_output_t *output);
static gboolean xmms_pipewire_status (xmms_output_t *output,
                                      xmms_playback_status_t status);
static void xmms_pipewire_flush (xmms_output_t *output);
static gboolean xmms_pipewire_volume_set (xmms_output_t *output,
                                          const gchar *channel, guint volume);
static gboolean xmms_pipewire_volume_get (xmms_output_t *output,
                                          const gchar **names, guint *values,
                                          guint *num_channels);

static gboolean xmms_pipewire_connect (xmms_output_t *output);
static void xmms_pipewire_apply_volume (xmms_pipewire_data_t *data);
static void xmms_pipewire_set_running (xmms_pipewire_data_t *data,
                                       gboolean running);

static void xmms_pipewire_on_process (void *userdata);
static void xmms_pipewire_on_param_changed (void *userdata, uint32_t id,
                                            const struct spa_pod *param);
static void xmms_pipewire_on_state_changed (void *userdata,
                                            enum pw_stream_state old,
                                            enum pw_stream_state state,
                                            const char *error);

static const struct pw_stream_events stream_events = {
	PW_VERSION_STREAM_EVENTS,
	.state_changed = xmms_pipewire_on_state_changed,
	.param_changed = xmms_pipewire_on_param_changed,
	.process = xmms_pipewire_on_process,
};

/*
 * Plugin header
 */

XMMS_OUTPUT_PLUGIN_DEFINE ("pipewire", "PipeWire Output", XMMS_VERSION,
                           "Output to a PipeWire server",
                           xmms_pipewire_plugin_setup);

static gboolean
xmms_pipewire_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);

	methods.new = xmms_pipewire_new;
	methods.destroy = xmms_pipewire_destroy;
	methods.status = xmms_pipewire_status;
	methods.flush = xmms_pipewire_flush;
	methods.volume_set = xmms_pipewire_volume_set;
	methods.volume_get = xmms_pipewire_volume_get;

	xmms_output_plugin_methods_set (plugin, &methods);

	xmms_output_plugin_config_property_register (plugin, "name",
	                                             XMMS_PIPEWIRE_DEFAULT_NAME,
	                                             NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "target", "",
	                                             NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "volume", "100",
	                                             NULL, NULL);

	return TRUE;
}

/*
 * Member functions
 */

static gboolean
xmms_pipewire_new (xmms_output_t *output)
{
	xmms_pipewire_data_t *data;
	xmms_config_property_t *cv;

	g_return_val_if_fail (output, FALSE);

	data = g_new0 (xmms_pipewire_data_t, 1);

	data->samplerate = 0; /* filled in once negotiated with the sink */
	data->channels = 0;

	cv = xmms_output_config_lookup (output, "volume");
	data->volume = xmms_config_property_get_int (cv);

	pw_init (NULL, NULL);

	data->loop = pw_thread_loop_new ("xmms2-pipewire", NULL);
	if (!data->loop) {
		xmms_log_error ("Could not create PipeWire thread loop");
		pw_deinit ();
		g_free (data);
		return FALSE;
	}

	xmms_output_private_data_set (output, data);

	if (!xmms_pipewire_connect (output)) {
		pw_thread_loop_destroy (data->loop);
		pw_deinit ();
		g_free (data);
		xmms_output_private_data_set (output, NULL);
		return FALSE;
	}

	/* Start the loop and wait for the sink format to be negotiated. The
	 * rate and channel count arrive asynchronously through param_changed. */
	pw_thread_loop_lock (data->loop);

	if (pw_thread_loop_start (data->loop) < 0) {
		xmms_log_error ("Could not start PipeWire thread loop");
		pw_thread_loop_unlock (data->loop);
		pw_stream_destroy (data->stream);
		pw_thread_loop_destroy (data->loop);
		pw_deinit ();
		g_free (data);
		xmms_output_private_data_set (output, NULL);
		return FALSE;
	}

	while (data->samplerate == 0 && !data->error) {
		if (pw_thread_loop_timed_wait (data->loop,
		                               XMMS_PIPEWIRE_NEGOTIATE_TIMEOUT) != 0)
			break;
	}

	pw_thread_loop_unlock (data->loop);

	if (data->samplerate == 0) {
		data->samplerate = XMMS_PIPEWIRE_FALLBACK_RATE;
		data->channels = XMMS_PIPEWIRE_FALLBACK_CHANNELS;
		data->stride = sizeof (xmms_samplefloat_t) * data->channels;
		xmms_log_info ("Sink format not negotiated, advertising %d Hz, %d ch",
		               data->samplerate, data->channels);
	}

	/* Apply the stored volume now that the channel count is known. */
	xmms_pipewire_apply_volume (data);

	xmms_output_format_add (output, XMMS_SAMPLE_FORMAT_FLOAT,
	                        data->channels, data->samplerate);

	return TRUE;
}

static gboolean
xmms_pipewire_connect (xmms_output_t *output)
{
	xmms_pipewire_data_t *data;
	xmms_config_property_t *cv;
	const gchar *name, *target;
	struct pw_properties *props;
	const struct spa_pod *params[1];
	uint8_t buffer[1024];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT (buffer, sizeof (buffer));
	int res;

	data = xmms_output_private_data_get (output);

	cv = xmms_output_config_lookup (output, "name");
	name = xmms_config_property_get_string (cv);
	if (!name || *name == '\0')
		name = XMMS_PIPEWIRE_DEFAULT_NAME;

	cv = xmms_output_config_lookup (output, "target");
	target = xmms_config_property_get_string (cv);
	if (target && *target == '\0')
		target = NULL;

	props = pw_properties_new (PW_KEY_MEDIA_TYPE, "Audio",
	                           PW_KEY_MEDIA_CATEGORY, "Playback",
	                           PW_KEY_MEDIA_ROLE, "Music",
	                           PW_KEY_APP_NAME, name,
	                           PW_KEY_NODE_NAME, name,
	                           NULL);
	if (target)
		pw_properties_set (props, PW_KEY_TARGET_OBJECT, target);

	data->stream = pw_stream_new_simple (pw_thread_loop_get_loop (data->loop),
	                                      name, props, &stream_events, output);
	if (!data->stream) {
		xmms_log_error ("Could not create PipeWire stream");
		return FALSE;
	}

	/* Leave the rate and channel count unset (0) so they are omitted from
	 * the offered format and PipeWire negotiates the sink's native values;
	 * they are read back in param_changed. */
	params[0] = spa_format_audio_raw_build (
		&b, SPA_PARAM_EnumFormat,
		&SPA_AUDIO_INFO_RAW_INIT (.format = SPA_AUDIO_FORMAT_F32));

	/* Connect inactive: the format is still negotiated (the stream settles
	 * in the paused state), but the node is not run until playback starts,
	 * so an idle daemon lets the sink suspend. */
	res = pw_stream_connect (data->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
	                         PW_STREAM_FLAG_AUTOCONNECT |
	                         PW_STREAM_FLAG_INACTIVE |
	                         PW_STREAM_FLAG_MAP_BUFFERS,
	                         params, 1);
	if (res < 0) {
		xmms_log_error ("Could not connect PipeWire stream: %s",
		                spa_strerror (res));
		pw_stream_destroy (data->stream);
		data->stream = NULL;
		return FALSE;
	}

	return TRUE;
}

static void
xmms_pipewire_destroy (xmms_output_t *output)
{
	xmms_pipewire_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (data->loop)
		pw_thread_loop_stop (data->loop);

	if (data->stream) {
		pw_stream_destroy (data->stream);
		data->stream = NULL;
	}

	if (data->loop) {
		pw_thread_loop_destroy (data->loop);
		data->loop = NULL;
	}

	pw_deinit ();

	g_free (data);
}

/*
 * Apply the currently stored volume to the connected stream. Must be called
 * without the thread loop locked; it takes the lock itself.
 */
static void
xmms_pipewire_apply_volume (xmms_pipewire_data_t *data)
{
	float volumes[SPA_AUDIO_MAX_CHANNELS];
	float linear;
	gint i, channels;

	if (!data->stream || data->channels <= 0)
		return;

	channels = MIN (data->channels, SPA_AUDIO_MAX_CHANNELS);

	/* PipeWire's channel volumes use a cubic mapping. */
	linear = data->volume / 100.0f;
	for (i = 0; i < channels; i++)
		volumes[i] = linear * linear * linear;

	pw_thread_loop_lock (data->loop);
	pw_stream_set_control (data->stream, SPA_PROP_channelVolumes,
	                       channels, volumes, 0);
	pw_thread_loop_unlock (data->loop);
}

/*
 * Start or stop pulling samples. Deactivating the stream stops the process
 * callback from being called at all, which lets PipeWire park the node and
 * eventually suspend the sink instead of running it on silence. Must be
 * called without the thread loop locked; it takes the lock itself.
 */
static void
xmms_pipewire_set_running (xmms_pipewire_data_t *data, gboolean running)
{
	if (!data->stream) {
		data->running = running;
		return;
	}

	pw_thread_loop_lock (data->loop);
	data->running = running;
	pw_stream_set_active (data->stream, running);
	pw_thread_loop_unlock (data->loop);
}

static gboolean
xmms_pipewire_status (xmms_output_t *output, xmms_playback_status_t status)
{
	xmms_pipewire_data_t *data;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	xmms_pipewire_set_running (data,
	                           status == XMMS_PLAYBACK_STATUS_PLAY);

	return TRUE;
}

static void
xmms_pipewire_flush (xmms_output_t *output)
{
	xmms_pipewire_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (!data->stream)
		return;

	pw_thread_loop_lock (data->loop);
	pw_stream_flush (data->stream, false);
	pw_thread_loop_unlock (data->loop);
}

static void
xmms_pipewire_on_process (void *userdata)
{
	xmms_output_t *output = (xmms_output_t *) userdata;
	xmms_pipewire_data_t *data;
	struct pw_buffer *pwb;
	struct spa_buffer *buf;
	guint8 *dst;
	gint n_frames, want, got = 0;

	data = xmms_output_private_data_get (output);
	if (!data || !data->stream || data->stride <= 0)
		return;

	pwb = pw_stream_dequeue_buffer (data->stream);
	if (!pwb) {
		XMMS_DBG ("PipeWire out of buffers");
		return;
	}

	buf = pwb->buffer;
	dst = buf->datas[0].data;
	if (!dst) {
		pw_stream_queue_buffer (data->stream, pwb);
		return;
	}

	n_frames = buf->datas[0].maxsize / data->stride;
	if (pwb->requested)
		n_frames = MIN ((guint64) n_frames, pwb->requested);

	want = n_frames * data->stride;

	/* When playing, pull converted samples from XMMS2. The read blocks
	 * only until data is available or end of stream is reached, at which
	 * point it returns a short count and drives the transition to stop. */
	if (data->running) {
		got = xmms_output_read (output, (gchar *) dst, want);
		if (got < 0)
			got = 0;
	}

	/* Pad the remainder with silence on pause or underrun. */
	if (got < want)
		memset (dst + got, 0, want - got);

	buf->datas[0].chunk->offset = 0;
	buf->datas[0].chunk->stride = data->stride;
	buf->datas[0].chunk->size = want;

	pw_stream_queue_buffer (data->stream, pwb);
}

static void
xmms_pipewire_on_param_changed (void *userdata, uint32_t id,
                                const struct spa_pod *param)
{
	xmms_output_t *output = (xmms_output_t *) userdata;
	xmms_pipewire_data_t *data;
	struct spa_audio_info_raw info;

	if (!param || id != SPA_PARAM_Format)
		return;

	data = xmms_output_private_data_get (output);
	if (!data)
		return;

	spa_zero (info);
	if (spa_format_audio_raw_parse (param, &info) < 0 ||
	    info.rate == 0 || info.channels == 0)
		return;

	if (data->samplerate != 0 &&
	    ((gint) info.rate != data->samplerate ||
	     (gint) info.channels != data->channels)) {
		/* XMMS2's pipeline is already fed at the previously negotiated
		 * format, so a mid-stream change would force PipeWire to convert.
		 * This is rare (the sink format changing during playback). */
		xmms_log_info ("PipeWire renegotiated %u Hz/%u ch (was %d Hz/%d ch)",
		               info.rate, info.channels,
		               data->samplerate, data->channels);
		return;
	}

	/* Wake up new() which is waiting for the negotiated format. */
	XMMS_DBG ("PipeWire negotiated sink format: %u Hz, %u ch",
	          info.rate, info.channels);
	data->channels = info.channels;
	data->stride = sizeof (xmms_samplefloat_t) * info.channels;
	data->samplerate = info.rate;
	pw_thread_loop_signal (data->loop, false);
}

static void
xmms_pipewire_on_state_changed (void *userdata, enum pw_stream_state old,
                                enum pw_stream_state state, const char *error)
{
	xmms_output_t *output = (xmms_output_t *) userdata;
	xmms_pipewire_data_t *data;
	xmms_error_t err;

	data = xmms_output_private_data_get (output);

	if (state == PW_STREAM_STATE_ERROR) {
		xmms_log_error ("PipeWire stream error: %s",
		                error ? error : "unknown");
		xmms_error_set (&err, XMMS_ERROR_GENERIC,
		                error ? error : "PipeWire stream error");
		xmms_output_set_error (output, &err);

		/* Unblock new() if it is still waiting for negotiation. */
		if (data) {
			data->error = TRUE;
			pw_thread_loop_signal (data->loop, false);
		}
	}
}

static gboolean
xmms_pipewire_volume_set (xmms_output_t *output, const gchar *channel_name,
                          guint volume)
{
	xmms_pipewire_data_t *data;
	xmms_config_property_t *cv;
	gchar buf[4];

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (channel_name, FALSE);
	g_return_val_if_fail (volume <= 100, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	data->volume = volume;
	xmms_pipewire_apply_volume (data);

	cv = xmms_output_config_lookup (output, "volume");
	g_snprintf (buf, sizeof (buf), "%u", volume);
	xmms_config_property_set_data (cv, buf);

	return TRUE;
}

static gboolean
xmms_pipewire_volume_get (xmms_output_t *output, const gchar **names,
                          guint *values, guint *num_channels)
{
	xmms_pipewire_data_t *data;

	g_return_val_if_fail (output, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);
	g_return_val_if_fail (num_channels, FALSE);

	if (!*num_channels) {
		*num_channels = 1;
		return TRUE;
	}

	g_return_val_if_fail (*num_channels == 1, FALSE);
	g_return_val_if_fail (names, FALSE);
	g_return_val_if_fail (values, FALSE);

	names[0] = "master";
	values[0] = data->volume;

	return TRUE;
}
