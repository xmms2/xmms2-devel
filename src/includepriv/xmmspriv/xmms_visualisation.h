/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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




#ifndef __XMMS_VISUALISATION_H__
#define __XMMS_VISUALISATION_H__

#include "xmms/xmms_sample.h"

/* this is in number of samples! */
#define FFT_BITS 10
#define FFT_LEN (1<<FFT_BITS)


typedef struct xmms_visualisation_St xmms_visualisation_t;

void xmms_visualisation_init ();
void xmms_visualisation_shutdown ();
xmms_visualisation_t *xmms_visualisation_new ();
void xmms_visualisation_calc (xmms_visualisation_t *vis, xmms_sample_t *buf, int len, guint32 pos);
void xmms_visualisation_format_set (xmms_visualisation_t *vis, xmms_audio_format_t *fmt);



#endif
