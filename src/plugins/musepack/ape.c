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

/*
 * APEv2 Specifications:
 * http://wiki.hydrogenaudio.org/index.php?title=APEv2_specification
 */

#include <stdlib.h>
#include <glib.h>

#include "ape.h"


/*
 * Defines
 */
#define get_int32(b,pos) ((((guchar *)(b))[(pos)+3]<<24) | \
                          (((guchar *)(b))[(pos)+2]<<16) | \
                          (((guchar *)(b))[(pos)+1]<<8)  | \
                           ((guchar *)(b))[(pos)])

#define TAG_HEADER_SIZE   32
#define TAG_SIGNATURE_SIZE 8

#define TAG_IS_APE(buff) (g_ascii_strncasecmp ((buff), "APETAGEX", 8) == 0)

#define TAG_FLAGS_TYPE    (6)
#define TAG_IS_TEXT(flags) (((flags) & TAG_FLAGS_TYPE) == 0)


/*
 * Type definitions
 */
struct xmms_apetag_St {
	xmms_xform_t *xform;

	gint version;
	gint items;
	gint flags;

	gint size;

	gint header;
	gint footer;
	gint data;

	GHashTable *hash;
};


/*
 * Function prototypes
 */
static gboolean xmms_apetag_cache_tag (xmms_apetag_t *tag);
static gboolean xmms_apetag_cache_tag_info (xmms_apetag_t *tag);
static gboolean xmms_apetag_cache_items (xmms_apetag_t *tag);


/*
 * Member functions
 */

/**
 * Initialize apetag object and fill it with metadata.
 * Will cleanup its own mess if init fails.
 */
xmms_apetag_t *
xmms_apetag_init (xmms_xform_t *xform)
{
	XMMS_DBG ("xmms_apetag_init");
	xmms_apetag_t *tag = g_new0 (xmms_apetag_t, 1);

	tag->xform = xform;

	return tag;
}


/**
 * Read the tag
 */
gboolean
xmms_apetag_read (xmms_apetag_t *tag)
{
	if (!xmms_apetag_cache_tag (tag)) {
		return FALSE;
	}

	if (!xmms_apetag_cache_tag_info (tag)) {
		return FALSE;
	}

	if (!xmms_apetag_cache_items (tag)) {
		return FALSE;
	}

	return TRUE;
}


/**
 * Destroy and restore position so that MPC decoder
 * plays happily ever after.
 */
void
xmms_apetag_destroy (xmms_apetag_t *tag)
{
	xmms_error_t err;

	g_return_if_fail (tag);

	xmms_error_reset (&err);
	xmms_xform_seek (tag->xform, 0, XMMS_XFORM_SEEK_SET, &err);

	if (tag->hash) {
		g_hash_table_destroy (tag->hash);
	}
	g_free (tag);
}


/**
 * Look up a string in the metadata hash.
 */
const gchar *
xmms_apetag_lookup_str (xmms_apetag_t *tag, const gchar *key)
{
	const gchar *value;

	g_return_val_if_fail (tag, NULL);
	g_return_val_if_fail (tag->hash, NULL);

	value = g_hash_table_lookup (tag->hash, key);

	return value;
}


/**
 * Look up an integer in the metadata hash.
 */
gint
xmms_apetag_lookup_int (xmms_apetag_t *tag, const gchar *key)
{
	const gchar *tmp;
	gint value = -1;

	g_return_val_if_fail (tag, -1);
	g_return_val_if_fail (tag->hash, -1);

	tmp = g_hash_table_lookup (tag->hash, key);
	if (tmp) {
		value = strtol (tmp, NULL, 10);
	}

	return value;
}


/**
 * See if there is an APEv2 tag at 'offset'.
 */
static gint
xmms_apetag_find_tag (xmms_apetag_t *tag, gint offset)
{
	gchar header[TAG_HEADER_SIZE];
	xmms_xform_seek_mode_t whence;
	xmms_error_t err;
	gint ret;

	g_return_val_if_fail (tag, -1);
	g_return_val_if_fail (tag->xform, -1);

	whence = (offset < 0) ? XMMS_XFORM_SEEK_END : XMMS_XFORM_SEEK_SET;

	xmms_error_reset (&err);
	ret = xmms_xform_seek (tag->xform, offset, whence, &err);
	if (ret > 0) {
		ret = xmms_xform_read (tag->xform, header, TAG_SIGNATURE_SIZE, &err);
		if (ret == TAG_SIGNATURE_SIZE) {
			if (TAG_IS_APE (header)) {
				offset = xmms_xform_seek (tag->xform, 0,
				                          XMMS_XFORM_SEEK_CUR,
				                          &err);
				offset -= TAG_SIGNATURE_SIZE;
			}
		}
	}

	return offset;
}


