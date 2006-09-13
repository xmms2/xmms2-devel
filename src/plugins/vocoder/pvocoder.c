
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "pvocoder.h"

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

struct pvocoder_s {
	/* Basic information about the stream */
	int channels;
	int chunksize;
	int overlaps;

	/* Index variables and input/output buffers */
	long index;
	double scale;
	double scaleidx;
	pvocoder_sample_t *win;
	pvocoder_sample_t *inbuf;
	pvocoder_sample_t *outbuf;

	/* Fourier transformation buffers */
	FFTWT(complex) **stft;
	FFTWT(complex) *stftbuf;
	FFTWT(plan) *stftplans;
	long stftidx;

	/* Inverse transformation buffers */
	FFTWT(complex) *invbuf;
	FFTWT(plan) invplan;

	/* Current phase information */
	FFTWT(complex) *phase;
};

static void pvocoder_reset(pvocoder_t *pvoc);
static void pvocoder_get_window(pvocoder_sample_t *buffer,
                                int chunksize,
                                int winsize);
static void pvocoder_calculate_chunk(pvocoder_t *pvoc, double index);

pvocoder_t *
pvocoder_init(int chunksize, int channels)
{
	pvocoder_t *ret;
	int i, nsamples;

	assert(chunksize > 0);
	assert(channels > 0);

	ret = calloc(1, sizeof(pvocoder_t));
	if (!ret)
		goto error_init;

	nsamples = chunksize * channels;
	ret->channels = channels;
	ret->chunksize = chunksize;
	ret->scale = 1.0;
	pvocoder_reset(ret);

	/* Create the (Hann) window for sample chunks */
	ret->win = FFTWT(malloc)(chunksize * sizeof(FFTWT(complex)));
	if (!ret->win)
		goto error_init;
	pvocoder_get_window(ret->win, chunksize, chunksize);

	/* Reserve inbuf and outbuf for 2 chunks so we can filter all overlaps */
	ret->inbuf = calloc(2 * nsamples, sizeof(pvocoder_sample_t));
	ret->outbuf = calloc(2 * nsamples, sizeof(pvocoder_sample_t));
	if (!ret->inbuf || !ret->outbuf)
		goto error_init;

	/* One buffer for each overlap plus the one of next chunk */
	ret->stft = calloc(ret->overlaps + 1, sizeof(FFTWT(complex *)));
	ret->stftbuf = FFTWT(malloc)((ret->overlaps + 1) * nsamples *
	                             sizeof(FFTWT(complex)));
	ret->stftplans = calloc(ret->overlaps + 1, sizeof(FFTWT(plan)));
	if (!ret->stft || !ret->stftbuf || !ret->stftplans)
		goto error_init;

	/* Update pointers into stftbuf array */
	for (i=0; i<=ret->overlaps; i++)
		ret->stft[i] = ret->stftbuf + i * nsamples;
	/* Calculate best plans for each stft buffer */
	for (i=0; i<=ret->overlaps; i++) {
		ret->stftplans[i] =
			FFTWT(plan_many_dft)(1, &chunksize, channels,
		                             ret->stft[i], NULL, channels, 1,
		                             ret->stft[i], NULL, channels, 1,
		                             FFTW_FORWARD, FFTW_MEASURE);
	}

	/* Allocate buffer for doing the calculations and inverse fft */
	ret->invbuf = FFTWT(malloc)(nsamples * sizeof(FFTWT(complex)));
	if (!ret->invbuf)
		goto error_init;

	ret->invplan = FFTWT(plan_many_dft)(1, &chunksize, channels,
	                                    ret->invbuf, NULL, channels, 1,
	                                    ret->invbuf, NULL, channels, 1,
	                                    FFTW_BACKWARD, FFTW_MEASURE);

	/* Allocate buffer for phase information of current stream */
	ret->phase = FFTWT(malloc)(nsamples/2 * sizeof(FFTWT(complex)));
	if (!ret->phase)
		goto error_init;

	return ret;

error_init:
	pvocoder_close(ret);
	return NULL;
}

void
pvocoder_close(pvocoder_t *pvoc)
{
	int i;

	if (pvoc) {
		FFTWT(free)(pvoc->phase);
		FFTWT(destroy_plan)(pvoc->invplan);
		FFTWT(free)(pvoc->invbuf);
		for (i=0; i<pvoc->overlaps; i++)
			FFTWT(destroy_plan)(pvoc->stftplans[i]);
		free(pvoc->stftplans);
		FFTWT(free)(pvoc->stftbuf);
		free(pvoc->stft);
		free(pvoc->inbuf);
		free(pvoc->outbuf);
		free(pvoc->win);
	}
	free(pvoc);
}

