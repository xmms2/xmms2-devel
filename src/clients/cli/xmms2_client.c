/**
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "xmmsclient.h"
#include "xmms/signal_xmms.h"

#define XMMS_MAX_URI_LEN 1024
#define STOPPED 1
#define PLAYING 2

static GMainLoop *mainloop;
gint duration;
gint status;
guint currentid;

void
print_mediainfo (GHashTable *entry)
{
	/*entry = xmmsc_playlist_get_mediainfo (conn, id);*/

	if (!entry)
		return;

	printf ("URI:    %s\n", (gchar *)g_hash_table_lookup (entry, "uri"));
	printf ("Artist: %-30s ", (gchar *)g_hash_table_lookup (entry, "artist"));
	printf ("Album: %-30s\n", (gchar *)g_hash_table_lookup (entry, "album"));
	printf ("Title:  %-30s ", (gchar *)g_hash_table_lookup (entry, "title"));
	printf ("Year: %s\n", (gchar *)g_hash_table_lookup (entry, "date"));

}

void
handle_playlist_mediainfo (void *userdata, void *arg)
{
	GHashTable *tab = (GHashTable *)arg;
	guint id;

	if (!arg)
		return;


	id = GPOINTER_TO_UINT (g_hash_table_lookup (tab, "id"));
	
	printf ("Got mediainfo msg for %d, current id is %d\n", id, currentid);
	
	if (id == currentid) {
		gchar *d;

		d = (gchar *)g_hash_table_lookup (tab, "duration");

		if (d) 
			duration = atoi (d);
		else
			duration = 0;

		print_mediainfo (tab);
	}

	xmmsc_playlist_entry_free (tab);
}

void
handle_information (void *userdata, void *arg) 
{
	gchar *message = arg;

	if (message) 
		printf ("Information: %s\n", message);
}

void
handle_playtime (void *userdata, void *arg) {
	guint tme = GPOINTER_TO_UINT(arg);
	if (status == STOPPED) 
		return;
	printf ("played time: %02d:%02d of %02d:%02d\r",
		tme/60000,
		(tme/1000)%60,
		duration/60000,
		(duration/1000)%60);
	fflush (stdout);
}

void
handle_playback_stopped (void *userdata, void *arg) 
{
	printf ("\nPlayback stopped...\n");
	status = STOPPED;
}

void
handle_disconnected (void *udata, void *arg)
{
	g_main_loop_quit (mainloop);
}

void
handle_currentid (void *userdata, void *arg) {
	guint id = GPOINTER_TO_UINT(arg);
	xmmsc_connection_t *conn = userdata;

	xmmsc_playlist_get_mediainfo (conn, id);

	currentid = id;

	status = PLAYING;

	fflush (stdout);
}

static guint lastid;

void
handle_playlist_list_mediainfo (xmmsc_connection_t *conn, void *arg)
{
	GHashTable *entry = (GHashTable *) arg;
	gchar *artist;
	gchar *title;
	gchar *str;
	gchar *uri;
	gchar *duration;
	guint id;
	guint tme;

	id = GPOINTER_TO_UINT (g_hash_table_lookup (entry, "id"));
	artist = (gchar *)g_hash_table_lookup (entry, "artist");
	title = (gchar *)g_hash_table_lookup (entry, "title");
	uri = (gchar *)g_hash_table_lookup (entry, "uri");
	duration = (gchar *)g_hash_table_lookup (entry, "duration");

	if (tme)
		tme = atoi (duration);

	duration = g_strdup_printf ("%02d:%02d", tme/60000, (tme/1000)%60);

	if (artist && title) {
		str = g_strdup_printf ("%s - %s", artist, title);
	} else {
		str = strrchr (uri, '/');
		if (!str)
			str = uri;
		else
			str++;

		str = xmmsc_decode_path (str);
	}
		
	printf ("%d\t%s (%s)\n",
		id, str, duration);

	g_free (duration);
	g_free (str);

	if (id == lastid) {
		xmmsc_deinit (conn);
		exit (0);
	}
}

void
handle_playlist_list (xmmsc_connection_t *conn, void *arg)
{
	guint32 *list=arg;
	gint i=0;

	while (list[i]) {
		xmmsc_playlist_get_mediainfo (conn, GPOINTER_TO_UINT(list[i]));
		i++;
	}
	lastid = GPOINTER_TO_UINT(list[--i]);
}

