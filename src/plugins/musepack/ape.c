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

typedef struct {
	gchar name[8];
	gint32 ver;
	gint32 size;
	gint32 count;
	gint32 flags;
	gchar zero[8];
} xmms_ape_tag_t;

static gboolean xmms_ape_tag_is_header (gint32 flags);
static gboolean xmms_ape_tag_is_text (gint32 flags);
static gboolean xmms_ape_tag_is_valid (xmms_ape_tag_t *tag);
/* Future preparation
static gboolean xmms_ape_tag_is_footer (gint32 flags);
static gboolean xmms_ape_tag_is_binary (gint32 flags);
static gboolean xmms_ape_tag_is_external (gint32 flags);
static gboolean xmms_ape_tag_is_read_only (gint32 flags);
*/

static gboolean
xmms_ape_tag_is_header (gint32 flags)
{
	return (flags & GINT32_FROM_LE (0x80000000)) ? TRUE : FALSE;
}

/*
static gboolean
xmms_ape_tag_is_footer (gint32 flags)
{
	return (flags & GINT32_FROM_LE (0x40000000)) ? TRUE : FALSE;
}
*/

static gboolean
xmms_ape_tag_is_text (gint32 flags)
{
	return ((flags & GINT32_FROM_LE (6)) == 0) ? TRUE : FALSE;
}

/*
static gboolean
xmms_ape_tag_is_binary (gint32 flags)
{
	return ((flags & GINT32_FROM_LE (6)) == 1) ? TRUE : FALSE;
}
*/

/*
static gboolean
xmms_ape_tag_is_external (gint32 flags)
{
	return ((flags & GINT32_FROM_LE (6)) == 2) ? TRUE : FALSE;
}
*/

/*
static gboolean
xmms_ape_tag_is_read_only (gint32 flags)
{
	return ((flags & GINT32_FROM_LE(1)) == 1) ? TRUE : FALSE;
}
*/


static gboolean
xmms_ape_tag_is_valid (xmms_ape_tag_t *tag)
{
	g_return_val_if_fail (tag, FALSE);

	if (g_strncasecmp (tag->name, "APETAGEX", 8) == 0) {
		return xmms_ape_tag_is_header (tag->flags);
	}
	return FALSE;
}


/**
 * Get the size of an APEv2 tag. 
 * Checks if the tag is valid and then returs the size.
 * @param buff a buffer to check for APEv2 tag.
 * @return the size of the tag or -1 on failure.
 */
gint
xmms_ape_get_size (gchar *buff)
{
	gint ret = -1;
	xmms_ape_tag_t *tag = (xmms_ape_tag_t *) buff;

	if (xmms_ape_tag_is_valid (tag))
		ret = GINT32_FROM_LE (tag->size);

	return ret;
}


/**
 * Extract the value of an APEv2 tag key.
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

	g_return_val_if_fail (key, NULL);
	g_return_val_if_fail (buff, NULL);

	while (pos < size) {
		gint32 len = buff[pos];
		/* step over 'length' */
		pos += sizeof (gint32); 

		gint32 flags = buff[pos];
		/* step over 'item flags' */
		pos += sizeof (gint32); 

		if (xmms_ape_tag_is_text(flags)) {
			gchar *item_key = &buff[pos];

			/* step over 'item key' and 'terminator' */
			pos += strlen (item_key) + 1; 
				 
			if (g_strncasecmp (item_key, key, strlen (key)) == 0) {
				item_val = (gchar *) g_new0 (gchar, len + 1);

				strncpy (item_val, &buff[pos], len);
				item_val[len] = '\0';

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
