/*  AudioCompress
 *  Copyright (C) 2002-2003 trikuare studios (http://trikuare.cx)
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

/* compress.c
** Compressor logic
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <glib.h>

#include "compress_config.h"
#include "compress.h"

compress_t *
compress_new (gint anticlip, gint target, gint maxgain, gint smooth,
              gint buckets)
{
	compress_t *compress = g_new0 (compress_t, 1);
	/* start with no gain; this removes the not-so-wonderful volume fade-in
	 * at the beginning of a new song. */
	compress->gain_current = compress->gain_target = 1 << GAINSHIFT;
	compress->pn = -1;

	compress_reconfigure (compress, anticlip, target, maxgain, smooth,
	                      buckets);

	return compress;
}

void
compress_reconfigure (compress_t *compress, gint anticlip, gint target,
                      gint gainmax, gint gainsmooth, gint buckets)
{
	compress->prefs.anticlip = anticlip;
	compress->prefs.target = target;
	compress->prefs.gainmax = gainmax;
	compress->prefs.gainsmooth = gainsmooth;
	compress->prefs.buckets = buckets;

	/* Allocate the peak structure */
	compress->peaks = g_realloc (compress->peaks,
	                             sizeof (gint)*compress->prefs.buckets);

	if (compress->prefs.buckets > compress->lastsize) {
		/* i could have sworn there was a glib function for this... */
		memset (compress->peaks + compress->lastsize, 0,
		        sizeof (gint)*(compress->prefs.buckets - compress->lastsize));
	}
	compress->lastsize = compress->prefs.buckets;
}

void
compress_free (compress_t *compress)
{
	if (compress->peaks) {
		g_free (compress->peaks);
	}

	g_free (compress);
}

void
compress_do (compress_t *compress, void *data, guint length)
{
	gint16 *audio = (gint16 *)data, *ap;
	gint peak, pos;
	gint i;
	gint gr, gf, gn;

	if (!compress->peaks) {
		return;
	}

	if (compress->pn == -1) {
		for (i = 0; i < compress->prefs.buckets; i++) {
			compress->peaks[i] = 0;
		}
	}
	compress->pn = (compress->pn + 1)%compress->prefs.buckets;

#ifdef DEBUG
	fprintf (stderr, "modifyNative16(0x%08x, %d)\n", (unsigned)data,
	         length);
#endif

	/* Determine peak's value and position */
	peak = 1;
	pos = 0;

#ifdef DEBUG
	fprintf (stderr, "finding peak(b=%d)\n", compress->pn);
#endif

	ap = audio;
	for (i = 0; i < length/2; i++) {
		gint val = *ap;
		if (val > peak) {
			peak = val;
			pos = i;
		} else if (-val > peak) {
			peak = -val;
			pos = i;
		}
		ap++;
	}

	compress->peaks[compress->pn] = peak;

	for (i = 0; i < compress->prefs.buckets; i++) {
		if (compress->peaks[i] > peak) {
			peak = compress->peaks[i];
			pos = 0;
		}
	}

	/* Determine target gain */
	gn = (1 << GAINSHIFT)*compress->prefs.target/peak;

	if (gn <(1 << GAINSHIFT)) {
		gn = 1 << GAINSHIFT;
	}

	compress->gain_target = (compress->gain_target
	                       *((1 << compress->prefs.gainsmooth) - 1) + gn)
	                       >> compress->prefs.gainsmooth;

	/* Give it an extra insignifigant nudge to counteract possible
	** rounding error
	*/

	if (gn < compress->gain_target) {
		compress->gain_target--;
	} else if (gn > compress->gain_target) {
		compress->gain_target++;
	}

	if (compress->gain_target > compress->prefs.gainmax << GAINSHIFT) {
		compress->gain_target = compress->prefs.gainmax << GAINSHIFT;
	}

	/* See if a peak is going to clip */
	gn = (1 << GAINSHIFT)*32768/peak;

	if (gn < compress->gain_target) {
		compress->gain_target = gn;

		if (compress->prefs.anticlip) {
			pos = 0;
		}

	} else {
		/* We're ramping up, so draw it out over the whole frame */
		pos = length;
	}

	/* Determine gain rate necessary to make target */
	if (!pos) {
		pos = 1;
	}

	gr = ((compress->gain_target - compress->gain_current) << 16)/pos;

	/* Do the shiznit */
	gf = compress->gain_current << 16;

#ifdef STATS
	fprintf (stderr, "\r%d gain = %2.2f%+.2e ", compress->gain_current,
	         compress->gain_current*1.0/(1 << GAINSHIFT),
	         (compress->gain_target - compress->gain_current)*1.0
	         /(1 << GAINSHIFT));
#endif

	ap = audio;
	for (i = 0; i < length/2; i++) {
		gint sample;

		/* Interpolate the gain */
		compress->gain_current = gf >> 16;
		if (i < pos) {
			gf += gr;
		} else if (i == pos) {
			gf = compress->gain_target << 16;
		}

		/* Amplify */
		sample = (*ap)*compress->gain_current >> GAINSHIFT;
		if (sample < -32768) {
#ifdef STATS
			compress->clip++;
#endif
			compress->clipped += -32768 - sample;
			sample = -32768;
		} else if (sample > 32767) {
#ifdef STATS
			compress->clip++;
#endif
			compress->clipped += sample - 32767;
			sample = 32767;
		}
		*ap++ = sample;
	}
#ifdef STATS
	fprintf (stderr, "clip %d b%-3d ", compress->clip, compress->pn);
#endif

#ifdef DEBUG
	fprintf (stderr, "\ndone\n");
#endif
}
