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




/** @file
 * Decoder.
 *
 * This file contains functions that manipulate xmms_decoder_t objects.
 *
 */


#include "xmms/decoder.h"
#include "xmms/dbus.h"
#include "xmms/plugin.h"
#include "xmms/object.h"
#include "xmms/util.h"
#include "xmms/output.h"
#include "xmms/transport.h"
#include "xmms/ringbuf.h"
#include "xmms/playlist.h"
#include "xmms/visualisation.h"
#include "xmms/effect.h"
#include "xmms/signal_xmms.h"

#include "internal/decoder_int.h"
#include "internal/transport_int.h"
#include "internal/plugin_int.h"
#include "internal/output_int.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

/*
 * Type definitions
 */


/** @defgroup Decoder Decoder
  * @ingroup XMMSServer
  * @{
  */

/**
 * Replaygain modes.
 */
typedef enum {
	XMMS_REPLAYGAIN_MODE_TRACK,
	XMMS_REPLAYGAIN_MODE_ALBUM
	/** @todo implement dynamic replaygain */
} xmms_replaygain_mode_t;

/**
 * Structure describing decoder-objects.
 * Do not modify this structure directly outside this file, use the functions.
 */
struct xmms_decoder_St {
	xmms_object_t object;

	gboolean running;
	GThread *thread;
	GMutex *mutex;

	xmms_plugin_t *plugin;
	xmms_transport_t *transport; /**< transport associated with decoder.
	                               *   This is where the decoder gets it
	                               *   data from
	                               */

	gpointer plugin_data;

	xmms_playlist_entry_t *entry;

	xmms_effect_t *effect;


	xmms_output_t *output; /**< output associated with decoder.
	                         *   The decoded data will be written
	                         *   to this output.
	                         */
	xmms_visualisation_t *vis;

	guint samplerate;

	xmms_ringbuf_t *buffer;

	gboolean use_replaygain; /**< is replaygain enabled in the
	                           *  config?
	                           */
	xmms_replaygain_mode_t replaygain_mode;
	gboolean use_replaygain_anticlip; /**< is replaygain
	                                    *  clipping prevention
	                                    *  enabled in the config?
	                                    */

	/* has_replaygain is used so we don't have to check
	 * whether replaygain is 1.0 for efficiency
	 */
	gboolean has_replaygain;
	gfloat replaygain; /* final gain, combines scale and peak values */

