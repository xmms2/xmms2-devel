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

/* Map avcodec_decode_audio2 into the deprecated version
 * avcodec_decode_audio in versions earlier than 51.28 */
#if LIBAVCODEC_VERSION_INT < 0x331c00
# define avcodec_decode_audio2 avcodec_decode_audio
#endif

/* Handle API change that happened in libavcodec 52.00 */
#if LIBAVCODEC_VERSION_INT < 0x340000
# define CONTEXT_BPS(codecctx) (codecctx)->bits_per_sample
#else
# define CONTEXT_BPS(codecctx) (codecctx)->bits_per_coded_sample
#endif

/* Before 52.23 AVPacket was defined in avformat.h which we
 * do not want to depend on, so we define part of it manually
 * on versions smaller than 52.23 (this makes me cry) */
#if LIBAVCODEC_VERSION_INT < 0x341700
typedef struct AVPacket {
        uint8_t *data;
        int size;
} AVPacket;
#endif

/* Same thing as above for av_init_packet and version 52.25 */
#if LIBAVCODEC_VERSION_INT < 0x341900
# define av_init_packet(pkt) do { \
    (pkt)->data = NULL; \
    (pkt)->size = 0; \
  } while(0)
#endif

/* Map avcodec_decode_audio3 into the deprecated version
 * avcodec_decode_audio2 in versions earlier than 52.26 */
#if LIBAVCODEC_VERSION_INT < 0x341a00
# define avcodec_decode_audio3(avctx, samples, frame_size_ptr, avpkt) \
    avcodec_decode_audio2(avctx, samples, frame_size_ptr, \
                          (avpkt)->data, (avpkt)->size)
#endif

/* Handle API change that happened in libavcodec 52.64 */
#if LIBAVCODEC_VERSION_INT < 0x344000
# define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#endif

/* Calling avcodec_init is not necessary after 53.04 (ffmpeg 0.9) */
#if LIBAVCODEC_VERSION_INT >= 0x350400
# define avcodec_init()
#endif

/* Map avcodec_alloc_context3 into the deprecated version
 * avcodec_alloc_context in versions earlier than 53.04 (ffmpeg 0.9) */
#if LIBAVCODEC_VERSION_INT < 0x350400
# define avcodec_alloc_context3(codec) \
    avcodec_alloc_context()
#endif

/* Map avcodec_open2 into the deprecated version
 * avcodec_open in versions earlier than 53.04 (ffmpeg 0.9) */
#if LIBAVCODEC_VERSION_INT < 0x350400
# define avcodec_open2(avctx, codec, options) \
    avcodec_open(avctx, codec)
#endif
