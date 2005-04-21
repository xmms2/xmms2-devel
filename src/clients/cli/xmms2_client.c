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

#include <xmms/xmmsclient.h>
#include <xmms/xmmsclient-glib.h>
#include <xmms/signal_xmms.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>

typedef struct {
	char *name;
	char *help;
	void (*func) (xmmsc_connection_t *conn, int argc, char **argv);
} cmds;

/**
 * Utils
 */

static gchar *statusformat;
static gchar *listformat;

static gchar defaultconfig[] = "ipcpath=NULL\nstatusformat=${artist} - ${title}\nlistformat=${artist} - ${title} (${minutes}:${seconds})\n";

static char *
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
static void
print_hash (const void *key, const void *value, void *udata)
{
	printf ("%s = %s\n", (char *)key, (char *)value);
}

static GHashTable *
read_config ()
{
	gchar *file;
	gchar *buffer;
	gchar **split;
	gint read_bytes;
	FILE *fp;
	struct stat st;
	GHashTable *config;
	int i = 0;

	file = g_strdup_printf ("%s/.xmms2/clients/cli.conf", g_get_home_dir ());

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

	config = g_hash_table_new (g_str_hash, g_str_equal);

	split = g_strsplit (buffer, "\n", 0);
	while (split[i]) {
		if (!split[i])
			break;

		gchar **s = g_strsplit (split[i], "=", 2);
		if (!s || !s[0] || !s[1])
			break;
		if (g_strcasecmp (s[1], "NULL") == 0) {
			g_hash_table_insert (config, s[0], NULL);
		} else {
			g_hash_table_insert (config, s[0], s[1]);
		}

		i++;
	}

	g_free (buffer);
	return config;
	
}


