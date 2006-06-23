#include <string.h>

#include <stdlib.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "client.h"
#include "daap_conn.h"
#include "cc_handlers.h"

#define COMMAND_LENGTH 64

/* from daap_conn */
extern session_t login_data;

/* from cc_handlers */
extern db_list_t databases;
extern song_list_t song_items;
extern pl_list_t playlists;
extern playlist_t playlist_items;

gint main(gint argc, gchar **argv) 
{
	gint port;
	gchar *host, *path;
	GIOChannel *sock_chan;
	GError *err = NULL;

	gchar command[COMMAND_LENGTH+1];

	if (argc <= 1 || argc >= 4) {
		g_printf("usage: %s host [port]\n", argv[0]);
		exit(0);
	}

	host = argv[1];

	if (argc == 2) {
		port = DAAP_DEFAULT_PORT;
	} else {
		port = atoi(argv[2]);
	}

	login_data.session_id = INVALID_ID;
	login_data.revision_id = INVALID_ID;
	login_data.request_id = 1;

	while (TRUE) {
		sock_chan = daap_open_connection(host, port);

		g_printf("Command > ");

		if (NULL == fgets(command, COMMAND_LENGTH, stdin)) {
			g_printf("Bye.\n");
			break;
		}
		/* remove newline */
		command[strlen(command)-1] = '\0';

		switch (daap_get_request_type(command)) {
			case REQUEST_STREAM:
				path = convert_command_to_path(command);
				if (!daap_request_stream(sock_chan, path, host)) {
					g_printf("Stream failed.\n");
				} else {
					g_printf("Stream successfully received\n");
				}
				break;

			case REQUEST_LOGIN:
				if (!daap_request_data(sock_chan, command, host)) {
					g_printf("Login failed.\n");
					break;
				}

				g_printf("Login successful, session-id is %d\n",
				         login_data.session_id);
				break;

			case REQUEST_UPDATE:
				path = convert_command_to_path(command);
				if (NULL == path) {
					g_printf("error: path is invalid\n");
					break;
				}
				if (!daap_request_data(sock_chan, path, host)) {
					g_printf("Update failed.\n");
					g_free(path);
					break;
				}
				g_free(path);
				
				g_printf("Update successful, revision-id is %d\n",
				         login_data.revision_id);
				break;

			case REQUEST_DATABASES:
				path = convert_command_to_path(command);
				if (NULL == path) {
					g_printf("error: path is invalid\n");
					break;
				}
				if (!daap_request_data(sock_chan, path, host)) {
					g_printf("Error on database request.\n");
					g_free(path);
					break;
				}
				g_free(path);

				g_printf("Received list of databases.\n");
				break;

			case REQUEST_ITEMS:
				path = convert_command_to_path(command);
				if (NULL == path) {
					g_printf("error: path is invalid\n");
					break;
				}
				if (!daap_request_data(sock_chan, path, host)) {
					g_printf("Error on song list request.\n");
					g_free(path);
					break;
				}
				g_free(path);

				g_printf("Received list of songs.\n");
				break;

			case REQUEST_PLAYLISTS:
				path = convert_command_to_path(command);
				if (NULL == path) {
					g_printf("error: path is invalid\n");
					break;
				}
				if (!daap_request_data(sock_chan, path, host)) {
					g_printf("Error on playlist request.\n");
					g_free(path);
					break;
				}
				g_free(path);

				g_printf("Received list of playlists.\n");
				break;

			case REQUEST_PLAYLIST_ITEMS:
				path = convert_command_to_path(command);
				if (NULL == path) {
					g_printf("error: path is invalid\n");
					break;
				}
				if (!daap_request_data(sock_chan, path, host)) {
					g_printf("Error on playlist item request.\n");
					g_free(path);
					break;
				}
				g_free(path);

				g_printf("Received list of playlist items.\n");
				break;

			case REQUEST_SERVER_INFO:
				path = convert_command_to_path(command);
				if (NULL == path) {
					g_printf("error: path is invalid\n");
					break;
				}
				if (!daap_request_data(sock_chan, path, host)) {
					g_printf("Error on server-info request.\n");
					g_free(path);
					break;
				}
				g_free(path);

				g_printf("Received server info.\n");
				break;

			case REQUEST_LOGOUT:
				path = convert_command_to_path(command);
				if (NULL == path) {
					g_printf("error: path is invalid\n");
					break;
				}
				if (!daap_request_data(sock_chan, path, host)) {
					g_printf("Error on logout.\n");
					g_free(path);
					break;
				}
				g_free(path);

				g_printf("Logout successful.\n");
				break;

			case REQUEST_CONTENT_CODES:
				g_printf("warning: content codes not yet supported\n");
				break;

			case REQUEST_UNKNOWN:
			default:
				g_printf("Unrecognized command: %s\n", command);
				break;
		}

		g_io_channel_shutdown(sock_chan, TRUE, &err);
		if (err != NULL) {
			g_printf("warning: failed to shutdown channel\n");
		}
	}

	return 0;
}