	/* below has todo with the resampler */
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
 * Static function prototypes
 */

static xmms_plugin_t *xmms_decoder_find_plugin (const gchar *mimetype);
static gpointer xmms_decoder_thread (gpointer data);
static void recalculate_resampler (xmms_decoder_t *decoder);
static guint32 resample (xmms_decoder_t *decoder, gint16 *buf, guint len);
static void xmms_decoder_mediainfo_property_set (xmms_decoder_t *decoder, gchar *key, gchar *value);
static void on_replaygain_cfg_changed (xmms_object_t *obj, gconstpointer data, gpointer udata);
static gfloat get_replaygain (xmms_decoder_t *decoder, xmms_playlist_entry_t *entry, gboolean *has_replaygain);
static void apply_replaygain (gint16 *buf, guint len, gfloat gain);

/*
 * Macros
 */

#define FRAC_BITS 16

/*
 * Public functions
 */

/** @} */


/**
 * @defgroup DecoderPlugin DecoderPlugin
 * @ingroup XMMSPLugin
 * @brief Decoder plugin documentation.
 *
 * A decoder plugin takes data from the transport and
 * unpacks/transforms it to stereo PCM 16bit data. A decoder
 * plugin needs to define the following methods to be valid:
 *
 * #XMMS_PLUGIN_METHOD_NEW allocates a new decoder. This will be 
 * called if can_handle returns TRUE. The prototype is:
 * @code
 * gboolean new (xmms_decoder_t *decoder, const gchar *mimetype);
 * @endcode
 * This method need to set the private data with xmms_decoder_data_set()
 *
 * #XMMS_PLUGIN_METHOD_INIT Initializes the decoders private data.
 * This is called when XMMS wants to play this file. Will only
 * be called once. Prototype is:
 * @code
 * gboolean init (xmms_decoder_t *decoder);
 * @endcode
 * Return TRUE if all works out. FALSE will abort the playback
 * of this source.
 *
 * #XMMS_PLUGIN_METHOD_DESTROY Destroys the decoder private data.
 * Prototype is:
 * @code
 * void destroy (xmms_decoder_t *decoder);
 * @endcode
 * Free all private data in the decoder struct here.
 *
 * #XMMS_PLUGIN_METHOD_CAN_HANDLE this method will be called
 * when XMMS has a new source of data to check if this plugin will
 * take care of this. The prototype is:
 * @code
 * gboolean can_handle (const gchar *mimetype);
 * @endcode
 * This method should return TRUE if we want to handle this mimetype.
 *
 * #XMMS_PLUGIN_METHOD_DECODE_BLOCK unpacks or transforms a 
 * block of data from the transporter. The method calls
 * xmms_transport_read() to read the data and then xmms_decoder_write()
 * after the data is transformed/unpacked. The prototype of this method
 * is:
 * @code
 * gboolen decode_block (xmms_decoder_t *decoder);
 * @endcode
 * This method should return TRUE if decode_block can be called
 * again. FALSE will discontinue decoding.
 *
 * #XMMS_PLUGIN_METHOD_GET_MEDIAINFO Extracts the mediainfo for this 
 * sourcetype. This will be called from the mediainfo thread when
 * something is added to the playlist. Prototype is:
 * @code
 * void mediainfo (xmms_decoder_t *decoder);
 * @endcode
 * After the data is extracted it should be filled in a
 * #xmms_playlist_entry_t and be set with 
 * xmms_decoder_entry_mediainfo_set()
 *
 * #XMMS_PLUGIN_METHOD_SEEK Will be called when XMMS wants to
 * seek in the current source. The prototype is:
 * @code
 * void seek (xmms_decoder_t *decoder, guint samples);
 * @endcode
 * The argument is how many relative samples we want to seek to.
 * This method should call xmms_transport_seek() to seek in transporter.
 *
 * @{
 */

/**
 * Extract plugin specific data from the decoder.
 * This should be set with #xmms_decoder_private_data_set.
 *
 * @sa xmms_decoder_private_data_set
 */

gpointer
xmms_decoder_private_data_get (xmms_decoder_t *decoder)
{
	gpointer ret;
	g_return_val_if_fail (decoder, NULL);

	ret = decoder->plugin_data;

	return ret;
}

/**
 * Set the private data for the plugin.
 * Data can be extracted by #xmms_decoder_private_data_get.
 *
 * @sa xmms_decoder_private_data_get
 */

void
xmms_decoder_private_data_set (xmms_decoder_t *decoder, gpointer data)
{
	g_return_if_fail (decoder);

	decoder->plugin_data = data;
}

/**
 * Get the transport associated with the decoder.
 *
 * @param decoder apanap
 * @return the associated transport
 */

xmms_transport_t *
xmms_decoder_transport_get (xmms_decoder_t *decoder)
{
	xmms_transport_t *ret;
	g_return_val_if_fail (decoder, NULL);

	g_mutex_lock (decoder->mutex);
	ret = decoder->transport;
	g_mutex_unlock (decoder->mutex);

	return ret;
}

/**
 * Set the samplerate to the decoder and resampler.
 * This can be called in init or new method.
 */
void
xmms_decoder_samplerate_set (xmms_decoder_t *decoder, guint rate)
{
	gchar samplerate[8];

	g_return_if_fail (decoder);
	g_return_if_fail (rate);

	decoder->samplerate = rate;

	if (decoder->output) {
		xmms_output_samplerate_set (decoder->output, rate);
		recalculate_resampler (decoder);
	}


/*	xmms_visualisation_samplerate_set (decoder->vis, rate);*/
/*	xmms_effect_samplerate_set (decoder->effect, rate);*/
	
	g_snprintf (samplerate, sizeof (samplerate), "%d", rate);
	xmms_decoder_mediainfo_property_set (decoder,
	                                     XMMS_PLAYLIST_ENTRY_PROPERTY_SAMPLERATE,
	                                     samplerate);
}


/**
 * Update Mediainfo in the entry.
 * Should be used in #XMMS_PLUGIN_METHOD_GET_MEDIAINFO to update the info and
 * propagate it to the clients.
 */
void
xmms_decoder_entry_mediainfo_set (xmms_decoder_t *decoder, xmms_playlist_entry_t *entry)
{
	g_return_if_fail (decoder);
	g_return_if_fail (entry);
	g_return_if_fail (decoder->transport);

	xmms_object_ref (entry);
	xmms_object_unref (decoder->entry);
	decoder->entry = entry;

	decoder->replaygain = get_replaygain (decoder, entry,
	                                      &decoder->has_replaygain);
	xmms_transport_entry_mediainfo_set (decoder->transport, entry);

}


/**
 * Read decoded data
 */

guint
xmms_decoder_read (xmms_decoder_t *decoder, gchar *buf, guint len)
{
	guint ret;
	
	g_return_val_if_fail (decoder, -1);
	g_return_val_if_fail (buf, -1);

	g_mutex_lock (decoder->mutex);
	ret = xmms_ringbuf_read (decoder->buffer, buf, len);
	g_mutex_unlock (decoder->mutex);

	return ret;
}

/**
  * Get the EOS state on the decoder buffer.
  * @returns TRUE if decoder is EOS
  */

gboolean
xmms_decoder_iseos (xmms_decoder_t *decoder)
{
	gboolean ret;
	g_mutex_lock (decoder->mutex);
	ret = decoder->thread ? FALSE : TRUE;
	g_mutex_unlock (decoder->mutex);
	return ret;
}

/**
 * Write decoded data.
 * Should be used in #XMMS_PLUGIN_METHOD_DECODE_BLOCK to write the decoded
 * pcm-data to the output. The data should be 32bits per stereosample, ie
 * pairs of signed 16-bits samples. (in what byter order?)
 * xmms_decoder_samplerate_set must be called before any data is passed
 * to this function.
 *
 * @param decoder
 * @param buf buffer containing pcmdata in 2*S16
 * @param len size of buffen in bytes
 *
 */
void
xmms_decoder_write (xmms_decoder_t *decoder, gchar *buf, guint len)
{
	g_return_if_fail (decoder);

	if (decoder->has_replaygain && decoder->use_replaygain) {
		apply_replaygain ((gint16 *) buf, len / 2,
		                  decoder->replaygain);
	}
	
	xmms_effect_run (decoder->effect, buf, len);

/*	xmms_visualisation_calc (decoder->vis, buf, len);*/

	g_mutex_lock (decoder->mutex);

	if (decoder->interpolator_ratio != decoder->decimator_ratio) {
		/* resampling needed */
		len = resample (decoder, (gint16 *)buf, len);
		buf = (char *)decoder->resamplebuf;
	}
	xmms_ringbuf_wait_free (decoder->buffer, len, decoder->mutex);
	xmms_ringbuf_write (decoder->buffer, buf, len);

	g_mutex_unlock (decoder->mutex);
	
	
}

/** @} */


/** @ingroup Decoder
  * @{
  */

guint
xmms_decoder_samplerate_get (xmms_decoder_t *decoder)
{
	g_return_val_if_fail (decoder, 0);

	return decoder->samplerate;
}

gboolean
xmms_decoder_seek_ms (xmms_decoder_t *decoder, guint milliseconds, xmms_error_t *err)
{
	guint samples;
	g_return_val_if_fail (decoder, FALSE);
	
	samples = (guint)(((gdouble) decoder->samplerate) * milliseconds / 1000);

	return xmms_decoder_seek_samples (decoder, samples, err);

}

gboolean
xmms_decoder_seek_samples (xmms_decoder_t *decoder, guint samples, xmms_error_t *err)
{
	xmms_decoder_seek_method_t meth;
	
	g_return_val_if_fail (decoder, FALSE);

	meth = xmms_plugin_method_get (decoder->plugin, XMMS_PLUGIN_METHOD_SEEK);

	g_return_val_if_fail (meth, FALSE);

	xmms_output_flush (decoder->output);

	if (!meth (decoder, samples)) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "Could not seek there");
		return FALSE;
	}

	xmms_output_played_samples_set (decoder->output, samples);

	return TRUE;
}

