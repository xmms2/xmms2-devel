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

/**
 * @file
 * Output plugin helper
 */

#include "xmms/output.h"
#include "xmms/object.h"
#include "xmms/ringbuf.h"
#include "xmms/util.h"
#include "xmms/dbus.h"
#include "xmms/config.h"
#include "xmms/plugin.h"
#include "xmms/signal_xmms.h"

#include "internal/plugin_int.h"
#include "internal/output_int.h"
#include "internal/decoder_int.h"

static gpointer xmms_output_thread (gpointer data);

static void xmms_output_start (xmms_output_t *output, xmms_error_t *err);
static void xmms_output_stop (xmms_output_t *output, xmms_error_t *err);
static void xmms_output_pause (xmms_output_t *output, xmms_error_t *err);
static void xmms_output_decoder_kill (xmms_output_t *output, xmms_error_t *err);
static void xmms_output_seekms (xmms_output_t *output, guint32 ms, xmms_error_t *error);
static void xmms_output_seeksamples (xmms_output_t *output, guint32 samples, xmms_error_t *error);
static guint xmms_output_status (xmms_output_t *output, xmms_error_t *error);
static guint xmms_output_current_id (xmms_output_t *output, xmms_error_t *error);

static void xmms_output_resume (xmms_output_t *output);

XMMS_METHOD_DEFINE (start, xmms_output_start, xmms_output_t *, NONE, NONE, NONE);
XMMS_METHOD_DEFINE (stop, xmms_output_stop, xmms_output_t *, NONE, NONE, NONE);
XMMS_METHOD_DEFINE (pause, xmms_output_pause, xmms_output_t *, NONE, NONE, NONE);
XMMS_METHOD_DEFINE (decoder_kill, xmms_output_decoder_kill, xmms_output_t *, NONE, NONE, NONE);
XMMS_METHOD_DEFINE (playtime, xmms_output_playtime, xmms_output_t *, UINT32, NONE, NONE);
XMMS_METHOD_DEFINE (seekms, xmms_output_seekms, xmms_output_t *, NONE, UINT32, NONE);
XMMS_METHOD_DEFINE (seeksamples, xmms_output_seeksamples, xmms_output_t *, NONE, UINT32, NONE);
XMMS_METHOD_DEFINE (status, xmms_output_status, xmms_output_t *, UINT32, NONE, NONE);
XMMS_METHOD_DEFINE (currentid, xmms_output_current_id, xmms_output_t *, UINT32, NONE, NONE);

/*
 * Type definitions
 */

/** @defgroup Output Output
  * @ingroup XMMSServer
  * @{
  */

typedef enum {
	XMMS_OUTPUT_TYPE_WR,
	XMMS_OUTPUT_TYPE_FILL,
} xmms_output_type_t;

struct xmms_output_St {
	xmms_object_t object;
	xmms_plugin_t *plugin;

	xmms_decoder_t *decoder;

	GMutex *mutex;
	GCond *cond;
	GCond *fill_cond;
	GThread *thread;
	gboolean running;

	guint played;
	guint played_time;
	/** Internal status, tells which state the
	    output really is in */
	guint status;
	/** Do we WANT to pause? */
	guint is_paused;

	gpointer plugin_data;

	xmms_output_type_t type;

	xmms_playlist_t *playlist;

	xmms_playlist_entry_t *playing_entry;

	guint samplerate;

	/** 
	 * Number of bytes totaly written to output driver,
	 * this is only for statistics...
	 */
	guint64 bytes_written; 

	/**
	 * How many times didn't we have enough data in the buffer?
	 */
	gint32 buffer_underruns;
};

/*
 * Public functions
 */


/**
  * Retrive the private data for the plugin that was set with
  * #xmms_output_private_data_set.
  */

gpointer
xmms_output_private_data_get (xmms_output_t *output)
{
	gpointer ret;
	g_return_val_if_fail (output, NULL);

	ret = output->plugin_data;

	return ret;
}

/**
  * Set the private data for the plugin that can be retrived
  * with #xmms_output_private_data_get later.
  */

void
xmms_output_private_data_set (xmms_output_t *output, gpointer data)
{
	output->plugin_data = data;
}

/**
  * Get the #xmms_plugin_t for this object.
  */

xmms_plugin_t *
xmms_output_plugin_get (xmms_output_t *output)
{
	g_return_val_if_fail (output, NULL);
	
	return output->plugin;
}

/**
  * Tell the output which playlist to use.
  */

