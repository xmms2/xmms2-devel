#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

#include "xmmsclient.h"

extern GtkWidget *playlistwin;
extern xmmsc_connection_t *conn;

void fill_playlist ();

void
on_prev_clicked                        (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_next_clicked                        (GtkButton       *button,
                                        gpointer         user_data)
{

	xmmsc_play_next (conn);

}


void
on_playlist_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{

	if (!playlistwin) {
		playlistwin = create_playlist ();
		fill_playlist ();
		gtk_widget_show (playlistwin);
	}
}


void
on_play_clicked                        (GtkButton       *button,
                                        gpointer         user_data)
{

	xmmsc_playback_start (conn);

}


void
on_playlist_destroy                    (GtkObject       *object,
                                        gpointer         user_data)
{
	playlistwin = NULL;
}


void
on_stop_clicked                        (GtkButton       *button,
                                        gpointer         user_data)
{

	xmmsc_playback_stop (conn);

}


void
on_playlist_row_activated              (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data)
{

}

