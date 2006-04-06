/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 Peter Alm, Tobias Rundström, Anders Gustafsson
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




/* encode.h
 * - encoding functions
 *
 * $Id: encode.h,v 1.4 2003/03/16 14:21:48 msmith Exp $
 *
 * Copyright (c) 2001 Michael Smith <msmith@labyrinth.net.au>
 *
 * This program is distributed under the terms of the GNU General
 * Public License, version 2. You may use, modify, and redistribute
 * it under the terms of this license. A copy should be included
 * with this source.
 */

#ifndef __ENCODE_H
#define __ENCODE_H

#include <ogg/ogg.h>
#include <vorbis/codec.h>

typedef struct {
    ogg_stream_state os;
    vorbis_block vb;
    vorbis_dsp_state vd;
    vorbis_info vi;

    int samples_in_current_page;
    int samplerate;
    ogg_int64_t prevgranulepos;
    int in_header;
} encoder_state;

encoder_state *encode_initialise(int channels, int rate, int managed,
    int min_br, int nom_br, int max_br, float quality,
    int serial, vorbis_comment *vc);
void encode_clear(encoder_state *s);
void encode_data_float(encoder_state *s, float **pcm, int samples);
void encode_data(encoder_state *s, signed char *buf, int bytes, int bigendian);
int encode_dataout(encoder_state *s, ogg_page *og);
void encode_finish(encoder_state *s);
int encode_flush(encoder_state *s, ogg_page *og);

#endif