void
xmms_output_playlist_set (xmms_output_t *output, xmms_playlist_t *playlist)
{
	g_return_if_fail (output);

	g_mutex_lock (output->mutex);

	if (output->playlist)
		xmms_object_unref (output->playlist);

	if (playlist)
		xmms_object_ref (playlist);

	output->playlist = playlist;

	g_mutex_unlock (output->mutex);
}

/*
 * Private functions
 */


/** Methods */

static void
xmms_output_decoder_kill (xmms_output_t *output, xmms_error_t *error)
{
	g_return_if_fail (output);

	g_mutex_lock (output->mutex);
	if (output->decoder) {
		xmms_decoder_stop (output->decoder);
	}
	g_mutex_unlock (output->mutex);
}

static void
xmms_output_seekms (xmms_output_t *output, guint32 ms, xmms_error_t *error)
{
	g_return_if_fail (output);
	g_mutex_lock (output->mutex);
	if (output->decoder) {
		xmms_decoder_seek_ms (output->decoder, ms, error);
	} else {
		/* Here we should set some data to the entry so it will start
		   play from the offset */
	}
	g_mutex_unlock (output->mutex);
}

static void
xmms_output_seeksamples (xmms_output_t *output, guint32 samples, xmms_error_t *error)
{
	g_return_if_fail (output);
	g_mutex_lock (output->mutex);
	if (output->decoder) {
		xmms_decoder_seek_samples (output->decoder, samples, error);
	}
	g_mutex_unlock (output->mutex);
}

static void
xmms_output_start (xmms_output_t *output, xmms_error_t *err)
{
	g_return_if_fail (output);

	g_mutex_lock (output->mutex);

	if (output->running) {
		if (output->is_paused)
			xmms_output_resume (output);
	} else {
		output->running = TRUE;
		xmms_object_ref (output); /* thread takes one ref */
		output->thread = g_thread_create (xmms_output_thread, output, FALSE, NULL);
	}

	if (output->type == XMMS_OUTPUT_TYPE_FILL) {
		xmms_output_status_method_t st;

		st = xmms_plugin_method_get (output->plugin,
					     XMMS_PLUGIN_METHOD_STATUS);

		st (output, XMMS_OUTPUT_STATUS_PLAY); 
	}

	g_mutex_unlock (output->mutex);

}

static void
xmms_output_stop (xmms_output_t *output, xmms_error_t *err)
{
	g_return_if_fail (output);

	g_mutex_lock (output->mutex);

	if (output->type == XMMS_OUTPUT_TYPE_WR) {
		XMMS_DBG ("STOP!");
		output->running = FALSE;
		g_cond_signal (output->cond);
	} else {
		xmms_output_status_method_t st;

		st = xmms_plugin_method_get (output->plugin,
					     XMMS_PLUGIN_METHOD_STATUS);

		st (output, XMMS_OUTPUT_STATUS_STOP); 
		output->running = FALSE;
		g_cond_signal (output->fill_cond);
	}

	g_mutex_unlock (output->mutex);
}

static void
xmms_output_pause (xmms_output_t *output, xmms_error_t *err)
{
	g_return_if_fail (output);
	g_mutex_lock (output->mutex);
	if (output->decoder) {
		XMMS_DBG ("pause!");
		output->is_paused = TRUE;
		if (output->type == XMMS_OUTPUT_TYPE_FILL) {
			g_cond_signal (output->fill_cond);
		}
	}
	g_mutex_unlock (output->mutex);
}


static guint
xmms_output_status (xmms_output_t *output, xmms_error_t *error)
{
	g_return_val_if_fail (output, XMMS_OUTPUT_STATUS_STOP);
	guint ret;

	g_mutex_lock (output->mutex);
	ret = output->status;
	g_mutex_unlock (output->mutex);
	return ret;
}

static guint
xmms_output_current_id (xmms_output_t *output, xmms_error_t *error)
{
	xmms_playlist_entry_t *entry;
	guint ret;

	entry = xmms_output_playing_entry_get (output, error);
	if (!entry)
		return 0;

	ret = xmms_playlist_entry_id_get (entry);
	xmms_object_unref (entry);
	return ret;
}

/**
  * Get the current #xmms_playlist_entry_t for the output.
  * This is the one is currently played.
  */

xmms_playlist_entry_t *
xmms_output_playing_entry_get (xmms_output_t *output, xmms_error_t *error)
{
	xmms_playlist_entry_t *ret;
	g_return_val_if_fail (output, NULL);

	g_mutex_lock (output->mutex);
	ret = output->playing_entry;
	if (ret)
		xmms_object_ref (ret);
	g_mutex_unlock (output->mutex);
	return ret;
}

/**
  * Get the current playtime in milliseconds.
  */

