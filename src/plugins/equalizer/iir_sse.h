/*
 *   PCM time-domain equalizer
 *
 *   Copyright (C) 2002-2006  Felipe Rivera <liebremx at users.sourceforge.net>
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
 *   $Id: iir_sse.h,v 1.2 2006/01/15 00:26:16 liebremx Exp $
 */
#ifndef IIR_SSE_H
#define IIR_SSE_H

/*
 * SSE filter implementation data structures
 */
#define MAX_SSE_VECTORS 8
typedef float v4sf __attribute__ ((vector_size(16))); /* vector of four single floats */
union f4vector 
{
  v4sf v;
  float f[4];
};
typedef struct
{
  union f4vector sse_iir_cf_alpha[MAX_SSE_VECTORS]; 
  union f4vector sse_iir_cf_beta[MAX_SSE_VECTORS]; 
  union f4vector sse_iir_cf_gamma[MAX_SSE_VECTORS];
  union f4vector sse_acc;
  union f4vector dummy[MAX_SSE_VECTORS]; /* Word alignment */
}sSupport;

typedef struct
{
  union f4vector sse_Y_n[MAX_SSE_VECTORS];
  union f4vector sse_Y_n1[MAX_SSE_VECTORS]; 
  union f4vector sse_Y_n2[MAX_SSE_VECTORS]; 
  union f4vector sse_X_n; 
  union f4vector sse_X_n1; 
  union f4vector sse_X_n2;
  union f4vector sse_Y2_n[MAX_SSE_VECTORS]; 
  union f4vector sse_Y2_n1[MAX_SSE_VECTORS]; 
  union f4vector sse_Y2_n2[MAX_SSE_VECTORS]; 
  union f4vector sse_X2_n; 
  union f4vector sse_X2_n1; 
  union f4vector sse_X2_n2;
  union f4vector dummy[10]; /* Word alignment */
}sHistory;

typedef struct
{ 
  union f4vector *p_sse_Y_n[EQ_CHANNELS];
  union f4vector *p_sse_Y_n1[EQ_CHANNELS];
  union f4vector *p_sse_Y_n2[EQ_CHANNELS];
  union f4vector *p_sse_X_n[EQ_CHANNELS];
  union f4vector *p_sse_X_n1[EQ_CHANNELS];
  union f4vector *p_sse_X_n2[EQ_CHANNELS];
  union f4vector *p_sse_Y2_n[EQ_CHANNELS];
  union f4vector *p_sse_Y2_n1[EQ_CHANNELS];
  union f4vector *p_sse_Y2_n2[EQ_CHANNELS];
  union f4vector *p_sse_X2_n[EQ_CHANNELS];
  union f4vector *p_sse_X2_n1[EQ_CHANNELS];
  union f4vector *p_sse_X2_n2[EQ_CHANNELS];
  union f4vector *p_sse_tmp;
  union f4vector *dummy[7]; /* Word alignment */
}sPtrs;

#endif
