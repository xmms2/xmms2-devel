#include "xmms/plugin.h"
#include "xmms/medialib.h"
#include "xmms/util.h"
#include "xmms/magic.h"
#include "xmms/playlist.h"
#include "xmms/xmms.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>

#include <sqlite.h>

#include "str.h"

/*
 * Type definitions
 */

typedef struct {
	sqlite *sq;
} xmms_sqlite_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_sqlite_new (xmms_medialib_t *medialib);
GList *xmms_sqlite_search (xmms_medialib_t *medialib, xmms_playlist_entry_t *search);
void xmms_sqlite_add_entry (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry);
void xmms_sqlite_close (xmms_medialib_t *medialib);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_MEDIALIB, "sqlite",
			"SQLite medialib " XMMS_VERSION,
		 	"SQLite backend to medialib");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "URL", "http://www.sqlite.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_sqlite_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEARCH, xmms_sqlite_search);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_ADD_ENTRY, xmms_sqlite_add_entry);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_sqlite_close);

	return plugin;
}

/*
 * Member functions
 */

void
xmms_sqlite_close (xmms_medialib_t *medialib)
{
	xmms_sqlite_data_t *data;
	g_return_if_fail (medialib);

	data = xmms_medialib_get_data (medialib);

	if (data) {
		sqlite_close (data->sq);
		g_free (data);
	}
}

gboolean
xmms_sqlite_new (xmms_medialib_t *medialib)
{
	sqlite *sql;
	xmms_sqlite_data_t *data;
	gchar dbpath[XMMS_PATH_MAX];
	gchar *err;
	const gchar *hdir;
	g_return_val_if_fail (medialib, FALSE);

	hdir = g_get_home_dir ();

	g_snprintf (dbpath, XMMS_PATH_MAX, "%s/.xmms2/medialib.db", hdir);

	sql = sqlite_open (dbpath, 0644, &err);
	if (!sql) {
		XMMS_DBG ("Error creating sqlite db: %s", err);
		free (err);
		return FALSE;
	}

	sqlite_exec (sql, "create table song (id int primary_key, uri, " 
			XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST ","
			XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM ","
			XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE ","
			XMMS_PLAYLIST_ENTRY_PROPERTY_YEAR ","
			XMMS_PLAYLIST_ENTRY_PROPERTY_TRACKNR ","
			XMMS_PLAYLIST_ENTRY_PROPERTY_GENRE ","
			XMMS_PLAYLIST_ENTRY_PROPERTY_BITRATE ","
			XMMS_PLAYLIST_ENTRY_PROPERTY_COMMENT ","
			XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION ", properties)", NULL, NULL, NULL);

	data = g_new0 (xmms_sqlite_data_t, 1);
	data->sq = sql;

	xmms_medialib_set_data (medialib, data);

	return TRUE;

}

static gchar *
add_to_query_str (gchar *query, gchar *str, gchar *field)
{
	gchar *tmp;
	gchar *ret;
	gchar *p;

	while ((p=strchr (str, '*')))
		*p='%';

	if (strlen (query) > 20) 
		tmp = g_strdup_printf ("AND %s LIKE '%s' ", field, str);
	else 
		tmp = g_strdup_printf ("WHERE %s LIKE '%s' ", field, str);

	XMMS_DBG ("Appendig '%s' to '%s'", tmp, query);

	ret = g_strconcat (query, tmp, NULL);

	g_free (tmp);
	g_free (query);

	return ret;
}

/** Takes a row in the database and splits adds it in a entry.
 * 
 * The format of the database is that they are stored value for value
 * if they are @sa well-knowns. Otherwise they are stored as a string
 * <encoded key>=<encoded value>¤<encoded key>=<encoded value>
 */

static int
xmms_sqlite_foreach (void *pArg, int argc, char **argv, char **columnName) 
{
	GList **list = pArg;
	xmms_playlist_entry_t *entry;
	gint i = 0;

	if (!argv) 
		return 0;

	entry = xmms_playlist_entry_new (NULL);
	while (columnName[i]) {

		if (!argv[i]) {
			i++;
			continue;
		}

		if (g_strcasecmp (columnName[i], "uri") == 0)
			xmms_playlist_entry_url_set (entry, argv[i]);
		
		if (g_strcasecmp (columnName[i], "title") == 0)
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE, argv[i]);

		if (g_strcasecmp (columnName[i], "artist") == 0)
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST, argv[i]);

		if (g_strcasecmp (columnName[i], "album") == 0)
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM, argv[i]);

		if (g_strcasecmp (columnName[i], "genre") == 0)
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_GENRE, argv[i]);

		if (g_strcasecmp (columnName[i], "comment") == 0)
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_COMMENT, argv[i]);

		if (g_strcasecmp (columnName[i], "bitrate") == 0)
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_BITRATE, argv[i]);
		
		if (g_strcasecmp (columnName[i], "duration") == 0)
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION, argv[i]);
		
		if (g_strcasecmp (columnName[i], "tracknr") == 0)
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TRACKNR, argv[i]);
		
		if (g_strcasecmp (columnName[i], "year") == 0)
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_YEAR, argv[i]);

		if (g_strcasecmp (columnName[i], "properties") == 0) {
			gchar **prop;
			gint j=0;

			prop = g_strsplit (argv[i], "¤", 0);

			if (!prop)
				continue;

			while (prop[j]) {
				gchar **val;
				gchar *key, *value;

				val = g_strsplit (prop[j], "=", 0);
				if (!val)
					continue;
				if (!val[0] || !val[1]) {
					g_strfreev (val);
					continue;
				}

				key = xmms_sqlite_korv_decode (val[0]);
				value = xmms_sqlite_korv_decode (val[1]);

				XMMS_DBG ("Adding %s=%s", key, value);
				xmms_playlist_entry_property_set (entry, key, value);

				g_free (key);
				g_free (value);

				g_strfreev (val);
				
				j++;
			}

			g_strfreev (prop);

			i++;
		}


		i++;
	}

	*list = g_list_append (*list, entry);

	return 0;
}

