/**
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "xmmsclient.h"

#define XMMS_MAX_URI_LEN 1024

static GMainLoop *mainloop;

void
handle_playtime (void *userdata, void *arg) {
	guint tme = GPOINTER_TO_UINT(arg);
	printf ("played time: %02d:%02d\r",
		tme/60000,
		(tme/1000)%60);
	fflush (stdout);
}

void
handle_mediainfo (void *userdata, void *arg) {
	guint id = GPOINTER_TO_UINT(arg);
	printf ("\nnow playing id: %d\n", id);
	fflush (stdout);
}

int
status_main(xmmsc_connection_t *conn)
{
	guint id;

	mainloop = g_main_loop_new (NULL, FALSE);

	xmmsc_set_callback (conn, XMMSC_CALLBACK_PLAYTIME_CHANGED, handle_playtime, NULL);
	xmmsc_set_callback (conn, XMMSC_CALLBACK_MEDIAINFO_CHANGED, handle_mediainfo, NULL);

	id = xmmsc_get_playing_id (conn);
	if (id) {
		GHashTable *entry;
		entry = xmmsc_playlist_get_mediainfo (conn, id);
		printf ("Playing %s\n", (gchar *)g_hash_table_lookup (entry, "uri"));
	}

	xmmsc_glib_setup_mainloop (conn, NULL);

	return 0;

}

#define streq(a,b) (strcmp(a,b)==0)

int
main(int argc, char **argv)
{
	xmmsc_connection_t *c;

	c = xmmsc_init ();

	if(!c){
		printf ("bad\n");
		exit (1);
	}

	if (!xmmsc_connect (c)){
		printf ("couldn't connect to xmms2d\n");
		exit (1);
	}

	if (argc<2 || streq (argv[1], "status")) {
		status_main (c);
	} else {

		if ( streq (argv[1], "next") ) {

			xmmsc_play_next (c);
			xmmsc_deinit (c);

			exit (0);

		} else if ( streq (argv[1], "shuffle") ) {

			xmmsc_playlist_shuffle (c);
			xmmsc_deinit (c);

			exit (0);

		} else if ( streq (argv[1], "jump") ) {

			if ( argc < 3 ) {
				printf ("usage: jump id\n");
				return 1;
			}

			xmmsc_playlist_jump (c, atoi (argv[2]));
			xmmsc_play_next (c);

			xmmsc_deinit (c);
			exit (0);
		} else if ( streq (argv[1], "remove") ) {
			int id;
			guint current = xmmsc_get_playing_id (c);

			if ( argc < 3 ) {
				printf ("usage: remove id\n");
				return 1;
			}

			id = atoi (argv[2]);

			if (id == current) {
				printf ("Can't remove playing song...\n");
				exit (0);
			}


			xmmsc_playlist_remove (c, id);
			xmmsc_deinit (c);
			exit (0);
		} else if ( streq (argv[1], "quit") ) {

			xmmsc_quit (c);
			xmmsc_deinit (c);

			exit(0);
		} else if ( streq (argv[1], "list") ) {

			GList *list;

			gint curr = xmmsc_get_playing_id (c);
			list = xmmsc_playlist_list (c);

			while (list) {
				xmmsc_playlist_entry_t *entry = list->data;
				printf ("%s%d\t%s\n",
					(curr==entry->id) ? "->":"  ",
					entry->id, entry->url);
				list = g_list_next (list);
			}
			xmmsc_deinit (c);
			
			exit (0);
		} else if ( streq (argv[1], "add") ) {
			int i;
			if ( argc < 3 ) {
				printf ("usage: add url [url...]\n");
				return 1;
			}
			for (i=2;i<argc;i++) {
				gchar nuri[XMMS_MAX_URI_LEN];
				if (!strchr (argv[i], ':')) {
					if (argv[i][0] == '/') {
						g_snprintf (nuri, XMMS_MAX_URI_LEN, "file://%s", argv[i]);
					} else {
						gchar *cwd = g_get_current_dir ();
						g_snprintf (nuri, XMMS_MAX_URI_LEN, "file://%s/%s", cwd, argv[i]);
						g_free (cwd);
					}
				} else {
					g_snprintf (nuri, XMMS_MAX_URI_LEN, "%s", argv[i]);
				}

				xmmsc_playlist_add (c, nuri);
			}
			xmmsc_deinit (c);
			exit (0);
		} else {
			printf ("Unknown command '%s'\n", argv[1]);
			return 1;
		}
	}
	g_main_loop_run (mainloop);
	printf ("...BOOM!\n");
	return 0;

}
