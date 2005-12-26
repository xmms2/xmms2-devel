/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2005 Daniel Svensson, <daniel@nittionio.nu> 
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
 * Partial APEv2 parser, handles text only. 
 * http://wiki.hydrogenaudio.org/index.php?title=APEv2_specification
 */

#include <glib.h>

#include "musepack.h"
#include "ape.h"

static gboolean xmms_ape_tag_is_header (gint32 flags);
static gboolean xmms_ape_tag_is_text (gint32 flags);

#define get_int32(b,pos) ((b[pos]<<24)|(b[pos+1]<<16)|(b[pos+2]<<8)|(b[pos+3])) 

#define APE_HEADER       ~0x7F
#define APE_FLAGS_TYPE    0x06


/**
 * Checks if the Header bit is set.
 */
static gboolean
xmms_ape_tag_is_header (gint32 flags)
{
	return (flags & APE_HEADER) ? TRUE : FALSE;
}

/**
 * Checks if the Text bit is set (no bits set in type flags)
 */
static gboolean
xmms_ape_tag_is_text (gint32 flags)
{
	return ((flags & APE_FLAGS_TYPE) == 0) ? TRUE : FALSE;
}


/**
 * Checks if the tag is valid.
 * @param buff a buffer to check for an APEv2 tag.
 * @param size the size of the buffer
 * @return true if the tag is valid, else false
 */
gboolean
xmms_ape_tag_is_valid (gchar *buff, gint len)
{
	guint32 flags;
	gboolean ret = FALSE;

	g_return_val_if_fail (buff, ret);

	if (len == XMMS_APE_HEADER_SIZE) {
		if (g_strncasecmp (buff, "APETAGEX", 8) == 0) {
			flags = GINT32_FROM_BE (get_int32 (buff, 20));
			ret = xmms_ape_tag_is_header (flags);
		}
	}

	return ret;
}


/**
 * Get the size of an APEv2 tag. 
 * @param buff a buffer with an APEv2 tag.
 * @param size the size of the buffer
 * @return the size of the tag or -1 on failure.
 */
gint32
xmms_ape_get_size (gchar *buff, gint len)
{
	gint ret = -1;
	guint32 size;

	g_return_val_if_fail (buff, ret);

	if (len == XMMS_APE_HEADER_SIZE) {
		size = get_int32 (buff, 12);
		ret = GUINT32_SWAP_LE_BE (size);
	}

	return ret;
}


/**
 * Extract the text value matching an APEv2 tag key.
 *
 * @param key the key to search the tag for.
 * @param buff a buffer with the APEv2 tag.
 * @param size the size of the buffer.
 * @return an allocated string with the value or NULL on failure.
 */
gchar *
xmms_ape_get_text (gchar *key, gchar *buff, gint size)
{
	gint pos = 0;
	gchar *item_val = NULL;
	gchar *item_key = NULL;
	gint32 len, flags;

	g_return_val_if_fail (key, NULL);
	g_return_val_if_fail (buff, NULL);

	while ((pos + 2 * sizeof (gint32)) < size) {
		len = GUINT32_SWAP_LE_BE (get_int32 (buff, pos));
		/* step over 'length' */
		pos += sizeof (gint32); 

		flags = GUINT32_SWAP_LE_BE (get_int32 (buff, pos));
		/* step over 'item flags' */
		pos += sizeof (gint32); 

		if (xmms_ape_tag_is_text (flags)) {
			item_key = &buff[pos];

			/* step over 'item key' and 'terminator' */
			pos += strlen (item_key) + 1; 
				 
			if (g_strncasecmp (item_key, key, strlen (key)) == 0) {
				item_val = (gchar *) g_new0 (gchar, len + 1);

				g_strlcpy (item_val, &buff[pos], len + 1);

				/* we've got our match! */
				break; 
			}

			/* no match, step over item_val */
			pos += len;

		} else {
			pos += 11;
		}
	}

	return item_val;
}
