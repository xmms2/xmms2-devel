/*  XMMS2 - X Music Multiplexer System
 *
 *  Copyright (C) 2003-2008 XMMS2 Team
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




/** @file
 * Xing-header parser.
 */

#include <glib.h>
#include "xing.h"

#include <xmms/xmms_log.h>

#include <string.h>

struct xmms_xing_St {
	gint flags;
	guint frames;
	guint bytes;
	guint toc[XMMS_XING_TOC_SIZE];
	xmms_xing_lame_t *lame;
};

xmms_xing_lame_t *
xmms_xing_get_lame (xmms_xing_t *xing)
{
	return xing->lame;
}

gboolean
xmms_xing_has_flag (xmms_xing_t *xing, xmms_xing_flags_t flag)
{
	return xing->flags & flag;
}

guint
xmms_xing_get_frames (xmms_xing_t *xing)
{
	return xing->frames;
}

guint
xmms_xing_get_bytes (xmms_xing_t *xing)
{
	return xing->bytes;
}

guint
xmms_xing_get_toc (xmms_xing_t *xing, gint index)
{
	g_return_val_if_fail (0 <= index && index < 100, -1);

	return xing->toc[index];
}

void
xmms_xing_free (xmms_xing_t *xing)
{
	if (xing->lame) {
		g_free (xing->lame);
	}
	g_free (xing);
}

/*
 * madplay - MPEG audio decoder and player
 * Copyright (C) 2000-2004 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define LAME_MAGIC (('L' << 24) | ('A' << 16) | ('M' << 8) | 'E')

static xmms_xing_lame_t *
parse_lame (struct mad_bitptr *ptr)
{
	struct mad_bitptr save = *ptr;
	unsigned long magic;
	unsigned char const *version;
	xmms_xing_lame_t *lame;

	lame = g_new0 (xmms_xing_lame_t, 1);

	/*
	if (*bitlen < 36 * 8)
		goto fail;
	*/

	/* bytes $9A-$A4: Encoder short VersionString */
	magic   = mad_bit_read (ptr, 4 * 8);

	if (magic != LAME_MAGIC)
		goto fail;

	XMMS_DBG ("LAME tag found!");

	version = mad_bit_nextbyte (ptr);

	mad_bit_skip (ptr, 5 * 8);

	/* byte $A5: Info Tag revision + VBR method */

	lame->revision = mad_bit_read (ptr, 4);
	if (lame->revision == 15)
		goto fail;

	lame->vbr_method = mad_bit_read (ptr, 4);

	/* byte $A6: Lowpass filter value (Hz) */

	lame->lowpass_filter = mad_bit_read (ptr, 8) * 100;

	/* bytes $A7-$AA: 32 bit "Peak signal amplitude" */

	lame->peak = mad_bit_read (ptr, 32) << 5;

	/* bytes $AB-$AC: 16 bit "Radio Replay Gain" */

	/*
	rgain_parse(&lame->replay_gain[0], ptr);
	*/

	/* bytes $AD-$AE: 16 bit "Audiophile Replay Gain" */

	/*
	rgain_parse(&lame->replay_gain[1], ptr);
	*/

	mad_bit_skip (ptr, 32);

	/*
	 * As of version 3.95.1, LAME writes Replay Gain values with a reference of
	 * 89 dB SPL instead of the 83 dB specified in the Replay Gain proposed
	 * standard. Here we compensate for the heresy.
	 */
	if (magic == LAME_MAGIC) {
		char str[6];
		/*
		unsigned major = 0, minor = 0, patch = 0;
		int i;
		*/

		memcpy (str, version, 5);
		str[5] = 0;

		/*
		sscanf(str, "%u.%u.%u", &major, &minor, &patch);

		if (major > 3 ||
		    (major == 3 && (minor > 95 ||
		    (minor == 95 && str[4] == '.')))) {
			for (i = 0; i < 2; ++i) {
				if (RGAIN_SET(&lame->replay_gain[i]))
					lame->replay_gain[i].adjustment -= 60;
			}
		}
		*/
	}

	/* byte $AF: Encoding flags + ATH Type */

	lame->flags    = mad_bit_read (ptr, 4);
	lame->ath_type = mad_bit_read (ptr, 4);

	/* byte $B0: if ABR {specified bitrate} else {minimal bitrate} */

	lame->bitrate = mad_bit_read (ptr, 8);

	/* bytes $B1-$B3: Encoder delays */

	lame->start_delay = mad_bit_read (ptr, 12);
	lame->end_padding = mad_bit_read (ptr, 12);

	/* byte $B4: Misc */

	lame->source_samplerate = mad_bit_read (ptr, 2);

	if (mad_bit_read (ptr, 1))
		lame->flags |= XMMS_XING_LAME_UNWISE;

	lame->stereo_mode   = mad_bit_read (ptr, 3);
	lame->noise_shaping = mad_bit_read (ptr, 2);

	/* byte $B5: MP3 Gain */

	lame->gain = mad_bit_read (ptr, 8);

	/* bytes $B6-B7: Preset and surround info */

	mad_bit_skip (ptr, 2);

	lame->surround = mad_bit_read (ptr,  3);
	lame->preset   = mad_bit_read (ptr, 11);

	/* bytes $B8-$BB: MusicLength */

	lame->music_length = mad_bit_read (ptr, 32);

	/* bytes $BC-$BD: MusicCRC */

	lame->music_crc = mad_bit_read (ptr, 16);

	/* bytes $BE-$BF: CRC-16 of Info Tag */

	/*
	if (mad_bit_read(ptr, 16) != crc)
		goto fail;
		*/

	return lame;

