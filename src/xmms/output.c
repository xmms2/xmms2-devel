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
#include "xmms/config.h"
#include "xmms/plugin.h"
#include "xmms/signal_xmms.h"
#include "xmms/ipc.h"

#include "internal/plugin_int.h"
#include "internal/output_int.h"
#include "internal/transport_int.h"
#include "internal/decoder_int.h"

static gpointer xmms_output_write_thread (gpointer data);

static void xmms_output_start (xmms_output_t *output, xmms_error_t *err);
void xmms_output_stop (xmms_output_t *output, xmms_error_t *err);
static void xmms_output_pause (xmms_output_t *output, xmms_error_t *err);
static void xmms_output_decoder_kill (xmms_output_t *output, xmms_error_t *err);
static void xmms_output_seekms (xmms_output_t *output, guint32 ms, xmms_error_t *error);
static void xmms_output_seeksamples (xmms_output_t *output, guint32 samples, xmms_error_t *error);
static guint xmms_output_status (xmms_output_t *output, xmms_error_t *error);
static guint xmms_output_current_id (xmms_output_t *output, xmms_error_t *error);

static void xmms_output_status_set (xmms_output_t *output, gint status);
static void status_changed (xmms_output_t *output, xmms_output_status_t status);
static gint xmms_output_read_unlocked (xmms_output_t *output, char *buffer, gint len);


XMMS_CMD_DEFINE (start, xmms_output_start, xmms_output_t *, NONE, NONE, NONE);
XMMS_CMD_DEFINE (stop, xmms_output_stop, xmms_output_t *, NONE, NONE, NONE);
XMMS_CMD_DEFINE (pause, xmms_output_pause, xmms_output_t *, NONE, NONE, NONE);
XMMS_CMD_DEFINE (decoder_kill, xmms_output_decoder_kill, xmms_output_t *, NONE, NONE, NONE);
XMMS_CMD_DEFINE (playtime, xmms_output_playtime, xmms_output_t *, UINT32, NONE, NONE);
XMMS_CMD_DEFINE (seekms, xmms_output_seekms, xmms_output_t *, NONE, UINT32, NONE);
XMMS_CMD_DEFINE (seeksamples, xmms_output_seeksamples, xmms_output_t *, NONE, UINT32, NONE);
XMMS_CMD_DEFINE (status, xmms_output_status, xmms_output_t *, UINT32, NONE, NONE);
XMMS_CMD_DEFINE (currentid, xmms_output_current_id, xmms_output_t *, UINT32, NONE, NONE);

/*
 * Type definitions
 */

/** @defgroup Output Output
  * @ingroup XMMSServer
  * @{
  */

struct xmms_output_St {
	xmms_object_t object;
	xmms_plugin_t *plugin;

	GQueue *decoder_list;
	xmms_decoder_t *decoder;

	GList *effects;

	GMutex *mutex;
	GCond *cond;
	GThread *write_thread;
	gboolean running;

	guint played;
	guint played_time;
	/** Internal status, tells which state the
	    output really is in */
	guint status;
	xmms_output_status_method_t status_method;

	gpointer plugin_data;

	xmms_playlist_t *playlist;

	/** Supported formats */
	GList *format_list;
	/** Active format */
	xmms_audio_format_t *format;

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


void
xmms_output_format_add (xmms_output_t *output, xmms_sample_format_t fmt, guint channels, guint rate)
{
        xmms_audio_format_t *f;
	
        g_return_if_fail (output);
        g_return_if_fail (fmt);
        g_return_if_fail (channels);
        g_return_if_fail (rate);
	
        f = xmms_sample_audioformat_new (fmt, channels, rate);
	
        g_return_if_fail (f);
	
	XMMS_DBG ("Adding outputformat %s-%d-%d",
	          xmms_sample_name_get (fmt),
	          channels, rate);

        output->format_list = g_list_append (output->format_list, f);
}

static void
xmms_output_format_set (xmms_output_t *output, xmms_audio_format_t *fmt)
{
	xmms_output_format_set_method_t fmt_set;
        g_return_if_fail (output);
        g_return_if_fail (fmt);

	if (output->format == fmt) {
		XMMS_DBG ("audio formats are equal, not updating");
		return;
	}
	
	fmt_set = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_FORMAT_SET);
	if (fmt_set)
		fmt_set (output, fmt);

        output->format = fmt;
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
	xmms_output_decoder_start (output);
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

	/** @todo UPDATE output->played HERE */

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

	/** @todo UPDATE output->played HERE */

	g_mutex_unlock (output->mutex);

}