/**
 * Look for APEv2 tag at a couple of diffrent places.
 */
static gboolean
xmms_apetag_cache_tag (xmms_apetag_t *tag)
{
	gint offset;

	g_return_val_if_fail (tag, FALSE);

	/* default position */
	offset = xmms_apetag_find_tag (tag, -32);
	if (offset > 0) {
		tag->footer = offset;
		XMMS_DBG ("default pos");
		return TRUE;
	}

	/* abnormal position (APEv2 followed by an ID3v1) */
	offset = xmms_apetag_find_tag (tag, -160);
	if (offset > 0) {
		XMMS_DBG ("default+id3 pos");
		tag->footer = offset;
		return TRUE;
	}

	/* crazy position */
	offset = xmms_apetag_find_tag (tag, 0);
	if (offset > 0) {
		XMMS_DBG ("first pos");
		tag->header = offset;
		return TRUE;
	}

	return FALSE;
}


/**
 * Collect interesting data from APEv2 tag header.
 */
static gboolean
xmms_apetag_cache_tag_info (xmms_apetag_t *tag)
{
	gchar buffer[TAG_HEADER_SIZE];
	gboolean ret = FALSE;
	xmms_error_t err;
	gint offset;
	gint res;

	g_return_val_if_fail (tag, ret);
	g_return_val_if_fail (tag->xform, ret);

	XMMS_DBG ("tag pos found");

	offset = MAX (tag->header, tag->footer);

	XMMS_DBG ("offset at: %d", offset);

	xmms_error_reset (&err);
	res = xmms_xform_seek (tag->xform, offset, XMMS_XFORM_SEEK_SET, &err);
	if (res > 0) {
		res = xmms_xform_read (tag->xform, buffer, TAG_HEADER_SIZE, &err);
		if (res == TAG_HEADER_SIZE) {

			XMMS_DBG ("checking for signs of any apetag");

			if (TAG_IS_APE (buffer)) {

				XMMS_DBG ("apev2 tag found");

				tag->version = get_int32 (buffer, 8);
				tag->size = get_int32 (buffer, 12);
				tag->items = get_int32 (buffer, 16);
				tag->flags = get_int32 (buffer, 20);

				XMMS_DBG ("version: %d, items: %d, flags: %d, size: %d",
				          tag->version, tag->items, tag->flags, tag->size);

				if (tag->header > 0) {
					tag->data = tag->header + TAG_HEADER_SIZE;
					XMMS_DBG ("data (header) offset at %d", tag->data);
				} else if (tag->footer > 0) {
					tag->data = tag->footer - tag->size + TAG_HEADER_SIZE;
					XMMS_DBG ("data (footer) offset at %d", tag->data);
				}

				ret = TRUE;
			}
		}
	}

	return ret;
}


/**
 * Iterate over the items and add the (key,value) pairs to a hash.
 */
static gboolean
xmms_apetag_cache_items (xmms_apetag_t *tag)
{
	gint res, i, pos = 0;
	gboolean ret = FALSE;
	xmms_error_t err;

	g_return_val_if_fail (tag, ret);

	xmms_error_reset (&err);

	res = xmms_xform_seek (tag->xform, tag->data, XMMS_XFORM_SEEK_SET, &err);
	if (res > 0) {
		gchar *buffer = g_new (gchar, tag->size);
		res = xmms_xform_read (tag->xform, buffer, tag->size, &err);
		if (res > 0) {
			ret = TRUE;

			tag->hash = g_hash_table_new_full (g_str_hash, g_str_equal,
			                                   g_free, g_free);

			for (i = 0; i < tag->items; i++) {

				gint size = get_int32 (buffer, pos);
				pos += sizeof (gint32);

				gint flags = get_int32 (buffer, pos);
				pos += sizeof (gint32);

				if (TAG_IS_TEXT (flags)) {

					gint tmp = strlen (&buffer[pos]) + 1;
					if ((pos+tmp+size) > tag->size) { /* sanity check */
						ret = FALSE;
						break;
					}

					gchar *key = g_utf8_strdown (&buffer[pos], tmp);
					pos += tmp;

					gchar *value = g_strndup (&buffer[pos], size);
					pos += size;

					XMMS_DBG ("tag[%s] = %s", key, value);

					g_hash_table_insert (tag->hash, key, value);

				} else {
					/* Some other kind of tag we don't care about */

					gint tmp = strlen (&buffer[pos]) + 1;
					if ((pos+tmp+size) > tag->size) { /* sanity check */
						ret = FALSE;
						break;
					}

					pos += tmp;
					pos += size;
				}
			}
		}
		g_free (buffer);
	}

	return ret;
}