void
pvocoder_set_scale(pvocoder_t *pvoc, double scale)
{
	assert(pvoc);

	if (!pvoc)
		return;
	pvoc->scale = scale;
}

/**
 * Add new chunk to stft table, mainly window the data, perform some
 * stft transforms and update the stft table accordingly.
 */
void
pvocoder_add_chunk(pvocoder_t *pvoc, pvocoder_sample_t *chunk)
{
	pvocoder_sample_t *inbuf;
	int nsamples, i, j;

	assert(pvoc);
	assert(chunk);

	/* Copy the added data to inbuf for windowing */
	nsamples = pvoc->chunksize * pvoc->channels;
	memmove(pvoc->inbuf, pvoc->inbuf + nsamples,
	        nsamples * sizeof(pvocoder_sample_t));
	memcpy(pvoc->inbuf + nsamples, chunk,
	       nsamples * sizeof(pvocoder_sample_t));

	/* Exchange two vectors in stft table (old last is new first) */
	memcpy(pvoc->stft[0], pvoc->stft[pvoc->overlaps],
	       nsamples * sizeof(FFTWT(complex)));

	/**
	 * Run windowing for overlapping chunk, the first one is skipped
	 * because it was already handled on last add.
	 */
	inbuf = pvoc->inbuf;
	for (i=1; i<=pvoc->overlaps; i++) {
		/* Window the current chunk */
		inbuf += nsamples / pvoc->overlaps;
		for (j=0; j<nsamples; j++) {
			pvoc->stft[i][j][0] = inbuf[j] * pvoc->win[j / pvoc->channels];
			pvoc->stft[i][j][1] = 0.0;
		}

		/* Perform fourier transformation for every channel */
		FFTWT(execute)(pvoc->stftplans[i]);

		/* Scale with 2/3 because Hann window changes amplitude */
		for (j=0; j<nsamples; j++) {
			pvoc->stft[i][j][0] *= 2.0/3.0;
			pvoc->stft[i][j][1] *= 2.0/3.0;
		}
	}

	/* Update the input idx of stft table with added chunks */
	pvoc->stftidx += pvoc->overlaps;
	if (pvoc->stftidx == 0) {
		/* Save first phase for correct reconstruction when scale is 1.0 */
		for (i=0; i<nsamples/2; i++)
			pvoc->phase[i][0] = atan2(pvoc->stft[0][i][1],
			                          pvoc->stft[0][i][0]);
	}
}

/**
 * Get one chunk of chunksize * channels (interleaved) if available and return
 * how many chunks need to be added before this request can be completed.
 * 
 * Generally if it returns > 0, run add_chunk and try again, if it returns 0
 * just handle the chunk buffer values and if it returns < 0 we have a problem.
 */
int
pvocoder_get_chunk(pvocoder_t *pvoc, pvocoder_sample_t *chunk) {
	int nsamples, pos, i, j;

	assert(pvoc);
	assert(chunk);

	nsamples = pvoc->chunksize * pvoc->channels;

	/**
	 * Run the calculation routine for as many times as there are overlaps
	 * to get a full chunk in outbuf that can be copied into use
	 */
	for (i=pvoc->index%pvoc->overlaps; i<pvoc->overlaps; i++) {
		double tmpidx;

		/**
		 * Calculate current position of outbuf and check that we have
		 * enough data in stft table for completion of this run, if not
		 * return information how many chunks would need to be added
		 */
		pos = i * nsamples / pvoc->overlaps;
		tmpidx = pvoc->scaleidx - pvoc->stftidx;
		if (tmpidx < 0 || tmpidx >= pvoc->overlaps) {
			if (tmpidx < 0)
				tmpidx -= pvoc->overlaps;
			return (tmpidx / pvoc->overlaps);
		}

		/**
		 * Call the data modification routine that also performs the
		 * inverse stft and windowing and copy the result to outbuf
		 */
		pvocoder_calculate_chunk(pvoc, tmpidx);
		for (j=0; j<nsamples; j++)
			pvoc->outbuf[pos+j] += pvoc->invbuf[j][0];

		/* Increment both output index and scaled input index */
		pvoc->index++;
		pvoc->scaleidx += pvoc->scale;
	}

	/* Copy the result from outbuf and make room for some new output data */
	if (i == pvoc->overlaps) {
		memcpy(chunk, pvoc->outbuf,
		       nsamples * sizeof(pvocoder_sample_t));
		memmove(pvoc->outbuf, pvoc->outbuf + nsamples,
		        nsamples * sizeof(pvocoder_sample_t));
		memset(pvoc->outbuf + nsamples, 0,
		       nsamples * sizeof(pvocoder_sample_t));
	}
	
	/* Rounding can cause too high or low values so we need to clip */
	for (i=0; i<nsamples; i++)
		chunk[i] = CLAMP(chunk[i], -1.0, 1.0);

	/* Operation was successfull so return 0 delta for needed chunks */
	return 0;
}

