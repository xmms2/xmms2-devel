/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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

#ifndef __XMMS_SC_COMMON_H__
#define __XMMS_SC_COMMON_H__

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_util.h"

#include "interface_entity.h"

xmmsc_sc_interface_entity_t *xmmsc_sc_locate_interface_entity (xmmsc_connection_t *c, xmmsv_t *path);
void xmmsc_sc_create_root_namespace (xmmsc_connection_t *c);

#endif /* __XMMS_SC_COMMON_H__ */
