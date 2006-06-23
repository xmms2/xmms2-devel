#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "cc_handlers.h"
#include "daap_md5.h"
#include "daap_conn.h"
#include "daap_util.h"

session_t login_data;

/* from cc_handlers */
extern db_list_t databases;
extern song_list_t song_items;
extern pl_list_t playlists;
extern playlist_t playlist_items;

GIOChannel * daap_open_connection(gchar *host, gint port)
{
	gint sockfd;
	struct sockaddr_in server;
	GIOChannel *sock_chan;
	GError *err = NULL;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket failed");
		exit(1);
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	/* FIXME? inet_aton will choke if non-IP addr hostname given */
	if (!inet_aton(host, &(server.sin_addr))) {
		g_printf("inet_aton failed, invalid address given.\n");
		exit(1);
	}

	if (connect(sockfd,
		        (struct sockaddr *) &server,
		        sizeof(struct sockaddr_in)) == -1) {
		perror("connect failed");
		exit(1);
	}

	sock_chan = g_io_channel_unix_new(sockfd);

	g_io_channel_set_flags(sock_chan, G_IO_FLAG_NONBLOCK, &err);
	if (NULL != err) {
		g_printf("Error setting nonblock flag: %s\n", err->message);
		exit(1);
	}

	g_io_channel_set_encoding(sock_chan, NULL, &err);
	if (NULL != err) {
		g_printf("Error setting encoding: %s\n", err->message);
		exit(1);
	}

	if(!g_io_channel_get_close_on_unref(sock_chan)) {
		g_io_channel_set_close_on_unref(sock_chan, TRUE);
	}

	return sock_chan;
}

void daap_generate_request(gchar **request, gchar *path, gchar *host)
{
	gint request_len;
	gint8 hash[33];

	memset(hash, 0, 33);

	*request = (gchar *) g_malloc0(sizeof(gchar) * MAX_REQUEST_LENGTH);
	if (NULL == *request) {
		g_printf("Error: couldn't allocate memory for request\n");
		exit(1);
	}

	/* FIXME: this code is meant to be called after some subsequent
	 * login or update or some such; we'll make some assumptions to be
	 * replaced by actual values later. */
#define DAAP_VERSION 3
	daap_hash_generate(DAAP_VERSION, (guchar *) path, 2, (guchar *) hash, login_data.request_id);

	g_sprintf(*request, "GET %s %s\r\n"
	                   "Host: %s\r\n"
	                   //"Cache-Control: no-cache\r\n"
	                   "Accept: */*\r\n"
	                   "User-Agent: %s\r\n"
	                   "Accept-Language: en-us, en;q=5.0\r\n"
	                   "Client-DAAP-Access-Index: 2\r\n"
	                   "Client-DAAP-Version: 3.0\r\n"
	                   "Client-DAAP-Validation: %s\r\n"
	                   "Client-DAAP-Request-ID: %d\r\n"
	                   /* TODO: not accepting gzip yet; can't handle it */
	                   //"Accept-Encoding: gzip\r\n"
	                   "Connection: close\r\n"
	                   "\r\n",
	          path, HTTP_VER_STRING, host, USER_AGENT, hash,
	          login_data.request_id);
	request_len = strlen(*request);
	
	*request = g_realloc(*request, sizeof(gchar)*(request_len+1));
	if (NULL == *request) {
		g_printf("warning: realloc failed for request\n");
	}
	(*request)[request_len] = '\0';
}

void daap_send_request(GIOChannel *sock_chan, gchar *request)
{
	gint n_bytes_to_send;

	n_bytes_to_send = strlen(request);

	write_buffer_to_channel(sock_chan, request, n_bytes_to_send);
}

void daap_receive_header(GIOChannel *sock_chan, gchar **header)
{
	gint linelen, n_total_bytes_recvd = 0;
	gchar *response, *recv_line;
	GIOStatus io_stat;
	GError *err = NULL;

	*header = NULL;

	response = (gchar *) g_malloc0(sizeof(gchar) * MAX_HEADER_LENGTH);
	if (NULL == response) {
		g_printf("Error: couldn't allocate memory for response.\n");
		return;
	}

	/* read data from the io channel one line at a time, looking for
	 * the end of the header */
	do {
		io_stat = g_io_channel_read_line(sock_chan, &recv_line, &linelen,
		                                 NULL, &err);
		if (io_stat == G_IO_STATUS_ERROR) {
			g_printf("Error reading from channel: %s\n", err->message);
			break;
		}

		if (NULL != recv_line) {
			memcpy(response+n_total_bytes_recvd, recv_line, linelen);
			n_total_bytes_recvd += linelen;

			if (strcmp(recv_line, "\r\n") == 0) {
				g_free(recv_line);
				*header = (gchar *) g_malloc0(sizeof(gchar) *
				                              n_total_bytes_recvd);
				if (NULL == *header) {
					g_printf("error: couldn't allocate header\n");
					break;
				}
				memcpy(*header, response, n_total_bytes_recvd);
				break;
			}

			g_free(recv_line);
		}

		if (io_stat == G_IO_STATUS_EOF) {
			g_printf("debug: EOF, breaking\n");
			break;
		}

		if (n_total_bytes_recvd >= MAX_HEADER_LENGTH) {
			g_printf("Warning: Maximum header size reached without finding "
			         "end of header; bailing.\n");
			break;
		}
	} while (TRUE);

	g_free(response);

	g_io_channel_flush(sock_chan, &err);
	if (NULL != err) {
		g_printf("Error flushing buffer: %s\n", err->message);
		return;
	}
}

void daap_stream_data(GIOChannel *input, GIOChannel *output, gchar *header)
{
	gint read_bytes;
	gchar stream_buf[BUFSIZ];

	do {
		read_bytes = read_buffer_from_channel(input, stream_buf, BUFSIZ);
		write_buffer_to_channel(output, stream_buf, read_bytes);
	} while (read_bytes > 0);

	login_data.request_id++;
}

void daap_handle_data(GIOChannel *sock_chan, gchar *header)
{
	gint response_length;
	gchar *response_data;

	response_length = get_data_length(header);
	
	if (BAD_CONTENT_LENGTH == response_length) {
		g_printf("warning: Header does not contain a \""CONTENT_LENGTH
		         "\" parameter.\n");
		g_printf("DEBUG: Header is:\n%s\n", header);
		return;
	} else if (0 == response_length) {
		g_printf("warning: "CONTENT_LENGTH" is zero, most likely the result of "
		         "a bad request.\n");
		g_printf("DEBUG: Header is:\n%s\n", header);
		return;
	}
	
	response_data = (gchar *) g_malloc0(sizeof(gchar) * response_length);
	if (NULL == response_data) {
		g_printf("error: could not allocate response memory\n");
		return;
	}

	read_buffer_from_channel(sock_chan, response_data, response_length);

	cc_handler(response_data, response_length);
}

gint get_data_length(gchar *header)
{
	gint len;
	gchar *content_length;

	content_length = strstr(header, CONTENT_LENGTH);
	if (NULL == content_length) {
		len = BAD_CONTENT_LENGTH;
	} else {
		content_length += strlen(CONTENT_LENGTH);
		len = atoi(content_length);
	}

	return len;
}

gint get_server_status(gchar *header) 
{
	gint status;
	gchar *server_status;

	server_status = strstr(header, HTTP_VER_STRING);
	if (NULL == server_status) {
		status = UNKNOWN_SERVER_STATUS;
	} else {
		server_status += strlen(HTTP_VER_STRING" ");
		status = atoi(server_status);
	}

	return status;
}

/* vim:noexpandtab:shiftwidth=4:set tabstop=4: */
