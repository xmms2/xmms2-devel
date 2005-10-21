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

#include "xmmspriv/xmms_output.h"
#include "xmmspriv/xmms_ringbuf.h"
#include "xmmspriv/xmms_plugin.h"
#include "xmmspriv/xmms_effect.h"
#include "xmmspriv/xmms_decoder.h"
#include "xmmspriv/xmms_sample.h"
#include "xmmspriv/xmms_transport.h"
#include "xmmspriv/xmms_medialib.h"
#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_ipc.h"
#include "xmms/xmms_object.h"
#include "xmms/xmms_config.h"


static gpointer xmms_output_write_thread (gpointer data);
static gboolean xmms_output_decoder_start (xmms_output_t *output);
static void xmms_output_decoder_stop (xmms_output_t *output, xmms_decoder_t *decoder);
static void xmms_output_format_set (xmms_output_t *output, xmms_audio_format_t *fmt);

static void xmms_output_start (xmms_output_t *output, xmms_error_t *err);
static void xmms_output_stop (xmms_output_t *output, xmms_error_t *err);
static void xmms_output_pause (xmms_output_t *output, xmms_error_t *err);
static void xmms_output_decoder_kill (xmms_output_t *output, xmms_error_t *err);
static void xmms_output_seekms (xmms_output_t *output, guint32 ms, xmms_error_t *error);
static void xmms_output_seeksamples (xmms_output_t *output, guint32 samples, xmms_error_t *error);
static guint xmms_output_status (xmms_output_t *output, xmms_error_t *error);
static guint xmms_output_current_id (xmms_output_t *output, xmms_error_t *error);

static void xmms_output_status_set (xmms_output_t *output, gint status);
static gboolean set_plugin (xmms_output_t *output, xmms_plugin_t *plugin);
static void status_changed (xmms_output_t *output, xmms_playback_status_t status);

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
  * @brief Output is responsible to put the decoded data on
  * the soundcard.
  * @{
  */

/*
 * locking order: decoder_mutex > status_mutex > write_mutex
 *                playtime_mutex is leaflock.
 */


struct xmms_output_St {
	xmms_object_t object;
	xmms_plugin_t *plugin;
	xmms_plugin_t *switch_plugin;

	GMutex *decoder_mutex;
	GQueue *decoder_list;
	xmms_decoder_t *decoder;

	GList *effects;

	/* Makes sure write and flush-calls doesn't happen on the same time */
	GMutex *api_mutex;

	/* */
	GMutex *write_mutex;
	GCond *write_cond;
	GThread *write_thread;
	gboolean write_running;
	gboolean write_paused;

	/* */
	GMutex *playtime_mutex;
	guint played;
	guint played_time;

	/** Internal status, tells which state the
	    output really is in */
	GMutex *status_mutex;
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

/** @} */

/*
 * Public functions
 */

/** 
 * @defgroup OutputPlugin OutputPlugin
 * @ingroup XMMSPlugin
 * @{
 */

/**
 * Retrieve the private data for the plugin that was set with
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
 * Add format to list of supported formats.
 * Should be called from initialisation function for every supported
 * format. Any call to the format_set method will be with one of these
 * formats.
 *
 * 
 */
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
	
        output->format_list = g_list_append (output->format_list, f);
}

gint
xmms_output_read (xmms_output_t *output, char *buffer, gint len)
{
	gint ret;
	xmms_output_buffersize_get_method_t buffersize_get_method;

	g_return_val_if_fail (output, -1);
	g_return_val_if_fail (buffer, -1);

	g_mutex_lock (output->decoder_mutex);
	if (!output->decoder) {
		output->decoder = g_queue_pop_head (output->decoder_list);
		if (!output->decoder) {
			xmms_output_status_set (output, XMMS_PLAYBACK_STATUS_STOP);
			g_mutex_unlock (output->decoder_mutex);
			return -1;
		}

		xmms_output_format_set (output, xmms_decoder_audio_format_to_get (output->decoder));
		output->played = 0;

		xmms_object_emit_f (XMMS_OBJECT (output),
		                    XMMS_IPC_SIGNAL_OUTPUT_CURRENTID,
		                    XMMS_OBJECT_CMD_ARG_UINT32,
		                    xmms_decoder_medialib_entry_get (output->decoder));
	}
	g_mutex_unlock (output->decoder_mutex);
	
	ret = xmms_decoder_read (output->decoder, buffer, len);

	if (ret > 0) {
		guint buffersize = 0;

		g_mutex_lock (output->playtime_mutex);
		output->played += ret;
		g_mutex_unlock (output->playtime_mutex);

		buffersize_get_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET);
		if (buffersize_get_method) {
			buffersize = buffersize_get_method (output);

			if (output->played < buffersize) {
				buffersize = output->played;
			}
		}

		g_mutex_lock (output->playtime_mutex);

		output->played_time = xmms_sample_bytes_to_ms (output->format, output->played - buffersize);

		xmms_object_emit_f (XMMS_OBJECT (output),
				    XMMS_IPC_SIGNAL_OUTPUT_PLAYTIME,
				    XMMS_OBJECT_CMD_ARG_UINT32,
				    output->played_time);

		g_mutex_unlock (output->playtime_mutex);

	} else if (xmms_decoder_iseos (output->decoder)) {
		xmms_output_decoder_stop (output, output->decoder);
		xmms_object_unref (output->decoder);
		output->decoder = NULL;
		return 0;
	}

	if (ret < len) {
		output->buffer_underruns++;
	}

	output->bytes_written += ret;
	
	return ret;
}

