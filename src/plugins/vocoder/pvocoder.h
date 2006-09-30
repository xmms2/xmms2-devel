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

