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
 * 
 */

#include "xmms/output.h"
#include "xmms/object.h"
#include "xmms/ringbuf.h"
#include "xmms/util.h"
#include "xmms/core.h"
#include "xmms/config.h"
#include "xmms/plugin.h"
#include "xmms/signal_xmms.h"

#include "internal/plugin_int.h"
#include "internal/output_int.h"

#define xmms_output_lock(t) g_mutex_lock ((t)->mutex)
#define xmms_output_unlock(t) g_mutex_unlock ((t)->mutex)

#define FRAC_BITS 16

static gpointer xmms_output_thread (gpointer data);
static guint32 resample (xmms_output_t *output, gint16 *buf, guint len);

/*
 * Type definitions
 */

struct xmms_output_St {
	xmms_object_t object;
	xmms_plugin_t *plugin;

	GMutex *mutex;
	GCond *cond;
	GThread *thread;
	gboolean running;

	guint played;
	gboolean is_open;
	gboolean is_paused;

	guint open_samplerate;
	guint samplerate;

	xmms_ringbuf_t *buffer;
	
	gpointer plugin_data;

	guint resamplelen;
	gint16 *resamplebuf;

	guint interpolator_ratio;
	guint decimator_ratio;

	gint16 last_r;
	gint16 last_l;

	guint32 opos;      /* position in output */
        guint32 opos_frac;

        guint32 opos_inc;  /* increment in output for each input-sample */
        guint32 opos_inc_frac;

        guint32 ipos;      /* position in the input stream */

};

/*
 * Public functions
 */

gpointer
xmms_output_plugin_data_get (xmms_output_t *output)
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

gboolean
xmms_output_volume_get (xmms_output_t *output, gint *left, gint *right)
{
	xmms_output_volume_get_method_t vol;
	g_return_val_if_fail (output, FALSE);

	vol = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_MIXER_GET);
	g_return_val_if_fail (vol, FALSE);

	return vol (output, left, right);
}

void
xmms_output_plugin_data_set (xmms_output_t *output, gpointer data)
{
	output->plugin_data = data;
}

/*
 * Private functions
 */

static void
recalculate_resampler (xmms_output_t *output)
{
	guint a,b;
	guint32 incr;

	/* calculate ratio */
	if(output->samplerate > output->open_samplerate){
		a = output->samplerate;
		b = output->open_samplerate;
	} else {
		b = output->samplerate;
		a = output->open_samplerate;
	}

	while (b != 0) { /* good 'ol euclid is helpful as usual */
		guint t = a % b;
		a = b;
		b = t;
	}

	XMMS_DBG ("Resampling ratio: %d:%d", 
		  output->samplerate / a, output->open_samplerate / a);

	output->interpolator_ratio = output->open_samplerate/a;
	output->decimator_ratio = output->samplerate/a;

	incr = ((gdouble)output->decimator_ratio / (gdouble) output->interpolator_ratio * (gdouble) (1UL << FRAC_BITS));
	
	output->opos_inc_frac=incr & ((1UL<<FRAC_BITS)-1);
	output->opos_inc = incr>>FRAC_BITS;

	/*
	 * calculate filter here
	 *
	 * We don't use no stinkning filter. Maybe we should,
	 * but I'm deaf anyway, I wont hear any difference.
	 */

}

/*
 * This resampler is based on the one in sox's rate.c:
 *
 * August 21, 1998
 * Copyright 1998 Fabrice Bellard.
 *
 * [Rewrote completly the code of Lance Norskog And Sundry
 * Contributors with a more efficient algorithm.]
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.  
 *
 */