gboolean
xmms_output_plugin_verify (xmms_plugin_t *plugin)
{
	gboolean w, s, o, c;

	g_return_val_if_fail (plugin, FALSE);

	if (!xmms_plugin_has_methods (plugin,
	                              XMMS_PLUGIN_METHOD_NEW,
	                              XMMS_PLUGIN_METHOD_DESTROY,
	                              XMMS_PLUGIN_METHOD_FLUSH,
	                              NULL)) {
		return FALSE;
	}

	w = !!xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_WRITE);
	s = !!xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_STATUS);

	if (!(!w ^ !s)) {
		return FALSE;
	}

	o = !!xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_OPEN);
	c = !!xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_CLOSE);

	/* 'write' plugins need these two methods, 'status' plugins may
	 * have neither of them
	 */
	return (w && o && c) || (s && !o && !c);
}

/** @} */

/*
 * Private functions
 */

static void
xmms_output_decoder_stop (xmms_output_t *output, xmms_decoder_t *decoder)
{
	xmms_medialib_entry_t entry;
	guint32 playtime;
	xmms_medialib_session_t *session = xmms_medialib_begin();

	entry = xmms_decoder_medialib_entry_get (decoder);
	playtime = xmms_output_playtime (output, NULL);

	xmms_medialib_logging_stop (session, entry, playtime);
	xmms_medialib_end (session);

	xmms_decoder_stop (decoder);
}

/** @addtogroup Output
 * @{
 */
/** Methods */

static void
xmms_output_decoder_kill (xmms_output_t *output, xmms_error_t *error)
{
	xmms_decoder_t *dec;

	g_return_if_fail (output);

	g_mutex_lock (output->decoder_mutex);
	if (xmms_output_status (output, NULL) != XMMS_PLAYBACK_STATUS_STOP) {
		xmms_output_decoder_start (output);
		/* unpause */
		xmms_output_status_set (output, XMMS_PLAYBACK_STATUS_PLAY);
	}
	dec = output->decoder;
	output->decoder = NULL;
	g_mutex_unlock (output->decoder_mutex);

	if (dec) {
		xmms_output_decoder_stop (output, dec);
		xmms_object_unref (dec);
	}
}

static void
xmms_output_seekms (xmms_output_t *output, guint32 ms, xmms_error_t *error)
{
	g_return_if_fail (output);
	g_mutex_lock (output->decoder_mutex);
	if (output->decoder) {
		gint32 pos;

		pos = xmms_decoder_seek_ms (output->decoder, ms, error);
		if (pos != -1) {
			g_mutex_lock (output->playtime_mutex);
			output->played = pos;
			g_mutex_unlock (output->playtime_mutex);
		}
			
	} else {
		/* Here we should set some data to the entry so it will start
		   play from the offset */
	}
	xmms_output_flush (output);

	g_mutex_unlock (output->decoder_mutex);
}

static void
xmms_output_seeksamples (xmms_output_t *output, guint32 samples, xmms_error_t *error)
{
	g_return_if_fail (output);
	g_mutex_lock (output->decoder_mutex);
	if (output->decoder) {
		gint32 pos;
		
		pos = xmms_decoder_seek_samples (output->decoder, samples, error);
		if (pos != -1) {
			g_mutex_lock (output->playtime_mutex);
			output->played = pos;
			g_mutex_unlock (output->playtime_mutex);
		}
	}

	xmms_output_flush (output);

	g_mutex_unlock (output->decoder_mutex);

}