void
pvocoder_get_final(pvocoder_t *pvoc, pvocoder_sample_t *chunk) {
	int nsamples;

	assert(pvoc);
	assert(chunk);

	nsamples = pvoc->chunksize * pvoc->channels;
	memcpy(chunk, pvoc->outbuf, nsamples * sizeof(pvocoder_sample_t));
	memset(pvoc->outbuf, 0, nsamples * sizeof(pvocoder_sample_t));
	pvocoder_reset(pvoc);
}

static void
pvocoder_reset(pvocoder_t *pvoc)
{
	/* 4 overlaps for (1-1/pvoc->overlaps) 75% should be a good value */
	pvoc->overlaps = 4;

	/* Reset index values */
	pvoc->index = 0;
	pvoc->scaleidx = 0.0;
	pvoc->stftidx = -2 * pvoc->overlaps;
}

/* Fill given chunk with a Hann window of specified size */
static void
pvocoder_get_window(pvocoder_sample_t *buffer, int chunksize, int winsize)
{
	int halfsize, halfwinsize, i;

	halfsize = chunksize / 2;
	halfwinsize = winsize / 2;

	for (i=0; i<halfwinsize; i++)
		buffer[halfsize-i] = 0.5 * (1.0 + cos(M_PI * i / halfwinsize));
	for (i=halfsize; i<chunksize; i++)
		buffer[i] = buffer[chunksize-i];
}

/**
 * First make linear interpolation of the absolute values of the two chunks
 * in stft table we are currently handling. Then add the current phase to our
 * buffer, calculate delta phase and add it to phase buffer for next run.
 * 
 * Finally run inverse fourier transformation and normalize and window the
 * result. Resulting data can be found from pvoc->invbuf later on.
 */
static void
pvocoder_calculate_chunk(pvocoder_t *pvoc, double index)
{
	FFTWT(complex) *buffer;
	int nsamples, idx, i, j;
	double diff;

	/* Calculate help variables we use in the routine */
	nsamples = pvoc->chunksize * pvoc->channels;
	idx = floor(index);
	diff = index - floor(index);
	buffer = pvoc->invbuf;

	/* Run modification loop for first half frequency data */
	for (i=0; i<nsamples/2; i++) {
		double dp;

		/**
		 * Interpolate absolute values of the stft buffers.
		 * buffer = (1-diff)*abs(stft[idx]) + diff*abs(stft[idx+1]);
		 */
		buffer[i][0] = (1.0-diff)*sqrt(pow(pvoc->stft[idx][i][0], 2) +
		                               pow(pvoc->stft[idx][i][1], 2));
		buffer[i][0] += diff*sqrt(pow(pvoc->stft[idx+1][i][0], 2) +
		                          pow(pvoc->stft[idx+1][i][1], 2));

		/**
		 * Add current phase into the buffer, notice that the fact that
		 * neither buffer nor phase includes config values which makes
		 * this operation much more simple.
		 * buffer = buffer*e^(j*phase);
		 */
		buffer[i][1] = buffer[i][0] * sin(pvoc->phase[i][0]);
		buffer[i][0] *= cos(pvoc->phase[i][0]);

		/**
		 * Calculate the phase delta for the frequency we are handling.
		 * dp = angle(stft[idx+1]) - angle(stft[idx]);
		 */
		dp = atan2(pvoc->stft[idx+1][i][1], pvoc->stft[idx+1][i][0]) -
		     atan2(pvoc->stft[idx][i][1], pvoc->stft[idx][i][0]);
		/* Phase delta is in [-2pi, 2pi] so we scale it to [-pi, pi] */
		dp -= 2 * M_PI * floor(dp / (2 * M_PI) + 0.5);

		/* Save current phase delta for next iteration */
		pvoc->phase[i][0] += dp;
	}

	/* Mirror the modifications to the second half as well */
	for (i=pvoc->channels; i<nsamples/2; i+=pvoc->channels) {
		for (j=0; j<pvoc->channels; j++) {
			buffer[nsamples-i+j][0] = buffer[i+j][0];
			buffer[nsamples-i+j][1] = -buffer[i+j][1];
		}
	}
	buffer[nsamples/2][0] = buffer[nsamples/2][1] = 0.0;

	/* Perform inverse fourier transform for each channel */
	FFTWT(execute)(pvoc->invplan);

	/* Window and normalize the resulting sample buffer */
	for (i=0; i<nsamples; i++) {
		buffer[i][0] *= pvoc->win[i / pvoc->channels] / pvoc->chunksize;
		buffer[i][1] = 0.0;
	}
}