static void
xmms_output_start (xmms_output_t *output, xmms_error_t *err)
{
	g_return_if_fail (output);

	if (!output->decoder && g_queue_get_length (output->decoder_list) == 0)
		xmms_output_decoder_start (output);

	g_mutex_lock (output->mutex);
	xmms_output_status_set (output, XMMS_OUTPUT_STATUS_PLAY);
	g_mutex_unlock (output->mutex);

}

void
xmms_output_stop (xmms_output_t *output, xmms_error_t *err)
{
	g_return_if_fail (output);

	g_mutex_lock (output->mutex);

	if (output->decoder) {
		xmms_decoder_stop (output->decoder);
		xmms_object_unref (output->decoder);
		output->decoder = NULL;
	}

	xmms_output_status_set (output, XMMS_OUTPUT_STATUS_STOP);
	g_mutex_unlock (output->mutex);
}

static void
xmms_output_pause (xmms_output_t *output, xmms_error_t *err)
{
	g_return_if_fail (output);

	g_mutex_lock (output->mutex);
	xmms_output_status_set (output, XMMS_OUTPUT_STATUS_PAUSE);
	g_mutex_unlock (output->mutex);
}


static guint
xmms_output_status (xmms_output_t *output, xmms_error_t *error)
{
	guint ret;
	g_return_val_if_fail (output, XMMS_OUTPUT_STATUS_STOP);

	g_mutex_lock (output->mutex);
	ret = output->status;
	g_mutex_unlock (output->mutex);
	return ret;
}

static guint
xmms_output_current_id (xmms_output_t *output, xmms_error_t *error)
{
	if (output->decoder)
		return xmms_decoder_medialib_entry_get (output->decoder);
	return 0;
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
xmms_output_status_set (xmms_output_t *output, gint status)
{
	if (output->status == status)
		return;

	output->status = status;
	g_mutex_unlock (output->mutex); /* must not hold lock in emit */
	output->status_method (output, status); 
	xmms_object_emit_f (XMMS_OBJECT (output),
			    XMMS_IPC_SIGNAL_OUTPUT_STATUS,
			    XMMS_OBJECT_CMD_ARG_UINT32,
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

static gboolean
xmms_output_open (xmms_output_t *output)
{
	xmms_output_open_method_t open_method;

	g_return_val_if_fail (output, FALSE);

	open_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_OPEN);

	if (!open_method || !open_method (output)) {
		xmms_log_error ("Couldn't open output device");
		return FALSE;
	}

	return TRUE;

}

static void
xmms_output_close (xmms_output_t *output)
{
	xmms_output_close_method_t close_method;

	g_return_if_fail (output);

	close_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_CLOSE);

	if (!close_method)
		return;

	close_method (output);

	output->format = NULL;
}

static GList *
get_effect_list (xmms_output_t *output)
{
	GList *list = NULL;
	gint i = 0;

	while (42) {
		xmms_config_value_t *cfg;
		xmms_plugin_t *plugin;
		gchar key[64];
		const gchar *name;

		g_snprintf (key, sizeof (key), "effect.order.%i", i++);

		cfg = xmms_config_lookup (key);
		if (!cfg)
			break;

		name = xmms_config_value_string_get (cfg);

		plugin = xmms_plugin_find (XMMS_PLUGIN_TYPE_EFFECT, name);
		if (plugin) {
			list = g_list_prepend (list,
			                       xmms_effect_new (plugin, output));

			/* xmms_plugin_find() increases the refcount and
			 * xmms_effect_new() does, too, so release one reference
			 * again here
			 */
			xmms_object_unref (plugin);
		}
	}

	return g_list_reverse (list);
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

	while (output->effects) {
		xmms_effect_free (output->effects->data);

		output->effects = g_list_remove_link (output->effects,
		                                      output->effects);
	}

	g_queue_free (output->decoder_list);
	g_mutex_free (output->mutex);
	g_cond_free (output->cond);
}

xmms_output_t *
xmms_output_new (xmms_plugin_t *plugin)
{
	xmms_output_t *output;
	xmms_output_new_method_t new;
	xmms_output_write_method_t wr;
	xmms_output_status_method_t st;
	
	g_return_val_if_fail (plugin, NULL);

	XMMS_DBG ("Trying to open output");

	output = xmms_object_new (xmms_output_t, xmms_output_destroy);
	output->plugin = plugin;
	output->mutex = g_mutex_new ();
	output->cond = g_cond_new ();
	output->decoder_list = g_queue_new ();
	output->effects = get_effect_list (output);

	output->bytes_written = 0;
	output->buffer_underruns = 0;

	new = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_NEW);

	if (!new (output)) {
		xmms_object_unref (output);
		return NULL;
	}
	
	xmms_ipc_object_register (XMMS_IPC_OBJECT_OUTPUT, XMMS_OBJECT (output));

	/* Broadcasts are always transmitted to the client if he
	 * listens to them. */
	xmms_ipc_broadcast_register (XMMS_OBJECT (output),
				     XMMS_IPC_SIGNAL_OUTPUT_MIXER_CHANGED);
	xmms_ipc_broadcast_register (XMMS_OBJECT (output),
				     XMMS_IPC_SIGNAL_OUTPUT_STATUS);
	xmms_ipc_broadcast_register (XMMS_OBJECT (output),
				     XMMS_IPC_SIGNAL_OUTPUT_CURRENTID);
	
	/* Signals are only emitted if the client has a pending question to it
	 * after the client recivies a signal, he must ask for it again */
	xmms_ipc_signal_register (XMMS_OBJECT (output),
				  XMMS_IPC_SIGNAL_OUTPUT_PLAYTIME);


	xmms_object_cmd_add (XMMS_OBJECT (output), 
				XMMS_IPC_CMD_START, 
				XMMS_CMD_FUNC (start));
	xmms_object_cmd_add (XMMS_OBJECT (output), 
				XMMS_IPC_CMD_STOP, 
				XMMS_CMD_FUNC (stop));
	xmms_object_cmd_add (XMMS_OBJECT (output), 
				XMMS_IPC_CMD_PAUSE, 
				XMMS_CMD_FUNC (pause));
	xmms_object_cmd_add (XMMS_OBJECT (output), 
				XMMS_IPC_CMD_DECODER_KILL, 
				XMMS_CMD_FUNC (decoder_kill));
	xmms_object_cmd_add (XMMS_OBJECT (output), 
				XMMS_IPC_CMD_CPLAYTIME, 
				XMMS_CMD_FUNC (playtime));
	xmms_object_cmd_add (XMMS_OBJECT (output), 
				XMMS_IPC_CMD_SEEKMS, 
				XMMS_CMD_FUNC (seekms));
	xmms_object_cmd_add (XMMS_OBJECT (output), 
				XMMS_IPC_CMD_SEEKSAMPLES, 
				XMMS_CMD_FUNC (seeksamples));
	xmms_object_cmd_add (XMMS_OBJECT (output), 
				XMMS_IPC_CMD_STATUS, 
				XMMS_CMD_FUNC (status));
	xmms_object_cmd_add (XMMS_OBJECT (output), 
				XMMS_IPC_CMD_CURRENTID, 
				XMMS_CMD_FUNC (currentid));

	output->status = XMMS_OUTPUT_STATUS_STOP;

	wr = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_WRITE);
	st = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_STATUS);
	g_return_val_if_fail ((!wr ^ !st), NULL);

	output->status_method = st ? st : status_changed;
	
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

