/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include "xmms/visualisation.h"
#include "xmms/object.h"
#include "xmms/core.h"

static GMutex *visuserslock;
static guint32 visusers = 0;

struct xmms_visualisation_St {
	xmms_object_t object;
	guint32 pos;
	guint samplerate;
	gint fft_data;
	gchar fft_buf[FFT_LEN*4];
	gfloat spec[FFT_LEN/2+1];
};

static void fft(gint16 *samples, gfloat *spec);


xmms_visualisation_t *
xmms_visualisation_init()
{
	xmms_visualisation_t *res;

	if (!visuserslock)
		visuserslock = g_mutex_new ();

	res = g_new0 (xmms_visualisation_t, 1);

	if (res) {
		xmms_object_init (XMMS_OBJECT (res));
	}

	return res;
}

void
xmms_visualisation_users_inc ()
{
	g_mutex_lock (visuserslock);
	visusers++;
	g_mutex_unlock (visuserslock);
}

void
xmms_visualisation_users_dec ()
{
	g_mutex_lock (visuserslock);
	visusers--;
	g_mutex_unlock (visuserslock);
}

static gboolean
xmms_visualisation_has_users ()
{
	gboolean res;

	g_mutex_lock (visuserslock);
	res = !!visusers;
	g_mutex_unlock (visuserslock);

	return res;
}

void
xmms_visualisation_destroy (xmms_visualisation_t *vis)
{
	xmms_object_cleanup (XMMS_OBJECT (vis));
	g_free(vis);
}


static void output_spectrum (xmms_visualisation_t *vis) {

	g_return_if_fail (vis->samplerate);

	vis->spec[0] = 1000.0f * vis->pos * FFT_LEN / vis->samplerate;
	
	xmms_core_vis_spectrum (vis->spec);

	vis->pos++;
}

void
xmms_visualisation_calc (xmms_visualisation_t *vis, gchar *buf, int len)
{
	gint t;

	g_return_if_fail (vis);

	if (!xmms_visualisation_has_users ()) {
		return;
	}

	if (vis->fft_data) {
		t = MIN (len, (FFT_LEN*4)-vis->fft_data);
		memcpy (vis->fft_buf + vis->fft_data, buf, t);
		vis->fft_data += t;
		len -= t;
		buf += t;
		if (vis->fft_data == FFT_LEN*4) {
			fft ((gint16 *)vis->fft_buf, vis->spec+1);
			output_spectrum (vis);
			vis->fft_data = 0;
		}
	}

	while (len > FFT_LEN*4) {
		fft ((gint16 *)buf, vis->spec+1);
		output_spectrum (vis);

		buf += FFT_LEN*4;
		len -= FFT_LEN*4;
	}

	if (len) {
		g_return_if_fail (!vis->fft_data);
		memcpy (vis->fft_buf, buf, len);
		vis->fft_data = len;
	}
}

void
xmms_visualisation_samplerate_set (xmms_visualisation_t *vis, guint rate)
{
	vis->samplerate = rate;
}

static void fft(gint16 *samples, gfloat *spec){
	gfloat  u_r,u_i, w_r,w_i, t_r,t_i;
	gint    nv2, k, l, le, le1, j, ip, i;
	gfloat buf[FFT_LEN][2];

	j = 0;
	for(i=0;i<FFT_LEN;i++){
		buf[i][0]  = (float)samples[j++];
		buf[i][0] += (float)samples[j++];
		buf[i][0] /= (float)(1<<15);
		buf[i][1]=0.0f;
	}

	/* reorder... */  /* this is crappy! Go rewrite it using real bitreversing */
    	nv2 = FFT_LEN / 2;
    	j = 1;
 	for (i = 1; i < FFT_LEN; i++ )
    	{
		if (i < j)
		{
	    		t_r = buf[i - 1][0];
	    		t_i = buf[i - 1][1];
	    		buf[i - 1][0] = buf[j - 1][0];
	    		buf[i - 1][1] = buf[j - 1][1];
	    		buf[j - 1][0] = t_r;
	    		buf[j - 1][1] = t_i;
		}
		k = nv2;
		while (k < j)
		{
	    		j = j - k;
	    		k = k / 2;
		}
		j = j + k;
    	}

	/* do fft */
 	for (l = 1; l <= FFT_BITS; l++) {
     	 	le = 1<<l;
	  	le1 = le / 2;
		u_r = 1.0;
		u_i = 0.0;
		w_r =  cos( M_PI / (float)le1 );
		w_i = -sin( M_PI / (float)le1 );
		for (j = 1; j <= le1; j++) {
	    		for (i = j; i <= FFT_LEN; i += le) {
				ip = i + le1;
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
	for (i=0; i<FFT_LEN/2; i++) {
		spec[i] = hypot(buf[i][0],buf[i][1]);
	}
	/* correct the scale */
	spec[0] /= 2;
	spec[FFT_LEN/2-1] /= 2;

}
