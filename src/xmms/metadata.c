/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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

#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "xmmspriv/xmms_metadata_mapper.h"
#include "xmmspriv/xmms_utils.h"

#include "xmms/xmms_log.h"
#include "xmms/xmms_xformplugin.h"

static gboolean xmms_xform_metadata_parse_string (xmms_xform_t *xform, const gchar *key, const gchar *value, gsize length);
static gboolean xmms_xform_metadata_parse_tracknumber (xmms_xform_t *xform, const gchar *key, const gchar *value, gsize length);
static gboolean xmms_xform_metadata_parse_discnumber (xmms_xform_t *xform, const gchar *key, const gchar *value, gsize length);
static gint xmms_xform_metadata_parse_number_slash_number (const gchar *value, gint *first, gint *second);

static const xmms_xform_metadata_mapping_t defaults[] = {
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,            xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_SORT,       xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ORIGINAL_ARTIST,   xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,             xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_SORT,        xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST,      xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST_SORT, xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,             xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE_SORT,        xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,              xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ORIGINALYEAR,      xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,           xmms_xform_metadata_parse_tracknumber },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_TOTALTRACKS,       xmms_xform_metadata_parse_number      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE,             xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT,           xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT_LANG,      xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK,        xmms_xform_metadata_parse_replay_gain },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM,        xmms_xform_metadata_parse_replay_gain },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK,        xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM,        xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION,       xmms_xform_metadata_parse_compilation },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID,          xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID,         xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID,          xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_BPM,               xmms_xform_metadata_parse_number      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_IS_VBR,            xmms_xform_metadata_parse_number      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET,         xmms_xform_metadata_parse_discnumber  },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_TOTALSET,          xmms_xform_metadata_parse_number      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_DESCRIPTION,       xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_GROUPING,          xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_PERFORMER,         xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_CONDUCTOR,         xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_REMIXER,           xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_DJMIXER,           xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_MIXER,             xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ARRANGER,          xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_PRODUCER,          xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_PUBLISHER,         xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_COMPOSER,          xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_LYRICIST,          xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ASIN,              xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_ISRC,              xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_BARCODE,           xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_CATALOGNUMBER,     xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_COPYRIGHT,         xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_ARTIST,    xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_FILE,      xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_PUBLISHER, xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_COPYRIGHT, xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_RELEASE_STATUS,    xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_RELEASE_TYPE,      xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_RELEASE_COUNTRY,   xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_RELEASE_FORMAT,    xmms_xform_metadata_parse_string      },
	{ XMMS_MEDIALIB_ENTRY_PROPERTY_CHANNEL,           xmms_xform_metadata_parse_string      },
};


static gboolean
xmms_xform_metadata_parse_string (xmms_xform_t *xform, const gchar *key,
                                  const gchar *value, gsize length)
{
	return xmms_xform_metadata_set_str (xform, key, value);
}

gboolean
xmms_xform_metadata_parse_number (xmms_xform_t *xform, const gchar *key,
                                  const gchar *value, gsize length)
{
	gchar *endptr = NULL;
	gint number;

	number = strtol (value, &endptr, 10);
	if (endptr > value && *endptr == '\0') {
		xmms_xform_metadata_set_int (xform, key, number);
		return TRUE;
	}
	return FALSE;
}

/* Parse a string like '1/10' and return how many integers were found */
static gint
xmms_xform_metadata_parse_number_slash_number (const gchar *value, gint *first, gint *second)
{
	gchar *endptr = NULL;
	gint ivalue;

	*first = *second = -1;

	ivalue = strtol (value, &endptr, 10);
	if (!(endptr > value)) {
		return 0;
	}

	*first = ivalue;

	if (*endptr != '/') {
		return 1;
	}

	value = (const gchar *) endptr + 1;

	ivalue = strtol (value, &endptr, 10);
	if (!(endptr > value)) {
		return 1;
	}

	*second = ivalue;

	return 2;
}