/**
 * Get the output associated with the decoder.
 */
xmms_output_t *
xmms_decoder_output_get (xmms_decoder_t *decoder)
{
	xmms_output_t *ret;
	g_return_val_if_fail (decoder, NULL);

	g_mutex_lock (decoder->mutex);
	ret = decoder->output;
	g_mutex_unlock (decoder->mutex);

	return ret;
}

/**
 * Get the plugin used to instanciate this decoder.
 */
xmms_plugin_t *
xmms_decoder_plugin_get (xmms_decoder_t *decoder)
{
	xmms_plugin_t *ret;
	g_return_val_if_fail (decoder, NULL);

	g_mutex_lock (decoder->mutex);
	ret = decoder->plugin;
	g_mutex_unlock (decoder->mutex);

	return ret;
}

/*
 * Private functions
 */

static void
xmms_decoder_destroy (xmms_object_t *object) 
{
	xmms_decoder_t *decoder = (xmms_decoder_t *)object;
	xmms_decoder_destroy_method_t destroy_method;
	xmms_config_value_t *val;

	XMMS_DBG ("Destroying decoder!");
	XMMS_DBG ("MEMDBG: DECODER DEAD %p", object);

	xmms_ringbuf_set_eos (decoder->buffer, TRUE);

	val = xmms_config_lookup ("decoder.use_replaygain");
	xmms_config_value_callback_remove (val, on_replaygain_cfg_changed);
	val = xmms_config_lookup ("decoder.replaygain_mode");
	xmms_config_value_callback_remove (val, on_replaygain_cfg_changed);
	val = xmms_config_lookup ("decoder.use_replaygain_anticlip");
	xmms_config_value_callback_remove (val, on_replaygain_cfg_changed);

	destroy_method = xmms_plugin_method_get (decoder->plugin, XMMS_PLUGIN_METHOD_DESTROY);

	if (destroy_method)
		destroy_method (decoder);

	xmms_ringbuf_destroy (decoder->buffer);
	g_mutex_free (decoder->mutex);
	xmms_transport_close (decoder->transport);
	xmms_object_unref (decoder->transport);
	xmms_object_unref (decoder->plugin);
	xmms_object_unref (decoder->entry);
/*	xmms_object_unref (decoder->vis);*/
}

