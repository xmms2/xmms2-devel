/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include "xmms2_client.h"

#include <locale.h>

/**
 * Utils
 */

static gchar *statusformat;
static gchar *listformat;

static gchar defaultconfig[] = "ipcpath=NULL\nstatusformat=${artist} - ${title}\nlistformat=${artist} - ${title} (${minutes}:${seconds})\n";

static GHashTable *config;

char *
format_url (char *item)
{
	char *url, rpath[PATH_MAX], *p;

	p = strchr (item, ':');
	if (!(p && p[1] == '/' && p[2] == '/')) {
		/* OK, so this is NOT an valid URL */

		if (!realpath (item, rpath))
			return NULL;

		if (!g_file_test (rpath, G_FILE_TEST_IS_REGULAR))
			return NULL;

		url = g_strdup_printf ("file://%s", rpath);
	} else {
		url = g_strdup_printf ("%s", item);
	}

	return url;
}


void
print_error (const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	
	va_start (ap, fmt);
	vsnprintf (buf, 1024, fmt, ap);
	va_end (ap);

	printf ("ERROR: %s\n", buf);

	exit (-1);
}

void
print_info (const char *fmt, ...)
{
	char buf[8096];
	va_list ap;
	
	va_start (ap, fmt);
	vsnprintf (buf, 8096, fmt, ap);
	va_end (ap);

	printf ("%s\n", buf);
}

void
print_hash (const void *key, xmmsc_result_value_type_t type, 
			const void *value, void *udata)
{
	if (type == XMMSC_RESULT_VALUE_TYPE_STRING) {
		printf ("%s = %s\n", (char *)key, (char *)value);
	} else {
		printf ("%s = %d\n", (char *)key, XPOINTER_TO_INT (value));
	}
}

static GHashTable *
read_config ()
{
	gchar file[PATH_MAX];
	gchar *buffer;
	gchar **split;
	gint read_bytes;
	FILE *fp;
	struct stat st;
	GHashTable *config;
	int i = 0;

	g_snprintf (file, sizeof (file), "%s/.xmms2/clients/cli.conf",
	            g_get_home_dir ());

	if (!g_file_test (file, G_FILE_TEST_EXISTS)) {
		gchar *dir = g_strdup_printf ("%s/.xmms2/clients", g_get_home_dir ());
		mkdir (dir, 0755);
		g_free (dir);
		fp = fopen (file, "w+");
		if (!fp) {
			print_error ("Could not create default configfile!!");
		}
		fwrite (defaultconfig, strlen (defaultconfig), 1, fp);
		fclose (fp);
	}

	if (!(fp = fopen (file, "r"))) {
		print_error ("Could not open configfile %s", file);
	}

	if (fstat (fileno (fp), &st) == -1) {
		perror ("fstat");
		return NULL;
	}

	buffer = g_malloc0 (st.st_size + 1);

	read_bytes = 0;
	while (read_bytes < st.st_size) {
		guint ret = fread (buffer + read_bytes, st.st_size - read_bytes, 1, fp);

		if (ret == 0) {
			break;
		}

		read_bytes += ret;
		g_assert (read_bytes >= 0);
	}

	config = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                g_free, g_free);

	split = g_strsplit (buffer, "\n", 0);

	while (split && split[i]) {
		gchar **s;

		if (!split[i])
			break;

		s = g_strsplit (split[i], "=", 2);
		if (!s)
			break;

		if (!s[0] || !s[1]) {
			g_strfreev (s);
			break;
		}

		if (g_strcasecmp (s[1], "NULL") == 0) {
			g_hash_table_insert (config, g_strdup (s[0]), NULL);
		} else {
			g_hash_table_insert (config, g_strdup (s[0]), g_strdup (s[1]));
		}

		g_strfreev (s);
		i++;
	}

	g_strfreev (split);
	g_free (buffer);

	return config;
}