static void
xmms_output_start (xmms_output_t *output, xmms_error_t *err)
{
	g_return_if_fail (output);

	g_mutex_lock (output->decoder_mutex);
	if (!output->decoder && output->decoder_list->length == 0)
		xmms_output_decoder_start (output);
	g_mutex_unlock (output->decoder_mutex);

	xmms_output_status_set (output, XMMS_PLAYBACK_STATUS_PLAY);
}

static void
xmms_output_stop (xmms_output_t *output, xmms_error_t *err)
{
	g_return_if_fail (output);

	xmms_output_status_set (output, XMMS_PLAYBACK_STATUS_STOP);

	g_mutex_lock (output->decoder_mutex);

	if (output->decoder) {
		xmms_output_decoder_stop (output, output->decoder);
		xmms_object_unref (output->decoder);
		output->decoder = NULL;
	}

	g_mutex_unlock (output->decoder_mutex);
}

static void
xmms_output_pause (xmms_output_t *output, xmms_error_t *err)
{
	g_return_if_fail (output);

	xmms_output_status_set (output, XMMS_PLAYBACK_STATUS_PAUSE);
}


static guint
xmms_output_status (xmms_output_t *output, xmms_error_t *error)
{
	guint ret;
	g_return_val_if_fail (output, XMMS_PLAYBACK_STATUS_STOP);

	g_mutex_lock (output->status_mutex);
	ret = output->status;
	g_mutex_unlock (output->status_mutex);
	return ret;
}

static guint
xmms_output_current_id (xmms_output_t *output, xmms_error_t *error)
{
	guint res = 0;

	g_mutex_lock (output->decoder_mutex);
	if (output->decoder)
		res = xmms_decoder_medialib_entry_get (output->decoder);
	g_mutex_unlock (output->decoder_mutex);

	return res;
}

/**
 * Get the current playtime in milliseconds.
 */
guint32
xmms_output_playtime (xmms_output_t *output, xmms_error_t *error)
{
	guint32 ret;
	g_return_val_if_fail (output, 0);

	g_mutex_lock (output->playtime_mutex);
	ret = output->played_time;
	g_mutex_unlock (output->playtime_mutex);

	return ret;
}


/**
 * @internal
 */

static void
xmms_output_status_set (xmms_output_t *output, gint status)
{
	g_mutex_lock (output->status_mutex);

	if (output->status != status) {
		if (status == XMMS_PLAYBACK_STATUS_PAUSE && output->status != XMMS_PLAYBACK_STATUS_PLAY) {
			XMMS_DBG ("Can only pause from play.");
		} else {
			output->status = status;
			output->status_method (output, status); 
			xmms_object_emit_f (XMMS_OBJECT (output),
					    XMMS_IPC_SIGNAL_PLAYBACK_STATUS,
					    XMMS_OBJECT_CMD_ARG_UINT32,
					    status);
		}
	}

	g_mutex_unlock (output->status_mutex);
}

static gboolean
xmms_output_open (xmms_output_t *output)
{
	xmms_output_open_method_t open_method;

	g_return_val_if_fail (output, FALSE);

	open_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_OPEN);
	g_assert (open_method);

	g_mutex_lock (output->api_mutex);
	if (!open_method (output)) {
		xmms_log_error ("Couldn't open output device");
		return FALSE;
	}
	g_mutex_unlock (output->api_mutex);

	return TRUE;

}

static void
xmms_output_close (xmms_output_t *output)
{
	xmms_output_close_method_t close_method;

	g_return_if_fail (output);

	output->format = NULL;

	close_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_CLOSE);
	g_assert (close_method);

	g_mutex_lock (output->api_mutex);
	close_method (output);
	g_mutex_unlock (output->api_mutex);
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
		if (!cfg) {
			/* this is just a ugly hack to have a configvalue
			   to set */
			xmms_config_value_register (key, "", NULL, NULL);
			break;
		}

		name = xmms_config_value_get_string (cfg);

		if (!name[0])
			break;

		plugin = xmms_plugin_find (XMMS_PLUGIN_TYPE_EFFECT, name);
		if (plugin) {
			list = g_list_prepend (list, xmms_effect_new (plugin));

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

	if (output->plugin) {
		dest = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_DESTROY);
		g_assert (dest);

		g_mutex_lock (output->api_mutex);
		dest (output);
		g_mutex_unlock (output->api_mutex);

		xmms_object_unref (output->plugin);
	}

	while (output->effects) {
		xmms_effect_free (output->effects->data);

		output->effects = g_list_remove_link (output->effects,
		                                      output->effects);
	}

	xmms_object_unref (output->playlist);

	g_queue_free (output->decoder_list);
	g_mutex_free (output->decoder_mutex);
	g_mutex_free (output->status_mutex);
	g_mutex_free (output->playtime_mutex);
	g_mutex_free (output->write_mutex);
	g_mutex_free (output->api_mutex);
	g_cond_free (output->write_cond);

	xmms_ipc_broadcast_unregister ( XMMS_IPC_SIGNAL_OUTPUT_MIXER_CHANGED);
	xmms_ipc_broadcast_unregister ( XMMS_IPC_SIGNAL_PLAYBACK_STATUS);
	xmms_ipc_broadcast_unregister ( XMMS_IPC_SIGNAL_OUTPUT_CURRENTID);
	xmms_ipc_signal_unregister (XMMS_IPC_SIGNAL_OUTPUT_PLAYTIME);
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_OUTPUT);
}