fail:
	g_free (lame);
	*ptr = save;
	return NULL;
}

xmms_xing_t *
xmms_xing_parse (struct mad_bitptr ptr)
{
	xmms_xing_t *xing;
	guint32 xing_magic;

	xing_magic = mad_bit_read (&ptr, 4*8);

	/* Xing or Info */
	if (xing_magic != 0x58696e67 && xing_magic != 0x496e666f) {
		return NULL;
	}

	xing = g_new0 (xmms_xing_t, 1);

	g_return_val_if_fail (xing, NULL);

	xing->flags = mad_bit_read (&ptr, 32);

	if (xmms_xing_has_flag (xing, XMMS_XING_FRAMES))
		xing->frames = mad_bit_read (&ptr, 32);

	if (xmms_xing_has_flag (xing, XMMS_XING_BYTES))
		xing->bytes = mad_bit_read (&ptr, 32);

	if (xmms_xing_has_flag (xing, XMMS_XING_TOC)) {
		gint i;
		for (i = 0; i < 100; i++)
			xing->toc[i] = mad_bit_read (&ptr, 8);
	}

	if (xmms_xing_has_flag (xing, XMMS_XING_SCALE)) {
		/* just move the pointer forward */
		mad_bit_read (&ptr, 32);
	}


	xing->lame = parse_lame (&ptr);

	/*
	if (strncmp ((gchar *)ptr.byte, "LAME", 4) == 0) {
		lame = g_new0 (xmms_xing_lame_t, 1);
		XMMS_DBG ("Parsing LAME tag");
		mad_bit_skip (&ptr, 4 * 8);
		mad_bit_nextbyte (&ptr);
		mad_bit_skip (&ptr, (8 * 5) + 12);
		lame->peak_amplitude = mad_bit_read (&ptr, 32);
		lame->radio_gain = mad_bit_read (&ptr, 16);
		lame->audiophile_gain = mad_bit_read (&ptr, 16);
		mad_bit_skip (&ptr, 16);
		lame->encoder_delay_start = mad_bit_read (&ptr, 12);
		lame->encoder_delay_stop = mad_bit_read (&ptr, 12);
		mad_bit_skip (&ptr, 8);
		lame->mp3_gain = mad_bit_read (&ptr, 8);
		xing->lame = lame;
	}
	*/

	if (xmms_xing_has_flag (xing, XMMS_XING_FRAMES) && xing->frames == 0) {
		xmms_log_info ("Corrupt xing header (frames == 0), ignoring");
		xmms_xing_free (xing);
		return NULL;
	}

	if (xmms_xing_has_flag (xing, XMMS_XING_BYTES) && xing->bytes == 0) {
		xmms_log_info ("Corrupt xing header (bytes == 0), ignoring");
		xmms_xing_free (xing);
		return NULL;
	}

	if (xmms_xing_has_flag (xing, XMMS_XING_TOC)) {
		gint i;
		for (i = 0; i < 99; i++) {
			if (xing->toc[i] > xing->toc[i + 1]) {
				xmms_log_info ("Corrupt xing header (toc not monotonic), ignoring");
				xmms_xing_free (xing);
				return NULL;
			}
		}
	}

	return xing;
}


