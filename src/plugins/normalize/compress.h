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

/* compress.h
** interface to audio compression
*/

#ifndef COMPRESS_H
#define COMPRESS_H

typedef struct compress_St {
	int *peaks;
	int gain_current, gain_target;
	int lastsize;
	int pn, clip, clipped;
	struct compress_prefs_St {
		int anticlip;
		int target;
		int gainmax;
		int gainsmooth;
		int buckets;
	} prefs;
} compress_t;

compress_t *compress_new (int anticlip, int target, int maxgain,
                          int smooth, int buckets);

void compress_reconfigure (compress_t *compress,
                           int anticlip,
                           int target,
                           int maxgain,
                           int smooth,
                           int buckets);

void compress_do (compress_t *compress, void *data,
                  unsigned num_samples);

void compress_free (compress_t *compress);

#endif
