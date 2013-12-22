/** @file avcodec_compat.h
 *  Compatibility header for libavcodec backwards compatibility
 *
 *  Copyright (C) 2011-2014 XMMS2 Team
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

#undef ABS
#ifdef HAVE_LIBAVCODEC_AVCODEC_H
# include "libavcodec/avcodec.h"
#else
# include "avcodec.h"
#endif

/* Map avcodec_free_frame to av_freep if the former doesn't exist.
 * (This is in versions earlier than 54.28.0 (libav) or 54.59.100 (ffmpeg)) */
#if ! HAVE_AVCODEC_FREE_FRAME
# define avcodec_free_frame av_freep
#endif

/* Map av_frame_alloc, av_frame_unref, av_frame_free into their
 * deprecated versions in versions earlier than 55.28.1 */
#if LIBAVCODEC_VERSION_INT < 0x371c01
# define av_frame_alloc avcodec_alloc_frame
# define av_frame_unref avcodec_get_frame_defaults
# define av_frame_free avcodec_free_frame
#endif
