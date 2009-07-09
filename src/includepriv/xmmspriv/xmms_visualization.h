/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

int32_t xmms_visualization_client_version (xmms_visualization_t *vis, xmms_error_t *err);
int32_t xmms_visualization_client_register (xmms_visualization_t *vis, xmms_error_t *err);
int32_t xmms_visualization_client_init_shm (xmms_visualization_t *vis, int32_t id, const char *shmid, xmms_error_t *err);
int32_t xmms_visualization_client_init_udp (xmms_visualization_t *vis, int32_t id, xmms_error_t *err);
int32_t xmms_visualization_client_property_set (xmms_visualization_t *vis, int32_t id, const gchar *key, const gchar *value, xmms_error_t *err);
int32_t xmms_visualization_client_properties_set (xmms_visualization_t *vis, int32_t id, xmmsv_t *prop, xmms_error_t *err);
void xmms_visualization_client_shutdown (xmms_visualization_t *vis, int32_t id, xmms_error_t *err);

void xmms_visualization_init (xmms_output_t *output);
void xmms_visualization_destroy (void);

#endif