/**
 * Switch to another output plugin.
 * @param output output pointer
 * @param new_plugin the new #xmms_plugin_t to use as output.
 * @returns TRUE on success and FALSE on failure
 */
gboolean
xmms_output_plugin_switch (xmms_output_t *output, xmms_plugin_t *new_plugin)
{
	xmms_plugin_t *old_plugin;
	gboolean ret;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (new_plugin, FALSE);

	xmms_output_stop (output, NULL);

	g_mutex_lock (output->status_mutex);

	old_plugin = output->plugin;

	ret = set_plugin (output, new_plugin);

	/* if the switch succeeded, release the reference to the old plugin
	 * now.
	 * if we couldn't switch to the new plugin, but we had a working
	 * plugin before, switch back to the old plugin.
	 */
	if (ret) {
		xmms_object_unref (old_plugin);
	} else if (old_plugin) {
		XMMS_DBG ("cannot switch plugin, going back to old one");
		set_plugin (output, old_plugin);
	}

	g_mutex_unlock (output->status_mutex);

	return ret;
}

/**
 * Allocate a new #xmms_output_t
 */
xmms_output_t *
xmms_output_new (xmms_plugin_t *plugin, xmms_playlist_t *playlist)
{
	xmms_output_t *output;

	g_return_val_if_fail (playlist, NULL);
	
	XMMS_DBG ("Trying to open output");

	output = xmms_object_new (xmms_output_t, xmms_output_destroy);

	output->api_mutex = g_mutex_new ();

	output->write_mutex = g_mutex_new ();
	output->write_cond = g_cond_new ();

	output->decoder_mutex = g_mutex_new ();
	output->decoder_list = g_queue_new ();
	output->effects = get_effect_list (output);
	output->playlist = playlist;

	output->status_mutex = g_mutex_new ();
	output->playtime_mutex = g_mutex_new ();

	xmms_ipc_object_register (XMMS_IPC_OBJECT_OUTPUT, XMMS_OBJECT (output));

	/* Broadcasts are always transmitted to the client if he
	 * listens to them. */
	xmms_ipc_broadcast_register (XMMS_OBJECT (output),
				     XMMS_IPC_SIGNAL_OUTPUT_MIXER_CHANGED);
	xmms_ipc_broadcast_register (XMMS_OBJECT (output),
				     XMMS_IPC_SIGNAL_PLAYBACK_STATUS);
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

	output->status = XMMS_PLAYBACK_STATUS_STOP;

	if (plugin) {
		if (!set_plugin (output, plugin)) {
			xmms_log_error ("couldn't initialize output plugin");
		}
	} else {
		xmms_log_error ("initalized output without a plugin, please fix!");
	}


	
	return output;
}

/**
 * Flush the buffers in soundcard.
 */
void
xmms_output_flush (xmms_output_t *output)
{
	xmms_output_flush_method_t flush;

	g_return_if_fail (output);
	
	flush = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_FLUSH);
	g_assert (flush);

	g_mutex_lock (output->api_mutex);
	flush (output);
	g_mutex_unlock (output->api_mutex);

}

static void 
decoder_ended (xmms_object_t *object, gconstpointer arg, gpointer data)
{
	xmms_output_t *output = data;
	XMMS_DBG ("Whoops. Decoder is dead, lets start a new!");
	if (xmms_playlist_advance (output->playlist)) {
		g_mutex_lock (output->decoder_mutex);
		xmms_output_decoder_start (output);
		g_mutex_unlock (output->decoder_mutex);
	}
}