guint32
xmms_output_playtime (xmms_output_t *output, xmms_error_t *error)
{
	guint32 ret;
	g_return_val_if_fail (output, 0);

	g_mutex_lock (output->mutex);
	ret = output->played_time;
	g_mutex_unlock (output->mutex);

	return ret;
}


/**
 * @internal
 */

static void
xmms_output_resume (xmms_output_t *output)
{
	g_return_if_fail (output);
	XMMS_DBG ("resuming");
	output->is_paused = FALSE;
	g_cond_signal (output->cond);
}

void
xmms_output_played_samples_set (xmms_output_t *output, guint samples)
{
	g_return_if_fail (output);
	output->played = samples * 4;
	XMMS_DBG ("Set played to %d", output->played);
}

static void
xmms_output_status_set (xmms_output_t *output, gint status)
{
	output->status = status;
	g_mutex_unlock (output->mutex); /* must not hold lock in emit */
	xmms_object_emit_f (XMMS_OBJECT (output),
			    XMMS_SIGNAL_OUTPUT_STATUS,
			    XMMS_OBJECT_METHOD_ARG_UINT32,
			    status);
	g_mutex_lock (output->mutex);
}

GList *
xmms_output_stats (xmms_output_t *output, GList *list)
{
	gchar *tmp;
	GList *ret;

	g_return_val_if_fail (output, NULL);

	g_mutex_lock (output->mutex);
	tmp = g_strdup_printf ("output.total_bytes=%llu", output->bytes_written);
	ret = g_list_append (list, tmp);
	tmp = g_strdup_printf ("output.buffer_underruns=%d", output->buffer_underruns);
	ret = g_list_append (ret, tmp);
	g_mutex_unlock (output->mutex);

	return ret;
}

void
xmms_output_samplerate_set (xmms_output_t *output, guint rate)
{
	xmms_output_samplerate_set_method_t samplerate_method;

	g_return_if_fail (output);
	g_return_if_fail (rate);

	samplerate_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET);
	XMMS_DBG ("want samplerate %d", rate);
	output->samplerate = samplerate_method (output, rate);
	XMMS_DBG ("samplerate set to: %d", output->samplerate);
}

guint
xmms_output_samplerate_get (xmms_output_t *output)
{
	g_return_val_if_fail (output, 0);

	return output->samplerate;
}

gboolean
xmms_output_open (xmms_output_t *output)
{
	xmms_output_open_method_t open_method;

	g_return_val_if_fail (output, FALSE);

	open_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_OPEN);

	if (!open_method || !open_method (output)) {
		xmms_log_error ("Couldn't open output device");
		return FALSE;
	}

	XMMS_DBG ("opening with samplerate: %d",output->samplerate);
	xmms_output_samplerate_set (output, output->samplerate);

	return TRUE;

}

void
xmms_output_close (xmms_output_t *output)
{
	xmms_output_close_method_t close_method;

	g_return_if_fail (output);

	close_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_CLOSE);

	if (!close_method)
		return;

	close_method (output);

}

static void
xmms_output_destroy (xmms_object_t *object)
{
	xmms_output_t *output = (xmms_output_t *)object;
	xmms_output_destroy_method_t dest;

	dest = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_DESTROY);

	if (dest) {
		dest (output);
	}

	xmms_object_unref (output->plugin);

	g_mutex_free (output->mutex);
	g_cond_free (output->cond);
	g_cond_free (output->fill_cond);
}

