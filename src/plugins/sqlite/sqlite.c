#include "xmms/plugin.h"
#include "xmms/medialib.h"
#include "xmms/util.h"
#include "xmms/magic.h"
#include "xmms/playlist.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>

#include <sqlite.h>

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

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_MEDIALIB, "sqlite",
			"SQLite medialib " VERSION,
		 	"SQLite backend to medialib");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "URL", "http://www.sqlite.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_METHOD_NEW, xmms_sqlite_new);
	xmms_plugin_method_add (plugin, XMMS_METHOD_SEARCH, xmms_sqlite_search);

	return plugin;
}

/*
 * Member functions
 */

gboolean
xmms_sqlite_new (xmms_medialib_t *medialib)
{
	sqlite *sql;
	xmms_sqlite_data_t *data;
	gchar dbpath[XMMS_PATH_MAX];
	gchar *err;
	g_return_val_if_fail (medialib, FALSE);

	g_snprintf (dbpath, XMMS_PATH_MAX, "%s/.xmms2/medialib.db", g_get_home_dir ());

	sql = sqlite_open (dbpath, 0644, &err);
	if (!sql) {
		XMMS_DBG ("Error creating sqlite db: %s", err);
		free (err);
		return FALSE;
	}

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

static int
xmms_sqlite_foreach (void *pArg, int argc, char **argv, char **columnName) 
{
	GList *list = (GList *)pArg;
	xmms_playlist_entry_t *entry;
	gint i = 0;

	if (!argv) 
		return 0;

	entry = xmms_playlist_entry_new (NULL);
	while (columnName[i]) {

		if (g_strcasecmp (columnName[i], "uri") == 0)
			xmms_playlist_entry_set_uri (entry, argv[i]);
		
		if (g_strcasecmp (columnName[i], "title") == 0)
			xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_TITLE, argv[i]);

		if (g_strcasecmp (columnName[i], "artist") == 0)
			xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_ARTIST, argv[i]);

		if (g_strcasecmp (columnName[i], "album") == 0)
			xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_ALBUM, argv[i]);

		if (g_strcasecmp (columnName[i], "genre") == 0)
			xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_GENRE, argv[i]);

		if (g_strcasecmp (columnName[i], "comment") == 0)
			xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_COMMENT, argv[i]);

		if (g_strcasecmp (columnName[i], "bitrate") == 0)
			xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_BITRATE, argv[i]);
		
		if (g_strcasecmp (columnName[i], "duration") == 0)
			xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_DURATION, argv[i]);
		
		if (g_strcasecmp (columnName[i], "tracknr") == 0)
			xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_TRACKNR, argv[i]);
		
		if (g_strcasecmp (columnName[i], "year") == 0)
			xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_YEAR, argv[i]);

		i++;
	}

	g_list_append (list, entry);

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

	g_return_val_if_fail (medialib, NULL);
	g_return_val_if_fail (search, NULL);

	data = xmms_medialib_get_data (medialib);

	g_return_val_if_fail (data, NULL);

	query = g_strdup_printf ("SELECT * FROM SONG ");

	xmms_playlist_entry_foreach_prop (search, xmms_sqlite_foreach_prop, &query);
	
	XMMS_DBG ("Medialib query = %s", query);

	ret = g_list_alloc ();

	if (sqlite_exec (data->sq, query, xmms_sqlite_foreach, ret, &err) != SQLITE_OK) {
		XMMS_DBG ("Query failed %s", err);
		g_free (query);
		return NULL;
	}

	g_free (query);


	while (ret) {
		xmms_playlist_entry_t *t = (xmms_playlist_entry_t*)ret->data;
		if (t)
			xmms_playlist_entry_print (t);

		ret = g_list_next (ret);
	}

	return ret;
	
}