void
format_pretty_list (xmmsc_connection_t *conn, GList *list) {
	uint count = 0;
	GList *n;
	gint mid;
	
	printf ("-[Result]-----------------------------------------------------------------------\n");
	printf ("Id   | Artist            | Album                     | Title\n");

	for (n = list; n; n = g_list_next (n)) {
		char *artist, *album, *title;
		xmmsc_result_t *res;
		mid = XPOINTER_TO_INT (n->data);

		if (mid) {
			res = xmmsc_medialib_get_info (conn, mid);
			xmmsc_result_wait (res);
		} else {
			print_error ("Empty result!");
		}

		xmmsc_result_get_dict_entry_str (res, "title", &title);
		if (title) {
			xmmsc_result_get_dict_entry_str (res, "artist", &artist);
			if (!artist)
				artist = "Unknown";

			xmmsc_result_get_dict_entry_str (res, "album", &album);
			if (!album)
				album = "Unknown";

			printf ("%-5.5d| %-17.17s | %-25.25s | %-25.25s\n", mid, artist, album, title);

		} else {
			char *url, *filename;
			xmmsc_result_get_dict_entry_str (res, "url", &url);
			if (url) {
				filename = g_path_get_basename (url);
				if (filename) {
					printf ("%-5.5d| %s\n", mid, filename);
					g_free (filename);
				}
			}
		}
		count++;
		xmmsc_result_unref (res);
	}
	printf ("-------------------------------------------------------------[Count:%6.d]-----\n", count);
}


/**
 * here comes all the cmd callbacks
 */

static void
add_item_to_playlist (xmmsc_connection_t *conn, char *item)
{
	xmmsc_result_t *res;
	char *url;

	url = format_url (item);
	if (!url) return;

	res = xmmsc_playlist_add (conn, url);
	g_free (url);

	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		printf ("something went wrong when adding it to the playlist\n");
		exit (-1);
	}

	print_info ("Adding %s", item);
	xmmsc_result_unref (res);
}

static void
add_directory_to_playlist (xmmsc_connection_t *conn, char *directory,
                           gboolean recursive)
{
	GDir *dir;
	GSList *entries = NULL;
	const char *entry;
	char buf[PATH_MAX];

	if (!(dir = g_dir_open (directory, 0, NULL))) {
		printf ("cannot open directory: %s\n", directory);
		return;
	}

	while ((entry = g_dir_read_name (dir))) {
		entries = g_slist_prepend (entries, strdup (entry));
	}

	g_dir_close (dir);

	/* g_dir_read_name() will return the entries in a undefined
	 * order, so sort the list now.
	 */
	entries = g_slist_sort (entries, (GCompareFunc) strcmp);

	while (entries) {
		g_snprintf (buf, sizeof (buf), "%s/%s", directory,
		          (char *) entries->data);

		if (g_file_test (buf, G_FILE_TEST_IS_DIR)) {
			if (recursive) {
				add_directory_to_playlist (conn, buf, recursive);
			}
		} else {
			add_item_to_playlist (conn, buf);
		}

		g_free (entries->data);
		entries = g_slist_delete_link (entries, entries);
	}
}

static void
cmd_addid (xmmsc_connection_t *conn, int argc, char **argv)
{
	int i;
	xmmsc_result_t *res;

	if (argc < 3) {
		print_error ("Need a medialib id to add");
	}

	for (i = 2; argv[i]; i++) {
		unsigned int id = atoi (argv[i]);
		if (id) {
			res = xmmsc_playlist_add_id (conn, id);
			xmmsc_result_wait (res);
			print_info ("Added medialib id %d to playlist", atoi(argv[i]));
			xmmsc_result_unref (res);
		}
	}
}

static void
cmd_addpls (xmmsc_connection_t *conn, int argc, char **argv)
{
	gint i;

	if (argc < 3) {
		print_error ("Need a playlist url to add");
	}

	for (i = 2; argv[i]; i++) {
		gchar *url;
		xmmsc_result_t *res;

		url = format_url (argv[i]);
		res = xmmsc_medialib_playlist_import (conn, "_xmms2cli", url);
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}

		xmmsc_result_unref (res);
		res = xmmsc_medialib_playlist_load (conn, "_xmms2cli");
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}
		print_info ("Added playlist %s", url);
		g_free (url);
	}



}

static void
cmd_main_status (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res = xmmsc_main_status (conn);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		print_error ("Woooops");
	}
	xmmsc_result_dict_foreach (res, print_hash, NULL);
	xmmsc_result_unref (res);
}

static void
cmd_add (xmmsc_connection_t *conn, int argc, char **argv)
{
	int i;

	if (argc < 3) {
		print_error ("Need a filename to add");
	}

	for (i = 2; argv[i]; i++) {
		add_item_to_playlist (conn, argv[i]);
	}
}

