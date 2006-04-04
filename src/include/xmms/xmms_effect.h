/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */




#ifndef _XMMS_EFFECT_H_
#define _XMMS_EFFECT_H_

#include "xmms/xmms_plugin.h"

typedef struct xmms_effect_St xmms_effect_t;

gpointer xmms_effect_private_data_get (xmms_effect_t *effect);
void xmms_effect_private_data_set (xmms_effect_t *effect, gpointer data);
xmms_plugin_t * xmms_effect_plugin_get (xmms_effect_t *effect);


#endif