static void
format_pretty_list (xmmsc_connection_t *conn, x_list_t *list) {
	uint count = 0;
	x_list_t *n;
	x_hash_t *e = NULL;
	char *mid;

	printf ("-[Result]-----------------------------------------------------------------------\n");
	printf ("Id   | Artist            | Album                     | Title\n");

	for (n = list; n; n = x_list_next (n)) {
		char *artist, *album, *title;
		xmmsc_result_t *res;
		mid = n->data;

		if (mid) {
			res = xmmsc_medialib_get_info (conn, atoi (mid));
			xmmsc_result_wait (res);
			if (!xmmsc_result_get_hashtable (res, &e))
				print_error ("broken result!");
		} else {
			print_error ("Empty result!");
		}

		title = x_hash_lookup (e, "title");
		if (title) {
			artist = x_hash_lookup (e, "artist");
			if (!artist)
				artist = "Unknown";

			album = x_hash_lookup (e, "album");
			if (!album)
				album = "Unknown";

			printf ("%-5.5s| %-17.17s | %-25.25s | %-25.25s\n", mid, artist, album, title);

		} else {
			char *url, *filename;
			url = x_hash_lookup (e, "url");
			if (url) {
				filename = g_path_get_basename (url);
				if (filename) {
					printf ("%-5.5s| %s\n", mid, filename);
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
cmd_mlib (xmmsc_connection_t *conn, int argc, char **argv)
{

	static char *mlibHelp = "Available medialib commands:\n\
search [artist=Dismantled]\n\
searchadd [artist=Dismantled]\n\
query [\"raw sql statment\"]\n\
save_playlist [playlistname]\n\
load_playlist [playlistname]\n\
list_playlist [playlistname]\n\
add [url]";

	if (argc < 3) {
		print_info (mlibHelp);
		return;
	}

	if (g_strcasecmp (argv[2], "add") == 0) {
		xmmsc_result_t *res;
		gint i;

		for (i = 3; argv[i]; i++) {
			char *url = format_url (argv[i]);
			if (!url) return;
			res = xmmsc_medialib_add_entry (conn, url);
			free (url);
			xmmsc_result_wait (res);
			printf ("Added %s to medialib\n", argv[i]);
			xmmsc_result_unref (res);
		}
	} else if (g_strcasecmp (argv[2], "searchadd") == 0) {
		xmmsc_result_t *res;
		char query[1024];
		char **s;
		
		s = g_strsplit (argv[3], "=", 0);

		if (!s[0] || !s[1])
			print_error ("key=value");

		g_snprintf (query, 1023, "select id from Media where key='%s' and value='%s'",s[0],s[1]);
		print_info ("%s", query);
		res = xmmsc_medialib_add_to_playlist (conn, query);
		xmmsc_result_wait (res);
		xmmsc_result_unref (res);
	} else if (g_strcasecmp (argv[2], "search") == 0) {
		char query[1024];
		char **s;
		x_list_t *l;
		xmmsc_result_t *res;
		x_hash_t *e;
		x_list_t *n = NULL;

		if (argc < 4) {
			print_error ("Supply a select statement");
		}

		s = g_strsplit (argv[3], "=", 0);
		
		g_snprintf (query, sizeof (query), "SELECT id FROM Media WHERE key='%s' and value='%s'", s[0], s[1]);
	
		res = xmmsc_medialib_select (conn, query);
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}

		if (!xmmsc_result_get_hashlist (res, &l)) {
			print_error ("Broken resultset...");
		}

		for (l = l; l; l = x_list_next (l)) {
			char *id;
			e = l->data;
			
			id = x_hash_lookup (e, "id");
			if (!id)
				print_error ("broken resultset");

			n = x_list_prepend (n, id);

		}

		format_pretty_list (conn, n);
		x_list_free (n);

	} else if (g_strcasecmp (argv[2], "query") == 0) {
		xmmsc_result_t *res;

		if (argc < 4) {
			print_error ("Supply a query");
		}
		
		res = xmmsc_medialib_select (conn, argv[3]);
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}
		
		{
			x_list_t *l, *n;
			x_hash_t *e;

			if (!xmmsc_result_get_hashlist (res, &l)) {
				print_error ("Broken resultset...");
			}

			for (n = l; n; n = x_list_next (n)) {
				e = n->data;
				x_hash_foreach (e, print_hash, NULL);
			}
			
		}

		xmmsc_result_unref (res);
	} else if (g_strcasecmp (argv[2], "list_playlist") == 0) {
		char query[1024];
		x_list_t *l, *n = NULL;
		x_hash_t *e;
		char *id;
		xmmsc_result_t *res;

		if (argc < 4) {
			print_error ("Supply a playlist name");
		}

		g_snprintf (query, sizeof (query), "SELECT id FROM Playlist WHERE name='%s'", argv[3]);

		res = xmmsc_medialib_select (conn, query);
		xmmsc_result_wait (res);

		if (!xmmsc_result_get_hashlist (res, &l))
			print_error ("Broken resultset!");

		e = l ? l->data : NULL;
		if (!e) 
			print_error ("No such playlist!");

		id = x_hash_lookup (e, "id");
		if (!id)
			print_error ("broken resultset!");
		
		g_snprintf (query, sizeof (query), "SELECT entry FROM Playlistentries WHERE playlist_id = %s", id);
		xmmsc_result_unref (res);

		res = xmmsc_medialib_select (conn, query);
		xmmsc_result_wait (res);
		
		if (!xmmsc_result_get_hashlist (res, &l))
			print_error ("Broken resultset!");

		for (l = l; l; l = x_list_next (l)) {
			gchar *entry;
			e = l->data;
			
			entry = x_hash_lookup (e, "entry");
			if (!entry)
				print_error ("broken resultset");

			if (g_strncasecmp (entry, "mlib", 4) == 0) {
				char *p = entry+7;
				n = x_list_prepend (n, p);
			}

		}

		n = x_list_reverse (n);
		format_pretty_list (conn, n);
		x_list_free (n);

	} else if (g_strcasecmp (argv[2], "save_playlist") == 0 ||
	           g_strcasecmp (argv[2], "load_playlist") == 0) {
		xmmsc_result_t *res;

		if (argc < 4) {
			print_error ("Supply a playlist name");
		}

		if (g_strcasecmp (argv[2], "save_playlist") == 0) {
			res = xmmsc_medialib_playlist_save_current (conn, argv[3]);
		} else if (g_strcasecmp (argv[2], "load_playlist") == 0) {
			res = xmmsc_medialib_playlist_load (conn, argv[3]);
		}

		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}

		xmmsc_result_unref (res);
	} else if (g_strcasecmp (argv[2], "import_playlist") == 0) {
		xmmsc_result_t *res;
		char *url;

		if (argc < 5) {
			print_error ("Supply a playlist name and url");
		}
	
		url = format_url (argv[4]);

		res = xmmsc_medialib_playlist_import (conn, argv[3], url);

		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}

		xmmsc_result_unref (res);
	} else if (g_strcasecmp (argv[2], "export_playlist") == 0) {
		xmmsc_result_t *res;
		char *file;
		char *mime;

		if (argc < 5) {
			print_error ("Supply a playlist name and a mimetype");
		}

		if (strcasecmp (argv[4], "m3u") == 0) {
			mime = "audio/mpegurl";
		} else if (strcasecmp (argv[4], "pls") == 0) {
			mime = "audio/x-scpls";
		} else if (strcasecmp (argv[4], "html") == 0) {
			mime = "text/html";
		} else {
			mime = argv[4];
		}

		res = xmmsc_medialib_playlist_export (conn, argv[3], mime);

		xmmsc_result_wait (res);
		
		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}

		if (!xmmsc_result_get_string (res, &file))
			print_error ("broken resultset!");

		fwrite (file, strlen (file), 1, stdout);

		xmmsc_result_unref (res);
	
	} else if (g_strcasecmp (argv[2], "addpath") == 0) {
		xmmsc_result_t *res;
		if (argc < 4) {
			print_error ("Supply a path to add!");
		}
		res = xmmsc_medialib_path_import (conn, argv[3]);
		xmmsc_result_wait (res);
		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}
		xmmsc_result_unref (res);
	} else {
		print_info (mlibHelp);
		print_error ("Unrecognised mlib command: %s\n", argv[2]);
	}
}

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
			printf ("not a directoy: %s\n", argv[i]);
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
cmd_remove (xmmsc_connection_t *conn, int argc, char **argv)
{
	int i;
	xmmsc_result_t *res;

	if (argc < 3) {
		print_error ("Remove needs a ID to be removed");
	}

	for (i = 2; argv[i]; i++) {
		res = xmmsc_playlist_remove (conn, atoi (argv[i]));
		xmmsc_result_wait (res);
		if (xmmsc_result_iserror (res)) {
			fprintf (stderr, "Couldn't remove %d (%s)\n", atoi (argv[i]), xmmsc_result_get_error (res));
		}
		xmmsc_result_unref (res);
	}
}