static void
cmd_radd (xmmsc_connection_t *conn, int argc, char **argv)
{
	int i;

	if (argc < 3)
		print_error ("Need a directory to add");

	for (i = 2; argv[i]; i++) {
		if (!g_file_test (argv[i], G_FILE_TEST_IS_DIR)) {
			printf ("not a directory: %s\n", argv[i]);
			continue;
		}

		add_directory_to_playlist (conn, argv[i], TRUE);
	}
}

static void
cmd_clear (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;

	res = xmmsc_playlist_clear (conn);
	xmmsc_result_wait (res);
	xmmsc_result_unref (res);

}

static void
cmd_shuffle (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	
	res = xmmsc_playlist_shuffle (conn);
	xmmsc_result_wait (res);
	xmmsc_result_unref (res);
	
}

static void
cmd_sort (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	
	if (argc != 3)
		print_error ("Sort needs a property to sort on, %d", argc);
	
	res = xmmsc_playlist_sort (conn, argv[2]);
	xmmsc_result_wait (res);
	xmmsc_result_unref (res);
}

int
cmp (const void *av, const void *bv)
{
	int result;
	int a = *(int *)av;
	int b = *(int *)bv;

	result = (a > b ? -1 : 1);

	if (a == b) 
		result = 0;

	return result;
}

static void
cmd_remove (xmmsc_connection_t *conn, int argc, char **argv)
{
	int i, j, size;
	xmmsc_result_t *res;
	int *sort = g_malloc (sizeof (int) * argc);

	if (argc < 3) {
		print_error ("Remove needs a ID to be removed");
	}

	i = 0;
	for (j = 2; j < argc; j++) {
		char *endptr = NULL;
		sort[i] = strtol (argv[j], &endptr, 10);
		if (endptr != argv[j]) i++;
	}
	size = i;

	qsort(sort, size, sizeof (int), &cmp);

	for (i = 0; i < size; i++) {
		int id = sort[i];
		res = xmmsc_playlist_remove (conn, id);
		xmmsc_result_wait (res);
		if (xmmsc_result_iserror (res)) {
			fprintf (stderr, "Couldn't remove %d (%s)\n", id, xmmsc_result_get_error (res));
		}
		xmmsc_result_unref (res);
	}
	
	g_free (sort);
}

static void
print_entry (const void *key, xmmsc_result_value_type_t type, 
			 const void *value, const char *source, void *udata)
{
	gchar *conv;
	gsize r, w;
	GError *err = NULL;

	if (type == XMMSC_RESULT_VALUE_TYPE_STRING) {
		conv = g_locale_from_utf8 (value, -1, &r, &w, &err);
		printf ("[%s] %s = %s\n", source, (char *)key, conv);
		g_free (conv);
	} else {
		printf ("[%s] %s = %d\n", source, (char *)key, XPOINTER_TO_INT (value));
	}

}

static void
cmd_info (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	guint id;

	if (argc > 2) {
		int cnt;

		for (cnt = 2; cnt < argc; cnt++) {
			id = strtoul (argv[cnt], (char**) NULL, 10);
			res = xmmsc_medialib_get_info (conn, id);
			xmmsc_result_wait (res);

			xmmsc_result_propdict_foreach (res, print_entry, NULL);
			xmmsc_result_unref (res);
		}

	} else {
		res = xmmsc_playback_current_id (conn);
		xmmsc_result_wait (res);
		if (!xmmsc_result_get_uint (res, &id))
			print_error ("Broken result");
		xmmsc_result_unref (res);
		
		res = xmmsc_medialib_get_info (conn, id);
		xmmsc_result_wait (res);
		xmmsc_result_propdict_foreach (res, print_entry, NULL);
		xmmsc_result_unref (res);
	}

}

static void
cmd_current (xmmsc_connection_t *conn, int argc, char **argv)
{ 
	xmmsc_result_t *res;
	gchar print_text[256];
	guint id;

	res = xmmsc_playback_current_id (conn);
	xmmsc_result_wait (res);
	if (!xmmsc_result_get_uint (res, &id))
		print_error ("Broken result");
	xmmsc_result_unref (res);

	res = xmmsc_medialib_get_info (conn, id);
	xmmsc_result_wait (res);

	if (argc > 2) {
	  xmmsc_entry_format (print_text, sizeof(print_text), argv[2], res);	
	} else {
	  xmmsc_entry_format (print_text, sizeof(print_text), 
		              "${artist} - ${title}", res);
	}

	printf("%s\n", print_text);
	xmmsc_result_unref (res);
}

