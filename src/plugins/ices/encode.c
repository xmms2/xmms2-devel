/* encode.c
 * - runtime encoding of PCM data.
 *
 * $Id: encode.c,v 1.16 2003/03/22 02:27:55 karl Exp $
 *
 * Copyright (c) 2001 Michael Smith <msmith@labyrinth.net.au>
 *
 * This program is distributed under the terms of the GNU General
 * Public License, version 2. You may use, modify, and redistribute
 * it under the terms of this license. A copy should be included
 * with this source.
 */

/*
 * Modifications for xmms2
 * Copyright (C) 2003-2012 XMMS2 Team
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>

#include "encode.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_sample.h"

#define MODULE "encode/"

struct encoder_state {
	/* General encoder configuration. */
	int min_br;
	int nom_br;
	int max_br;
	int channels;
	int rate;

	gboolean encoder_inited;

	/* Ogg state. Remains active for the lifetime of the encoder. */
	ogg_stream_state os;
	int serial;
	gboolean in_header; /* TRUE if the stream is still within a vorbis
						 * header. */
	gboolean flushing; /* TRUE if the end of stream is reached and
						* we're just flushing the ogg data. */

	/* Used for ogg page size management. See xmms_ices_encoder_output
	 * for details. */
	int samples_in_current_page;
	ogg_int64_t previous_granulepos;

	/* Vorbis state. May be recreated if there is a change in
	 * rate/channels. */
	vorbis_info vi;
	vorbis_block vb;
	vorbis_dsp_state vd;
};


/* Create an ogg stream and vorbis encoder, with the configuration
 * specified in the encoder_state.
 */
static gboolean
xmms_ices_encoder_create (encoder_state *s, vorbis_comment *vc)
{
	ogg_packet header[3];

	if (s->encoder_inited) {
		XMMS_DBG ("OOPS: xmms_ices_encoder_create called "
		          "with s->encoder_inited == TRUE !");
	}

	XMMS_DBG ("Creating encoder in ABR mode: min/avg/max bitrate %d/%d/%d",
	          s->min_br, s->nom_br, s->max_br);

	/* Create the Vorbis encoder. */
	vorbis_info_init (&s->vi);
	if (vorbis_encode_init (&s->vi, s->channels, s->rate,
	                        s->max_br, s->nom_br, s->min_br) < 0)
		return FALSE;
	vorbis_analysis_init (&s->vd, &s->vi);
	vorbis_block_init (&s->vd, &s->vb);

	/* Initialize the ogg stream and input the vorbis header
	 * packets. */
	ogg_stream_init (&s->os, s->serial++);
	vorbis_analysis_headerout (&s->vd, vc, &header[0], &header[1], &header[2]);
	ogg_stream_packetin (&s->os, &header[0]);
	ogg_stream_packetin (&s->os, &header[1]);
	ogg_stream_packetin (&s->os, &header[2]);

	s->in_header = TRUE;
	s->flushing = FALSE;
	s->samples_in_current_page = 0;
	s->previous_granulepos = 0;
	s->encoder_inited = TRUE;

	return TRUE;
}

/* Free the ogg and vorbis encoder state associated with
 * encoder_state, if the encoder is present.
 */
static void
xmms_ices_encoder_free (encoder_state *s)
{
	if (s->encoder_inited) {
		ogg_stream_clear (&s->os);
		vorbis_block_clear (&s->vb);
		vorbis_dsp_clear (&s->vd);
		vorbis_info_clear (&s->vi);
		s->encoder_inited = FALSE;
	}
}


encoder_state *
xmms_ices_encoder_init (int min_br, int nom_br, int max_br)
{
	encoder_state *s;

	/* If none of these are set, it's obviously not supposed to be managed */
	if (nom_br <= 0)
		return NULL;

	s = g_new0 (encoder_state, 1);

	s->min_br = min_br;
	s->nom_br = nom_br;
	s->max_br = max_br;
	s->serial = 0;
	s->in_header = FALSE;
	s->encoder_inited = FALSE;

	return s;
}

void xmms_ices_encoder_fini (encoder_state *s) {
	xmms_ices_encoder_free (s);
	g_free (s);
}

/* Start a new logical ogg stream. */
gboolean xmms_ices_encoder_stream_change (encoder_state *s, int rate,
                                          int channels, vorbis_comment *vc)
{
	xmms_ices_encoder_free (s);
	s->rate = rate;
	s->channels = channels;
	return xmms_ices_encoder_create (s, vc);
}

/* Encode the given data into Ogg Vorbis. */
void xmms_ices_encoder_input (encoder_state *s, xmms_samplefloat_t *buf, int bytes)
{
	float **buffer;
	int i,j;
	int channels = s->vi.channels;
	int samples = bytes / (sizeof (xmms_samplefloat_t)*channels);

	buffer = vorbis_analysis_buffer (&s->vd, samples);

	for (i = 0; i < samples; i++)
		for (j = 0; j < channels; j++)
			buffer[j][i] = buf[i*channels + j];

	vorbis_analysis_wrote (&s->vd, samples);
	s->samples_in_current_page += samples;
}

/* Mark the end of the vorbis encoding, flush the vorbis buffers, and
 * mark the ogg stream as being in the flushing state. */
void xmms_ices_encoder_finish (encoder_state *s)
{
	ogg_packet op;

	vorbis_analysis_wrote (&s->vd, 0);

	while (vorbis_analysis_blockout (&s->vd, &s->vb)==1) {
		vorbis_analysis (&s->vb, NULL);
		vorbis_bitrate_addblock (&s->vb);
		while (vorbis_bitrate_flushpacket (&s->vd, &op))
			ogg_stream_packetin (&s->os, &op);
	}

	s->flushing = TRUE;
}

/* Returns TRUE if an ogg page was output, FALSE if there is nothing
 * left to do.
 */
gboolean xmms_ices_encoder_output (encoder_state *s, ogg_page *og)
{
	ogg_packet op;

	/* As long as we're still in the header, we still have the header
	 * packets to output. Loop over those before going to the actual
	 * vorbis data. */
	if (s->in_header) {
		if (ogg_stream_flush (&s->os, og))
			return TRUE;
		else
			s->in_header = FALSE;
	}

	/* If we're flushing the end of the stream, just output. */
	if (s->flushing) {
		if (ogg_stream_flush (&s->os, og))
			return TRUE;
		else
			return FALSE;
	}

	/* Flush the vorbis analysis stream into ogg packets, and add
	 * those to the ogg packet stream. */
	while (vorbis_analysis_blockout (&s->vd, &s->vb) == 1) {
		vorbis_analysis (&s->vb, NULL);
		vorbis_bitrate_addblock (&s->vb);

		while (vorbis_bitrate_flushpacket (&s->vd, &op))
			ogg_stream_packetin (&s->os, &op);
	}

	/* For live encoding, we want to stream pages regularly, rather
	 * than burst huge pages. Therefore, we periodically manually
	 * flush the stream. */
	if (s->samples_in_current_page > s->rate * 2) {
		if (!ogg_stream_flush (&s->os, og))
			return FALSE;
	} else {
		if (!ogg_stream_pageout (&s->os, og))
			return FALSE;
	}

	/* At this point, we have an ogg page in og. Keep bookkeeping
	 * accurate regarding the number of samples still in the page
	 * buffer, and return. */
	s->samples_in_current_page -= (ogg_page_granulepos (og)
	                               - s->previous_granulepos);
	s->previous_granulepos = ogg_page_granulepos (og);

	return TRUE;
}