xmms_output_t *
xmms_output_new (xmms_plugin_t *plugin)
{
	xmms_output_t *output;
	xmms_output_new_method_t new;
	xmms_output_write_method_t wr;
	
	g_return_val_if_fail (plugin, NULL);

	XMMS_DBG ("Trying to open output");

	output = xmms_object_new (xmms_output_t, xmms_output_destroy);
	output->plugin = plugin;
	output->mutex = g_mutex_new ();
	output->cond = g_cond_new ();
	output->fill_cond = g_cond_new ();

	output->samplerate = 44100;
	output->bytes_written = 0;
	output->buffer_underruns = 0;

	new = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_NEW);

	if (!new (output)) {
		xmms_object_unref (output);
		return NULL;
	}
	
	wr = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_WRITE);
	if (!wr) {
		output->type = XMMS_OUTPUT_TYPE_FILL;
	} else {
		output->type = XMMS_OUTPUT_TYPE_WR;
	}

	xmms_dbus_register_object ("output", XMMS_OBJECT (output));

	xmms_dbus_register_onchange (XMMS_OBJECT (output),
				     XMMS_SIGNAL_OUTPUT_MIXER_CHANGED);
	xmms_dbus_register_onchange (XMMS_OBJECT (output),
				     XMMS_SIGNAL_OUTPUT_PLAYTIME);
	xmms_dbus_register_onchange (XMMS_OBJECT (output),
				     XMMS_SIGNAL_OUTPUT_STATUS);
	xmms_dbus_register_onchange (XMMS_OBJECT (output),
				     XMMS_SIGNAL_OUTPUT_CURRENTID);


	xmms_object_method_add (XMMS_OBJECT (output), 
				XMMS_METHOD_START, 
				XMMS_METHOD_FUNC (start));
	xmms_object_method_add (XMMS_OBJECT (output), 
				XMMS_METHOD_STOP, 
				XMMS_METHOD_FUNC (stop));
	xmms_object_method_add (XMMS_OBJECT (output), 
				XMMS_METHOD_PAUSE, 
				XMMS_METHOD_FUNC (pause));
	xmms_object_method_add (XMMS_OBJECT (output), 
				XMMS_METHOD_DECODER_KILL, 
				XMMS_METHOD_FUNC (decoder_kill));
	xmms_object_method_add (XMMS_OBJECT (output), 
				XMMS_METHOD_CPLAYTIME, 
				XMMS_METHOD_FUNC (playtime));
	xmms_object_method_add (XMMS_OBJECT (output), 
				XMMS_METHOD_SEEKMS, 
				XMMS_METHOD_FUNC (seekms));
	xmms_object_method_add (XMMS_OBJECT (output), 
				XMMS_METHOD_SEEKSAMPLES, 
				XMMS_METHOD_FUNC (seeksamples));
	xmms_object_method_add (XMMS_OBJECT (output), 
				XMMS_METHOD_STATUS, 
				XMMS_METHOD_FUNC (status));
	xmms_object_method_add (XMMS_OBJECT (output), 
				XMMS_METHOD_CURRENTID, 
				XMMS_METHOD_FUNC (currentid));

	output->status = XMMS_OUTPUT_STATUS_STOP;
	
	return output;
}

void
xmms_output_flush (xmms_output_t *output)
{
	xmms_output_flush_method_t flush;

	g_return_if_fail (output);
	
	flush = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_FLUSH);
	g_return_if_fail (flush);

	flush (output);

}

xmms_plugin_t *
xmms_output_find_plugin (gchar *name)
{
	GList *list, *l;
	xmms_plugin_t *plugin = NULL;

	g_return_val_if_fail (name, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_OUTPUT);

	for (l = list; l; l = g_list_next (l)) {
		if (!g_strcasecmp (xmms_plugin_shortname_get (l->data),
		                   name)) {
			plugin = l->data;
			xmms_object_ref (plugin);
			break;
		}
	}

	xmms_plugin_list_destroy (list);

	return plugin;
}
	

gint
xmms_output_read (xmms_output_t *output, char *buffer, gint len)
{
	gint ret;
	xmms_output_buffersize_get_method_t buffersize_get_method;

	g_return_val_if_fail (output, -1);
	g_return_val_if_fail (buffer, -1);

	buffersize_get_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET);

	g_mutex_lock (output->mutex);
	
	if (!output->decoder) {
		g_mutex_unlock (output->mutex);
		return 0;
	}
	
	ret = xmms_decoder_read (output->decoder, buffer, len);
	
	if (ret > 0) {
	
		output->played += ret;
		/** @todo some places we are counting in bytes,
		  in other in number of samples. Maybe we
		  want a xmms_sample_t and a XMMS_SAMPLE_SIZE */

		output->played_time = (guint)(output->played/(4.0f*output->samplerate/1000.0f));

		if (buffersize_get_method) {
			guint buffersize = buffersize_get_method (output);
			buffersize = buffersize/(2.0f*output->samplerate/1000.0f);

			if (output->played_time >= buffersize) {
				output->played_time -= buffersize;
			} else {
				output->played_time = 0;
			}
		}

		xmms_object_emit_f (XMMS_OBJECT (output),
				    XMMS_SIGNAL_OUTPUT_PLAYTIME,
				    XMMS_OBJECT_METHOD_ARG_UINT32,
				    output->played_time);

	} else if (xmms_decoder_iseos (output->decoder)) {
		g_cond_signal (output->fill_cond);
	}

	if (ret < len) {
		output->buffer_underruns++;
	}

	output->bytes_written += ret;
	
	g_mutex_unlock (output->mutex);

	return ret;
}