void
setup_playlist (xmmsc_connection_t *conn)
{
	mainloop = g_main_loop_new (NULL, FALSE);

	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_LIST, handle_playlist_list, conn);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_MEDIAINFO, handle_playlist_list_mediainfo, conn);
	
	xmmsc_glib_setup_mainloop (conn, NULL);

	return;
}

int
status_main(xmmsc_connection_t *conn)
{
	mainloop = g_main_loop_new (NULL, FALSE);

	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYBACK_PLAYTIME, handle_playtime, NULL);
	xmmsc_set_callback (conn, XMMS_SIGNAL_CORE_INFORMATION, handle_information, NULL);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYBACK_CURRENTID, handle_currentid, conn);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYBACK_STOP, handle_playback_stopped, conn);
	xmmsc_set_callback (conn, XMMS_SIGNAL_CORE_DISCONNECT, handle_disconnected, conn);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_MEDIAINFO, handle_playlist_mediainfo, conn);

	xmmsc_get_playing_id (conn);

	xmmsc_glib_setup_mainloop (conn, NULL);

	return 0;

}


#define streq(a,b) (strcmp(a,b)==0)

int
main(int argc, char **argv)
{
	xmmsc_connection_t *c;

	duration = 0;

	c = xmmsc_init ();

	if(!c){
		printf ("bad\n");
		exit (1);
	}

	if (!xmmsc_connect (c)){
		printf ("couldn't connect to xmms2d: %s\n",xmmsc_get_last_error(c));
		exit (1);
	}

	if (argc<2 || streq (argv[1], "status")) {
		status_main (c);
	} else {

		if ( streq (argv[1], "next") ) {

			xmmsc_play_next (c);
			xmmsc_deinit (c);

			exit (0);

		} else if ( streq (argv[1], "prev") ) {

			xmmsc_play_prev (c);
			xmmsc_deinit (c);

			exit (0);


		} else if ( streq (argv[1], "shuffle") ) {

			xmmsc_playlist_shuffle (c);
			xmmsc_deinit (c);

			exit (0);

		} else if ( streq (argv[1], "clear") ) {

			xmmsc_playlist_clear (c);
			xmmsc_deinit (c);

			exit (0);

		} else if ( streq (argv[1], "stop") ) {

			xmmsc_playback_stop (c);
			xmmsc_deinit (c);

			exit (0);

		} else if ( streq (argv[1], "play") ) {

			xmmsc_playback_start (c);
			xmmsc_deinit (c);

			exit (0);


		} else if ( streq (argv[1], "jump") ) {

			if ( argc < 3 ) {
				printf ("usage: jump id\n");
				return 1;
			}

			xmmsc_playlist_jump (c, atoi (argv[2]));

			xmmsc_deinit (c);
			exit (0);
		} else if ( streq (argv[1], "seek") ) {
			guint seconds;

			if ( argc < 3 ) {
				printf ("usage: seek seconds\n");
				return 1;
			}

			seconds = atoi (argv[2]) * 1000;

			xmmsc_playback_seek (c, seconds);

			xmmsc_deinit (c);
			exit (0);
		} else if ( streq (argv[1], "remove") ) {
			int id;

			if ( argc < 3 ) {
				printf ("usage: remove id\n");
				return 1;
			}

			id = atoi (argv[2]);

			if (id == currentid) {
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
		} else if ( streq (argv[1], "info") ) {
			gint id;

			if (argc < 3)
				id = currentid;
			else
				id = atoi (argv[2]);

			xmmsc_playlist_get_mediainfo (c, id);

			xmmsc_deinit (c);

			exit(0);
		} else if ( streq (argv[1], "config") ) {
			gint id;

			xmmsc_configval_set (c, argv[2], argv[3]);

			xmmsc_deinit (c);

			exit(0);
		} else if ( streq (argv[1], "list") ) {

			xmmsc_playlist_list (c);

			setup_playlist (c);

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

				xmmsc_playlist_add (c, xmmsc_encode_path (nuri));
			}
			xmmsc_deinit (c);
			exit (0);
		} else {
			printf ("Unknown command '%s'\n", argv[1]);
			return 1;
		}
	}
	g_main_loop_run (mainloop);
	if (c)
		xmmsc_deinit (c);
	printf ("\n...BOOM!\n");
	return 0;

}
