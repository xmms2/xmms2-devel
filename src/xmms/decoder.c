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
#include "xmms/core.h"
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

/*
 * Type definitions
 */



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
	xmms_playlist_entry_t *entry; /**< the current entry that is played */

	gpointer plugin_data;

	xmms_effect_t *effect;


	xmms_output_t *output;       /**< output associated with decoder.
				      *   The decoded data will be written
				      *   to this output.
				      */
	xmms_visualisation_t *vis;

	gint samplerate;

};

/*
 * Static function prototypes
 */

static void xmms_decoder_destroy_real (xmms_decoder_t *decoder);
static xmms_plugin_t *xmms_decoder_find_plugin (const gchar *mimetype);
static gpointer xmms_decoder_thread (gpointer data);

/*
 * Macros
 */

#define xmms_decoder_lock(d) g_mutex_lock ((d)->mutex)
#define xmms_decoder_unlock(d) g_mutex_unlock ((d)->mutex)

/*
 * Public functions
 */


/**
 * @defgroup Decoder Decoder
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
 * This should be set with xmms_decoder_plugin_data_set.
 *
 * @sa xmms_decoder_plugin_data_set
 */

gpointer
xmms_decoder_plugin_data_get (xmms_decoder_t *decoder)
{
	gpointer ret;
	g_return_val_if_fail (decoder, NULL);

	ret = decoder->plugin_data;

	return ret;
}

/**
 * Set the private data for the plugin.
 * Data can be extracted by xmms_decoder_plugin_data_get.
 *
 * @sa xmms_decoder_plugin_data_get
 */

void
xmms_decoder_plugin_data_set (xmms_decoder_t *decoder, gpointer data)
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

	xmms_decoder_lock (decoder);
	ret = decoder->transport;
	xmms_decoder_unlock (decoder);

	return ret;
}

/**
 * Set the samplerate to the decoder and resampler.
 * This can be called in init or new method.
 */
void
xmms_decoder_samplerate_set (xmms_decoder_t *decoder, guint rate)
{
	gchar *r;

	g_return_if_fail (decoder);
	g_return_if_fail (rate);

	if (decoder->output)
		xmms_output_samplerate_set (decoder->output, rate);
	xmms_visualisation_samplerate_set (decoder->vis, rate);
	xmms_effect_samplerate_set (decoder->effect, rate);
	decoder->samplerate = rate;
	
	r = g_strdup_printf ("%d", rate);
	xmms_playlist_entry_property_set (decoder->entry, XMMS_PLAYLIST_ENTRY_PROPERTY_SAMPLERATE, r);
	g_free (r);
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

	xmms_playlist_entry_property_copy (entry, decoder->entry);
	xmms_core_playlist_mediainfo_changed (xmms_playlist_entry_id_get (decoder->entry));
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
	xmms_output_t *output;

	g_return_if_fail (decoder);
	
	output = xmms_decoder_output_get (decoder);

	g_return_if_fail (output);

	xmms_effect_run (decoder->effect, buf, len);

	xmms_visualisation_calc (decoder->vis, buf, len);

	xmms_output_write (output, buf, len);
}

/** @} */

guint
xmms_decoder_samplerate_get (xmms_decoder_t *decoder)
{
	g_return_val_if_fail (decoder, 0);

	return decoder->samplerate;
}

void
xmms_decoder_seek_ms (xmms_decoder_t *decoder, guint milliseconds)
{
	guint samples;
	g_return_if_fail (decoder);
	
	samples = (guint)(((gdouble) decoder->samplerate) * milliseconds / 1000);

	xmms_decoder_seek_samples (decoder, samples);

}

void
xmms_decoder_seek_samples (xmms_decoder_t *decoder, guint samples)
{
	xmms_decoder_seek_method_t meth;
	
	g_return_if_fail (decoder);

	meth = xmms_plugin_method_get (decoder->plugin, XMMS_PLUGIN_METHOD_SEEK);

	g_return_if_fail (meth);

	xmms_output_flush (decoder->output);

	meth (decoder, samples);

	xmms_output_played_samples_set (decoder->output, samples);
}

/**
 * Get the output associated with the decoder.
 */
xmms_output_t *
xmms_decoder_output_get (xmms_decoder_t *decoder)
{
	xmms_output_t *ret;
	g_return_val_if_fail (decoder, NULL);

	xmms_decoder_lock (decoder);
	ret = decoder->output;
	xmms_decoder_unlock (decoder);

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

	xmms_decoder_lock (decoder);
	ret = decoder->plugin;
	xmms_decoder_unlock (decoder);

	return ret;
}


/**
 * Creates a new decoder for the specified mimetype.
 * This call creates a decoder that can handle the specified mimetype.
 * The transport is started but not the output, which must be driven by
 * hand. @sa tar.c for example.
 */
xmms_decoder_t *
xmms_decoder_new_stacked (xmms_output_t *output, xmms_transport_t *transport, xmms_playlist_entry_t *entry)
{
	xmms_decoder_t *decoder;

	xmms_playlist_entry_ref (entry);

	decoder = xmms_decoder_new ();
	/** @todo felhantering??? */
	xmms_decoder_open (decoder, entry);
	decoder->transport = transport;
	decoder->output = output;
	decoder->entry = entry;

	XMMS_DBG ("starting threads..");
	xmms_transport_start (transport);
	XMMS_DBG ("transport started");
	return decoder;
}