gint
xmms_output_read (xmms_output_t *output, char *buffer, gint len)
{
	gint ret;
	
	g_mutex_lock (output->mutex);
	ret = xmms_output_read_unlocked (output, buffer, len);
	g_mutex_unlock (output->mutex);

	return ret;

}

static gint
xmms_output_read_unlocked (xmms_output_t *output, char *buffer, gint len)
{
	gint ret;
	xmms_output_buffersize_get_method_t buffersize_get_method;

	g_return_val_if_fail (output, -1);
	g_return_val_if_fail (buffer, -1);

	if (!output->decoder) {
		output->decoder = g_queue_pop_head (output->decoder_list);
		if (!output->decoder) {
			xmms_output_status_set (output, XMMS_OUTPUT_STATUS_STOP);
			output->running = FALSE;
			return -1;
		}

		xmms_output_format_set (output, xmms_decoder_audio_format_to_get (output->decoder));
		output->played = 0;

		xmms_object_emit_f (XMMS_OBJECT (output),
		                    XMMS_IPC_SIGNAL_OUTPUT_CURRENTID,
		                    XMMS_OBJECT_CMD_ARG_UINT32,
		                    xmms_decoder_medialib_entry_get (output->decoder));


	}
	
	g_mutex_unlock (output->mutex);
	ret = xmms_decoder_read (output->decoder, buffer, len);
	g_mutex_lock (output->mutex);

	if (ret > 0) {
		guint buffersize = 0;

		output->played += ret;

		buffersize_get_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET);
		if (buffersize_get_method) {
			buffersize = buffersize_get_method (output);

			if (output->played < buffersize) {
				buffersize = output->played;
			}
		}

		output->played_time = xmms_sample_bytes_to_ms (output->format, output->played - buffersize);

		xmms_object_emit_f (XMMS_OBJECT (output),
				    XMMS_IPC_SIGNAL_OUTPUT_PLAYTIME,
				    XMMS_OBJECT_CMD_ARG_UINT32,
				    output->played_time);

	} else if (xmms_decoder_iseos (output->decoder)) {
		xmms_decoder_stop (output->decoder);
		xmms_object_unref (output->decoder);
		output->decoder = NULL;
		g_mutex_unlock (output->mutex);
		return 0;
	}

	if (ret < len) {
		output->buffer_underruns++;
	}

	output->bytes_written += ret;
	
	return ret;
}

