/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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

#ifndef __XMMS_COLLQUERY_H__
#define __XMMS_COLLQUERY_H__

#include <glib.h>


/*
 * Public definitions
 */

#define XMMS_COLLQUERY_DEFAULT_BASE  "url"


/*
 * Private defintions
 */

#include "xmmsc/xmmsc_coll.h"
#include "xmmspriv/xmms_collection.h"


/*
 * Public functions
 */

GString* xmms_collection_get_query (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, guint limit_start, guint limit_len, xmmsv_t *order, xmmsv_t *fetch, xmmsv_t *group);


#endif
