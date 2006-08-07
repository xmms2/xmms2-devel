#ifndef DAAP_CONN_H
#define DAAP_CONN_H

#define MAX_REQUEST_LENGTH 1024
#define MAX_HEADER_LENGTH (1024 * 16)

#define BAD_CONTENT_LENGTH -1

#define DAAP_VERSION 3

#define HTTP_OK               200
#define HTTP_NO_CONTENT       204
#define HTTP_BAD_REQUEST      400
#define HTTP_FORBIDDEN        403
#define HTTP_NOT_FOUND        404
#define UNKNOWN_SERVER_STATUS -1

#define DAAP_URL_PREFIX "daap://"
#define HTTP_VER_STRING "HTTP/1.1"
#define CONTENT_LENGTH "Content-Length: "
#define CONTENT_TYPE "Content-Type: "
#if 0
#define USER_AGENT "XMMS2 (dev release)"
#else
#define USER_AGENT "iTunes/4.6 (Windows; N)"
#endif

GIOChannel * daap_open_connection(gchar *host, gint port);
void
daap_generate_request(gchar **request, gchar *path, gchar *host, gint request_id);
void daap_send_request(GIOChannel *sock_chan, gchar *request);
void daap_receive_header(GIOChannel *sock_chan, gchar **header);
cc_data_t * daap_handle_data(GIOChannel *sock_chan, gchar *header);
void daap_stream_data(GIOChannel *input, GIOChannel *output, gchar *header);

gint get_data_length(gchar *header);
gint get_server_status(gchar *header);

#endif
