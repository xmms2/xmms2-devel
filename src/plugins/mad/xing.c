/** @file XING Header reader.
 *
 *
 * Orgin of this code from:
 * mad - MPEG audio decoder
 * Copyright (C) 2000-2001 Robert Leslie
 *
 * Modified for XMMS2.
 */

#include <string.h>

# include "xing.h"
# include "mad.h"

/* Our indians may go poop-poop... */
# define XING_MAGIC	(('X' << 24) | ('i' << 16) | ('n' << 8) | 'g') 
/* * so lets do like this instead... */

/*#define XING_MAGIC "Xing"*/

/*
 * NAME:	xing->init()
 * DESCRIPTION:	initialize Xing structure
 */
void xing_init(struct xing *xing)
{
  xing->flags = 0;
}

/*
 * NAME:	xing->parse()
 * DESCRIPTION:	parse a Xing VBR header
 */
int xing_parse(struct xing *xing, struct mad_bitptr ptr, unsigned int bitlen)
{

/*  if (bitlen < 64 || memcmp (ptr.byte, XING_MAGIC, 4))*/
  if (bitlen < 64 || mad_bit_read(&ptr, 32) != XING_MAGIC)
    goto fail;

  xing->flags = mad_bit_read(&ptr, 32);
  bitlen -= 64;

  if (xing->flags & XING_FRAMES) {
    if (bitlen < 32)
      goto fail;

    xing->frames = mad_bit_read(&ptr, 32);
    bitlen -= 32;
  }

  if (xing->flags & XING_BYTES) {
    if (bitlen < 32)
      goto fail;

    xing->bytes = mad_bit_read(&ptr, 32);
    bitlen -= 32;
  }

  if (xing->flags & XING_TOC) {
    int i;

    if (bitlen < 800)
      goto fail;

    for (i = 0; i < 100; ++i)
      xing->toc[i] = mad_bit_read(&ptr, 8);

    bitlen -= 800;
  }

  if (xing->flags & XING_SCALE) {
    if (bitlen < 32)
      goto fail;

    xing->scale = mad_bit_read(&ptr, 32);
    bitlen -= 32;
  }

  return 0;

 fail:
  xing->flags = 0;
  return -1;
}