xmms_decoder_t *
xmms_decoder_new ()
{
	xmms_decoder_t *decoder;
	xmms_config_value_t *val;

	decoder = xmms_object_new (xmms_decoder_t, xmms_decoder_destroy);
	decoder->mutex = g_mutex_new ();
/*	decoder->vis = xmms_visualisation_init ();*/

	val = xmms_config_lookup ("decoder.buffersize");
	decoder->buffer = xmms_ringbuf_new (xmms_config_value_int_get (val));

	val = xmms_config_lookup ("decoder.use_replaygain");
	xmms_config_value_callback_set (val, on_replaygain_cfg_changed,
	                                decoder);
	decoder->use_replaygain = !!xmms_config_value_int_get (val);

	val = xmms_config_lookup ("decoder.replaygain_mode");
	xmms_config_value_callback_set (val, on_replaygain_cfg_changed,
	                                decoder);
	if (!g_ascii_strcasecmp (xmms_config_value_string_get (val),
	                         "album")) {
		decoder->replaygain_mode = XMMS_REPLAYGAIN_MODE_ALBUM;
	} else {
		decoder->replaygain_mode = XMMS_REPLAYGAIN_MODE_TRACK;
	}

	val = xmms_config_lookup ("decoder.use_replaygain_anticlip");
	xmms_config_value_callback_set (val, on_replaygain_cfg_changed,
	                                decoder);
	decoder->use_replaygain_anticlip = !!xmms_config_value_int_get (val);

	decoder->replaygain = 1.0;
	decoder->has_replaygain = FALSE;
	
	XMMS_DBG ("MEMDBG: DECODER NEW %p", decoder);

	return decoder;
}