static gboolean
xmms_output_decoder_start (xmms_output_t *output)
{
	xmms_medialib_entry_t entry;
	xmms_decoder_t *decoder = NULL;
	xmms_medialib_session_t *session;

	g_return_val_if_fail (output, FALSE);

	if (output->decoder_list->length > 0)
		return TRUE;

	while (TRUE) {
		xmms_transport_t *t;

		entry = xmms_playlist_current_entry (output->playlist);
		if (!entry)
		  return FALSE;

		t = xmms_transport_new ();
		if (!t)
			return FALSE;
		
		if (!xmms_transport_open (t, entry)) {
			xmms_object_unref (t);
			if (!xmms_playlist_advance (output->playlist))
				return FALSE;
			continue;
		}

		xmms_transport_start (t);

		decoder = xmms_decoder_new ();
		
		if (!decoder) {
			xmms_transport_stop (t);
			xmms_object_unref (t);
			return FALSE;
		}
		
		if (!xmms_decoder_open (decoder, t)) {
			xmms_transport_stop (t);
			xmms_object_unref (t);
			xmms_object_unref (decoder);
			return FALSE;
		}
		
		xmms_object_unref (t);

		if (decoder)
			break;
	}

	if (!xmms_decoder_init_for_decoding (decoder, output->format_list,
	                                     output->effects)) {
		xmms_log_error ("Couldn't initialize decoder");

		xmms_object_unref (decoder);
		return FALSE;
	}

	xmms_object_connect (XMMS_OBJECT (decoder), XMMS_IPC_SIGNAL_DECODER_THREAD_EXIT,
			     decoder_ended, output);

	xmms_decoder_start (decoder);

	g_queue_push_tail (output->decoder_list, decoder);

	session = xmms_medialib_begin ();
	xmms_medialib_logging_start (session, xmms_decoder_medialib_entry_get (decoder));
	xmms_medialib_end (session);

	return TRUE;
}


/**
 * @internal
 */
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


/* Used when we have to drive the output... */

static void 
status_changed (xmms_output_t *output, xmms_playback_status_t status)
{
	g_mutex_lock (output->write_mutex);

	if (status == XMMS_PLAYBACK_STATUS_PLAY) {
		XMMS_DBG ("Starting playback!");

		if (output->write_paused) {
			g_cond_signal (output->write_cond);
			output->write_paused = FALSE;
		} else {
			g_return_if_fail (!output->write_running);
			output->write_running = TRUE;

			output->write_thread = g_thread_create (xmms_output_write_thread, output, FALSE, NULL);
		}
	} else if (status == XMMS_PLAYBACK_STATUS_STOP) {
		XMMS_DBG ("Stopping playback!");

		output->write_running = FALSE;
		output->write_paused = FALSE;
		g_cond_signal (output->write_cond);

		output->write_thread = NULL;
	} else if (status == XMMS_PLAYBACK_STATUS_PAUSE) {
		XMMS_DBG ("Pausing playback!");
		output->write_paused = TRUE;
	}

	g_mutex_unlock (output->write_mutex);
}

static gboolean
set_plugin (xmms_output_t *output, xmms_plugin_t *plugin)
{
	xmms_output_destroy_method_t dest;
	xmms_output_new_method_t new;
	xmms_output_status_method_t st;
	gboolean ret;

	g_assert (output);
	g_assert (plugin);

	/* first, shut down the current plugin if present */
	if (output->plugin) {
		dest = xmms_plugin_method_get (output->plugin,
		                               XMMS_PLUGIN_METHOD_DESTROY);
		g_assert (dest);

		dest (output);
		output->plugin = NULL;
		output->status_method = NULL;
	}

	/* every plugin needs a 'new' method */
	new = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_NEW);
	g_assert (new);

	/* output->plugin needs to be set before we can call the
	 * NEW method
	 */
	output->plugin = plugin;
	ret = new (output);

	if (ret) {
		/* determine what kind of output plugin this is */
		st = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_STATUS);
		output->status_method = st ? st : status_changed;
	} else {
		output->plugin = NULL;
	}

	return ret;
}

static gpointer
xmms_output_write_thread (gpointer data)
{
	xmms_output_t *output = data;
	xmms_output_write_method_t write_method;

	xmms_output_open (output);

	write_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_WRITE);

	g_mutex_lock (output->write_mutex);
	
	while (output->write_running) {
		gchar buffer[4096];
		gint ret;

		if (output->write_paused) {
			g_cond_wait (output->write_cond, output->write_mutex);
			continue;
		}

		g_mutex_unlock (output->write_mutex);

		ret = xmms_output_read (output, buffer, 4096);


		if (ret > 0) {
			g_mutex_lock (output->api_mutex);
			write_method (output, buffer, ret);
			g_mutex_unlock (output->api_mutex);
		}
		g_mutex_lock (output->write_mutex);

	}

	g_mutex_unlock (output->write_mutex);

	xmms_output_close (output);

	XMMS_DBG ("Output driving thread exiting!");

	return NULL;
}

/** @} */
