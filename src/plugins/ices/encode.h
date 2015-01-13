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

/*
 * Modifications for xmms2
 * Copyright (C) 2003-2015 XMMS2 Team
 */

#ifndef __ENCODE_H
#define __ENCODE_H

#include <glib.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include <xmms/xmms_sample.h>

typedef struct encoder_state encoder_state;

encoder_state *xmms_ices_encoder_init(int min_br, int nom_br, int max_br);
void xmms_ices_encoder_fini(encoder_state *s);
gboolean xmms_ices_encoder_stream_change(encoder_state *s, int rate,
                                         int channels, vorbis_comment *vc);
void xmms_ices_encoder_input(encoder_state *s, xmms_samplefloat_t *buf, int n_samples);
void xmms_ices_encoder_finish(encoder_state *s);
gboolean xmms_ices_encoder_output(encoder_state *s, ogg_page *og);

#endif

