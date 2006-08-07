#ifndef DAAP_CMD_H
#define DAAP_CMD_H

#include "cc_handlers.h"

/* returns: session_id */
guint
daap_command_login(gchar *host, gint port, guint request_id);

/* returns: revision_id */
guint
daap_command_update(gchar *host, gint port, guint session_id, guint request_id);

/* returns: TRUE on success */
gboolean
daap_command_logout(gchar *host, gint port, guint session_id, guint request_id);

/* returns: list of database IDs */
GSList * daap_command_db_list(gchar *host, gint port, guint session_id,
                              guint revision_id, guint request_id);

/* returns: list of songs in a given database */
GSList * daap_command_song_list(gchar *host, gint port, guint session_id,
                               guint revision_id, guint request_id, gint db_id);

/* returns: channel corresponding to streaming song data */
GIOChannel * daap_command_init_stream(gchar *host, gint port, guint session_id,
                                      guint revision_id, guint request_id,
                                      gint dbid, gchar *song);

#endif