static void
xmms_sqlite_foreach_prop (gpointer key, gpointer value, gpointer udata)
{
	gchar **query = (gchar **)udata;

	*query = add_to_query_str (*query, value, key);

}

GList *
xmms_sqlite_search (xmms_medialib_t *medialib, xmms_playlist_entry_t *search)
{
	xmms_sqlite_data_t *data;
	gchar *query;
	GList *ret;
	gchar *err;
	gchar *uri;

	g_return_val_if_fail (medialib, NULL);
	g_return_val_if_fail (search, NULL);

	data = xmms_medialib_get_data (medialib);

	g_return_val_if_fail (data, NULL);

	uri = xmms_playlist_entry_url_get (search);

	if (uri) {
		gchar *p;

		while ((p=strchr (uri, '*')))
			*p='%';

		query = g_strdup_printf ("SELECT * FROM SONG WHERE URI LIKE \"%s\" ", uri);
	} else {
		query = g_strdup_printf ("SELECT * FROM SONG ");
	}

	xmms_playlist_entry_property_foreach (search, xmms_sqlite_foreach_prop, &query);
	
	XMMS_DBG ("Medialib query = %s", query);

	if (sqlite_exec (data->sq, query, xmms_sqlite_foreach, &ret, &err) != SQLITE_OK) {
		XMMS_DBG ("Query failed %s", err);
		g_free (query);
		return NULL;
	}

	g_free (query);

	return ret;
	
}


void
xmms_sqlite_add_foreach (gpointer key, gpointer value, gpointer udata)
{
	gchar **store = (gchar **)udata;

	if (xmms_playlist_entry_iswellknown ((gchar*)key)) {
	
		if (store[0]) {
			gchar *f = store[0];
			store[0] = g_strconcat (store[0], ", \"", (gchar *)value, "\"", NULL);
			g_free (f);
		} else {
			store[0] = g_strconcat ("\"", (gchar *)value, "\"", NULL);
		}

		if (store[1]) {
			gchar *f = store[1];
			store[1] = g_strconcat (store[1], ", ", (gchar *)key, NULL);
			g_free (f);
		} else {
			store[1] = g_strdup ((gchar *)key);
		}

	} else {
		gchar *tmp, *tmp2, *codedstr;

		tmp = xmms_sqlite_korv_encode_string ((gchar*)key);
		tmp2 = xmms_sqlite_korv_encode_string ((gchar*)value);

		codedstr = g_strdup_printf ("%s=%s", tmp, tmp2);

		g_free (tmp);
		g_free (tmp2);
		
		if (store[2]) {
			gchar *f = store[2];
			store[2] = g_strconcat (store[2], "¤", codedstr, NULL);
			g_free (f);
		} else {
			store[2] = g_strdup (codedstr);
		}

		g_free (codedstr);
	}



}


void
xmms_sqlite_add_entry (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry) 
{
	gchar *query, *keys, *values, *props, *err;
	xmms_sqlite_data_t *data;
	gchar **store = g_malloc0 (sizeof (gchar*)*3);

	g_return_if_fail (medialib);
	g_return_if_fail (entry);

	data = xmms_medialib_get_data (medialib);

	g_return_if_fail (data);

	/** @todo Fulhack */
	store[0] = NULL;
	store[1] = NULL;
	store[2] = NULL;
	
	xmms_playlist_entry_property_foreach (entry, xmms_sqlite_add_foreach, store);

	values = store[0];
	keys = store[1];
	props = store[2];

	if (!values || !keys)
		goto cleanup;

	if (props) {
		query = g_strdup_printf ("INSERT INTO SONG (uri, %s, properties) VALUES (\"%s\", %s, \"%s\")",
				keys, xmms_playlist_entry_url_get (entry),
				values, props);
	} else {
		query = g_strdup_printf ("INSERT INTO SONG (uri, %s) VALUES (\"%s\", %s)",
				keys,
				xmms_playlist_entry_url_get (entry),
				values);
	}

	XMMS_DBG ("Query = %s", query);

	if (sqlite_exec (data->sq, query, NULL, NULL, &err) != SQLITE_OK)
		XMMS_DBG ("Ouch! %s", err);


cleanup:
	if (values)
		g_free (values);
	if (props)
		g_free (props);
	if (keys)
		g_free (keys);

	g_free (store);

}

