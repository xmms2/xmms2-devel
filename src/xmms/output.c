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

#define xmms_output_lock(t) XMMS_MTX_LOCK ((t)->mutex)
#define xmms_output_unlock(t) XMMS_MTX_UNLOCK ((t)->mutex)


static gpointer xmms_output_thread (gpointer data);
/*static void xmms_output_status_changed (xmms_object_t *object, gconstpointer data, gpointer udata);*/

void xmms_output_start (xmms_output_t *output, xmms_error_t *err);
void xmms_output_stop (xmms_output_t *output, xmms_error_t *err);
void xmms_output_pause (xmms_output_t *output, xmms_error_t *err);
guint32 xmms_output_playtime (xmms_output_t *output, xmms_error_t *err);
static void xmms_output_decoder_kill (xmms_output_t *output, xmms_error_t *err);

XMMS_METHOD_DEFINE (start, xmms_output_start, xmms_output_t *, NONE, NONE, NONE);
XMMS_METHOD_DEFINE (stop, xmms_output_stop, xmms_output_t *, NONE, NONE, NONE);
XMMS_METHOD_DEFINE (pause, xmms_output_pause, xmms_output_t *, NONE, NONE, NONE);
XMMS_METHOD_DEFINE (decoder_kill, xmms_output_decoder_kill, xmms_output_t *, NONE, NONE, NONE);
XMMS_METHOD_DEFINE (playtime, xmms_output_playtime, xmms_output_t *, UINT32, NONE, NONE);

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
	GThread *thread;
	gboolean running;

	guint played;
	guint played_time;
	gboolean is_paused;

	gpointer plugin_data;

	xmms_output_type_t type;

	xmms_playlist_t *playlist;

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

gpointer
xmms_output_private_data_get (xmms_output_t *output)
{
	gpointer ret;
	g_return_val_if_fail (output, NULL);

	ret = output->plugin_data;

	return ret;
}

xmms_plugin_t *
xmms_output_plugin_get (xmms_output_t *output)
{
	g_return_val_if_fail (output, NULL);
	
	return output->plugin;
}

void
xmms_output_private_data_set (xmms_output_t *output, gpointer data)
{
	output->plugin_data = data;
}

void
xmms_output_playlist_set (xmms_output_t *output, xmms_playlist_t *playlist)
{
	g_return_if_fail (output);

	xmms_output_lock (output);

	if (output->playlist)
		xmms_object_unref (output->playlist);

	if (playlist)
		xmms_object_ref (playlist);

	output->playlist = playlist;

	xmms_output_unlock (output);
}


XMMS_METHOD_DEFINE (mixer_set, xmms_output_mixer_set, xmms_output_t *, NONE, UINT32, UINT32);
void
xmms_output_mixer_set (xmms_output_t *output, gint left, gint right, xmms_error_t *err)
{
	xmms_output_mixer_set_method_t set;
	g_return_if_fail (output);

	set = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_MIXER_SET);
	if (set) {
		xmms_object_method_arg_t *arg;
		if (!set (output, left, right)) {
			xmms_error_set (err, XMMS_ERROR_GENERIC, 
					"Couldn't set mixer");
			return;
		}

		arg = xmms_object_arg_new (XMMS_OBJECT_METHOD_ARG_UINT32, 
					   GUINT_TO_POINTER (left<<right));

		xmms_object_emit (XMMS_OBJECT (output),
				  XMMS_SIGNAL_OUTPUT_MIXER_CHANGED,
				  arg);

		g_free (arg);

	}
}

XMMS_METHOD_DEFINE (mixer_get, xmms_output_mixer_get, xmms_output_t *, UINT32, NONE, NONE);
guint
xmms_output_mixer_get (xmms_output_t *output, xmms_error_t *err)
{
	xmms_output_mixer_get_method_t get;
	guint left, right;

	g_return_val_if_fail (output, 0);

	get = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_MIXER_GET);
	if (get) { 
		if (get (output, &left, &right)) {
			return 100;
		}
	}

	return 0;
}

/*
 * Private functions
 */

/**
 * @internal
 */

GList *
xmms_output_stats (xmms_output_t *output, GList *list)
{
	gchar *tmp;
	GList *ret;

	g_return_val_if_fail (output, NULL);

	XMMS_MTX_LOCK (output->mutex);
	tmp = g_strdup_printf ("output.total_bytes=%llu", output->bytes_written);
	ret = g_list_append (list, tmp);
	tmp = g_strdup_printf ("output.buffer_underruns=%d", output->buffer_underruns);
	ret = g_list_append (ret, tmp);
	XMMS_MTX_UNLOCK (output->mutex);

	return ret;
}


