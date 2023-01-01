/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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
#include <xmmsc/xmmsc_compiler.h>
#include <xmms_configuration.h>

#define XMMS_PLUGIN_SHORTNAME_MAX_LEN 32

G_BEGIN_DECLS

typedef struct xmms_plugin_desc_St {
	xmms_plugin_type_t type;
	gint api_version;
	gchar shortname[XMMS_PLUGIN_SHORTNAME_MAX_LEN];
	const gchar *name;
	const gchar *version;
	const gchar *description;
	gboolean (*setup_func)(gpointer);
} xmms_plugin_desc_t;

#define XMMS_PLUGIN_DEFINE(type, api_ver, shname, name, ver, desc, setupfunc)	\
	xmms_plugin_desc_t XMMS_PUBLIC XMMS_PLUGIN_DESC_SYMBOL_NAME = { \
		type,							\
		api_ver,						\
		shname,							\
		name,							\
		ver,							\
		desc,							\
		setupfunc						\
	};

G_END_DECLS

#endif /* __XMMS_PLUGIN_H__ */
