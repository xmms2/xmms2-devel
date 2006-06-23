#ifndef CLIENT_H
#define CLIENT_H

#define INVALID_ID -1

#define DAAP_DEFAULT_PORT 3689

#define MAX_PATH_LENGTH 1024

typedef enum {
	REQUEST_LOGIN,
	REQUEST_SERVER_INFO,
	REQUEST_CONTENT_CODES,
	REQUEST_UPDATE,
	REQUEST_DATABASES,
	REQUEST_ITEMS,
	REQUEST_PLAYLISTS,
	REQUEST_PLAYLIST_ITEMS,
	REQUEST_STREAM,
	REQUEST_LOGOUT,
	REQUEST_UNKNOWN,
} request_t;

gboolean daap_request_data(GIOChannel *sock_chan, gchar *path, gchar *host);
gboolean daap_request_stream(GIOChannel *sock_chan, gchar *path, gchar *host);
request_t daap_get_request_type(gchar *command);
gchar * convert_command_to_path(gchar *command);
GIOChannel * prompt_for_file();
gboolean shutdown_file(GIOChannel *outfile);

#endif

/* vim:noexpandtab:shiftwidth=4:set tabstop=4: */