/*
void
xmms_output_write (xmms_output_t *output, gpointer buffer, gint len)
{
	g_return_if_fail (output);
	g_return_if_fail (buffer);

	xmms_output_lock (output);
	if (output->samplerate != output->open_samplerate) {
		guint32 res;
		res = resample (output, (gint16 *)buffer, len);
		xmms_ringbuf_wait_free (output->buffer, res, output->mutex);
		xmms_ringbuf_write (output->buffer, output->resamplebuf, res);
	} else {
		xmms_ringbuf_wait_free (output->buffer, len, output->mutex);
		xmms_ringbuf_write (output->buffer, buffer, len);
	}
	xmms_output_unlock (output);

}
*/

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
		XMMS_DBG ("Couldn't open output device");
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

	g_mutex_free (output->mutex);
	g_cond_free (output->cond);
}

xmms_output_t *
xmms_output_new (xmms_plugin_t *plugin)
{
	xmms_output_t *output;
	xmms_output_new_method_t new;
	xmms_output_write_method_t wr;
	xmms_config_value_t *val;
	
	g_return_val_if_fail (plugin, NULL);

	XMMS_DBG ("Trying to open output");

	val = xmms_config_lookup ("core.output_buffersize");

	output = xmms_object_new (xmms_output_t, xmms_output_destroy);
	output->plugin = plugin;
	output->mutex = g_mutex_new ();
	output->cond = g_cond_new ();

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


	xmms_object_method_add (XMMS_OBJECT (output), 
				XMMS_METHOD_MIXERSET, 
				XMMS_METHOD_FUNC (mixer_set));
	xmms_object_method_add (XMMS_OBJECT (output), 
				XMMS_METHOD_MIXERGET, 
				XMMS_METHOD_FUNC (mixer_get));

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
	GList *list;
	xmms_plugin_t *plugin = NULL;

	g_return_val_if_fail (name, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_OUTPUT);

	while (list) {
		plugin = (xmms_plugin_t*) list->data;
		if (g_strcasecmp (xmms_plugin_shortname_get (plugin), name) == 0) {
			return plugin;
		}
		list = g_list_next (list);
	}

	return NULL;
}
	
void
xmms_output_start (xmms_output_t *output, xmms_error_t *err)
{
	g_return_if_fail (output);

	xmms_output_lock (output);

	if (output->type == XMMS_OUTPUT_TYPE_WR) {
		if (output->running) {
			if (output->is_paused)
				xmms_output_resume (output);
		} else {
			output->running = TRUE;
			xmms_object_ref (output); /* thread takes one ref */
			output->thread = g_thread_create (xmms_output_thread, output, TRUE, NULL);
		}
	} else {
		xmms_output_status_method_t st;

		st = xmms_plugin_method_get (output->plugin,
					     XMMS_PLUGIN_METHOD_STATUS);

		st (output, XMMS_PLAYBACK_PLAY); /* @TODO */

	}

	xmms_output_unlock (output);

}

void
xmms_output_stop (xmms_output_t *output, xmms_error_t *err)
{
	g_return_if_fail (output);

	xmms_output_lock (output);

	if (output->type == XMMS_OUTPUT_TYPE_WR) {
		XMMS_DBG ("STOP!");
		output->is_paused = FALSE;
		output->running = FALSE;
		g_cond_signal (output->cond);
	} else {
		xmms_output_status_method_t st;

		st = xmms_plugin_method_get (output->plugin,
					     XMMS_PLUGIN_METHOD_STATUS);

		st (output, XMMS_PLAYBACK_STOP); /* @TODO */

	}

	xmms_output_unlock (output);
}

void
xmms_output_played_samples_set (xmms_output_t *output, guint samples)
{
	g_return_if_fail (output);
	XMMS_MTX_LOCK (output->mutex);
	output->played = samples * 4;
	XMMS_DBG ("Set played to %d", output->played);
	XMMS_MTX_UNLOCK (output->mutex);
}


gboolean
xmms_output_is_paused (xmms_output_t *output)
{
	gboolean ret;
	g_return_val_if_fail (output, FALSE);

	xmms_output_lock (output);
	ret = output->is_paused;
	xmms_output_unlock (output);

	return ret;
}

void
xmms_output_pause (xmms_output_t *output, xmms_error_t *err)
{
	g_return_if_fail (output);
	xmms_output_lock (output);
	XMMS_DBG ("pause!");
	output->is_paused = TRUE;
	xmms_output_unlock (output);
}

void
xmms_output_resume (xmms_output_t *output)
{
	g_return_if_fail (output);
	XMMS_DBG ("resuming");
	output->is_paused = FALSE;
	g_cond_signal (output->cond);
}

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