static gboolean
xmms_xform_metadata_parse_tracknumber (xmms_xform_t *xform, const gchar *key,
                                       const gchar *value, gsize length)
{
	gint tracknr, tracktotal;

	xmms_xform_metadata_parse_number_slash_number (value, &tracknr, &tracktotal);
	if (tracknr > 0) {
		const gchar *metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR;
		xmms_xform_metadata_set_int (xform, metakey, tracknr);
	}
	if (tracktotal > 0) {
		const gchar *metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TOTALTRACKS;
		xmms_xform_metadata_set_int (xform, metakey, tracktotal);
	}

	return tracknr > 0;
}

static gboolean
xmms_xform_metadata_parse_discnumber (xmms_xform_t *xform, const gchar *key,
                                      const gchar *value, gsize length)
{
	gint partofset, totalset;

	xmms_xform_metadata_parse_number_slash_number (value, &partofset, &totalset);
	if (partofset > 0) {
		const gchar *metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET;
		xmms_xform_metadata_set_int (xform, metakey, partofset);
	}
	if (totalset > 0) {
		const gchar *metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TOTALSET;
		xmms_xform_metadata_set_int (xform, metakey, totalset);
	}

	return partofset > 0;
}

gboolean
xmms_xform_metadata_parse_replay_gain (xmms_xform_t *xform, const gchar *key,
                                       const gchar *value, gsize length)
{
	gchar *endptr = NULL;
	gdouble number;

	number = strtod (value, &endptr);
	if (endptr > value) {
		gchar buffer[8];
		g_snprintf (buffer, sizeof (buffer), "%f",
		            pow (10.0, number / 20));
		xmms_xform_metadata_set_str (xform, key, buffer);
		return TRUE;
	}

	return FALSE;
}

gboolean
xmms_xform_metadata_parse_compilation (xmms_xform_t *xform, const gchar *key,
                                       const gchar *value, gsize length)
{
	const gchar *musicbrainz_va_id = "89ad4ac3-39f7-470e-963a-56509c546377";
	gchar *endptr = NULL;
	gint number;

	if (strcasecmp (musicbrainz_va_id, value) == 0) {
		return xmms_xform_metadata_set_int (xform, key, 1);
	}

	number = strtol (value, &endptr, 10);
	if (endptr > value && *endptr == '\0' && number > 0) {
		return xmms_xform_metadata_set_int (xform, key, 1);
	}

	return FALSE;
}

static GHashTable *
xmms_metadata_mapper_get_default (void)
{
	GHashTable *table;
	gint i;

	table = g_hash_table_new (xmms_strcase_hash, xmms_strcase_equal);

	for (i = 0; i < G_N_ELEMENTS (defaults); i++) {
		g_hash_table_insert (table, (gpointer) defaults[i].key, (gpointer) &defaults[i]);
	}

	return table;
}

GHashTable *
xmms_metadata_mapper_init (const xmms_xform_metadata_basic_mapping_t *basic_mappings, gint basic_count,
                           const xmms_xform_metadata_mapping_t *mappings, gint count)
{
	GHashTable *default_mapper, *result;
	gint i;

	result = g_hash_table_new (xmms_strcase_hash, xmms_strcase_equal);

	default_mapper = xmms_metadata_mapper_get_default ();

	/* Initialize default mappers */
	for (i = 0; i < basic_count; i++) {
		const xmms_xform_metadata_mapping_t *mapping;

		mapping = g_hash_table_lookup (default_mapper, basic_mappings[i].to);
		if (mapping == NULL) {
			xmms_log_error ("No default metadata mapper for '%s'", basic_mappings[i].to);
			continue;
		}

		g_hash_table_insert (result, (gpointer) basic_mappings[i].from, (gpointer) mapping);
	}

	for (i = 0; i < count; i++) {
		g_hash_table_insert (result, (gpointer) mappings[i].key, (gpointer) &mappings[i]);
	}

	g_hash_table_unref (default_mapper);

	return result;
}

gboolean
xmms_metadata_mapper_match (GHashTable *table, xmms_xform_t *xform,
                            const gchar *key, const gchar *value, gsize length)
{
	const xmms_xform_metadata_mapping_t *mapper;

	mapper = g_hash_table_lookup (table, key);
	if (mapper != NULL) {
		return mapper->func (xform, mapper->key, value, length);
	}

	return FALSE;
}
