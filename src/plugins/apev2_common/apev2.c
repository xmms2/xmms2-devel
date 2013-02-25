/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2005-2013 XMMS2 Team
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

/*
 * Demuxing related information gathered from ffmpeg and libdemac source.
 * APEv2 specification can be downloaded from the hydrogenaudio wiki:
 * http://wiki.hydrogenaudio.org/index.php?title=APEv2_specification
 */

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_bindata.h>

#define APE_TAG_FLAG_HAS_HEADER    0x80000000
#define APE_TAG_FLAG_HAS_FOOTER    0x40000000
#define APE_TAG_FLAG_IS_HEADER     0x20000000
#define APE_TAG_FLAG_DATA_TYPE     0x00000006
#define APE_TAG_FLAG_READ_ONLY     0x00000001

#define APE_TAG_DATA_TYPE_UTF8     0x00000000
#define APE_TAG_DATA_TYPE_BINARY   0x00000002
#define APE_TAG_DATA_TYPE_LOCATOR  0x00000004


static gboolean
xmms_apetag_handle_tag_coverart (xmms_xform_t *xform, const gchar *key,
                                 const gchar *value, gsize length)
{
	const gchar *mime, *ptr;
	gchar *filename;
	gchar hash[33];
	gsize size;

	if (*value == '\0') {
		return FALSE;
	}

	filename = g_strndup (value, length);
	if (!filename) {
		return FALSE;
	}

	if (g_str_has_suffix (filename, "jpg")) {
		mime = "image/jpeg";
	} else if (g_str_has_suffix (filename, "png")) {
		mime = "image/png";
	} else {
		XMMS_DBG ("Unknown image format: %s", filename);
		g_free (filename);
		return FALSE;
	}

	ptr = value + strlen (filename) + 1;
	size = length - (ptr - value);

	if (xmms_bindata_plugin_add ((const guchar *) ptr, size, hash)) {
		const gchar *metakey;

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT;
		xmms_xform_metadata_set_str (xform, metakey, hash);

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT_MIME;
		xmms_xform_metadata_set_str (xform, metakey, mime);
	}

	g_free (filename);

	return TRUE;
}


static guint32
xmms_apetag_get_le32 (guchar *data)
{
	return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
}

static gboolean
xmms_apetag_read (xmms_xform_t *xform)
{
	guchar buffer[32], *tagdata;
	xmms_error_t error;
	guint version, tag_size, items, flags;
	gint64 tag_position;
	gint pos, i, ret;

	g_return_val_if_fail (xform, FALSE);

	/* Try to find the 32-byte footer from the end of file */
	tag_position = xmms_xform_seek (xform, -32, XMMS_XFORM_SEEK_END, &error);
	if (tag_position < 0) {
		/* Seeking failed, failed to read tags */
		return FALSE;
	}

	/* Read footer data if seeking was possible */
	ret = xmms_xform_read (xform, buffer, 32, &error);
	if (ret != 32) {
		xmms_log_error ("Failed to read APE tag footer");
		return FALSE;
	}

	/* Check that the footer is valid, if not continue searching */
	if (memcmp (buffer, "APETAGEX", 8)) {
		/* Try to find the 32-byte footer before 128-byte ID3v1 tag */
		tag_position = xmms_xform_seek (xform, -160, XMMS_XFORM_SEEK_END, &error);
		if (tag_position < 0) {
			/* Seeking failed, failed to read tags */
			xmms_log_error ("Failed to seek to APE tag footer");
			return FALSE;
		}

		/* Read footer data if seeking was possible */
		ret = xmms_xform_read (xform, buffer, 32, &error);
		if (ret != 32) {
			xmms_log_error ("Failed to read APE tag footer");
			return FALSE;
		}

		if (memcmp (buffer, "APETAGEX", 8)) {
			/* Didn't find any APE tag from the file */
			return FALSE;
		}
	}

	version = xmms_apetag_get_le32 (buffer + 8);
	tag_size = xmms_apetag_get_le32 (buffer + 12);
	items = xmms_apetag_get_le32 (buffer + 16);
	flags = xmms_apetag_get_le32 (buffer + 20);

	if (flags & APE_TAG_FLAG_IS_HEADER) {
		/* We need a footer, not a header... */
		return FALSE;
	}

	if (version != 1000 && version != 2000) {
		xmms_log_error ("Invalid tag version, the writer is probably corrupted!");
		return FALSE;
	}

	/* Seek to the beginning of the actual tag data */
	ret = xmms_xform_seek (xform, tag_position - tag_size + 32,
	                       XMMS_XFORM_SEEK_SET, &error);
	if (ret < 0) {
		xmms_log_error ("Couldn't seek to the tag starting position, returned %d", ret);
		return FALSE;
	}

	tagdata = g_malloc (tag_size);
	ret = xmms_xform_read (xform, tagdata, tag_size, &error);
	if (ret != tag_size) {
		xmms_log_error ("Couldn't read the tag data, returned %d", ret);
		g_free (tagdata);
		return FALSE;
	}

	pos = 0;
	for (i = 0; i < items; i++) {
		gint itemlen, flags;
		gchar *key, *item;

		itemlen = xmms_apetag_get_le32 (tagdata + pos);
		pos += 4;
		flags = xmms_apetag_get_le32 (tagdata + pos);
		pos += 4;
		key = (gchar *) tagdata + pos;
		pos += strlen (key) + 1;

		switch (flags & APE_TAG_FLAG_DATA_TYPE) {
			case APE_TAG_DATA_TYPE_UTF8:
				item = g_strndup ((gchar *) tagdata + pos, itemlen);
				break;
			case APE_TAG_DATA_TYPE_BINARY:
				item = g_malloc (itemlen);
				memcpy (item, tagdata + pos, itemlen);
				break;
			case APE_TAG_DATA_TYPE_LOCATOR:
				item = NULL;
				break;
		}

		if (item != NULL && !xmms_xform_metadata_mapper_match (xform, key, item, itemlen)) {
			if ((flags & APE_TAG_FLAG_DATA_TYPE) == APE_TAG_DATA_TYPE_UTF8) {
				XMMS_DBG ("Unhandled tag '%s' = '%s'", key, item);
			} else {
				XMMS_DBG ("Unhandled tag '%s' = '(binary)'", key);
			}
		}

		g_free (item);
		pos += itemlen;
	}
	g_free (tagdata);

	return TRUE;
}
