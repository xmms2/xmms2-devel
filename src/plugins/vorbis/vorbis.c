/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#include <glib.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <xmms/xmms_xformplugin.h>

#include "../vorbis_common/common.c"

XMMS_XFORM_PLUGIN ("vorbis",
                   "Vorbis Decoder", XMMS_VERSION,
                   "Xiph's Ogg/Vorbis decoder",
                   xmms_vorbis_plugin_setup);

static void
xmms_vorbis_set_duration (xmms_xform_t *xform, guint dur)
{
	xmms_xform_metadata_set_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
	                             dur * 1000);
}

static gulong
xmms_vorbis_ov_read (OggVorbis_File *vf, gchar *buf, gint len, gint bigendian,
                     gint sampsize, gint signd, gint *outbuf)
{
	gulong ret;

	do {
		ret = ov_read (vf, buf, len, bigendian, sampsize, signd, outbuf);
	} while (ret == OV_HOLE);

	return ret;
}
