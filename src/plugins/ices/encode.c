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

#ifdef HAVE_CONFIG_H
 #include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>

#include "encode.h"
#include "xmms/util.h"

#define MODULE "encode/"

void encode_clear(encoder_state *s)
{
    if(s)
    {
        ogg_stream_clear(&s->os);
        vorbis_block_clear(&s->vb);
        vorbis_dsp_clear(&s->vd);
        vorbis_info_clear(&s->vi);
        free(s);
    }
}

encoder_state *encode_initialise(int channels, int rate, int managed,
        int min_br, int nom_br, int max_br, float quality,
        int serial, vorbis_comment *vc)
{
    encoder_state *s = calloc(1, sizeof(encoder_state));
    ogg_packet h1,h2,h3;

    /* If none of these are set, it's obviously not supposed to be managed */
    if (nom_br < 0 && min_br < 0 && max_br < 0) {
        managed = 0;
    }

    if (managed) {
        XMMS_DBG("Encoder initialising with bitrate management: %d "
                "channels, %d Hz, minimum bitrate %d, nominal %d, "
                "maximum %d", channels, rate, min_br, nom_br, max_br);
    } else {
        if (min_br > 0 || max_br > 0) {
            XMMS_DBG("Encoder initialising in constrained VBR mode: %d "
                    "channels, %d Hz, quality %f, minimum bitrate %d, "
                    "maximum %d", channels, rate, quality, min_br, max_br);
        } else {
            XMMS_DBG("Encoder initialising in VBR mode: %d channel(s), %d Hz, "
                    "quality %f", channels, rate, quality);
        }
    }

    /* Have vorbisenc choose a mode for us */
    vorbis_info_init(&s->vi);

    if (managed) {
        if (vorbis_encode_setup_managed(&s->vi, channels, rate,
                    max_br>0?max_br:-1, nom_br, min_br>0?min_br:-1)) {
            XMMS_DBG("Failed to configure managed encoding for "
                    "%d channel(s), at %d Hz, with bitrates %d max %d "
                    "nominal, %d min", channels, rate, max_br, nom_br, min_br);
            vorbis_info_clear(&s->vi);
            free(s);
            return NULL;
        }
    } else {
        if (vorbis_encode_setup_vbr(&s->vi, channels, rate, quality*0.1)) {
            XMMS_DBG("Failed to configure VBR encoding for %d channel(s), "
                    "at %d Hz, quality level %f", channels, rate, quality);
            vorbis_info_clear(&s->vi);
            free(s);
            return NULL;
        }

        if (max_br > 0 || min_br > 0) {
            struct ovectl_ratemanage_arg ai;
            vorbis_encode_ctl(&s->vi, OV_ECTL_RATEMANAGE_GET, &ai);
            ai.bitrate_hard_min = min_br;
            ai.bitrate_hard_max = max_br;
            ai.management_active = 1;
            vorbis_encode_ctl(&s->vi, OV_ECTL_RATEMANAGE_SET, &ai);
        }
    }

    if (managed && nom_br < 0) {
        vorbis_encode_ctl(&s->vi, OV_ECTL_RATEMANAGE_AVG, NULL);
    } else if (!managed) {
        vorbis_encode_ctl(&s->vi, OV_ECTL_RATEMANAGE_SET, NULL);
    }
    
    vorbis_encode_setup_init(&s->vi);

    vorbis_analysis_init(&s->vd, &s->vi);
    vorbis_block_init(&s->vd, &s->vb);

    ogg_stream_init(&s->os, serial);

    vorbis_analysis_headerout(&s->vd, vc, &h1,&h2,&h3);
    ogg_stream_packetin(&s->os, &h1);
    ogg_stream_packetin(&s->os, &h2);
    ogg_stream_packetin(&s->os, &h3);

    s->in_header = 1;
    s->samplerate = rate;
    s->samples_in_current_page = 0;
    s->prevgranulepos = 0;

    return s;
}

void encode_data_float(encoder_state *s, float **pcm, int samples)
{
    float **buf;
    int i;

    buf = vorbis_analysis_buffer(&s->vd, samples); 

    for(i=0; i < s->vi.channels; i++)
    {
        memcpy(buf[i], pcm[i], samples*sizeof(float));
    }

    vorbis_analysis_wrote(&s->vd, samples);

    s->samples_in_current_page += samples;
}

/* Requires little endian data (currently) */
void encode_data(encoder_state *s, signed char *buf, int bytes, int bigendian)
{
    float **buffer;
    int i,j;
    int channels = s->vi.channels;
    int samples = bytes/(2*channels);

    buffer = vorbis_analysis_buffer(&s->vd, samples);

    if(bigendian)
    {
        for(i=0; i < samples; i++)
        {
            for(j=0; j < channels; j++)
            {
                buffer[j][i]=((buf[2*(i*channels + j)]<<8) |
                              (0x00ff&(int)buf[2*(i*channels + j)+1]))/32768.f;
            }
        }
    }
    else
    {
        for(i=0; i < samples; i++)
        {
            for(j=0; j < channels; j++)
            {
                buffer[j][i]=((buf[2*(i*channels + j) + 1]<<8) |
                              (0x00ff&(int)buf[2*(i*channels + j)]))/32768.f;
            }
        }
    }

    vorbis_analysis_wrote(&s->vd, samples);

    s->samples_in_current_page += samples;
}


/* Returns:
 *   0     No output at this time
 *   >0    Page produced
 *
 * Caller should loop over this to ensure that we don't end up with
 * excessive buffering in libvorbis.
 */
int encode_dataout(encoder_state *s, ogg_page *og)
{
    ogg_packet op;
    int result;

    if(s->in_header)
    {
        result = ogg_stream_flush(&s->os, og);
        if(result==0) 
        {
            s->in_header = 0;
            return encode_dataout(s,og);
        }
        else
            return 1;
    }
    else
    {
        while(vorbis_analysis_blockout(&s->vd, &s->vb)==1)
        {
            vorbis_analysis(&s->vb, NULL);
            vorbis_bitrate_addblock(&s->vb);

            while(vorbis_bitrate_flushpacket(&s->vd, &op)) 
                ogg_stream_packetin(&s->os, &op);
        }

        /* FIXME: Make this threshold configurable.
         * We don't want to buffer too many samples in one page when doing
         * live encoding - that's fine for non-live encoding, but breaks
         * badly when doing things live. 
         * So, we flush the stream if we have too many samples buffered
         */
        if(s->samples_in_current_page > s->samplerate * 2)
        {
            /*LOG_DEBUG1("Forcing flush: Too many samples in current page (%d)",
                    s->samples_in_current_page); */
            result = ogg_stream_flush(&s->os, og);
        }
        else
            result = ogg_stream_pageout(&s->os, og);

        if(result==0)
            return 0;
        else /* Page found! */
        {
            s->samples_in_current_page -= ogg_page_granulepos(og) - 
                    s->prevgranulepos;
            s->prevgranulepos = ogg_page_granulepos(og);
            return 1;
        }
    }
}

void encode_finish(encoder_state *s)
{
    ogg_packet op;
    vorbis_analysis_wrote(&s->vd, 0);

    while(vorbis_analysis_blockout(&s->vd, &s->vb)==1)
    {
        vorbis_analysis(&s->vb, NULL);
        vorbis_bitrate_addblock(&s->vb);
        while(vorbis_bitrate_flushpacket(&s->vd, &op))
            ogg_stream_packetin(&s->os, &op);
    }

}

int encode_flush(encoder_state *s, ogg_page *og)
{
    int result = ogg_stream_pageout(&s->os, og);

    if(result<=0)
        return 0;
    else
        return 1;
}
