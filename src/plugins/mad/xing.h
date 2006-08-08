/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 */




#ifndef __XING_H_
#define __XING_H_

#include <mad.h>

#define XMMS_XING_TOC_SIZE 100

typedef enum {
	XMMS_XING_FRAMES = 1UL << 0,
	XMMS_XING_BYTES  = 1UL << 1,
	XMMS_XING_TOC    = 1UL << 2,
	XMMS_XING_SCALE  = 1UL << 3,
} xmms_xing_flags_t;

/*
struct xmms_xing_lame_St {
	guint8 tagrev;
	guint8 lowpass;
	guint32 peak_amplitude;
	guint16 radio_gain;
	guint16 audiophile_gain;
	guint8 encoder_flags;
	guint8 spec_bitrate;
	union {
		guint8 b1;
		guint8 b2;
		guint8 b3;
	} encoder_delay;
	guint8 misc;
	guint8 mp3_gain;
	guint16 preset;
	guint32 musiclenght;
	guint16 musiccrc;
	guint16 crcofinfo;
};
*/
struct xmms_xing_lame_St {
	guint32 peak_amplitude;
	guint16 radio_gain;
	guint16 audiophile_gain;
	guint16 encoder_delay_start;
	guint16 encoder_delay_stop;
	guint8 mp3_gain;
};

struct xmms_xing_St;
typedef struct xmms_xing_St xmms_xing_t;
typedef struct xmms_xing_lame_St xmms_xing_lame_t;

gboolean xmms_xing_has_flag (xmms_xing_t *xing, xmms_xing_flags_t flag);
guint xmms_xing_get_frames (xmms_xing_t *xing);
guint xmms_xing_get_bytes (xmms_xing_t *xing);
guint xmms_xing_get_toc (xmms_xing_t *xing, gint index);
xmms_xing_t *xmms_xing_parse (struct mad_bitptr ptr);
void xmms_xing_free (xmms_xing_t *xing);
xmms_xing_lame_t *xmms_xing_get_lame (xmms_xing_t *xing);

#endif