static gpointer
xmms_output_thread (gpointer data)
{
	xmms_output_t *output;
	xmms_output_buffersize_get_method_t buffersize_get_method;
	xmms_output_write_method_t write_method;

	output = (xmms_output_t*)data;
	g_return_val_if_fail (data, NULL);

	if (!xmms_output_open (output)) {
		xmms_log_error ("Couldn't open output device");
		xmms_object_emit (XMMS_OBJECT (output), XMMS_SIGNAL_OUTPUT_OPEN_FAIL, NULL);
		xmms_object_unref (output);
		return NULL;
	}

	write_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_WRITE);

	buffersize_get_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET);

	g_mutex_lock (output->mutex);
	while (output->running) {
		gchar buffer[4096];
		gint ret;

		if (!output->decoder) {
			xmms_playlist_entry_t *entry;

			entry = xmms_playlist_advance (output->playlist);

			if (!entry) {
				output->running = FALSE;
				continue;
			}

			output->decoder = xmms_playlist_entry_start (entry);

			if (!output->decoder) {
				output->running = FALSE;
				xmms_object_unref (entry);
				continue;
			}
			output->playing_entry = entry;

			xmms_object_emit_f (XMMS_OBJECT (output),
					    XMMS_SIGNAL_OUTPUT_CURRENTID,
					    XMMS_OBJECT_METHOD_ARG_UINT32,
					    xmms_playlist_entry_id_get (entry));

			xmms_object_emit_f (XMMS_OBJECT (output),
			                    XMMS_SIGNAL_OUTPUT_CURRENT_ENTRY,
			                    XMMS_OBJECT_METHOD_ARG_PLAYLIST_ENTRY,
			                    output->playing_entry);

			output->played = 0;
			xmms_decoder_start (output->decoder, NULL, output);
		}

		if (output->is_paused || !output->decoder) {
			XMMS_DBG ("output is waiting!");
			if (output->is_paused) {
				xmms_output_status_set (output, XMMS_OUTPUT_STATUS_PAUSE);
			}

			g_cond_wait (output->cond, output->mutex);
			continue;
		}
		
		if (output->status != XMMS_OUTPUT_STATUS_PLAY) {
			xmms_output_status_set (output, XMMS_OUTPUT_STATUS_PLAY);
		}


		if (output->type == XMMS_OUTPUT_TYPE_FILL) {
			XMMS_DBG ("Waiting...");
			g_cond_wait (output->fill_cond, output->mutex);
		} else {
			ret = xmms_decoder_read (output->decoder, buffer, 4096);
		}

		if (ret > 0 && output->type == XMMS_OUTPUT_TYPE_WR) {

			g_mutex_unlock (output->mutex);
			/* Call the plugins write method */
			write_method (output, buffer, ret);
			g_mutex_lock (output->mutex);

			/* For statistics! */
			output->bytes_written += ret;

			output->played += ret;
			/** @todo some places we are counting in bytes,
			    in other in number of samples. Maybe we
			    want a xmms_sample_t and a XMMS_SAMPLE_SIZE */
			
			output->played_time = (guint)(output->played/(4.0f*output->samplerate/1000.0f));

			if (buffersize_get_method) {
				guint buffersize = buffersize_get_method (output);
				buffersize = buffersize/(2.0f*output->samplerate/1000.0f);

				if (output->played_time >= buffersize) {
					output->played_time -= buffersize;
				} else {
					output->played_time = 0;
				}
			}

			/* Emit playtime */
			xmms_object_emit_f (XMMS_OBJECT (output),
					    XMMS_SIGNAL_OUTPUT_PLAYTIME,
					    XMMS_OBJECT_METHOD_ARG_UINT32,
					    output->played_time);
		}

		if (xmms_decoder_iseos (output->decoder)) {
			XMMS_DBG ("decoder is EOS!");
			xmms_output_status_set (output, XMMS_OUTPUT_STATUS_STOP);
			xmms_decoder_stop (output->decoder);
			xmms_object_unref (output->decoder);
			xmms_object_unref (output->playing_entry);
			output->decoder = NULL;
			output->playing_entry = NULL;
			xmms_output_flush (output);
		}
	}

	if (output->decoder) {
		xmms_output_status_set (output, XMMS_OUTPUT_STATUS_STOP);
		xmms_decoder_stop (output->decoder);
		xmms_object_unref (output->decoder);
		xmms_object_unref (output->playing_entry);
		output->decoder = NULL;
		output->playing_entry = NULL;
		xmms_output_flush (output);
		output->is_paused = FALSE;
	}

	g_mutex_unlock (output->mutex);

	xmms_output_close (output);

	xmms_object_unref (output);

	return NULL;
}

/** @} */
