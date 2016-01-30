/*  XMMS2 - X Music Multiplexer System
 *
 *  Copyright (C) 2003-2016 XMMS2 Team
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
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

enum {
	XMMS_XING_LAME_NSPSYTUNE    = 0x01,
	XMMS_XING_LAME_NSSAFEJOINT  = 0x02,
	XMMS_XING_LAME_NOGAP_NEXT   = 0x04,
	XMMS_XING_LAME_NOGAP_PREV   = 0x08,

	XMMS_XING_LAME_UNWISE       = 0x10
};

enum xmms_xing_lame_vbr {
	XMMS_XING_LAME_VBR_CONSTANT      = 1,
	XMMS_XING_LAME_VBR_ABR           = 2,
	XMMS_XING_LAME_VBR_METHOD1       = 3,
	XMMS_XING_LAME_VBR_METHOD2       = 4,
	XMMS_XING_LAME_VBR_METHOD3       = 5,
	XMMS_XING_LAME_VBR_METHOD4       = 6,
	XMMS_XING_LAME_VBR_CONSTANT2PASS = 8,
	XMMS_XING_LAME_VBR_ABR2PASS      = 9
};

enum xmms_xing_lame_source {
	XMMS_XING_LAME_SOURCE_32LOWER  = 0x00,
	XMMS_XING_LAME_SOURCE_44_1     = 0x01,
	XMMS_XING_LAME_SOURCE_48       = 0x02,
	XMMS_XING_LAME_SOURCE_HIGHER48 = 0x03
};

enum xmms_xing_lame_mode {
	XMMS_XING_LAME_MODE_MONO      = 0x00,
	XMMS_XING_LAME_MODE_STEREO    = 0x01,
	XMMS_XING_LAME_MODE_DUAL      = 0x02,
	XMMS_XING_LAME_MODE_JOINT     = 0x03,
	XMMS_XING_LAME_MODE_FORCE     = 0x04,
	XMMS_XING_LAME_MODE_AUTO      = 0x05,
	XMMS_XING_LAME_MODE_INTENSITY = 0x06,
	XMMS_XING_LAME_MODE_UNDEFINED = 0x07
};

enum xmms_xing_lame_surround {
	XMMS_XING_LAME_SURROUND_NONE      = 0,
	XMMS_XING_LAME_SURROUND_DPL       = 1,
	XMMS_XING_LAME_SURROUND_DPL2      = 2,
	XMMS_XING_LAME_SURROUND_AMBISONIC = 3
};

enum xmms_xing_lame_preset {
	XMMS_XING_LAME_PRESET_NONE          =    0,
	XMMS_XING_LAME_PRESET_V9            =  410,
	XMMS_XING_LAME_PRESET_V8            =  420,
	XMMS_XING_LAME_PRESET_V7            =  430,
	XMMS_XING_LAME_PRESET_V6            =  440,
	XMMS_XING_LAME_PRESET_V5            =  450,
	XMMS_XING_LAME_PRESET_V4            =  460,
	XMMS_XING_LAME_PRESET_V3            =  470,
	XMMS_XING_LAME_PRESET_V2            =  480,
	XMMS_XING_LAME_PRESET_V1            =  490,
	XMMS_XING_LAME_PRESET_V0            =  500,
	XMMS_XING_LAME_PRESET_R3MIX         = 1000,
	XMMS_XING_LAME_PRESET_STANDARD      = 1001,
	XMMS_XING_LAME_PRESET_EXTREME       = 1002,
	XMMS_XING_LAME_PRESET_INSANE        = 1003,
	XMMS_XING_LAME_PRESET_STANDARD_FAST = 1004,
	XMMS_XING_LAME_PRESET_EXTREME_FAST  = 1005,
	XMMS_XING_LAME_PRESET_MEDIUM        = 1006,
	XMMS_XING_LAME_PRESET_MEDIUM_FAST   = 1007
};

struct xmms_xing_lame_St {
	unsigned char revision;
	unsigned char flags;

	enum xmms_xing_lame_vbr vbr_method;
	unsigned short lowpass_filter;

	mad_fixed_t peak;
	/*
	struct rgain replay_gain[2];
	*/

	unsigned char ath_type;
	unsigned char bitrate;

	unsigned short start_delay;
	unsigned short end_padding;

	enum xmms_xing_lame_source source_samplerate;
	enum xmms_xing_lame_mode stereo_mode;
	unsigned char noise_shaping;

	signed char gain;
	enum xmms_xing_lame_surround surround;
	enum xmms_xing_lame_preset preset;

	unsigned long music_length;
	unsigned short music_crc;
};

/*
struct xmms_xing_lame_St {
	guint32 peak_amplitude;
	guint16 radio_gain;
	guint16 audiophile_gain;
	guint16 encoder_delay_start;
	guint16 encoder_delay_stop;
	guint8 mp3_gain;
};
*/

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
