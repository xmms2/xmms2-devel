/*
 *   PCM time-domain equalizer
 *
 *   Copyright (C) 2002-2006  Felipe Rivera <liebremx at users sourceforge net>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   $Id: iir_sse.c,v 1.7 2006/01/15 00:26:32 liebremx Exp $
 */

#include <math.h>
#include "iir.h"
#include "iir_sse.h"

#ifdef BENCHMARK
#include <stdio.h>
#include "benchmark.h"
#endif

/* Volume gain
 * values should be between 0.0 and 1.0
 * Use the preamp from XMMS for now
 * */

/* Gain for each band
 * values should be between -0.2 and 1.0 */
static union f4vector gain[MAX_SSE_VECTORS][EQ_CHANNELS];
static sSupport support;
static sHistory history[EQ_CHANNELS];
static sPtrs ptrs;
static int sse_loop_num;
static void setup_gain(int count_start);
static void load_coeffs(int bands);


void set_gain(int index, int chn, float val)
{
  gain[index/4][chn].f[index%4] = val;
}

void clean_history(void)
{
  int i;

  sse_loop_num = ceil((float)band_count/4.);
  load_coeffs(band_count);
  setup_gain(band_count);

  /* Zero the history arrays */
  memset(history, 0, sizeof(history));
  /* Init pointers */
  for (i=0; i<EQ_CHANNELS; i++)
  {
    ptrs.p_sse_Y_n[i]  = history[i].sse_Y_n;
    ptrs.p_sse_Y_n1[i]  = history[i].sse_Y_n1;
    ptrs.p_sse_Y_n2[i]  = history[i].sse_Y_n2;
    ptrs.p_sse_X_n[i]  = &history[i].sse_X_n;
    ptrs.p_sse_X_n1[i] = &history[i].sse_X_n1;
    ptrs.p_sse_X_n2[i] = &history[i].sse_X_n2;
    ptrs.p_sse_Y2_n[i]  = history[i].sse_Y2_n;
    ptrs.p_sse_Y2_n1[i]  = history[i].sse_Y2_n1;
    ptrs.p_sse_Y2_n2[i]  = history[i].sse_Y2_n2;
    ptrs.p_sse_X2_n[i]  = &history[i].sse_X2_n;
    ptrs.p_sse_X2_n1[i] = &history[i].sse_X2_n1;
    ptrs.p_sse_X2_n2[i] = &history[i].sse_X2_n2;
  }
}

static void load_coeffs(int bands)
{
  int i;
  memset(support.sse_iir_cf_alpha, 0, sizeof(support.sse_iir_cf_alpha));
  memset(support.sse_iir_cf_beta, 0, sizeof(support.sse_iir_cf_beta));
  memset(support.sse_iir_cf_gamma, 0, sizeof(support.sse_iir_cf_gamma));
  for (i = 0; i < bands; i++)
  {
    support.sse_iir_cf_alpha[i/4].f[i%4] = iir_cf[i].alpha;
    support.sse_iir_cf_beta [i/4].f[i%4] = iir_cf[i].beta;
    support.sse_iir_cf_gamma[i/4].f[i%4] = iir_cf[i].gamma;
  }
}

static void setup_gain(int count_start)
{
  int j;
  for (; count_start < 32; count_start++)
    for (j = 0; j < EQ_CHANNELS; j++)
      gain[count_start/4][j].f[count_start%4] = 0.;
}