static int
res_has_key (xmmsc_result_t *res, const char *key)
{
	return xmmsc_result_get_dict_entry_type (res, key) != XMMSC_RESULT_VALUE_TYPE_NONE;
}

static void
cmd_list (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	GError *err = NULL;
	gulong total_playtime = 0;
	guint p = 0;
	guint pos = 0;
	gsize r, w;

	res = xmmsc_playlist_current_pos (conn);
	xmmsc_result_wait (res);
	xmmsc_result_get_uint (res, &p);
	xmmsc_result_unref (res);

	res = xmmsc_playlist_list (conn);
	xmmsc_result_wait (res);

	for (; xmmsc_result_list_valid (res); xmmsc_result_list_next (res)) {
		char line[80];
		int playtime;
		gchar *conv;
		unsigned int ui;
		xmmsc_result_t *res2;

		if (!xmmsc_result_get_uint (res, &ui))
			print_error ("Broken result");

		g_clear_error (&err);

		res2 = xmmsc_medialib_get_info (conn, ui);
		xmmsc_result_wait (res2);

		xmmsc_result_get_dict_entry_int32 (res2, "duration", &playtime);
		total_playtime += playtime;
		
		if (res_has_key (res2, "channel")) {
			if (res_has_key (res2, "title")) {
				xmmsc_entry_format (line, sizeof (line), "[stream] ${title}", res2);
			} else {
				xmmsc_entry_format (line, sizeof (line), "${channel}", res2);
			}
		} else if (!res_has_key (res2, "title")) {
			char *url, *filename;
		  	char dur[10];
			
			xmmsc_entry_format (dur, sizeof (dur), "(${minutes}:${seconds})", res2);
			
			xmmsc_result_get_dict_entry_str (res2, "url", &url);
			if (url) {
				filename = g_path_get_basename (url);
				if (filename) {
					g_snprintf (line, sizeof (line), "%s %s", filename, dur);
					g_free (filename);
				} else {
					g_snprintf (line, sizeof (line), "%s %s", url, dur);
				}
			}
		} else {
			xmmsc_entry_format (line, sizeof(line), listformat, res2);
		}

		conv = g_locale_from_utf8 (line, -1, &r, &w, &err);

		if (p == pos) {
			print_info ("->[%d/%d] %s", pos, ui, conv);
		} else {
			print_info ("  [%d/%d] %s", pos, ui, conv);
		}

		pos ++ ;

		g_free (conv);

		if (err) {
			print_info ("convert error %s", err->message);
		}

		xmmsc_result_unref (res2);

	}

	xmmsc_result_unref (res);

	print_info ("\nTotal playtime: %d:%02d:%02d",
	            total_playtime / 3600000,
	            (total_playtime / 60000) % 60,
	            (total_playtime / 1000) % 60);

}
	
static void
cmd_play (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	res = xmmsc_playback_start (conn);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't start playback: %s\n", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}

static void
cmd_stop (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	res = xmmsc_playback_stop (conn);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't stop playback: %s\n", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

}

static void
cmd_pause (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	res = xmmsc_playback_pause (conn);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't pause playback: %s\n", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

}

static void
do_reljump (xmmsc_connection_t *conn, int where)
{
	xmmsc_result_t *res;

	res = xmmsc_playlist_set_next_rel (conn, where);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't advance in playlist: %s\n", xmmsc_result_get_error (res));
		return;
	}
	xmmsc_result_unref (res);

	res = xmmsc_playback_tickle (conn);
	xmmsc_result_wait (res);
	xmmsc_result_unref (res);
}

static void
cmd_next (xmmsc_connection_t *conn, int argc, char **argv)
{
	do_reljump (conn, 1);
}

static void
cmd_prev (xmmsc_connection_t *conn, int argc, char **argv)
{
	do_reljump (conn, -1);
}

