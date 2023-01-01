/** @file daap_conn.c
 *  Manages the connection to a DAAP server.
 *
 *  Copyright (C) 2006-2023 XMMS2 Team
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "cc_handlers.h"
#include "daap_md5.h"
#include "daap_conn.h"
#include "daap_util.h"

#include <xmms/xmms_log.h>
#include <xmmsc/xmmsc_ipc_transport.h>
#include <xmmsc/xmmsc_ipc_msg.h>
#include <xmmsc/xmmsc_sockets.h>


static GIOChannel *
daap_conn_make_iochannel (GSocketConnection *connection)
{
	GError     *err       = NULL;
	int         fd        = g_socket_get_fd (g_socket_connection_get_socket (connection));
	GIOChannel *sock_chan = g_io_channel_unix_new (fd);

	g_io_channel_set_flags (sock_chan, G_IO_FLAG_NONBLOCK, &err);
	if (NULL != err) {
		XMMS_DBG ("Error setting nonblock flag: %s\n", err->message);
		g_io_channel_unref (sock_chan);
		g_clear_error (&err);
		return NULL;
	}

	g_io_channel_set_encoding (sock_chan, NULL, &err);
	if (NULL != err) {
		XMMS_DBG ("Error setting encoding: %s\n", err->message);
		g_io_channel_unref (sock_chan);
		g_clear_error (&err);
		return NULL;
	}

	return sock_chan;
}

void
daap_conn_free (xmms_daap_conn_t *conn)
{
	GError *error = NULL;

	g_io_channel_unref (conn->chan);
	if (!g_io_stream_close (G_IO_STREAM (conn->conn), NULL, &error) && error) {
		XMMS_DBG ("Error closing IO stream: %s", error->message);
		g_clear_error (&error);
	}
	g_object_unref (conn->conn);
	g_free (conn);
}

xmms_daap_conn_t *
daap_conn_new (gchar *host, gint port)
{
	xmms_daap_conn_t         *conn        = NULL;
	GSocketClient            *client      = NULL;
	GError                   *error         = NULL;

	conn = g_new0 (xmms_daap_conn_t, 1);
	client = g_socket_client_new ();
	conn->conn = g_socket_client_connect_to_host (client, host, port, NULL, &error);
	g_object_unref (client);

	if (!conn->conn) {
		if (error)
			xmms_log_error ("Error with g_socket_client_connect_to_host: %s", error->message);
		else
			xmms_log_error ("Error with g_socket_client_connect_to_host");
		goto err;
	}

	conn->chan = daap_conn_make_iochannel (conn->conn);
	if (!conn->chan) {
		goto err_conn;
	}

	if (G_IS_TCP_CONNECTION (conn->conn))
		g_tcp_connection_set_graceful_disconnect (G_TCP_CONNECTION (conn->conn), TRUE);

	return conn;

 err_conn:
		if (!g_io_stream_close (G_IO_STREAM (conn->conn), NULL, &error) && error) {
			XMMS_DBG ("Error closing IO stream: %s", error->message);
			g_clear_error (&error);
		}
		g_object_unref (conn->conn);
 err:
		g_free (conn);
		return NULL;
}

gchar *
daap_generate_request (const gchar *path, gchar *host, gint request_id)
{
	gchar *req;
	gint8 hash[33];

	memset (hash, 0, 33);

	daap_hash_generate (DAAP_VERSION, (guchar *) path, 2, (guchar *) hash,
	                    request_id);

	req = g_strdup_printf ("GET %s %s\r\n"
	                       "Host: %s\r\n"
	                       "Accept: */*\r\n"
	                       "User-Agent: %s\r\n"
	                       "Accept-Language: en-us, en;q=5.0\r\n"
	                       "Client-DAAP-Access-Index: 2\r\n"
	                       "Client-DAAP-Version: 3.0\r\n"
	                       "Client-DAAP-Validation: %s\r\n"
	                       "Client-DAAP-Request-ID: %d\r\n"
	                       "Connection: close\r\n"
	                       "\r\n",
	                       path, HTTP_VER_STRING, host,
	                       USER_AGENT, hash, request_id);
	return req;
}