int iir(void *d, int length, int nch, int extra_filtering)
{
  /* Turn ON Flush-to-zero mode to avoid exceptions on underflow */
  FTZ_ON;
  short *data = d;
  /* Indexes for the history arrays
   * These have to be kept between calls to this function
   * hence they are static */
  int index, band, channel;
  int tempint;

#ifdef BENCHMARK
  start_counter();
#endif /* BENCHMARK */

  /**
   * IIR filter equation is
   * y[n] = 2 * (alpha*(x[n]-x[n-2]) + gamma*y[n-1] - beta*y[n-2])
   *
   * NOTE: The 2 factor was introduced in the coefficients to save
   * 			a multiplication
   *
   * This algorithm cascades two filters to get nice filtering
   * at the expense of extra CPU cycles
   */
  /* 16bit, 2 bytes per sample, so divide by two the length of
   * the buffer (length is in bytes)
   */
  for (index = 0; index < length/2; index+=nch)
  {
    /* pcm = preamp[channel]*(float)data[index+channel]; */
    for (channel = 0; channel < nch; channel++)
    {
      support.sse_acc.f[0] = support.sse_acc.f[1] = support.sse_acc.f[2] = support.sse_acc.f[3] = 0.;
      ptrs.p_sse_X_n[channel]->f[3] = data[index+channel];
      ptrs.p_sse_X_n[channel]->f[3] *= preamp[channel];
      ptrs.p_sse_X_n[channel]->f[0] = ptrs.p_sse_X_n[channel]->f[1]
        = ptrs.p_sse_X_n[channel]->f[2] = ptrs.p_sse_X_n[channel]->f[3];

      /* For each band */
      for (band = 0; band < sse_loop_num; band++)
      {
        ptrs.p_sse_Y_n[channel][band].v =
          (ptrs.p_sse_X_n[channel]->v-ptrs.p_sse_X_n2[channel]->v) * support.sse_iir_cf_alpha[band].v
          +  ptrs.p_sse_Y_n1[channel][band].v * support.sse_iir_cf_gamma[band].v
          - ptrs.p_sse_Y_n2[channel][band].v * support.sse_iir_cf_beta[band].v;
        support.sse_acc.v += ptrs.p_sse_Y_n[channel][band].v*gain[band][channel].v;
      } /* For each band */

      support.sse_acc.v = support.sse_acc.v + support.sse_acc.v;
      if (extra_filtering)
      {
        ptrs.p_sse_X2_n[channel]->f[0] = ptrs.p_sse_X2_n[channel]->f[1] =
          ptrs.p_sse_X2_n[channel]->f[2] = ptrs.p_sse_X2_n[channel]->f[3] =
            support.sse_acc.f[0] + support.sse_acc.f[1] + support.sse_acc.f[2] + support.sse_acc.f[3];
          for (band = 0; band < sse_loop_num; band++)
          {
            ptrs.p_sse_Y2_n[channel][band].v =
              (ptrs.p_sse_X2_n[channel]->v-ptrs.p_sse_X2_n2[channel]->v) * support.sse_iir_cf_alpha[band].v
              +  ptrs.p_sse_Y2_n1[channel][band].v * support.sse_iir_cf_gamma[band].v
              - ptrs.p_sse_Y2_n2[channel][band].v * support.sse_iir_cf_beta[band].v;
            support.sse_acc.v += ptrs.p_sse_Y2_n[channel][band].v*gain[band][channel].v;
          } /* For each band */
      }
      /* Round and convert to integer */
      tempint = support.sse_acc.f[0] + support.sse_acc.f[1] + support.sse_acc.f[2] + support.sse_acc.f[3] + ptrs.p_sse_X_n[channel]->f[0]*0.25;

      /* Limit the output */
      if (tempint < -32768)
        data[index+channel] = -32768;
      else if (tempint > 32767)
        data[index+channel] = 32767;
      else
        data[index+channel] = tempint;

      ptrs.p_sse_tmp  = ptrs.p_sse_Y_n2[channel];
      ptrs.p_sse_Y_n2[channel] = ptrs.p_sse_Y_n1[channel];
      ptrs.p_sse_Y_n1[channel] = ptrs.p_sse_Y_n[channel];
      ptrs.p_sse_Y_n[channel] = ptrs.p_sse_tmp;
      ptrs.p_sse_tmp  = ptrs.p_sse_X_n2[channel];
      ptrs.p_sse_X_n2[channel] = ptrs.p_sse_X_n1[channel];
      ptrs.p_sse_X_n1[channel] = ptrs.p_sse_X_n[channel];
      ptrs.p_sse_X_n[channel] = ptrs.p_sse_tmp;
      ptrs.p_sse_tmp  = ptrs.p_sse_Y2_n2[channel];
      ptrs.p_sse_Y2_n2[channel] = ptrs.p_sse_Y2_n1[channel];
      ptrs.p_sse_Y2_n1[channel] = ptrs.p_sse_Y2_n[channel];
      ptrs.p_sse_Y2_n[channel] = ptrs.p_sse_tmp;
      ptrs.p_sse_tmp  = ptrs.p_sse_X2_n2[channel];
      ptrs.p_sse_X2_n2[channel] = ptrs.p_sse_X2_n1[channel];
      ptrs.p_sse_X2_n1[channel] = ptrs.p_sse_X2_n[channel];
      ptrs.p_sse_X2_n[channel] = ptrs.p_sse_tmp;
    }
  }/* For each pair of samples */

#ifdef BENCHMARK
  timex += get_counter();
  blength += length;
  if (count++ == 1024)
  {
    printf("FLOATING POINT: %f %d\n",timex/1024.0, blength/1024);
    blength = 0;
    timex = 0.;
    count = 0;
  }
#endif /* BENCHMARK */

  /* Turn OFF Flush-to-zero mode to avoid exceptions on underflow */
  FTZ_OFF;
  return length;
}
