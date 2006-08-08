/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
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

xmms_xing_t *
xmms_xing_parse (struct mad_bitptr ptr)
{
	xmms_xing_t *xing;
	guint32 xing_magic;
	xmms_xing_lame_t *lame;

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

	if (strncmp ((gchar *)ptr.byte, "LAME", 4) == 0) {
		XMMS_DBG ("Parsing LAME tag");
		mad_bit_skip (&ptr, 8 * 20);
		lame = g_new0 (xmms_xing_lame_t, 1);
		mad_bit_skip (&ptr, 16);
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

	return xing;
}

