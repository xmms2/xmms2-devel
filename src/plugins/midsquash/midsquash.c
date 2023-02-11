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
 *
 *  ---------------------------------
 *
 *  MIDISquash helper plugin for XMMS2 by Adam Nielsen <malvineous@shikadi.net>
 *
 *  This file implements a MIDI "squash" xform plugin, which converts multiple
 *  MIDI tracks (like you might find in a format-1 .mid file) into a single
 *  track (like in a format-0 file.)
 *
 *  Since the FluidSynth plugin is only designed to accept a single MIDI track,
 *  all multi-track files must be converted to a single track to be played.
 *  Rather than require each format handler to do this conversion itself,
 *  those handlers can provide the MIDI data in audio/miditracks format, and
 *  this plugin will take care of the conversion.
 *
 *  The audio/miditracks format is simply a list of chunks, where each chunk
 *  contains a four-byte big endian length value, followed by that many
 *  bytes of standard MIDI data.  It's like the MTrk chunks in a normal MIDI
 *  file joined one after the other, just without the "MTrk" text.
 */

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_sample.h>
#include <xmms/xmms_medialib.h>
#include <xmms/xmms_log.h>

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * Type definitions
 */

typedef struct xmms_midsquash_data_St {
	GString *midi0_data;
	gulong pos;
} xmms_midsquash_data_t;

typedef struct xmms_midsquash_event_St {
	gulong time;
	guchar running_status;  /* 0 or the last event if this uses running status */
	gchar *offset;
	gulong length;
} xmms_midsquash_event_t;

/*
 * Function prototypes
 */

static gboolean xmms_midsquash_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_midsquash_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static void xmms_midsquash_destroy (xmms_xform_t *xform);
static gboolean xmms_midsquash_init (xmms_xform_t *xform);
static gint64 xmms_midsquash_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err);
static gulong xmms_midisquash_read_midi_num (const gchar *s, gulong *j);
static gint xmms_midsquash_sort_events(gconstpointer a, gconstpointer b);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN_DEFINE ("midsquash",
                          "Multitrack MIDI squasher",
                          XMMS_VERSION,
                          "Multitrack MIDI squasher",
                          xmms_midsquash_plugin_setup);

static gboolean
xmms_midsquash_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_midsquash_init;
	methods.destroy = xmms_midsquash_destroy;
	methods.read = xmms_midsquash_read;
	methods.seek = xmms_midsquash_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	/*
	xmms_plugin_info_add (plugin, "URL", "https://github.com/xmms2");
	xmms_plugin_info_add (plugin, "Author", "Adam Nielsen <malvineous@shikadi.net>");
	xmms_plugin_info_add (plugin, "License", "LGPL");
	*/

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/miditracks",
	                              XMMS_STREAM_TYPE_END);
	return TRUE;
}

static void
xmms_midsquash_destroy (xmms_xform_t *xform)
{
	xmms_midsquash_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	if (data->midi0_data)
		g_string_free (data->midi0_data, TRUE);

	g_free (data);
}

/* Seeking is in bytes.  This assumes the calling function is smart enough not
 * to seek past the start or end of the file.
 */
static gint64
xmms_midsquash_seek (xmms_xform_t *xform, gint64 samples,
               xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_midsquash_data_t *data;
	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	/* Adjust our internal pointer by the seek amount */
	switch (whence) {
		case XMMS_XFORM_SEEK_CUR:
			data->pos += samples;
			break;
		case XMMS_XFORM_SEEK_SET:
			data->pos = samples;
			break;
		case XMMS_XFORM_SEEK_END:
			data->pos = data->midi0_data->len + samples;
			break;
	}

	return data->pos;
}

static gulong
xmms_midisquash_read_midi_num (const gchar *s, gulong *j)
{
	gint i;
	gulong val = 0;

	for (i = 0; i < 4; i++) {
		val |= s[*j] & 0x7F;
		if (s[(*j)++] & 0x80) val <<= 7;
		else break;
	}

	return val;
}

