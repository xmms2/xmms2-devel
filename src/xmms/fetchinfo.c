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
#include "xmmspriv/xmms_fetch_info.h"
#include <string.h>

xmms_fetch_info_t *
xmms_fetch_info_new (s4_sourcepref_t *prefs)
{
	xmms_fetch_info_t *info;

	info = g_new (xmms_fetch_info_t, 1);
	info->fs = s4_fetchspec_create ();
	info->ft = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
	                                  (GDestroyNotify) g_hash_table_destroy);

	/* We always need the media library id for all entries */
	s4_fetchspec_add (info->fs, "song_id", prefs, S4_FETCH_PARENT);

	return info;
}

void
xmms_fetch_info_free (xmms_fetch_info_t *info)
{
	s4_fetchspec_free (info->fs);
	g_hash_table_destroy (info->ft);
	g_free (info);
}

int
xmms_fetch_info_add_key (xmms_fetch_info_t *info, void *object,
                         const char *key, s4_sourcepref_t *prefs)
{
	GHashTable *table;
	int index, null = 0;

	if (key != NULL && strcmp (key, "id") == 0)
		return 0;

	table = g_hash_table_lookup (info->ft, object);
	if (table == NULL) {
		table = g_hash_table_new (g_str_hash, g_str_equal);
		g_hash_table_insert (info->ft, object, table);
	}

	if (key == NULL) {
		/* TODO: ???? */
		key = "__NULL__";
		null = 1;
	}

	if ((index = GPOINTER_TO_INT (g_hash_table_lookup (table, key))) == 0) {
		index = s4_fetchspec_size (info->fs);
		g_hash_table_insert (table, (void*)key, GINT_TO_POINTER (index));
		if (null)
			key = NULL;
		s4_fetchspec_add (info->fs, key, prefs, S4_FETCH_DATA);
	}

	return index;
}

int
xmms_fetch_info_get_index (xmms_fetch_info_t *info, void *object, const char *key)
{
	GHashTable *table = g_hash_table_lookup (info->ft, object);
	if (key == NULL)
		key = "__NULL__";
	return GPOINTER_TO_INT (g_hash_table_lookup (table, key));
}
