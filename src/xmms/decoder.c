/** @file
 * Decoder.
 *
 * This file contains functions that manipulate xmms_decoder_t objects.
 *
 */


#include "decoder.h"
#include "core.h"
#include "decoder_int.h"
#include "plugin.h"
#include "plugin_int.h"
#include "object.h"
#include "util.h"
#include "output.h"
#include "output_int.h"
#include "transport.h"
#include "transport_int.h"
#include "ringbuf.h"
#include "playlist.h"
#include "visualisation.h"

#include <string.h>

/*
 * Type definitions
 */

#define FRAC_BITS 16


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

	guint samplerate;
	guint output_samplerate;

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

	xmms_output_t *output;       /**< output associated with decoder.
				      *   The decoded data will be written
				      *   to this output.
				      */
	xmms_visualisation_t *vis;

};

/*
 * Static function prototypes
 */

static void xmms_decoder_destroy_real (xmms_decoder_t *decoder);
static xmms_plugin_t *xmms_decoder_find_plugin (const gchar *mimetype);
static gpointer xmms_decoder_thread (gpointer data);
static guint32 resample (xmms_decoder_t *decoder, gint16 *buf, guint len);

/*
 * Macros
 */

#define xmms_decoder_lock(d) g_mutex_lock ((d)->mutex)
#define xmms_decoder_unlock(d) g_mutex_unlock ((d)->mutex)

/*
 * Public functions
 */

gpointer
xmms_decoder_plugin_data_get (xmms_decoder_t *decoder)
{
	gpointer ret;
	g_return_val_if_fail (decoder, NULL);

	ret = decoder->plugin_data;

	return ret;
}

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

static void
recalculate_resampler (xmms_decoder_t *decoder)
{
	guint a,b;
	guint32 incr;

	/* calculate ratio */
	if(decoder->samplerate > decoder->output_samplerate){
		a = decoder->samplerate;
		b = decoder->output_samplerate;
	} else {
		b = decoder->samplerate;
		a = decoder->output_samplerate;
	}

	while (b != 0) { /* good 'ol euclid is helpful as usual */
		guint t = a % b;
		a = b;
		b = t;
	}

	XMMS_DBG ("Resampling ratio: %d:%d", 
		  decoder->samplerate / a, decoder->output_samplerate / a);

	decoder->interpolator_ratio = decoder->output_samplerate/a;
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

void
xmms_decoder_samplerate_set (xmms_decoder_t *decoder, guint rate)
{
	g_return_if_fail (decoder);
	g_return_if_fail (rate);
	
	if (decoder->samplerate != rate) {
		xmms_visualisation_samplerate_set (decoder->vis, rate);
		decoder->samplerate = rate;
		decoder->output_samplerate = xmms_output_samplerate_set (decoder->output, rate);
		recalculate_resampler (decoder);
	}
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

	decoder = xmms_decoder_new (entry);
	decoder->transport = transport;
	decoder->output = output;
	decoder->entry = entry;

	XMMS_DBG ("starting threads..");
	xmms_transport_start (transport);
	XMMS_DBG ("transport started");
	return decoder;
}


/**
 * Update Mediainfo in the entry.
 * Should be used in XMMS_PLUIGN_METHOD_GET_MEDIAINFO to update the info and
 * propagate it to the clients.
 */
void
xmms_decoder_entry_mediainfo_set (xmms_decoder_t *decoder, xmms_playlist_entry_t *entry)
{
	g_return_if_fail (decoder);
	g_return_if_fail (entry);

	xmms_playlist_entry_copy_property (entry, decoder->entry);
	xmms_core_playlist_mediainfo_changed (xmms_playlist_entry_id_get (entry));
}


/**
 * Write decoded data.
 * Should be used in XMMS_PLUGIN_METHOD_DECODE_BLOCK to write the decoded
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

	xmms_visualisation_calc (decoder->vis, buf, len);

	if (decoder->samplerate != decoder->output_samplerate) {
		guint32 res;
		res=resample (decoder, (gint16 *)buf, len);
		xmms_output_write (output, decoder->resamplebuf, res);
	} else {
		xmms_output_write (output, buf, len);
	}

}

/*
 * Private functions
 */

xmms_decoder_t *
xmms_decoder_new (xmms_playlist_entry_t *entry)
{
	xmms_plugin_t *plugin;
	xmms_decoder_t *decoder;
	xmms_decoder_new_method_t new_method;
	const gchar *mimetype;

	g_return_val_if_fail (entry, NULL);

	xmms_playlist_entry_ref (entry);

	mimetype = xmms_playlist_entry_mimetype_get (entry);

	XMMS_DBG ("Trying to create decoder for mime-type %s", mimetype);
	
	plugin = xmms_decoder_find_plugin (mimetype);
	if (!plugin)
		return NULL;
	
	XMMS_DBG ("Found plugin: %s", xmms_plugin_name_get (plugin));

	decoder = g_new0 (xmms_decoder_t, 1);
	xmms_object_init (XMMS_OBJECT (decoder));
	decoder->plugin = plugin;
	decoder->mutex = g_mutex_new ();
	decoder->entry = entry;

	decoder->vis = xmms_visualisation_init();

	new_method = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_NEW);

	if (!new_method || !new_method (decoder, mimetype)) {
		XMMS_DBG ("new failed");
		xmms_playlist_entry_unref (entry);
		g_free (decoder);
	}

	return decoder;
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
		xmms_decoder_lock (decoder);
		decoder->running = FALSE;
		xmms_decoder_unlock (decoder);
		g_thread_join (decoder->thread);
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
xmms_decoder_start (xmms_decoder_t *decoder, xmms_transport_t *transport, xmms_output_t *output)
{
	g_return_if_fail (decoder);
	g_return_if_fail (output);

	decoder->running = TRUE;
	decoder->transport = transport;
	decoder->output = output;
	xmms_output_set_eos (output, FALSE);
	decoder->thread = g_thread_create (xmms_decoder_thread, decoder, TRUE, NULL); 
}

/*void
xmms_decoder_wait (xmms_decoder_t *decoder)
{
	g_return_if_fail (decoder);


}
*/
xmms_playlist_entry_t *
xmms_decoder_get_mediainfo (xmms_decoder_t *decoder, 
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

static void
xmms_decoder_destroy_real (xmms_decoder_t *decoder)
{
	xmms_decoder_destroy_method_t destroy_method;
	
	g_return_if_fail (decoder);
	
	destroy_method = xmms_plugin_method_get (decoder->plugin, XMMS_PLUGIN_METHOD_DESTROY);
	if (destroy_method)
		destroy_method (decoder);

	g_mutex_free (decoder->mutex);

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

	XMMS_DBG ("Decoder thread quiting");

	return NULL;
}