static void
print_entry (const void *key, const void *value, void *udata)
{
	printf ("%s = %s\n", (char *)key, (char *)value);
}

static void
cmd_info (xmmsc_connection_t *conn, int argc, char **argv)
{
	x_hash_t *entry;
	int id;

	if (argc > 2) {
		int cnt;

		for (cnt = 2; cnt < argc; cnt++) {
			id = strtoul (argv[cnt], (char**) NULL, 10);
			entry = xmmscs_medialib_get_info (conn, id);

			if (entry != NULL) {
				x_hash_foreach (entry, print_entry, NULL);
				x_hash_destroy (entry);
			}
		}

	} else {
		id = xmmscs_playback_current_id (conn);
		entry = xmmscs_medialib_get_info (conn, id);

		if (entry) {
			x_hash_foreach (entry, print_entry, NULL);
			x_hash_destroy (entry);
		}
	}

}

static void
cmd_list (xmmsc_connection_t *conn, int argc, char **argv)
{
	x_list_t *list;
	x_list_t *l;
	GError *err = NULL;
	gulong total_playtime = 0;
	guint p;
	guint pos = 0;
	gsize r, w;

	list = xmmscs_playlist_list (conn);

	if (!list)
		return;

	p = xmmscs_playlist_current_pos (conn);

	for (l = list; l; l = x_list_next (l)) {
		x_hash_t *tab;
		char line[80];
		const char *playtime;
		gchar *conv;
		unsigned int i = XPOINTER_TO_UINT (l->data);

		g_clear_error (&err);

		tab = xmmscs_medialib_get_info (conn, i);

		playtime = x_hash_lookup (tab, "duration");
		if (playtime) {
			total_playtime += strtoul (playtime, NULL, 10);
		}

		if (x_hash_lookup (tab, "channel") && x_hash_lookup (tab, "title")) {
			xmmsc_entry_format (line, sizeof (line), "[stream] ${title}", tab);
		} else if (x_hash_lookup (tab, "channel") && !x_hash_lookup (tab, "title")) {
			xmmsc_entry_format (line, sizeof (line), "${channel}", tab);
		} else if (!x_hash_lookup (tab, "title")) {
			char *url, *filename;
		  	char dur[10];
			
			xmmsc_entry_format (dur, sizeof (dur), "(${minutes}:${seconds})", tab);
			
			url = x_hash_lookup (tab, "url");
			if (url) {
				filename = g_path_get_basename (url);
				if (filename) {
					g_snprintf (line, sizeof (line), "%s %s", filename, dur);
					g_free (filename);
				}
			}
		} else {
			xmmsc_entry_format (line, sizeof(line), listformat, tab);
		}

		conv = g_convert (line, -1, "ISO-8859-1", "UTF-8", &r, &w, &err);

		if (p == pos) {
			print_info ("->[%d/%d] %s", pos, i, conv);
		} else {
			print_info ("  [%d/%d] %s", pos, i, conv);
		}

		pos ++ ;

		g_free (conv);

		if (err) {
			print_info ("convert error %s", err->message);
		}
		x_hash_destroy (tab);	
	}

	print_info ("\nTotal playtime: %d:%02d:%02d",
	            total_playtime / 3600000,
	            (total_playtime / 60000) % 60,
	            (total_playtime / 1000) % 60);

	free (list);
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
	int id,seconds,duration,cur_playtime;
	x_hash_t *lista;
	xmmsc_result_t *res;

	if (argc < 3) {
		print_error ("You need to specify a number of seconds. Usage:\n"
			     "xmms2 seek seconds  - will seek to seconds\n"
			     "xmms2 seek +seconds - will add seconds\n"
			     "xmms2 seek -seconds - will remove seconds");
	}
	
	id = xmmscs_playback_current_id (conn);
	lista = xmmscs_medialib_get_info (conn, id);
	duration = atoi (x_hash_lookup (lista, "duration"));
	cur_playtime = xmmscs_playback_playtime (conn);
	x_hash_destroy (lista);

	if (!duration)
		duration = 0;

	if (argv[2][0] == '+') {
		
		seconds=(atoi (argv[2]+1) * 1000) + cur_playtime;

		if (seconds >= duration) {
			printf ("Trying to seek to a higher value then total_playtime, Skipping to next song\n");
			do_reljump (conn, 1);
		} else {
			printf ("Adding %s seconds to stream and jumping to %d\n",argv[2]+1, seconds/1000);
		}
		
	} else if (argv[2][0] == '-') {
		seconds = cur_playtime - atoi (argv[2]+1) * 1000;
		
		if (seconds < 0) {
			printf ("Trying to seek to a non positive value, seeking to 0\n");
			seconds=0;
		} else {
			printf ("Removing %s seconds to stream and jumping to %d\n",argv[2]+1, seconds/1000);
		}
		
	} else {
		seconds = atoi (argv[2]) * 1000;
	}
	
	res = xmmsc_playback_seek_ms (conn, seconds);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't seek to that position: %s\n", xmmsc_result_get_error (res));
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

	if (argc < 4) {
		print_error ("You need to specify a configkey and a value");
	}

	res = xmmsc_configval_set (conn, argv[2], argv[3]);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't set config value: %s\n", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}

static void
cmd_config_list (xmmsc_connection_t *conn, int argc, char **argv)
{
	x_list_t *l;

	l = xmmscs_configval_list (conn);
	
	if (!l)
		print_error ("Ooops :%s", xmmsc_get_last_error (conn));
	
	print_info ("Config Values:");
	
	while (l) {
		char *val = xmmscs_configval_get (conn, l->data);

		print_info ("%s = %s", l->data, val);
		g_free (val);
		l = x_list_next (l);
	}
	
	x_list_free (l);
}

static void
cmd_save_playlist (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;

	if (argc < 3) {
		print_error ("Need a filename to save playlist");
		return;
	}

	res = xmmsc_playlist_save (conn, argv[2]);

	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Unable to save playlist to file: %s\n", xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

static void
cmd_move (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	unsigned int id;
	signed int movement;

	if (argc < 4) {
		print_error ("You'll need to specifiy id and movement");
	}

	id = atoi (argv[2]);
	movement = atoi (argv[3]);

	res = xmmsc_playlist_move (conn, id, movement);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Unable to move playlist entry: %s\n", xmmsc_result_get_error (res));
		exit (-1);
	}

	print_info ("Moved %u, %d steps", id, movement);
	
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

	if (((dur / 1000) % 60) == ((last_dur / 1000) % 60))
		goto cont;

	last_dur = dur;

	conv =  g_convert (songname, -1, "ISO-8859-1", "UTF-8", &r, &w, &err);
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
	x_hash_t *hash;
	gchar *tmp;

	hash = xmmscs_medialib_get_info (c, id);

	if (!hash) {
		printf ("no mediainfo!\n");
	} else {
		printf ("\n");
		if (x_hash_lookup (hash, "channel") && x_hash_lookup (hash, "title")) {
			xmmsc_entry_format (songname, sizeof (songname),
					    "[stream] ${title}", hash);
		} else if (x_hash_lookup (hash, "channel")) {
			xmmsc_entry_format (songname, sizeof (songname),
					    "${title}", hash);
		} else if (!x_hash_lookup (hash, "title")) {
			char *url, *filename;
			url = x_hash_lookup (hash, "url");
			if (url) {
				filename = g_path_get_basename (url);
				if (filename) {
					g_snprintf (songname, sizeof (songname), "%s", filename);
					g_free (filename);
				}
			}
		} else {
			xmmsc_entry_format (songname, sizeof (songname),
					    statusformat, hash);
		}
		tmp = x_hash_lookup (hash, "duration");
		if (tmp)
			curr_dur = atoi (tmp);
		else
			curr_dur = 0;

		x_hash_destroy (hash);
	}
}

static void
quit (void *data)
{
	printf ("bye crule world!\n");
	exit (0);
}

static void
cmd_status (xmmsc_connection_t *conn, int argc, char **argv)
{
	GMainLoop *ml;
	
	ml = g_main_loop_new (NULL, FALSE);

	/* Setup onchange signal for mediainfo */
	XMMS_CALLBACK_SET (conn, xmmsc_broadcast_playback_current_id, handle_current_id, conn);
	XMMS_CALLBACK_SET (conn, xmmsc_signal_playback_playtime, handle_playtime, NULL);
	XMMS_CALLBACK_SET (conn, xmmsc_playback_current_id, handle_current_id, conn);
	XMMS_CALLBACK_SET (conn, xmmsc_broadcast_medialib_entry_changed, handle_mediainfo_update, conn);

	xmmsc_disconnect_callback_set (conn, quit, NULL);
	xmmsc_ipc_setup_with_gmain (conn);

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
	{ "radd", "adds a directory recursively to the playlist", cmd_radd },
	{ "clear", "clears the playlist and stops playback", cmd_clear },
	{ "shuffle", "shuffles the playlist", cmd_shuffle },
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
	{ "save", "save the playlist", cmd_save_playlist },
	{ "move", "move a entry in the playlist", cmd_move },

	{ "mlib", "medialib manipulation", cmd_mlib },

	{ "status", "go into status mode", cmd_status },
	{ "info", "information about current entry", cmd_info },
	{ "config", "set a config value", cmd_config },
	{ "configlist", "list all config values", cmd_config_list },
	/*{ "statistics", "get statistics from server", cmd_stats },
	 */
	{ "quit", "make the server quit", cmd_quit },
	{ "help", "print help about a command", cmd_help},

	{ NULL, NULL, NULL },
};

int
main (int argc, char **argv)
{
	xmmsc_connection_t *connection;
	GHashTable *config;
	char *path;
	int i;

	config = read_config ();

	statusformat = g_hash_table_lookup (config, "statusformat");
	listformat = g_hash_table_lookup (config, "listformat");

	connection = xmmsc_init ("XMMS2 CLI");

	if (!connection) {
		print_error ("Could not init xmmsc_connection, this is a memory problem, fix your os!");
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

