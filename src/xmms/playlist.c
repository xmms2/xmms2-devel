
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>
#include <db.h>

#include "playlist.h"


/*
 * Public functions
 */

gboolean
xmms_playlist_add (xmms_playlist_t *playlist, xmms_playlist_entry_t *file, gint options)
{
	DBT key, data;
	int ret;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = &file->uri;
	key.size = strlen (file->uri);

	data.data = file;
	data.size = sizeof (xmms_playlist_entry_t);

	switch (options) {
		case XMMS_PL_APPEND:
		{
			if ((ret = playlist->dbp->put (playlist->dbp, 
							NULL,
							&key,
							&data,
							DB_APPEND)) != 0) {
				playlist->dbp->err (playlist->dbp, ret, "Add to playlist");
				return FALSE;
			}
			break;
		}
		case XMMS_PL_PREPEND:
			break;
		case XMMS_PL_INSERT:
			break;
	}

	return TRUE;

}

xmms_playlist_entry_t *
xmms_playlist_pop (xmms_playlist_t *playlist)
{

	return NULL;

}

xmms_playlist_t *
xmms_playlist_open_create (gchar *path) 
{
	DB *dbp;
	int ret;
	xmms_playlist_t *pl;

	if (db_create (&dbp, NULL, 0) != 0) {
		g_error ("Couldn't init playlist");
		return NULL;
	}

	/* set the db error calling method to our own. */
	dbp->set_errcall (dbp, xmms_playlist_db3_err);
	
	dbp->set_pagesize (dbp, 4096); /* fine number indeed */
	
	/* set the length of the records */
	dbp->set_re_len (dbp, sizeof (xmms_playlist_entry_t));
	dbp->set_re_pad (dbp, 0);

	if ( (ret = dbp->open (dbp, path, NULL, DB_QUEUE, DB_CREATE | DB_THREAD, 0664)) != 0) {

		dbp->err (dbp, ret, NULL);
		return NULL;
	}

	pl = g_new0 (xmms_playlist_t,1);

	pl->dbp = dbp;
	pl->path = path;

	return pl;
}

void
xmms_playlist_close (xmms_playlist_t *playlist)
{
	playlist->dbp->close (playlist->dbp, 0);
}

void
xmms_playlist_db3_err (const char *errpfx, char *msg) 
{
	g_error ("%s", msg);

}
