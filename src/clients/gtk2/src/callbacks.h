#include <gtk/gtk.h>


void
on_prev_clicked                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_next_clicked                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_playlist_clicked                    (GtkButton       *button,
                                        gpointer         user_data);

void
on_play_clicked                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_playlist_destroy                    (GtkObject       *object,
                                        gpointer         user_data);

void
on_stop_clicked                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_playlist_row_activated              (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data);

void
on_playlist_Add_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_playlist_clear_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_playlist_delete_clicked             (GtkButton       *button,
                                        gpointer         user_data);

void
on_shuffle_clicked                     (GtkButton       *button,
                                        gpointer         user_data);