gboolean
xmms_decoder_open (xmms_decoder_t *decoder, xmms_transport_t *transport)
{
	xmms_plugin_t *plugin;
	xmms_decoder_new_method_t new_method;
	const gchar *mimetype;

	g_return_val_if_fail (decoder, FALSE);
	g_return_val_if_fail (transport, FALSE);

	mimetype = xmms_transport_mimetype_get (transport);

	XMMS_DBG ("Trying to create decoder for mime-type %s", mimetype);
	
	plugin = xmms_decoder_find_plugin (mimetype);
	if (!plugin)
		return FALSE;
	
	xmms_object_ref (transport);

	XMMS_DBG ("Found plugin: %s", xmms_plugin_name_get (plugin));

	decoder->transport = transport;
	decoder->plugin = plugin;

	new_method = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_NEW);

	if (!new_method || !new_method (decoder, mimetype)) {
		xmms_log_error ("open failed");
		xmms_object_unref (transport);
		return FALSE;
	}

	return TRUE;
}


/**
 * Dispatch execution of the decoder.
 *
 * Associates the decoder with a transport and output.
 * Blesses it with a life of its own (a new thread is created)
 *
 * @param decoder
 * @param transport
 * @param effect
 * @param output
 *
 */
void
xmms_decoder_start (xmms_decoder_t *decoder, 
		    xmms_effect_t *effect, 
		    xmms_output_t *output)
{
	g_return_if_fail (decoder);
	g_return_if_fail (output);

	decoder->running = TRUE;
	decoder->effect = effect;
	decoder->output = output;
	decoder->thread = g_thread_create (xmms_decoder_thread, decoder, FALSE, NULL); 
}

void
xmms_decoder_stop (xmms_decoder_t *decoder)
{
	g_return_if_fail (decoder);
	g_mutex_lock (decoder->mutex);
	decoder->running = FALSE;
	xmms_ringbuf_set_eos (decoder->buffer, TRUE);
	g_mutex_unlock (decoder->mutex);
}

void
xmms_decoder_mediainfo_get (xmms_decoder_t *decoder, 
			    xmms_transport_t *transport)
{
	xmms_decoder_get_mediainfo_method_t mediainfo;
	g_return_if_fail (decoder);
	g_return_if_fail (transport);

	decoder->transport = transport;

	mediainfo = xmms_plugin_method_get (decoder->plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO);
	if (!mediainfo) {
		xmms_log_error ("get_mediainfo failed");
		return;
	}

	mediainfo (decoder);

	return;
}

/*
 * Static functions
 */

