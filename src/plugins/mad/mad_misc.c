
#include <mad.h>

/* This code is cut&pasted from
 * MAD's WinAMP plugin.
 *
 * mad - MPEG audio decoder
 * Copyright (C) 2000-2001 Robert Leslie
 *
 */

struct dither {
  mad_fixed_t error[3];
  mad_fixed_t random;
};

/*
 * NAME:	prng()
 * DESCRIPTION:	32-bit pseudo-random number generator
 */
static inline
unsigned long prng(unsigned long state)
{
  return (state * 0x0019660dL + 0x3c6ef35fL) & 0xffffffffL;
}

static inline
signed long linear_dither(unsigned int bits, mad_fixed_t sample,
			  struct dither *dither, unsigned long *clipped,
			  mad_fixed_t *clipping)
{
  unsigned int scalebits;
  mad_fixed_t output, mask, random;

  enum {
    MIN = -MAD_F_ONE,
    MAX =  MAD_F_ONE - 1
  };

  /* noise shape */
  sample += dither->error[0] - dither->error[1] + dither->error[2];

  dither->error[2] = dither->error[1];
  dither->error[1] = dither->error[0] / 2;

  /* bias */
  output = sample + (1L << (MAD_F_FRACBITS + 1 - bits - 1));

  scalebits = MAD_F_FRACBITS + 1 - bits;
  mask = (1L << scalebits) - 1;

  /* dither */
  random  = prng(dither->random);
  output += (random & mask) - (dither->random & mask);

  dither->random = random;

  /* clip */
  if (output > MAX) {
    ++*clipped;
    if (output - MAX > *clipping)
      *clipping = output - MAX;

    output = MAX;

    if (sample > MAX)
      sample = MAX;
  }
  else if (output < MIN) {
    ++*clipped;
    if (MIN - output > *clipping)
      *clipping = MIN - output;

    output = MIN;

    if (sample < MIN)
      sample = MIN;
  }

  /* quantize */
  output &= ~mask;

  /* error feedback */
  dither->error[0] = sample - output;

  /* scale */
  return output >> scalebits;
}

#include <stdio.h>

unsigned int 
pack_pcm(unsigned char *data, unsigned int nsamples,
		 mad_fixed_t const *left, mad_fixed_t const *right,
		 int resolution, unsigned long *clipped,
		 mad_fixed_t *clipping)
{
	static struct dither left_dither, right_dither;
	unsigned char const *start;
	register signed long sample0, sample1;
	int effective, bytes;

	start     = data;
	effective = (resolution > 24) ? 24 : resolution;
	bytes     = resolution / 8;

	if (right) {  /* stereo */
		while (nsamples--) {
			sample0 = linear_dither(effective, *left++, &left_dither,
									clipped, clipping);
			sample1 = linear_dither(effective, *right++, &right_dither,
									clipped, clipping);

			switch (resolution) {
				case 8:
					data[0] = sample0 ^ 0x80;
					data[1] = sample1 ^ 0x80;
					break;

				case 32:
					sample0 <<= 8;
					sample1 <<= 8;
					data[        3] = sample0 >> 24;
					data[bytes + 3] = sample1 >> 24;
				case 24:
					data[        2] = sample0 >> 16;
					data[bytes + 2] = sample1 >> 16;
				case 16:
					data[        1] = sample0 >>  8;
					data[bytes + 1] = sample1 >>  8;
					data[        0] = sample0 >>  0;
					data[bytes + 0] = sample1 >>  0;
			}

			data += bytes * 2;
		}
	}
	else {  /* mono */
		while (nsamples--) {
			sample0 = linear_dither(effective, *left++, &left_dither,
									clipped, clipping);

			switch (resolution) {
				case 8:
					data[0] = sample0 ^ 0x80;
					break;

				case 32:
					sample0 <<= 8;
					data[3] = sample0 >> 24;
				case 24:
					data[2] = sample0 >> 16;
				case 16:
					data[1] = sample0 >>  8;
					data[0] = sample0 >>  0;
			}
			data += bytes;
		}
	}

	return data - start;
}
