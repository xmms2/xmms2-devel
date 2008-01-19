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




#ifndef __XMMS_PLUGIN_H__
#define __XMMS_PLUGIN_H__

#include <glib.h>
#include <xmmsc/xmmsc_idnumbers.h>
#include "xmms_configuration.h"

typedef struct xmms_plugin_desc_St {
	xmms_plugin_type_t type;
	gint api_version;
	const gchar *shortname;
	const gchar *name;
	const gchar *version;
	const gchar *description;
	gboolean (*setup_func)(gpointer);
} xmms_plugin_desc_t;

#define XMMS_PLUGIN(type, api_ver, shname, name, ver, desc, setupfunc)	\
	const xmms_plugin_desc_t XMMS_PLUGIN_DESC = {				\
		type,							\
		api_ver,						\
		shname,							\
		name,							\
		ver,							\
		desc,							\
		setupfunc						\
	};
		

#endif /* __XMMS_PLUGIN_H__ */
