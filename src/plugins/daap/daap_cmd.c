#include "daap_cmd.h"
#include "daap_conn.h"

static cc_data_t *
daap_request_data(GIOChannel *chan, gchar *path, gchar *host, guint request_id);
static gboolean
daap_request_stream(GIOChannel *chan, gchar *path, gchar *host, guint request_id);

guint
daap_command_login(gchar *host, gint port, guint request_id) {
	GIOChannel *chan;
	gchar *request;
	cc_data_t *cc_data;

	guint session_id;

	chan = daap_open_connection(host, port);
	g_return_val_if_fail(NULL != chan, 0);

	request = g_strdup("/login");
	
	cc_data = daap_request_data(chan, request, host, request_id);
	session_id = cc_data->session_id;

	g_free(request);
	cc_data_free(cc_data);
	g_io_channel_shutdown(chan, TRUE, NULL);

	return session_id;
}

guint
daap_command_update(gchar *host, gint port, guint session_id, guint request_id) {
	GIOChannel *chan;
	gchar *tmp, *request;
	cc_data_t *cc_data;

	guint revision_id;

	chan = daap_open_connection(host, port);
	g_return_val_if_fail(NULL != chan, -1);

	tmp = g_strdup_printf("?session-id=%d", session_id);
	request = g_strconcat("/update", tmp, NULL);
	g_free(tmp);
	
	cc_data = daap_request_data(chan, request, host, request_id);
	revision_id = cc_data->revision_id;

	g_free(request);
	cc_data_free(cc_data);
	g_io_channel_shutdown(chan, TRUE, NULL);

	return revision_id;
}

gboolean daap_command_logout(gchar *host, gint port, guint session_id, guint request_id)
{
	GIOChannel *chan;
	gchar *tmp, *request;

	chan = daap_open_connection(host, port);
	g_return_val_if_fail(NULL != chan, -1);

	tmp = g_strdup_printf("?session-id=%d", session_id);
	request = g_strconcat("/logout", tmp, NULL);
	g_free(tmp);
	
	/*cc_data = */daap_request_data(chan, request, host, request_id);
	//revision_id = cc_data->id;

	g_free(request);
	g_io_channel_shutdown(chan, TRUE, NULL);

	return /*revision_id*/TRUE;
}

GSList * daap_command_db_list(gchar *host, gint port, guint session_id,
                              guint revision_id, guint request_id)
{
	GIOChannel *chan;
	gchar *request;
	cc_data_t *cc_data;

	GSList * db_id_list;

	chan = daap_open_connection(host, port);
	g_return_val_if_fail(NULL != chan, NULL);

	request =g_strdup_printf("/databases?session-id=%d&revision-id=%d",
	                         session_id, revision_id);
	
	cc_data = daap_request_data(chan, request, host, request_id);
	g_free(request);
	g_return_val_if_fail(NULL != cc_data, NULL);
	/* TODO deep copy the list and destroy cc_data */
	db_id_list = cc_data->record_list;

	cc_data_free(cc_data);
	g_io_channel_shutdown(chan, TRUE, NULL);

	return db_id_list;
}

GSList * daap_command_song_list(gchar *host, gint port, guint session_id,
                                guint revision_id, guint request_id, gint db_id)
{
	GIOChannel *chan;
	gchar *request;
	cc_data_t *cc_data;

	GSList * song_list;

	chan = daap_open_connection(host, port);
	g_return_val_if_fail(NULL != chan, NULL);

	request =g_strdup_printf("/databases/%d/items?session-id=%d&revision-id=%d",
	                         db_id, session_id, revision_id);
	
	cc_data = daap_request_data(chan, request, host, request_id);
	/* TODO deep copy the list and destroy cc_data */
	song_list = cc_data->record_list;

	g_free(request);
	cc_data_free(cc_data);
	g_io_channel_shutdown(chan, TRUE, NULL);

	return song_list;
}

GIOChannel * daap_command_init_stream(gchar *host, gint port, guint session_id,
                                      guint revision_id, guint request_id,
                                      gint dbid, gchar *song)
{
	GIOChannel *chan;
	gchar *request;
	gboolean ok;

	chan = daap_open_connection(host, port);
	g_return_val_if_fail(NULL != chan, NULL);

	request = g_strdup_printf("/databases/%d/items%s"
	                          "?session-id=%d",
	                          dbid, song, session_id);
	
	ok = daap_request_stream(chan, request, host, request_id);
	g_free(request);

	g_return_val_if_fail(ok, NULL);
	return chan;	
}

static cc_data_t * 
daap_request_data(GIOChannel *chan, gchar *path, gchar *host, guint request_id)
{
	guint status;
	gchar *request = NULL, *header = NULL;
	cc_data_t *retval;

	daap_generate_request(&request, path, host, request_id);
	daap_send_request(chan, request);
	g_free(request);

	daap_receive_header(chan, &header);
	g_return_val_if_fail(NULL != header, NULL);

	status = get_server_status(header);

	switch (status) {
		case UNKNOWN_SERVER_STATUS:
			/*g_printf("Server status is unknown, "
			         "possibly due to a bad response.\n");*/
			retval = NULL;
		case HTTP_NO_CONTENT:
			//g_printf("Logout successful.\n");
			break;
		case HTTP_NOT_FOUND:
			/*g_printf("Server returned %d (file not found).\n", status);*/
			break;
		case HTTP_BAD_REQUEST:
		case HTTP_FORBIDDEN:
			/*g_printf("Server returned %d (bad request).\n", status);*/
			retval = NULL;
		case HTTP_OK:
		default:
			retval = daap_handle_data(chan, header);
			break;
	}
	g_free(header);
	
	return retval;
}

static gboolean
daap_request_stream(GIOChannel *chan, gchar *path, gchar *host, guint request_id)
{
	guint status;
	gchar *request = NULL, *header = NULL;

	daap_generate_request(&request, path, host, request_id);
	daap_send_request(chan, request);
	g_free(request);

	daap_receive_header(chan, &header);
	g_return_val_if_fail(header != NULL, FALSE);

	status = get_server_status(header);
	if (HTTP_OK == status) {
		/*
		out_chan = prompt_for_file();
		if (NULL == out_chan) {
			return FALSE;
		}
		*/
	} else {
		//g_printf("Server error: %d\n", status);
		g_free(header);
		return FALSE;
	}

	/*daap_stream_data(chan, out_chan, header);

	shutdown_file(out_chan);
	*/
	g_free(header);

	return TRUE;
}

