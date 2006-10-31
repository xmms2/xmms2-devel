/*  Phase vocoder routine for time-stretching of audio signal
 *  Copyright (C) 2006  Juho Vähä-Herttua
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301, USA.
 */

#ifndef PVOCODER_H
#define PVOCODER_H

#include <fftw3.h>

typedef struct pvocoder_s pvocoder_t;
typedef float pvocoder_sample_t;

/* Define which precision API we want to use */
#define FFTWT(func) fftwf_ ## func

pvocoder_t *pvocoder_init(int chunksize, int channels);
void pvocoder_close(pvocoder_t *pvocoder);
void pvocoder_set_scale(pvocoder_t *pvoc, double scale);
void pvocoder_set_attack_detection(pvocoder_t *pvoc, int enabled);
void pvocoder_add_chunk(pvocoder_t *pvoc, pvocoder_sample_t *chunk);
int pvocoder_get_chunk(pvocoder_t *pvoc, pvocoder_sample_t *chunk);
void pvocoder_get_final(pvocoder_t *pvoc, pvocoder_sample_t *chunk);

#endif

