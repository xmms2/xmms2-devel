#ifndef __MAD_MISC_H_
#define __MAD_MISC_H_

#include <mad.h>

pack_pcm(unsigned char *data, unsigned int nsamples,
      mad_fixed_t const *left, mad_fixed_t const *right,
      int resolution, unsigned long *clipped,
      mad_fixed_t *clipping);


#endif