static guint32
resample (xmms_output_t *output, gint16 *buf, guint len)
{

	guint32 written = 0;
	guint outlen;
	gint inlen = len / 4;
	gint16 last_r = output->last_r;
	gint16 last_l = output->last_l;
	gint16 *ibuf = buf;
	gint16 *obuf;
	
	outlen = 1 + len * output->interpolator_ratio  / output->decimator_ratio;

	if (output->resamplelen != outlen) {
		output->resamplelen = outlen;
		output->resamplebuf = g_realloc (output->resamplebuf, outlen*4);
		g_return_val_if_fail (output->resamplebuf, 0);
	}
	
	obuf = output->resamplebuf;
	
	while (inlen > 0) {
		guint32 tmp;
		gint16 cur_r,cur_l;
		gdouble t; /** @todo we need no double here,
			       could go fixed point all way */
		
		while (output->ipos <= output->opos) {
			last_r = *ibuf++;
			last_l = *ibuf++;
			output->ipos++;
			inlen--;
			if (inlen <= 0) {
				goto done;
			}
		}
		cur_r = *ibuf;
		cur_l = *(ibuf+1);
		
		t = ((gdouble)output->opos_frac) / (1UL<<FRAC_BITS);
		*obuf++ = (gdouble) last_r * (1.0-t) + (gdouble) cur_r * t;
		*obuf++ = (gdouble) last_l * (1.0-t) + (gdouble) cur_l * t;
		
		/* update output position */
		tmp = output->opos_frac + output->opos_inc_frac;
		output->opos += output->opos_inc + (tmp >> FRAC_BITS);
		output->opos_frac = tmp & ((1UL << FRAC_BITS)-1);

		written += 2*sizeof(gint16); /* count bytes */

	}
 done:
	output->last_r = last_r;
	output->last_l = last_l;
	
	g_return_val_if_fail (written/4<outlen, 0);
	
	return written;
		
}

/**
 * @internal
 */
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

void
xmms_output_samplerate_set (xmms_output_t *output, guint rate)
{
	xmms_output_samplerate_set_method_t samplerate_method;

	g_return_if_fail (output);
	g_return_if_fail (rate);

	output->samplerate = rate;
	if (output->is_open) {
		samplerate_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET);
		XMMS_DBG ("want samplerate %d", rate);
		output->open_samplerate = samplerate_method (output, rate);
		XMMS_DBG ("samplerate set to: %d", output->open_samplerate);
	} else {
		output->open_samplerate = rate;
	}
	recalculate_resampler (output);

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

	output->is_open = TRUE;

	XMMS_DBG ("opening with samplerate: %d",output->open_samplerate);
	xmms_output_samplerate_set (output, output->open_samplerate);

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

	output->is_open = FALSE;

}

xmms_output_t *
xmms_output_new (xmms_plugin_t *plugin)
{
	xmms_output_t *output;
	xmms_output_new_method_t new;
	
	g_return_val_if_fail (plugin, NULL);

	XMMS_DBG ("Trying to open output");

	output = g_new0 (xmms_output_t, 1);
	xmms_object_init (XMMS_OBJECT (output));
	output->plugin = plugin;
	output->mutex = g_mutex_new ();
	output->cond = g_cond_new ();
	output->buffer = xmms_ringbuf_new (32768);
	output->is_open = FALSE;
	output->open_samplerate = 44100;
	
	new = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_NEW);

	if (!new (output)) {
		g_mutex_free (output->mutex);
		g_cond_free (output->cond);
		xmms_ringbuf_destroy (output->buffer);
		g_free (output);
		return NULL;
	}
	
	return output;
}

void
xmms_output_flush (xmms_output_t *output)
{
	xmms_output_flush_method_t flush;

	g_return_if_fail (output);
	
	flush = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_FLUSH);
	g_return_if_fail (flush);

	g_mutex_lock (output->mutex);

	xmms_ringbuf_clear (output->buffer);

	flush (output);

	g_mutex_unlock (output->mutex);
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
xmms_output_start (xmms_output_t *output)
{
	g_return_if_fail (output);

	output->running = TRUE;
	output->thread = g_thread_create (xmms_output_thread, output, TRUE, NULL);
}