static void
cmd_seek (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	guint id;
	guint pt = 0, ms;
	gint dur = 0;

	if (argc < 3) {
                print_error ("You need to specify a number of seconds. Usage:\n"
                             "xmms2 seek n  - seek to absolute position in song\n"
                             "xmms2 seek +n - seek n seconds forward in song\n"
                             "xmms2 seek -n - seek n seconds backwards");
        }

	res = xmmsc_playback_current_id (conn);
	xmmsc_result_wait (res);
	if (!xmmsc_result_get_uint (res, &id))
		print_error ("Broken result");
	xmmsc_result_unref (res);
	
	res = xmmsc_medialib_get_info (conn, id);
	xmmsc_result_wait (res);
	xmmsc_result_get_dict_entry_int32 (res, "duration", &dur);
	xmmsc_result_unref (res);

	if (argv[2][0] == '+' || argv[2][0] == '-') {
		res = xmmsc_playback_playtime (conn);
		xmmsc_result_wait (res);
		if (!xmmsc_result_get_uint (res, &pt))
			print_error ("Broken result");
		xmmsc_result_unref (res);
	}

	ms = pt + 1000 * atoi (argv[2]);

	if (dur && ms > dur) {
		printf ("Skipping to next song\n");
		do_reljump (conn, 1);
		return;
	}

	if (ms < 0)
		ms = 0;
	
	res = xmmsc_playback_seek_ms (conn, ms);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res))
		fprintf (stderr, "Couldn't seek to %d ms: %s\n", ms, xmmsc_result_get_error (res));
        xmmsc_result_unref (res);

}


static void
cmd_plugin_list (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	guint type;

	if (argc < 3) {
		type = XMMS_PLUGIN_TYPE_ALL;
	} else if (g_strcasecmp (argv[2], "output") == 0) {
		type = XMMS_PLUGIN_TYPE_OUTPUT;
	} else if (g_strcasecmp (argv[2], "transport") == 0) {
		type = XMMS_PLUGIN_TYPE_TRANSPORT;
	} else if (g_strcasecmp (argv[2], "decoder") == 0) {
		type = XMMS_PLUGIN_TYPE_DECODER;
	} else if (g_strcasecmp (argv[2], "effect") == 0) {
		type = XMMS_PLUGIN_TYPE_EFFECT;
	} else if (g_strcasecmp (argv[2], "playlist") == 0) {
		type = XMMS_PLUGIN_TYPE_PLAYLIST;
	} else {
		print_error ("no such plugin type!");
	}

	res = xmmsc_plugin_list (conn, type);
	xmmsc_result_wait (res);

	while (xmmsc_result_list_valid (res)) {
		gchar *name, *shortname, *desc;
		xmmsc_result_get_dict_entry_str (res, "name", &name);
		xmmsc_result_get_dict_entry_str (res, "shortname", &shortname);
		xmmsc_result_get_dict_entry_str (res, "description", &desc);

		printf ("%s - %s\n", shortname, desc);
		
		xmmsc_result_list_next (res);
	}

	xmmsc_result_unref (res);

}

static void
cmd_quit (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	res = xmmsc_quit (conn);
	xmmsc_result_wait (res);
	xmmsc_result_unref (res);
}

static void
cmd_config (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	char *key, *value;

	if (argc < 4) {
		print_error ("You need to specify a configkey and a value");
	}

	key = argv[2];

	if (strcmp (argv[3], "=") == 0)
		value = argv[4];
	else
		value = argv[3];

	if(value == NULL) {
		print_error ("You need to specify a configkey and a value");
	}

	res = xmmsc_configval_set (conn, key, value);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't set config value: %s\n", xmmsc_result_get_error (res));
	} else {
		print_info ("Config value %s set to %s", key, value);
	}
	xmmsc_result_unref (res);
}

static void
cmd_config_list (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;

	res = xmmsc_configval_list (conn);
	xmmsc_result_wait (res);
	
	for (; xmmsc_result_list_valid (res); xmmsc_result_list_next (res)) {
		xmmsc_result_t *res2;
		char *name, *value;
		xmmsc_result_get_string (res, &name);

		res2 = xmmsc_configval_get (conn, name);
		xmmsc_result_wait (res2);
		xmmsc_result_get_string (res2, &value);

		print_info ("%s = %s", name, value);

		xmmsc_result_unref (res2);

	}

	xmmsc_result_unref (res);
}

static void
cmd_move (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	unsigned int cur_pos, new_pos;

	if (argc < 4) {
		print_error ("You'll need to specifiy current and new position");
	}

	cur_pos = atoi (argv[2]);
	new_pos = atoi (argv[3]);

	res = xmmsc_playlist_move (conn, cur_pos, new_pos);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Unable to move playlist entry: %s\n", xmmsc_result_get_error (res));
		exit (-1);
	}

	print_info ("Moved %u to %u", cur_pos, new_pos);
	
}


