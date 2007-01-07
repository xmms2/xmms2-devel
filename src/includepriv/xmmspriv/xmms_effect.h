/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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




#ifndef _XMMS_PRIV_EFFECT_H_
#define _XMMS_PRIV_EFFECT_H_

#include <glib.h>
#include "xmms/xmms_effect.h"
#include "xmms/xmms_plugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_medialib.h"

/*
 * Type definitions
 */


xmms_effect_t *xmms_effect_new (xmms_plugin_t *plugin);
void xmms_effect_free (xmms_effect_t *effect);
gboolean xmms_effect_format_set (xmms_effect_t *effect, xmms_audio_format_t *fmt);
void xmms_effect_entry_set (xmms_effect_t *effect, xmms_medialib_entry_t entry);
void xmms_effect_run (xmms_effect_t *effect, xmms_sample_t *buf, guint len);
gboolean xmms_effect_plugin_verify (xmms_plugin_t *plugin);
gboolean xmms_effect_plugin_register (xmms_plugin_t *plugin);

#endif