static void
on_replaygain_cfg_changed (xmms_object_t *obj, gconstpointer data,
                           gpointer udata)
{
	const gchar *name;
	xmms_decoder_t *decoder = udata;

	g_mutex_lock (decoder->mutex);

	name = xmms_config_value_name_get ((xmms_config_value_t *) obj);

	if (!g_ascii_strcasecmp (name, "decoder.use_replaygain")) {
		decoder->use_replaygain = !!atoi (data);
	} else if (!g_ascii_strcasecmp (name, "decoder.replaygain_mode")) {
		if (!g_ascii_strcasecmp (data, "album")) {
			decoder->replaygain_mode = XMMS_REPLAYGAIN_MODE_ALBUM;
		} else {
			decoder->replaygain_mode = XMMS_REPLAYGAIN_MODE_TRACK;
		}
	} else if (!g_ascii_strcasecmp (name,
	                                "decoder.use_replaygain_anticlip")) {
		decoder->use_replaygain_anticlip = !!atoi (data);
	}

	decoder->replaygain = get_replaygain (decoder, decoder->entry,
	                                      &decoder->has_replaygain);

	g_mutex_unlock (decoder->mutex);
}

static gfloat
get_replaygain (xmms_decoder_t *decoder, xmms_playlist_entry_t *entry,
                gboolean *has_replaygain)
{
	gfloat s, p;
	gchar *key_s, *key_p;
	const gchar *tmp;

	if (decoder->replaygain_mode == XMMS_REPLAYGAIN_MODE_TRACK) {
		key_s = XMMS_PLAYLIST_ENTRY_PROPERTY_GAIN_TRACK;
		key_p = XMMS_PLAYLIST_ENTRY_PROPERTY_PEAK_TRACK;
	} else {
		key_s = XMMS_PLAYLIST_ENTRY_PROPERTY_GAIN_ALBUM;
		key_p = XMMS_PLAYLIST_ENTRY_PROPERTY_PEAK_ALBUM;
	}

	tmp = xmms_playlist_entry_property_get (entry, key_s);
	s = tmp ? atof (tmp) : 1.0;

	tmp = xmms_playlist_entry_property_get (entry, key_p);
	p = tmp ? atof (tmp) : 1.0;

	if (decoder->use_replaygain_anticlip && s * p > 1.0) {
		s = 1.0 / p;
	}

	s = MIN (s, 15.0);

	/* This is NOT a value calculated by some scientist who has
	 * studied the ear for two decades.
	 * If you have a better value holler now, or keep your peace
	 * forever.
	 */
	*has_replaygain = (fabs (s - 1.0) > 0.001);

	return s;
}

static void
apply_replaygain (gint16 *buf, guint len, gfloat gain)
{
	guint i;

	for (i = 0; i < len; i++) {
		gfloat sample = buf[i] * gain;
		buf[i] = CLAMP (sample, G_MININT16, G_MAXINT16);
	}
}

