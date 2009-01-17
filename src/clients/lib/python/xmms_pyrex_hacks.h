#ifndef __XMMS_PYREX_HACKS_H__
#define __XMMS_PYREX_HACKS_H__

#include "xmmsc/xmmsv.h"

/*
 * pyrex doesn't know about const, so we do some tricks to get rid of
 * compiler warnings
 */

typedef const char *xmms_pyrex_constcharp_t;
typedef const char **xmms_pyrex_constcharpp_t;
typedef const unsigned char **xmms_pyrex_constucharpp_t;

#endif