static void
cmd_jump (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;

	if (argc < 3) {
		print_error ("You'll need to specify a ID to jump to.");
	}

	res = xmmsc_playlist_set_next (conn,  atoi (argv[2]));
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't jump to that song: %s\n", xmmsc_result_get_error (res));
		xmmsc_result_unref (res);
		return;
	}
	xmmsc_result_unref (res);

	res = xmmsc_playback_tickle (conn);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't go to next song: %s\n", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}

/* STATUS FUNCTIONS */

static gboolean has_songname;
static gchar songname[60];
static gint curr_dur;
static guint last_dur = 0;

static void
handle_playtime (xmmsc_result_t *res, void *userdata)
{
	guint dur;
	GError *err = NULL;
	gsize r, w;
	gchar *conv;
	xmmsc_result_t *newres;

	if (xmmsc_result_iserror (res)) {
		print_error ("apan");
		goto cont;
	}
	
	if (!xmmsc_result_get_uint (res, &dur)) {
		print_error ("korv");
		goto cont;
	}

	if(!has_songname)
		goto cont;

	if (((dur / 1000) % 60) == ((last_dur / 1000) % 60))
		goto cont;

	last_dur = dur;

	conv =  g_locale_from_utf8 (songname, -1, &r, &w, &err);
	printf ("\rPlaying: %s: %02d:%02d of %02d:%02d", conv,
	        dur / 60000, (dur / 1000) % 60, curr_dur / 60000,
	        (curr_dur / 1000) % 60);
	g_free (conv);

	fflush (stdout);

cont:
	newres = xmmsc_result_restart (res);
	xmmsc_result_unref (res);
	xmmsc_result_unref (newres);
}

static guint current_id;
static void do_mediainfo (xmmsc_connection_t *c, guint id);

static void
handle_current_id (xmmsc_result_t *res, void *userdata)
{
	if (!xmmsc_result_get_uint (res, &current_id))
		return;

	do_mediainfo ((xmmsc_connection_t *)userdata, current_id);
}

static void
handle_mediainfo_update (xmmsc_result_t *res, void *userdata)
{
	guint id;

	if (!xmmsc_result_get_uint (res, &id))
		return;

	if (id == current_id)
		do_mediainfo ((xmmsc_connection_t *)userdata, current_id);

}

static void
do_mediainfo (xmmsc_connection_t *c, guint id)
{
	xmmsc_result_t *res;

	res = xmmsc_medialib_get_info (c, id);
	xmmsc_result_wait (res);

	printf ("\n");
	if (res_has_key (res, "channel") && res_has_key (res, "title")) {
		xmmsc_entry_format (songname, sizeof (songname),
				    "[stream] ${title}", res);
		has_songname = TRUE;
	} else if (res_has_key (res, "channel")) {
		xmmsc_entry_format (songname, sizeof (songname),
				    "${title}", res);
		has_songname = TRUE;
	} else if (!res_has_key (res, "title")) {
		char *url, *filename;
		xmmsc_result_get_dict_entry_str (res, "url", &url);
		if (url) {
			filename = g_path_get_basename (url);
			if (filename) {
				g_snprintf (songname, sizeof (songname), "%s", filename);
				g_free (filename);
				has_songname = TRUE;
			}
		}
	} else {
		xmmsc_entry_format (songname, sizeof (songname),
				    statusformat, res);
		has_songname = TRUE;
	}

	xmmsc_result_get_dict_entry_int32 (res, "duration", &curr_dur);

	xmmsc_result_unref (res);
}

static void
quit (void *data)
{
	printf ("\nbye cruel world!\n");
	exit (0);
}

static void
cmd_status (xmmsc_connection_t *conn, int argc, char **argv)
{
	GMainLoop *ml;
	
	ml = g_main_loop_new (NULL, FALSE);

	has_songname = FALSE;

	/* Setup onchange signal for mediainfo */
	XMMS_CALLBACK_SET (conn, xmmsc_broadcast_playback_current_id, handle_current_id, conn);
	XMMS_CALLBACK_SET (conn, xmmsc_signal_playback_playtime, handle_playtime, NULL);
	XMMS_CALLBACK_SET (conn, xmmsc_playback_current_id, handle_current_id, conn);
	XMMS_CALLBACK_SET (conn, xmmsc_broadcast_medialib_entry_changed, handle_mediainfo_update, conn);
	xmmsc_disconnect_callback_set (conn, quit, NULL);
	xmmsc_mainloop_gmain_init (conn);

	g_main_loop_run (ml);
}