void
daap_send_request (GIOChannel *sock_chan, gchar *request)
{
	gint n_bytes_to_send;

	n_bytes_to_send = strlen (request);

	write_buffer_to_channel (sock_chan, request, n_bytes_to_send);
}

void
daap_receive_header (GIOChannel *sock_chan, gchar **header)
{
	guint n_total_bytes_recvd = 0;
	gsize linelen;
	gchar *response, *recv_line;
	GIOStatus io_stat;
	GError *err = NULL;

	if (NULL != header) {
		*header = NULL;
	}

	response = g_malloc0 (MAX_HEADER_LENGTH);
	if (NULL == response) {
		XMMS_DBG ("Error: couldn't allocate memory for response.\n");
		return;
	}

	/* read data from the io channel one line at a time, looking for
	 * the end of the header */
	do {
		io_stat = g_io_channel_read_line (sock_chan, &recv_line, &linelen,
		                                  NULL, &err);
		if (io_stat == G_IO_STATUS_ERROR) {
			XMMS_DBG ("Error reading from channel: %s\n", err->message);
			break;
		}

		if (NULL != recv_line) {
			memcpy (response+n_total_bytes_recvd, recv_line, linelen);
			n_total_bytes_recvd += linelen;

			if (strcmp (recv_line, "\r\n") == 0) {
				g_free (recv_line);
				if (NULL != header) {
					*header = g_malloc0 (n_total_bytes_recvd);
					if (NULL == *header) {
						XMMS_DBG ("error: couldn't allocate header\n");
						break;
					}
					memcpy (*header, response, n_total_bytes_recvd);
				}
				break;
			}

			g_free (recv_line);
		}

		if (io_stat == G_IO_STATUS_EOF) {
			break;
		}

		if (n_total_bytes_recvd >= MAX_HEADER_LENGTH) {
			XMMS_DBG ("Warning: Maximum header size reached without finding "
			          "end of header; bailing.\n");
			break;
		}
	} while (TRUE);

	g_free (response);

	if (sock_chan) {
		g_io_channel_flush (sock_chan, &err);
		if (NULL != err) {
			XMMS_DBG ("Error flushing buffer: %s\n", err->message);
			return;
		}
	}
}

cc_data_t *
daap_handle_data (GIOChannel *sock_chan, gchar *header)
{
	cc_data_t * retval;
	gint response_length;
	gchar *response_data;

	response_length = get_data_length (header);

	if (BAD_CONTENT_LENGTH == response_length) {
		XMMS_DBG ("warning: Header does not contain a \""CONTENT_LENGTH
		          "\" parameter.\n");
		return NULL;
	} else if (0 == response_length) {
		XMMS_DBG ("warning: "CONTENT_LENGTH" is zero, most likely the result of "
		          "a bad request.\n");
		return NULL;
	}

	response_data = g_malloc0 (response_length);
	if (NULL == response_data) {
		XMMS_DBG ("error: could not allocate response memory\n");
		return NULL;
	}

	read_buffer_from_channel (sock_chan, response_data, response_length);

	retval = cc_handler (response_data, response_length);
	g_free (response_data);

	return retval;
}

gint
get_data_length (gchar *header)
{
	gint len;
	gchar *content_length;

	content_length = strstr (header, CONTENT_LENGTH);
	if (NULL == content_length) {
		len = BAD_CONTENT_LENGTH;
	} else {
		content_length += strlen (CONTENT_LENGTH);
		len = atoi (content_length);
	}

	return len;
}

gint
get_server_status (gchar *header)
{
	gint status;
	gchar *server_status;

	server_status = strstr (header, HTTP_VER_STRING);
	if (NULL == server_status) {
		status = UNKNOWN_SERVER_STATUS;
	} else {
		server_status += strlen (HTTP_VER_STRING" ");
		status = atoi (server_status);
	}

	return status;
}

