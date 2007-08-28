#include <math.h>
#include "common.h"

#define FFT_LEN XMMSC_VISUALIZATION_WINDOW_SIZE
/* TODO: better way, check this! */
#define FFT_BITS 9

/* Log scale settings */
#define AMP_LOG_SCALE_THRESHOLD0	0.001f
#define AMP_LOG_SCALE_DIVISOR		6.908f	/* divisor = -log threshold */
#define FREQ_LOG_SCALE_BASE		2.0f

static gfloat window[FFT_LEN];
static gfloat spec[FFT_LEN/2];
static gboolean fft_ready = FALSE;
static gboolean fft_done;

void fft_init ()
{
	if (!fft_ready) {
		int i;
		/* calculate Hann window used to reduce spectral leakage */
		for (i = 0; i < FFT_LEN; i++) {
			window[i] = 0.5 - 0.5 * cos (2.0 * M_PI * i / FFT_LEN);
		}
		fft_done = TRUE;
	}
	fft_done = FALSE;
}

/* interesting:	data->value.uint32 = xmms_sample_samples_to_ms (vis->format, pos); */

static void
fft (short *samples, gfloat *spec)
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
			k >>= 1;
		}

		j += k;
	}

	/* do fft */
	for (l = 1; l <= FFT_BITS; l++) {
		gint le = 1 << l;
		gint le1 = le / 2;
		gfloat u_r = 1.0;
		gfloat u_i = 0.0;
		gfloat w_r =  cosf (M_PI / (float) le1);
		gfloat w_i = -sinf (M_PI / (float) le1);

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

/**
 * Calcualte the FFT on the decoded data buffer.
 */
short
fill_buffer_fft (int16_t* dest, int size, short *src)
{
	int i;
	float tmp;

	if (size != FFT_LEN * 2) {
		return 0;
	}

	if (!fft_done) {
		fft (src, spec);
		fft_done = TRUE;
	}

	/* TODO: more sophisticated! */
	for (i = 0; i < FFT_LEN / 2; ++i) {
		if (spec[i] >= 1.0) {
			dest[i] = htons (SHRT_MAX);
		} else if (spec[i] < 0.0) {
			dest[i] = 0;
		} else {
			tmp = spec[i];
			if (tmp > AMP_LOG_SCALE_THRESHOLD0) {
//				tmp = 1.0f + (logf (tmp) /  AMP_LOG_SCALE_DIVISOR);
			} else {
				tmp = 0.0f;
			}
			dest[i] = htons ((int16_t)(tmp * SHRT_MAX));
		}
	}
	return FFT_LEN / 2;
}

short
fill_buffer (int16_t *dest, xmmsc_vis_properties_t* prop, int channels, int size, short *src)
{
	int i, j;
	if (prop->type == VIS_PEAK) {
		short l = 0, r = 0;
		for (i = 0; i < size; i += channels) {
			if (src[i] > 0 && src[i] > l) {
				l = src[i];
			}
			if (src[i] < 0 && -src[i] > l) {
				l = -src[i];
			}
			if (channels > 1) {
				if (src[i+1] > 0 && src[i+1] > r) {
					r = src[i+1];
				}
				if (src[i+1] < 0 && -src[i+1] > r) {
					r = -src[i+1];
				}
			}
		}
		if (channels == 1) {
			r = l;
		}
		if (prop->stereo) {
			dest[0] = htons (l);
			dest[1] = htons (r);
			size = 2;
		} else {
			dest[0] = htons ((l + r) / 2);
			size = 1;
		}
	}
	if (prop->type == VIS_PCM) {
		for (i = 0, j = 0; i < size; i += channels, j++) {
			short *l, *r;
			if (prop->pcm_hardwire) {
				l = &dest[j*2];
				r = &dest[j*2 + 1];
			} else {
				l = &dest[j];
				r = &dest[size/channels + j];
			}
			*l = htons (src[i]);
			if (prop->stereo) {
				if (channels > 1) {
					*r = htons (src[i+1]);
				} else {
					*r = htons (src[i]);
				}
			}
		}
		size /= channels;
		if (prop->stereo) {
			size *= 2;
		}
	}
	if (prop->type == VIS_SPECTRUM) {
		size = fill_buffer_fft (dest, size, src);
	}
	return size;
}