gboolean daap_request_data(GIOChannel *sock_chan, gchar *path, gchar *host)
{
	gint status;
	gchar *request = NULL, *header = NULL;

	daap_generate_request(&request, path, host);
	daap_send_request(sock_chan, request);
	g_free(request);

	daap_receive_header(sock_chan, &header);
	if (NULL == header) {
		g_printf("error: header not properly received.\n");
		return FALSE;
	}

	status = get_server_status(header);

	switch (status) {
		case UNKNOWN_SERVER_STATUS:
			g_printf("Server status is unknown, "
			         "possibly due to a bad response.\n");
			return FALSE;
		case HTTP_NO_CONTENT:
			//g_printf("Logout successful.\n");
			break;
		case HTTP_NOT_FOUND:
			g_printf("Server returned %d (file not found).\n", status);
			break;
		case HTTP_BAD_REQUEST:
		case HTTP_FORBIDDEN:
			g_printf("Server returned %d (bad request).\n", status);
			return FALSE;
		case HTTP_OK:
		default:
			daap_handle_data(sock_chan, header);
			break;
	}
	g_free(header);
	
	return TRUE;
}

gboolean daap_request_stream(GIOChannel *sock_chan, gchar *path, gchar *host)
{
	gint status;
	gchar *request = NULL, *header = NULL;
	GIOChannel *out_chan;

	daap_generate_request(&request, path, host);
	daap_send_request(sock_chan, request);
	g_free(request);

	daap_receive_header(sock_chan, &header);
	if (NULL == header) {
		g_printf("error: header not properly received.\n");
		return FALSE;
	}

	status = get_server_status(header);
	if (HTTP_OK == status) {
		out_chan = prompt_for_file();
		if (NULL == out_chan) {
			return FALSE;
		}
	} else {
		g_printf("Server error: %d\n", status);
		g_free(header);
		return FALSE;
	}

	daap_stream_data(sock_chan, out_chan, header);
	g_free(header);

	shutdown_file(out_chan);

	return TRUE;
}

request_t daap_get_request_type(gchar *command)
{
	request_t retval;

	if (strncmp(command, "/login", strlen("/login")) == 0) {
		retval = REQUEST_LOGIN;
	} else if (strncmp(command, "/update", strlen("/update")) == 0) {
		retval = REQUEST_UPDATE;
	} else if (strncmp(command, "/databases", strlen("/databases")) == 0) {
		if (NULL != strstr(command, "/containers")) {
			if (NULL != strstr(command, "/items")) {
				retval = REQUEST_PLAYLIST_ITEMS;
			} else {
				retval = REQUEST_PLAYLISTS;
			}
		} else if (NULL != strstr(command, "/items")) {
			/* FIXME what if it's not an mp3? */
			if (NULL != strstr(command, ".mp3")) {
				retval = REQUEST_STREAM;
			} else {
				retval = REQUEST_ITEMS;
			}
		} else {
			retval = REQUEST_DATABASES;
		}
	} else if (strncmp(command, "/server-info", strlen("/server-info")) == 0) {
		retval = REQUEST_SERVER_INFO;
	} else if (strncmp(command, "/logout", strlen("/logout")) == 0) {
		retval = REQUEST_LOGOUT;
	} else if (strncmp(command, "/content-codes", strlen("/content-codes")) ==
	           0) {
		retval = REQUEST_CONTENT_CODES;
	} else {
		retval = REQUEST_UNKNOWN;
	}

	return retval;
}

gchar * convert_command_to_path(gchar *command)
{
	gchar *path_ids, *path;
	/* FIXME again, what if not mp3? */
	if (login_data.revision_id != INVALID_ID &&
	    NULL == strstr(command, ".mp3")) {
		path_ids = g_strdup_printf("?session-id=%d&revision-id=%d",
		                           login_data.session_id,
								   login_data.revision_id);
	} else if (login_data.session_id != INVALID_ID) {
		path_ids = g_strdup_printf("?session-id=%d",
		                           login_data.session_id);
	} else {
		g_printf("warning: no session_id or revision_id exists, "
		         "did you forget to login?\n");
		//return NULL;
		path_ids = strdup(command);
	}

	if (NULL == path_ids) {
		g_printf("could not allocate memory for path_ids\n");
		return NULL;
	}
	path = g_strconcat(command, path_ids, NULL);
	g_free(path_ids);
	if (NULL == path) {
		g_printf("could not allocate memory for path\n");
		return NULL;
	}

	return path;
}

GIOChannel * prompt_for_file()
{
	gchar filename[1024];
	FILE *outfile;
	GIOChannel *out_chan;
	GError *err = NULL;

	printf("Filename > ");
	if (NULL == fgets(filename, 1024, stdin)) {
		return NULL;
	}
	filename[strlen(filename)-1] = '\0';

	outfile = fopen(filename, "w");
	if (NULL == outfile) {
		g_printf("could not open file \"%s\" for writing\n", filename);
		return NULL;
	}

	out_chan = g_io_channel_unix_new(fileno(outfile));
	
	g_io_channel_set_encoding(out_chan, NULL, &err);
	if (NULL != err) {
		g_printf("error setting encoding: %s\n", err->message);
		return NULL;
	}

	return out_chan;
}

gboolean shutdown_file(GIOChannel *outfile)
{
	GError *err = NULL;

	g_io_channel_shutdown(outfile, FALSE, &err);
	if (NULL != err) {
		g_printf("error shutting down channel: %s\n", err->message);
		return FALSE;
	}
	
	return TRUE;
}

/* vim:noexpandtab:shiftwidth=4:set tabstop=4: */