static gpointer
xmms_output_thread (gpointer data)
{
	xmms_output_t *output;
	xmms_output_buffersize_get_method_t buffersize_get_method;
	xmms_output_write_method_t write_method;

	output = (xmms_output_t*)data;
	g_return_val_if_fail (data, NULL);

	if (!xmms_output_open (output)) {
		XMMS_DBG ("Couldn't open output device");
		xmms_object_emit (XMMS_OBJECT (output), XMMS_SIGNAL_OUTPUT_OPEN_FAIL, NULL);
		xmms_object_unref (output);
		return NULL;
	}


	write_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_WRITE);
	g_return_val_if_fail (write_method, NULL);

	buffersize_get_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET);

	xmms_output_lock (output);
	while (output->running) {
		gchar buffer[4096];
		gint ret;

		if (!output->decoder) {
			output->decoder = xmms_playlist_next_start (output->playlist);
			if (!output->decoder) {
				output->running = FALSE;
				continue;
			}
			output->played = 0;
			xmms_decoder_start (output->decoder, NULL, output);
		}

		if (output->is_paused || !output->decoder) {
			XMMS_DBG ("output is waiting!");
			g_cond_wait (output->cond, output->mutex);
			continue;
		}

		ret = xmms_decoder_read (output->decoder, buffer, 4096);

		if (ret > 0) {
			xmms_output_unlock (output);
			/* Call the plugins write method */
			write_method (output, buffer, ret);
			xmms_output_lock (output);

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
/*				XMMS_DBG ("buffer: %dms", buffersize);*/

				if (output->played_time >= buffersize) {
					output->played_time -= buffersize;
				} else {
					output->played_time = 0;
				}
			}

			/* Emit playtime */
			{
				xmms_object_method_arg_t *arg;
				arg = xmms_object_arg_new (XMMS_OBJECT_METHOD_ARG_UINT32, GUINT_TO_POINTER (output->played_time));
				xmms_object_emit (XMMS_OBJECT (output), XMMS_SIGNAL_OUTPUT_PLAYTIME, arg);
				g_free (arg);
			}
		}

		if (xmms_decoder_iseos (output->decoder)) {
			XMMS_DBG ("decoder is EOS!");
			xmms_playlist_get_next_entry (output->playlist);
			xmms_object_unref (output->decoder);
			output->decoder = NULL;
			xmms_output_flush (output);
		}
	}

	if (output->decoder) {
		xmms_object_unref (output->decoder);
		output->decoder = NULL;
		xmms_output_flush (output);
	}

	xmms_output_unlock (output);

	xmms_output_close (output);

	xmms_object_unref (output);

	return NULL;
}


static void
xmms_output_decoder_kill (xmms_output_t *output, xmms_error_t *error)
{
	g_return_if_fail (output);

	XMMS_MTX_LOCK (output->mutex);
	if (output->decoder) {
		xmms_decoder_stop (output->decoder);
	}
	XMMS_MTX_UNLOCK (output->mutex);
}



#if 0 /* TO BE FIXED ---------------------------------------------
	 ---------------------------------------------------------
	 ---------------------------------------------------------
	 ---------------------------------------------------------
	 ---------------------------------------------------------
	 ---------------------------------------------------------
      */
gint
xmms_output_read (xmms_output_t *output, char *buffer, gint len)
{
	gint ret;
	xmms_output_buffersize_get_method_t buffersize_get_method;

	g_return_val_if_fail (output, -1);
	g_return_val_if_fail (buffer, -1);

	buffersize_get_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET);

	xmms_output_lock (output);
	
	if (!output->is_open) {
		output->is_open = TRUE;
		/*xmms_output_samplerate_set (output, output->open_samplerate);
		 */
	}
	
	ret = xmms_output_decoder_read (output, buffer, len);

	if (ret > 0) {
		guint played_time;

		output->played += ret;
		/** @todo some places we are counting in bytes,
		  in other in number of samples. Maybe we
		  want a xmms_sample_t and a XMMS_SAMPLE_SIZE */

		played_time = (guint)(output->played/(4.0f*output->open_samplerate/1000.0f));

		if (buffersize_get_method) {
			guint buffersize = buffersize_get_method (output);
			buffersize = buffersize/(2.0f*output->open_samplerate/1000.0f);

			if (played_time >= buffersize) {
				played_time -= buffersize;
			} else {
				played_time = 0;
			}
		}
		xmms_playback_playtime_set (xmms_core_playback_get (output->core), played_time);
	}

	if (ret < len) {
		output->buffer_underruns++;
	}

	output->bytes_written += ret;
	
	xmms_output_unlock (output);

	return ret;
}


static void
xmms_output_status_changed (xmms_object_t *object,
			    gconstpointer data,
			    gpointer udata)
{

	xmms_output_t *output = (xmms_output_t *)udata;
	xmms_output_status_method_t st;
	xmms_object_method_arg_t *arg = (xmms_object_method_arg_t *)data;

	g_return_if_fail (output);
	g_return_if_fail (arg);

	st = xmms_plugin_method_get (output->plugin,
				     XMMS_PLUGIN_METHOD_STATUS);

	if (st) {
		st (output, arg->retval.uint32);
	}

}
#endif