cmds commands[];

static void cmd_help (xmmsc_connection_t *conn, int argc, char **argv) {

	int i;
	if (argc == 2) {
		// print help message for all commands
		print_info ("Available commands:");
		for (i = 0; commands[i].name; i++) {
			print_info ("  %s - %s", commands[i].name, commands[i].help);
		}
	}
	else if (argc == 3) {
		// print help for specified command
		for (i = 0; commands[i].name; i++) {
			if (g_strcasecmp (commands[i].name, argv[2]) == 0) {
				print_info ("  %s - %s", commands[i].name, commands[i].help);
			}
		}
	}
}

/**
 * Defines all available commands.
 */

cmds commands[] = {
	/* Playlist managment */
	{ "add", "adds a URL to the playlist", cmd_add },
	{ "addid", "adds a Medialib id to the playlist", cmd_addid },
	{ "addpls", "adds a Playlist file to the current playlist", cmd_addpls },
	{ "radd", "adds a directory recursively to the playlist", cmd_radd },
	{ "clear", "clears the playlist", cmd_clear },
	{ "shuffle", "shuffles the playlist", cmd_shuffle },
	{ "sort", "sort the playlist", cmd_sort },
	{ "remove", "removes something from the playlist", cmd_remove },
	{ "list", "lists the playlist", cmd_list },
	
	/* Playback managment */
	{ "play", "starts playback", cmd_play },
	{ "stop", "stops playback", cmd_stop },
	{ "pause", "pause playback", cmd_pause },
	{ "next", "play next song", cmd_next },
	{ "prev", "play previous song", cmd_prev },
	{ "seek", "seek to a specific place in current song", cmd_seek },
	{ "jump", "take a leap in the playlist", cmd_jump },
	{ "move", "move a entry in the playlist", cmd_move },

	{ "mlib", "medialib manipulation - type 'xmms2 mlib' for more extensive help", cmd_mlib },

	{ "status", "go into status mode", cmd_status },
	{ "info", "information about current entry", cmd_info },
	{ "current", "formatted information about the current entry", cmd_current },
	{ "config", "set a config value", cmd_config },
	{ "configlist", "list all config values", cmd_config_list },
	{ "plugin_list", "list all plugins loaded in the server", cmd_plugin_list },
	{ "mainstats", "get status msg from serevr", cmd_main_status },
	/*{ "statistics", "get statistics from server", cmd_stats },
	 */
	{ "quit", "make the server quit", cmd_quit },
	{ "help", "print help about a command", cmd_help},

	{ NULL, NULL, NULL },
};

static void
free_config ()
{
	if (config)
		g_hash_table_destroy (config);
}

int
main (int argc, char **argv)
{
	xmmsc_connection_t *connection;
	char *path;
	int i;

	setlocale (LC_ALL, "");

	config = read_config ();
	atexit (free_config);

	statusformat = g_hash_table_lookup (config, "statusformat");
	listformat = g_hash_table_lookup (config, "listformat");

	connection = xmmsc_init ("xmms2-cli");

	if (!connection) {
		print_error ("Could not init xmmsc_connection!");
	}

	path = getenv ("XMMS_PATH");
	if (!path) {
		path = g_hash_table_lookup (config, "ipcpath");
	}

	if (!xmmsc_connect (connection, path)) {
		print_error ("Could not connect to xmms2d: %s", xmmsc_get_last_error (connection));
	}

	if (argc < 2) {

		xmmsc_unref (connection);


		print_info ("Available commands:");
		
		for (i = 0; commands[i].name; i++) {
			print_info ("  %s - %s", commands[i].name, commands[i].help);
		}
		

		exit (0);

	}

	for (i = 0; commands[i].name; i++) {

		if (g_strcasecmp (commands[i].name, argv[1]) == 0) {
			commands[i].func (connection, argc, argv);
			xmmsc_unref (connection);
			exit (0);
		}

	}

	xmmsc_unref (connection);
	
	print_error ("Could not find any command called %s", argv[1]);

	return -1;

}