static gint
xmms_midsquash_sort_events(gconstpointer a, gconstpointer b)
{
	xmms_midsquash_event_t *ta = (xmms_midsquash_event_t *)a;
	xmms_midsquash_event_t *tb = (xmms_midsquash_event_t *)b;
	if (ta->time == tb->time) return 0;
	if (ta->time < tb->time) return -1;
	return 1;
}

static gboolean
xmms_midsquash_init (xmms_xform_t *xform)
{
	xmms_error_t error;
	xmms_midsquash_data_t *data;
	gulong track_len, len;
	gint32 ticks_per_quarter_note;
	const gchar *metakey;
	guchar buf[4096];
	gint ret;
	guchar prev_event = 0;
	GArray *events = NULL;
	GArray *track_data = NULL;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_midsquash_data_t, 1);
	xmms_xform_private_data_set (xform, data);

	data->midi0_data = NULL;

	if (!xmms_xform_auxdata_get_int (xform, "tempo", &ticks_per_quarter_note)) {
		XMMS_DBG ("MIDI xform missing 'tempo' auxdata value");
		goto error_cleanup;
	}
	xmms_xform_auxdata_set_int (xform, "tempo", ticks_per_quarter_note);

	/* Load all the tracks */
	events = g_array_new(FALSE, FALSE, sizeof(xmms_midsquash_event_t));
	track_data = g_array_new(FALSE, FALSE, sizeof(GString *));
	while (xmms_xform_read (xform, buf, 4, &error) == 4) {
		track_len = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
		GString *t = g_string_sized_new(track_len);

		ret = xmms_xform_read (xform, t->str, track_len, &error);

		g_array_append_val(track_data, t);

		/* Run through the MIDI data in this track, and convert it to a list of
		 * events in absolute ticks (instead of delta ticks.)
		 */
		gulong i = 0;
		gulong abs_ticks = 0;
		while (i < track_len) {
			abs_ticks += xmms_midisquash_read_midi_num(t->str, &i);

			xmms_midsquash_event_t event;
			gboolean ignore_event = FALSE;
			event.time = abs_ticks;
			event.offset = &t->str[i];
			gulong i0 = i;

			/* Read MIDI event */
			guchar midi_event = t->str[i];

			if (!(midi_event & 0x80)) {
				/* This is a running-status byte */
				midi_event = prev_event;
				event.running_status = prev_event;
			} else {
				if ((midi_event & 0xF0) != 0xF0) {
					/* Meta events (0xF0 through 0xFF) can appear in the middle of
					 * running-status data without affecting the running status, so we
					 * only update the 'last event' if this isn't a meta-event.
					 */
					prev_event = midi_event;
				}
				event.running_status = 0;
				i++;
			}

			switch (midi_event & 0xF0) {
				case 0x80:  /* Note off (two bytes) */ i += 2; break;
				case 0x90:  /* Note on (two bytes) */ i += 2; break;
				case 0xA0:  /* Key pressure (two bytes) */ i += 2; break;
				case 0xB0:  /* Controller change (two bytes) */ i += 2; break;
				case 0xC0:  /* Instrument change (one byte) */ i += 1; break;
				case 0xD0:  /* Channel pressure (one byte) */ i += 1; break;
				case 0xE0:  /* Pitch bend (two bytes) */ i += 2; break;
				case 0xF0: {
					if (midi_event == 0xFF) { /* Meta-event */
						if (t->str[i] == 0x2F) {
							/* This is an end-of-track event, so we need to ignore it
							 * otherwise the song will end as soon as we encounter the end
							 * of the shortest track (which could be quite early on.)
							 */
							ignore_event = TRUE;
						}
						i++; /* event type */
					} /* else sysex */
					len = xmms_midisquash_read_midi_num(t->str, &i);
					i += len;
					break;
				}
				default:
					XMMS_DBG ("Corrupted MIDI file (invalid event 0x%02X)", midi_event);
					goto error_cleanup;
			}
			event.length = i - i0;
			if (!ignore_event)
				g_array_append_val(events, event);
		} /* end loop: run through all the track's events */
	}

	/* Now that all the events have been read in, in absolute time, sorting them
	 * will put them all in playable order.
	 */
	g_array_sort(events, xmms_midsquash_sort_events);

	/* Now copy all the sorted events into a big array, which will be used as
	 * the output data.
	 */
	data->midi0_data = g_string_new("");
	gulong last_time = 0;
	guint64 playtime_us = 0;
	gulong us_per_quarter_note = 500000;
	gulong i, j;
	guchar val;
	for (i = 0; i < events->len; i++) {
		xmms_midsquash_event_t *e;
		e = &g_array_index(events, xmms_midsquash_event_t, i);

		/* Calculate the delta time and write it out in MIDI style */
		gulong delta_ticks = e->time - last_time;
		if (delta_ticks & (0x7F << 21)) {
			val = ((delta_ticks >> 21) & 0x7F) | 0x80;
			g_string_append_len(data->midi0_data, (gchar *)&val, 1);
		}
		if (delta_ticks & ((0x7F << 21) | (0x7F << 14))) {
			val = ((delta_ticks >> 14) & 0x7F) | 0x80;
			g_string_append_len(data->midi0_data, (gchar *)&val, 1);
		}
		if (delta_ticks & ((0x7F << 21) | (0x7F << 14) | (0x7F << 7))) {
			val = ((delta_ticks >>  7) & 0x7F) | 0x80;
			g_string_append_len(data->midi0_data, (gchar *)&val, 1);
		}
		val = delta_ticks & 0x7F;
		g_string_append_len(data->midi0_data, (gchar *)&val, 1);
		last_time = e->time;

		if (e->running_status) {
			/* This event is a continuation of the previous event, but since we're
			 * combining all the events the previous event may no longer be the same,
			 * so we take the easy way out and just write the event out in full here.
			 */
			g_string_append_len(data->midi0_data, (gchar *)&e->running_status, 1);
		}

		/* Copy the actual event across from the track data */
		g_string_append_len(data->midi0_data, e->offset, e->length);

		/* Check for a tempo change event */
		if ((e->offset[0] & 0xF0) == 0xF0) {
			if (e->offset[1] == 0x51) { /* tempo event */
				j = 2;
				len = xmms_midisquash_read_midi_num(e->offset, &j);
				len += j;
				us_per_quarter_note = 0;
				for (; j < len; j++) {
					us_per_quarter_note <<= 8;
					us_per_quarter_note |= (unsigned char)(e->offset[j]);
				}
			}
		}

		/* Update the song length */
		playtime_us += delta_ticks * (us_per_quarter_note / ticks_per_quarter_note);
	}

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/rawmidi",
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             16,
	                             XMMS_STREAM_TYPE_END);


	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
	xmms_xform_metadata_set_int (xform, metakey,
	                             playtime_us / 1000);

	ret = TRUE;

always_cleanup:

	/* Release all the track and event data */
	if (track_data) {
		for (i = 0; i < track_data->len; i++) {
			g_string_free(g_array_index(track_data, GString *, i), TRUE);
		}
		g_array_free(track_data, TRUE);
	}

	if (events)
		g_array_free(events, TRUE);

	return ret;

error_cleanup:

	/* Don't need to free data->midi0_data as we don't goto cleanup after it
	 * has been allocated.
	 */

	g_free (data);

	ret = FALSE;
	goto always_cleanup;
}

static gint
xmms_midsquash_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err)
{
	xmms_midsquash_data_t *data;
	data = xmms_xform_private_data_get (xform);

	if (data->pos + len > data->midi0_data->len)
		len = data->midi0_data->len - data->pos;

	memcpy(buf, &(data->midi0_data->str[data->pos]), len);
	data->pos += len;

	return len;
}