static void 
decoder_ended (xmms_object_t *object, gconstpointer arg, gpointer data)
{
	xmms_output_t *output = data;
	XMMS_DBG ("Whoops. Decoder is dead, lets start a new!");
	if (output->running) {
		if (xmms_playlist_advance (output->playlist))
			xmms_output_decoder_start (output);
	}
}

gboolean
xmms_output_decoder_start (xmms_output_t *output)
{
	xmms_medialib_entry_t entry;
	xmms_decoder_t *decoder = NULL;

	g_return_val_if_fail (output, FALSE);

	while (TRUE) {
		xmms_transport_t *t;
		const gchar *mime;

		entry = xmms_playlist_current_entry (output->playlist);

		g_return_val_if_fail (entry, FALSE);

		t = xmms_transport_new ();
		if (!t)
			return FALSE;
		
		if (!xmms_transport_open (t, entry)) {
			xmms_transport_close (t);
			xmms_object_unref (t);
			continue;
		}

		xmms_transport_start (t);

		/*
		 * Waiting for the mimetype forever
		 * All transports MUST set a mimetype,
		 * NULL on error
		 */
		XMMS_DBG ("Waiting for mimetype");
		mime = xmms_transport_mimetype_get_wait (t);
		if (!mime) {
			xmms_transport_close (t);
			xmms_object_unref (t);
			return FALSE;
		}

		XMMS_DBG ("mime-type: %s", mime);
		
		decoder = xmms_decoder_new ();
		
		if (!decoder) {
			xmms_transport_close (t);
			xmms_object_unref (t);
			return FALSE;
		}
		
		if (!xmms_decoder_open (decoder, t)) {
			xmms_transport_close (t);
			xmms_object_unref (t);
			xmms_object_unref (decoder);
			return FALSE;
		}
		
		xmms_object_unref (t);

		if (decoder)
			break;
	}

	if (!xmms_decoder_init (decoder, output->format_list)) {
		XMMS_DBG ("Couldn't initialize decoder");

		xmms_object_unref (decoder);
		return FALSE;
	}

	xmms_object_connect (XMMS_OBJECT (decoder), XMMS_IPC_SIGNAL_DECODER_THREAD_EXIT,
			     decoder_ended, output);

	xmms_decoder_start (decoder, output->effects, output);
	g_queue_push_tail (output->decoder_list, decoder);

	return TRUE;
}


/* Used when we have to drive the output... */

static void 
status_changed (xmms_output_t *output, xmms_output_status_t status)
{
	if (status == XMMS_OUTPUT_STATUS_PLAY) {
		XMMS_DBG ("Starting playback!");

		g_mutex_lock (output->mutex);

		if (output->running) {
			/* we seem to have thread already, just wake up */
			g_cond_signal (output->cond);
		} else {
			output->running = TRUE;

			output->write_thread = g_thread_create (xmms_output_write_thread, output, FALSE, NULL);
			g_mutex_unlock (output->mutex);
		}
	} else if (status == XMMS_OUTPUT_STATUS_STOP) {
		XMMS_DBG ("Stopping playback!");

		g_mutex_lock (output->mutex);

		output->running = FALSE;
		g_cond_signal (output->cond);
		g_mutex_unlock (output->mutex);

		output->write_thread = NULL;
	}	
}

static gpointer
xmms_output_write_thread (gpointer data)
{
	xmms_output_t *output = data;
	xmms_output_write_method_t write_method;

	xmms_output_open (output);

	write_method = xmms_plugin_method_get (output->plugin,
	                                       XMMS_PLUGIN_METHOD_WRITE);

	g_mutex_lock (output->mutex);
	
	while (output->running) {
		gchar buffer[4096];
		gint ret;

		if (output->status == XMMS_OUTPUT_STATUS_PAUSE) {
			XMMS_DBG ("output is waiting!");
			g_cond_wait (output->cond, output->mutex);
			continue;
		}
		

		ret = xmms_output_read_unlocked (output, buffer, 4096);

		if (ret > 0) {
			g_mutex_unlock (output->mutex);
			write_method (output, buffer, ret);
			g_mutex_lock (output->mutex);
		}
	}

	g_mutex_unlock (output->mutex);

	xmms_output_close (output);

	XMMS_DBG ("Output driving thread exiting!");

	return NULL;
}

/** @} */
