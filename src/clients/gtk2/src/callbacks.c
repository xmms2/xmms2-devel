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

	xmmsc_play_prev (conn);

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

static void
add_filename (GtkWidget *files, gpointer udata)
{
	gchar **selected_filename;
	gchar *url;
	gint i = 0;
	selected_filename = gtk_file_selection_get_selections (GTK_FILE_SELECTION (udata));

	while (selected_filename[i]) {
		url = g_strdup_printf ("file://%s", selected_filename[i]);
		xmmsc_playlist_add (conn, url);
		g_free (url);
		i ++;
	}

	g_strfreev (selected_filename);

}


void
on_playlist_Add_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{

	GtkWidget *filedialog;

	filedialog = gtk_file_selection_new ("Select the files you want to add");
	gtk_file_selection_set_select_multiple (GTK_FILE_SELECTION (filedialog), TRUE);
	g_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filedialog)->ok_button),
					"clicked",
					G_CALLBACK (add_filename),
					(gpointer) filedialog);

	g_signal_connect_swapped (GTK_OBJECT (GTK_FILE_SELECTION (filedialog)->ok_button),
					"clicked",
					G_CALLBACK (gtk_widget_destroy), 
					(gpointer) filedialog); 

	g_signal_connect_swapped (GTK_OBJECT (GTK_FILE_SELECTION (filedialog)->cancel_button),
					"clicked",
					G_CALLBACK (gtk_widget_destroy),
					(gpointer) filedialog); 

	gtk_widget_show (filedialog);

}


void
on_playlist_clear_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{

	xmmsc_playlist_clear (conn);

}


void
on_playlist_delete_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{

}