void
xmms_output_set_eos (xmms_output_t *output, gboolean eos)
{
	g_return_if_fail (output);
	
	xmms_ringbuf_set_eos (output->buffer, eos);
	if (!eos) {
		g_cond_signal (output->cond);
	}

	output->played = 0;

}

void
xmms_output_played_samples_set (xmms_output_t *output, guint samples)
{
	g_return_if_fail (output);
	g_mutex_lock (output->mutex);
	output->played = samples * 4;
	XMMS_DBG ("Set played to %d", output->played);
	g_mutex_unlock (output->mutex);
}


gboolean
xmms_output_is_paused (xmms_output_t *output)
{
	g_return_val_if_fail (output, FALSE);

	return output->is_paused;
}

void
xmms_output_pause (xmms_output_t *output)
{
	g_return_if_fail (output);
	output->is_paused = TRUE;
}

void
xmms_output_resume (xmms_output_t *output)
{
	g_return_if_fail (output);
	g_cond_signal (output->cond);
}

static gpointer
xmms_output_thread (gpointer data)
{
	xmms_output_t *output;
	xmms_output_buffersize_get_method_t buffersize_get_method;
	xmms_output_write_method_t write_method;

	output = (xmms_output_t*)data;
	g_return_val_if_fail (data, NULL);

	write_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_WRITE);
	g_return_val_if_fail (write_method, NULL);

	buffersize_get_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET);

	xmms_output_lock (output);
	while (output->running) {
		gchar buffer[4096];
		gint ret;

		xmms_ringbuf_wait_used (output->buffer, 4096, output->mutex);

		if (!output->is_open) {
			if (!xmms_output_open (output)) {
				XMMS_DBG ("Couldn't open output device");
				xmms_object_emit (XMMS_OBJECT (output), XMMS_SIGNAL_OUTPUT_OPEN_FAIL, NULL);
			}
		}

		if (output->is_paused) {
			XMMS_DBG ("Paused");
			g_cond_wait (output->cond, output->mutex);
			XMMS_DBG ("Resumed");
			output->is_paused = FALSE;
		}

		ret = xmms_ringbuf_read (output->buffer, buffer, 4096);
		
		if (ret > 0) {
			guint played_time;
			xmms_output_unlock (output);
			write_method (output, buffer, ret);
			xmms_output_lock (output);

			output->played += ret;
			/** @todo some places we are counting in bytes,
			    in other in number of samples. Maybe we
			    want a xmms_sample_t and a XMMS_SAMPLE_SIZE */
			
			played_time = (guint)(output->played/(4.0f*output->open_samplerate/1000.0f));

			if (buffersize_get_method) {
				guint buffersize = buffersize_get_method (output);
				buffersize = buffersize/(2.0f*output->open_samplerate/1000.0f);
/*				XMMS_DBG ("buffer: %dms", buffersize);*/

				if (played_time >= buffersize) {
					played_time -= buffersize;
				} else {
					played_time = 0;
				}
			}
			xmms_core_playtime_set (played_time);

			
		} else if (xmms_ringbuf_iseos (output->buffer)) {
			GTimeVal time;

			xmms_output_unlock (output);
			xmms_object_emit (XMMS_OBJECT (output), XMMS_SIGNAL_OUTPUT_EOS_REACHED, NULL);
			xmms_output_lock (output);
			output->played = 0;
			
			while (xmms_ringbuf_iseos (output->buffer)) {
				g_get_current_time (&time);
				g_time_val_add (&time, 10 * G_USEC_PER_SEC);
				if (!g_cond_timed_wait (output->cond, output->mutex, &time)){
					if (output->is_open) {
						XMMS_DBG ("Timed out");
						xmms_output_close (output);
					}
				}
			}

		}
	}
	xmms_output_unlock (output);

	/* FIXME: Cleanup */
	
	return NULL;
}
