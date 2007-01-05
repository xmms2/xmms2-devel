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




/** @file
 *
 */

#include <math.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>

#include "xmmspriv/xmms_visualisation.h"
#include "xmmspriv/xmms_ipc.h"
#include "xmmspriv/xmms_sample.h"
#include "xmms/xmms_object.h"


/** @defgroup Visualisation Visualisation
  * @ingroup XMMSServer
  * @brief Visulation draws a FFT and feeds it to the client.
  * @{
  */

/**
 * The structure for the vis module
 */
struct xmms_visualisation_St {
	xmms_object_t object;
	xmms_audio_format_t *format;
	gint fft_data;
	gchar fft_buf[FFT_LEN*4];
	gfloat spec[FFT_LEN/2];
	GList *list;
	gint needed;
};

gfloat window[FFT_LEN];

static xmms_visualisation_t *vis;
static void fft (gint16 *samples, gfloat *spec);
static void xmms_visualisation_destroy (xmms_object_t *object);

/**
 * Initialize the Vis module.
 */
void
xmms_visualisation_init ()
{
	int i;
	vis = xmms_object_new (xmms_visualisation_t, xmms_visualisation_destroy);
	xmms_ipc_object_register (XMMS_IPC_OBJECT_VISUALISATION, XMMS_OBJECT (vis));
	xmms_ipc_signal_register (XMMS_OBJECT (vis),
	                          XMMS_IPC_SIGNAL_VISUALISATION_DATA);

	/* prealloc list */
	for (i = 0; i < FFT_LEN/2 + 1; i++) {
		xmms_object_cmd_value_t *data;
		data = xmms_object_cmd_value_uint_new (INT_MAX);
		vis->list = g_list_prepend (vis->list, data);
	}

	/* calculate Hann window used to reduce spectral leakage */
	for (i = 0; i < FFT_LEN; i++) {
		window[i] = 0.5 - 0.5 * cos (2.0*M_PI*i/FFT_LEN);
	}

}

/**
 * Free all resoures used by visualisation module.
 */
void xmms_visualisation_shutdown ()
{
}

static void
xmms_visualisation_destroy (xmms_object_t *object)
{
	xmms_ipc_signal_unregister (XMMS_IPC_SIGNAL_VISUALISATION_DATA);
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_VISUALISATION);
}

/**
 * Allocate the visualisation.
 */
xmms_visualisation_t *
xmms_visualisation_new ()
{
	xmms_object_ref (vis);
	return vis;
}

static void output_spectrum (xmms_visualisation_t *vis, guint32 pos)
{
	xmms_object_cmd_value_t *data;
	GList *node = vis->list;
	int i;

	data = (xmms_object_cmd_value_t *)node->data;
	data->value.uint32 = xmms_sample_samples_to_ms (vis->format, pos);

	node = g_list_next (node);
	for (i = 0; i < FFT_LEN / 2; i++) {
		gfloat tmp = vis->spec[i];
		data = (xmms_object_cmd_value_t *)node->data;
		
		if (tmp >= 1.0)
			data->value.uint32 = INT_MAX;
		else if (tmp < 0.0)
			data->value.uint32 = 0;
		else
			data->value.uint32 = (guint)(tmp * INT_MAX);

		node = g_list_next (node);
	}

	xmms_object_emit_f (XMMS_OBJECT (vis),
	                    XMMS_IPC_SIGNAL_VISUALISATION_DATA,
	                    XMMS_OBJECT_CMD_ARG_LIST,
	                    vis->list);

}

/**
 * Calcualte the FFT on the decoded data buffer.
 */
void
xmms_visualisation_calc (xmms_visualisation_t *vis, xmms_sample_t *buf, int len, guint32 pos)
{
	gint t;

	g_return_if_fail (vis);

	if (vis->format->format != XMMS_SAMPLE_FORMAT_S16)
		return;

	if (xmms_ipc_has_pending (XMMS_IPC_SIGNAL_VISUALISATION_DATA)) {
		vis->needed = 20;
	} else if (vis->needed) {
		vis->needed--;
	}

	if (!vis->needed)
		return;

	if (vis->fft_data) {
		pos -= vis->fft_data / 4;

		t = MIN (len, (FFT_LEN*4)-vis->fft_data);
		memcpy (vis->fft_buf + vis->fft_data, buf, t);
		vis->fft_data += t;
		len -= t;
		buf += t;
		if (vis->fft_data == FFT_LEN*4) {
			fft ((gint16 *)vis->fft_buf, vis->spec);
			output_spectrum (vis, pos);
			vis->fft_data = 0;
		}
	}

	while (len > FFT_LEN*4) {
		fft ((gint16 *)buf, vis->spec);
		output_spectrum (vis, pos);

		buf += FFT_LEN*4;
		len -= FFT_LEN*4;
		pos += FFT_LEN;
	}

	if (len) {
		g_return_if_fail (!vis->fft_data);
		memcpy (vis->fft_buf, buf, len);
		vis->fft_data = len;
	}
}

/**
 * Tell the visualisation what audio format we use
 */
void
xmms_visualisation_format_set (xmms_visualisation_t *vis, xmms_audio_format_t *fmt)
{
	vis->format = fmt;
}

static void
fft (gint16 *samples, gfloat *spec)
{
	gint nv2, k, l, j = 0, i;
	gfloat t_r, t_i;
	gfloat buf[FFT_LEN][2];

	for (i = 0; i < FFT_LEN; i++){
		buf[i][0]  = (float) samples[j++];
		buf[i][0] += (float) samples[j++];
		buf[i][0] /= (float) (1 << 17);
		buf[i][0] *= window[i];
		buf[i][1] = 0.0f;
	}

	/* reorder... */  /* this is crappy! Go rewrite it using real bitreversing */
	nv2 = FFT_LEN / 2;
	j = 1;

	for (i = 1; i < FFT_LEN; i++) {
		if (i < j) {
			t_r = buf[i - 1][0];
			t_i = buf[i - 1][1];
			buf[i - 1][0] = buf[j - 1][0];
			buf[i - 1][1] = buf[j - 1][1];
			buf[j - 1][0] = t_r;
			buf[j - 1][1] = t_i;
		}

		k = nv2;

		while (k < j) {
			j -= k;
			k /= 2;
		}

		j += k;
	}

	/* do fft */
	for (l = 1; l <= FFT_BITS; l++) {
		gint le = 1 << l;
		gint le1 = le / 2;
		gfloat u_r = 1.0;
		gfloat u_i = 0.0;
		gfloat w_r =  cos (M_PI / (float) le1);
		gfloat w_i = -sin (M_PI / (float) le1);

		for (j = 1; j <= le1; j++) {
			for (i = j; i <= FFT_LEN; i += le) {
				gint ip = i + le1;

				t_r = buf[ip - 1][0] * u_r - u_i * buf[ip - 1][1];
				t_i = buf[ip - 1][1] * u_r + u_i * buf[ip - 1][0];

				buf[ip - 1][0] = buf[i - 1][0] - t_r;
				buf[ip - 1][1] = buf[i - 1][1] - t_i;

				buf[i - 1][0] =  buf[i - 1][0] + t_r;
				buf[i - 1][1] =  buf[i - 1][1] + t_i;
			}

			t_r = u_r * w_r - w_i * u_i;
			u_i = w_r * u_i + w_i * u_r;
			u_r = t_r;
		}
	}

	/* output abs-value instead */
	for (i = 0; i < nv2; i++) {
		spec[i] = hypot (buf[i][0], buf[i][1]);
	}

	/* correct the scale */
	spec[0] /= 2;
	spec[nv2 - 1] /= 2;

}

/** @} */
