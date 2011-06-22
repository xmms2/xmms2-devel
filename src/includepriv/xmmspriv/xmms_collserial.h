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

#ifndef __XMMS_COLLSERIAL_H__
#define __XMMS_COLLSERIAL_H__

#include <glib.h>


/*
 * Public definitions
 */

#define XMMS_COLLSERIAL_ATTR_ID  "_serialized_id"


/*
 * Private defintions
 */

#include "xmmsc/xmmsv_coll.h"
#include "xmmspriv/xmms_collection.h"


/*
 * Public functions
 */

void xmms_collection_dag_restore (xmms_coll_dag_t *dag);
void xmms_collection_dag_save (xmms_coll_dag_t *dag);


#endif