static void
recalculate_resampler (xmms_decoder_t *decoder)
{
	guint a,b;
	guint32 incr;
	guint open_samplerate;

	open_samplerate = xmms_output_samplerate_get (decoder->output);

	/* calculate ratio */
	if(decoder->samplerate > open_samplerate){
		a = decoder->samplerate;
		b = open_samplerate;
	} else {
		b = decoder->samplerate;
		a = open_samplerate;
	}

	while (b != 0) { /* good 'ol euclid is helpful as usual */
		guint t = a % b;
		a = b;
		b = t;
	}

	XMMS_DBG ("Resampling ratio: %d:%d", 
		  decoder->samplerate / a, open_samplerate / a);

	decoder->interpolator_ratio = open_samplerate/a;
	decoder->decimator_ratio = decoder->samplerate/a;

	incr = ((gdouble)decoder->decimator_ratio / (gdouble) decoder->interpolator_ratio * (gdouble) (1UL << FRAC_BITS));
	
	decoder->opos_inc_frac=incr & ((1UL<<FRAC_BITS)-1);
	decoder->opos_inc = incr>>FRAC_BITS;

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
resample (xmms_decoder_t *decoder, gint16 *buf, guint len)
{

	guint32 written = 0;
	guint outlen;
	gint inlen = len / 4;
	gint16 last_r = decoder->last_r;
	gint16 last_l = decoder->last_l;
	gint16 *ibuf = buf;
	gint16 *obuf;
	
	outlen = 1 + len * decoder->interpolator_ratio  / decoder->decimator_ratio;

	if (decoder->resamplelen != outlen) {
		decoder->resamplelen = outlen;
		decoder->resamplebuf = g_realloc (decoder->resamplebuf, outlen*4);
		g_return_val_if_fail (decoder->resamplebuf, 0);
	}
	
	obuf = decoder->resamplebuf;
	
	while (inlen > 0) {
		guint32 tmp;
		gint16 cur_r,cur_l;
		gdouble t; /** @todo we need no double here,
			       could go fixed point all way */
		
		while (decoder->ipos <= decoder->opos) {
			last_r = *ibuf++;
			last_l = *ibuf++;
			decoder->ipos++;
			inlen--;
			if (inlen <= 0) {
				goto done;
			}
		}
		cur_r = *ibuf;
		cur_l = *(ibuf+1);
		
		t = ((gdouble)decoder->opos_frac) / (1UL<<FRAC_BITS);
		*obuf++ = (gdouble) last_r * (1.0-t) + (gdouble) cur_r * t;
		*obuf++ = (gdouble) last_l * (1.0-t) + (gdouble) cur_l * t;
		
		/* update output position */
		tmp = decoder->opos_frac + decoder->opos_inc_frac;
		decoder->opos += decoder->opos_inc + (tmp >> FRAC_BITS);
		decoder->opos_frac = tmp & ((1UL << FRAC_BITS)-1);

		written += 2*sizeof(gint16); /* count bytes */

	}
 done:
	decoder->last_r = last_r;
	decoder->last_l = last_l;
	
	g_return_val_if_fail (written/4<outlen, 0);
	
	return written;
		
}



static xmms_plugin_t *
xmms_decoder_find_plugin (const gchar *mimetype)
{
	GList *list, *node;
	xmms_plugin_t *plugin = NULL;
	xmms_decoder_can_handle_method_t can_handle;

	g_return_val_if_fail (mimetype, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_DECODER);
	XMMS_DBG ("List: %p", list);
	
	for (node = list; node; node = g_list_next (node)) {
		plugin = node->data;
		XMMS_DBG ("Trying plugin: %s", xmms_plugin_name_get (plugin));
		can_handle = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE);
		
		if (!can_handle)
			continue;

		if (can_handle (mimetype)) {
			xmms_object_ref (plugin);
			break;
		}
	}
	if (!node)
		plugin = NULL;

	if (list)
		xmms_plugin_list_destroy (list);

	return plugin;
}

static void
xmms_decoder_mediainfo_property_set (xmms_decoder_t *decoder, gchar *key, gchar *value)
{
	g_return_if_fail (decoder);
	g_return_if_fail (key);
	g_return_if_fail (value);
	g_return_if_fail (decoder->transport);

	xmms_transport_mediainfo_property_set (decoder->transport, key, value);

}

static gpointer
xmms_decoder_thread (gpointer data)
{
	xmms_decoder_t *decoder = data;
	xmms_decoder_decode_block_method_t decode_block;
	xmms_decoder_init_method_t init_meth;

	g_return_val_if_fail (decoder, NULL);

	decode_block = xmms_plugin_method_get (decoder->plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK);
	if (!decode_block)
		return NULL;

	init_meth = xmms_plugin_method_get (decoder->plugin, XMMS_PLUGIN_METHOD_INIT);
	if (init_meth) {
		if (!init_meth (decoder))
			return NULL;
	}
	
	xmms_object_ref (decoder);

	g_mutex_lock (decoder->mutex);

	while (42) {
		gboolean ret;
		
		g_mutex_unlock (decoder->mutex);
		ret = decode_block (decoder);
		g_mutex_lock (decoder->mutex);
		
		if (!ret || !decoder->running) {
			break;
		}
	}
	g_mutex_unlock (decoder->mutex);

	decoder->thread = NULL;
	XMMS_DBG ("Decoder thread quitting");

	xmms_object_unref (decoder);

	return NULL;
}

/** @} */