/*
 * Private functions
 */

xmms_decoder_t *
xmms_decoder_new ()
{
	xmms_decoder_t *decoder;

	decoder = g_new0 (xmms_decoder_t, 1);
	xmms_object_init (XMMS_OBJECT (decoder));
	decoder->mutex = g_mutex_new ();
	decoder->vis = xmms_visualisation_init();

	return decoder;
}

gboolean
xmms_decoder_open (xmms_decoder_t *decoder, xmms_playlist_entry_t *entry)
{
	xmms_plugin_t *plugin;
	xmms_decoder_new_method_t new_method;
	const gchar *mimetype;

	g_return_val_if_fail (entry, FALSE);
	g_return_val_if_fail (decoder, FALSE);

	mimetype = xmms_playlist_entry_mimetype_get (entry);

	XMMS_DBG ("Trying to create decoder for mime-type %s", mimetype);
	
	plugin = xmms_decoder_find_plugin (mimetype);
	if (!plugin)
		return FALSE;
	
	xmms_playlist_entry_ref (entry);

	XMMS_DBG ("Found plugin: %s", xmms_plugin_name_get (plugin));

	decoder->entry = entry;
	decoder->plugin = plugin;

	new_method = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_NEW);

	if (!new_method || !new_method (decoder, mimetype)) {
		XMMS_DBG ("open failed");
		xmms_playlist_entry_unref (entry);
		return FALSE;
	}

	return TRUE;
}


/**
 * Free resources used by decoder.
 *
 * The decoder is signalled to stop working and free up any resources
 * it has been using.
 *
 * @param decoder the decoder to free. After this called it must not be referenced again.
 */
void
xmms_decoder_destroy (xmms_decoder_t *decoder)
{
	g_return_if_fail (decoder);

	if (decoder->thread) {
		XMMS_DBG ("decoder thread still alive?");
		xmms_decoder_lock (decoder);
		decoder->running = FALSE;
		xmms_decoder_unlock (decoder);
		//g_thread_join (decoder->thread);
	} else {
		xmms_decoder_destroy_real (decoder);
	}
		
}

/**
 * Dispatch execution of the decoder.
 *
 * Associates the decoder with a transport and output.
 * Blesses it with a life of its own (a new thread is created)
 *
 * @param decoder
 * @param transport
 * @param output
 *
 */
void
xmms_decoder_start (xmms_decoder_t *decoder, xmms_transport_t *transport, xmms_effect_t *effect, xmms_output_t *output)
{
	g_return_if_fail (decoder);
	g_return_if_fail (output);

	decoder->running = TRUE;
	decoder->transport = transport;
	decoder->effect = effect;
	decoder->output = output;
	xmms_output_set_eos (output, FALSE);
	decoder->thread = g_thread_create (xmms_decoder_thread, decoder, TRUE, NULL); 
}

void
xmms_decoder_wait (xmms_decoder_t *decoder)
{
	g_return_if_fail (decoder);

	if (decoder->running) {
		g_thread_join (decoder->thread);
		//decoder->thread = NULL;
	}

}

xmms_playlist_entry_t *
xmms_decoder_mediainfo_get (xmms_decoder_t *decoder, 
			    xmms_transport_t *transport)
{
	xmms_decoder_get_mediainfo_method_t mediainfo;
	g_return_val_if_fail (decoder, NULL);
	g_return_val_if_fail (transport, NULL);

	decoder->transport = transport;

	mediainfo = xmms_plugin_method_get (decoder->plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO);
	if (!mediainfo) {
		XMMS_DBG ("get_mediainfo failed");
		return NULL;
	}

	mediainfo (decoder);

	return decoder->entry;
}

/*
 * Static functions
 */

static void
xmms_decoder_destroy_real (xmms_decoder_t *decoder)
{
	xmms_decoder_destroy_method_t destroy_method;
	
	g_return_if_fail (decoder);
	
	destroy_method = xmms_plugin_method_get (decoder->plugin, XMMS_PLUGIN_METHOD_DESTROY);
	if (destroy_method)
		destroy_method (decoder);

	g_mutex_free (decoder->mutex);

	XMMS_DBG ("Decoder unref");
	xmms_playlist_entry_unref (decoder->entry);

	xmms_visualisation_destroy(decoder->vis);
	xmms_object_cleanup (XMMS_OBJECT (decoder));
	g_free (decoder);
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
			xmms_plugin_ref (plugin);
			break;
		}
	}
	if (!node)
		plugin = NULL;

	if (list)
		xmms_plugin_list_destroy (list);

	return plugin;
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

	xmms_object_emit (XMMS_OBJECT (decoder), XMMS_SIGNAL_PLAYBACK_CURRENTID, GUINT_TO_POINTER (xmms_playlist_entry_id_get (decoder->entry)));
	
	xmms_decoder_lock (decoder);

	while (42) {
		gboolean ret;
		
		xmms_decoder_unlock (decoder);
		ret = decode_block (decoder);
		xmms_decoder_lock (decoder);
		
		if (!ret || !decoder->running) {
			break;
		}
	}
	xmms_output_set_eos (decoder->output, TRUE);
	g_mutex_unlock (decoder->mutex);

	decoder->thread = NULL;
	XMMS_DBG ("Decoder thread quitting");
	xmms_decoder_destroy_real (decoder);

	return NULL;
}

