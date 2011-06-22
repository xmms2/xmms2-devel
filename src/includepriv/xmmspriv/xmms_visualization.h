/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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


#ifndef __XMMS_VISUALIZATION_H__
#define __XMMS_VISUALIZATION_H__

#include "xmms/xmms_object.h"
#include "xmms/xmms_error.h"
#include "xmms/xmms_sample.h"
#include "xmmspriv/xmms_output.h"

struct xmms_visualization_St;
typedef struct xmms_visualization_St xmms_visualization_t;

xmms_visualization_t *xmms_visualization_new (xmms_output_t *output);

#endif
