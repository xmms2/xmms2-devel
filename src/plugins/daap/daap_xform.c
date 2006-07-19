
/* XXX as of the current implementation, logging in to multiple DAAP servers 
 * in the same xmms2d instance is impossible. in addition, there is no method
 * of logging out of the logged-in servers. */

#include "xmms/xmms_defs.h"
#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"

#include "daap_cmd.h"
#include "daap_util.h"

#include <stdlib.h>
#include <glib.h>

#define DEFAULT_DAAP_PORT 3689

/*
 * Type definitions
 */

typedef struct {
	gchar *url, *host, *command;
	gint port;

	gchar *request;
	gchar *buffer;

	GIOChannel *channel;

	xmms_error_t status;
} xmms_daap_data_t;

typedef struct {
	gboolean logged_in;

	guint session_id;
	guint revision_id;
	guint request_id;
} xmms_daap_login_data_t;

static xmms_daap_login_data_t login_data;

/*
 * Function prototypes
 */

static gboolean xmms_daap_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_daap_init (xmms_xform_t *xform);
static void xmms_daap_destroy (xmms_xform_t *xform);
static gint xmms_daap_read (xmms_xform_t *xform, void *buffer,
                            gint len, xmms_error_t *error);
static GList * xmms_daap_browse (xmms_xform_t *xform,
                                 const gchar *url,
                                 xmms_error_t *error);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN("daap",
                  "DAAP access plugin",
                  "SoC",
                  "Accesses iTunes music shares",
                  xmms_daap_plugin_setup);

static gboolean
get_data_from_url(const gchar *url, gchar **host, gint *port, gchar **cmd)
{
	gint host_len;
	const gchar *host_begin, *cmd_begin, *port_begin;

	host_begin = url;
	host_begin += sizeof(gchar) * strlen("daap://");

	cmd_begin = strstr(host_begin, "/");
	g_return_val_if_fail(cmd_begin != NULL, FALSE);

	port_begin = strstr(host_begin, ":");
	if ((NULL == port_begin) || (port_begin > cmd_begin)) {
		*port = DEFAULT_DAAP_PORT;
	} else {
		*port = atoi(port_begin + sizeof(gchar)*strlen(":"));
	}

	if (NULL == port_begin) {
		host_len = (gint) (cmd_begin - host_begin);
	} else {
		host_len = (gint) (port_begin - host_begin);
	}
	*host = (gchar *) g_malloc0(host_len+1);
	g_return_val_if_fail(*host != NULL, FALSE);
	memcpy(*host, host_begin, host_len);

	if (NULL != cmd) {
		*cmd = (gchar *) g_malloc0(sizeof(gchar) * (strlen(cmd_begin)+1));
		g_return_val_if_fail(*cmd != NULL, FALSE);
		strncpy(*cmd, cmd_begin, sizeof(gchar) * strlen(cmd_begin));
	}

	return TRUE;
}

static gboolean
xmms_daap_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT(methods);
	methods.init = xmms_daap_init;
	methods.destroy = xmms_daap_destroy;
	methods.read = xmms_daap_read;
	methods.browse = xmms_daap_browse;

	xmms_xform_plugin_methods_set(xform_plugin, &methods);

	xmms_xform_plugin_indata_add(xform_plugin,
                                 XMMS_STREAM_TYPE_MIMETYPE,
                                 "application/x-url",
                                 XMMS_STREAM_TYPE_URL,
                                 "daap://*",
								 XMMS_STREAM_TYPE_END);

	return TRUE;
}

/*
 * Member functions
 */

static gboolean
xmms_daap_init (xmms_xform_t *xform)
{
	gint dbid;
	GSList *dbid_list = NULL;
	xmms_daap_data_t *data;
	const gchar *url;

	g_return_val_if_fail(xform != NULL, FALSE);

	url = xmms_xform_indata_get_str(xform, XMMS_STREAM_TYPE_URL);

	g_return_val_if_fail(url != NULL, FALSE);

	data = xmms_xform_private_data_get(xform);

	if (NULL == data) {
		data = g_malloc0(sizeof(xmms_daap_data_t));
		g_return_val_if_fail(data != NULL, FALSE);
	}

	data->url = g_strdup(url);
	get_data_from_url(data->url, &(data->host), &(data->port), &(data->command));

	if (login_data.logged_in == FALSE) {
		login_data.request_id = 1;
		login_data.session_id = daap_command_login(data->host, data->port,
		                                           login_data.request_id);
		if (login_data.session_id != 0) {
			login_data.logged_in = TRUE;
		}
	}

	login_data.revision_id = daap_command_update(data->host, data->port, 
	                                             login_data.session_id,
	                                             login_data.request_id);
	dbid_list = daap_command_db_list(data->host, data->port, login_data.session_id,
	                                 login_data.revision_id, login_data.request_id);
	g_return_val_if_fail(dbid_list != NULL, FALSE);

	/* XXX: see the browse function below */
	dbid = ((cc_list_item_t *) dbid_list->data)->dbid;
	/* want to request a stream, but don't read the data yet */
	data->channel = daap_command_init_stream(data->host, data->port,
	                                         login_data.session_id,
											 login_data.revision_id,
											 login_data.request_id, dbid,
	                                         data->command);
	g_return_val_if_fail(data->channel != NULL, FALSE);
	login_data.request_id++;

	xmms_xform_private_data_set(xform, data);
	//g_free(data);

	xmms_xform_outdata_type_add(xform,
	                            XMMS_STREAM_TYPE_MIMETYPE,
	                            "application/octet-stream",
	                            XMMS_STREAM_TYPE_END);

	return TRUE;
}

static void
xmms_daap_destroy (xmms_xform_t *xform)
{
	xmms_daap_data_t *data;

	data = xmms_xform_private_data_get(xform);

	g_io_channel_shutdown(data->channel, TRUE, NULL);
	g_io_channel_unref(data->channel);

	g_free(data->url);
	g_free(data->host);
	g_free(data->command);
	g_free(data);
}

static gint
xmms_daap_read (xmms_xform_t *xform, void *buffer, gint len, xmms_error_t *error)
{
	xmms_daap_data_t *data;
	guint read_bytes = 0;
	GIOStatus status;

	data = xmms_xform_private_data_get(xform);

	/* request is performed, header is stripped. now read the data. */
	while (read_bytes == 0) {
		status = g_io_channel_read_chars(data->channel, buffer, len, &read_bytes, NULL);
		if (status == G_IO_STATUS_EOF || status == G_IO_STATUS_ERROR) {
			break;
		}
	}
	
	return read_bytes;
}

static GList * xmms_daap_browse (xmms_xform_t *xform, const gchar *url,
                                 xmms_error_t *error)
{
	gboolean ok;
	gchar *songurl;
	GSList *dbid_list = NULL, *dbid_list_head;
	GSList *song_list = NULL, *song_list_head;
	GList *ret = NULL;
	GHashTable *h = NULL;
	xmms_daap_data_t *data;
	cc_list_item_t *db_data, *song_data;
	
	data = xmms_xform_private_data_get(xform);

	if (NULL == data) {
		data = g_malloc0(sizeof(xmms_daap_data_t));
		g_return_val_if_fail(data != NULL, NULL);
	}

	data->url = g_strdup(url);

	ok = get_data_from_url(url, &(data->host), &(data->port), NULL);
	g_return_val_if_fail(ok, NULL);

	if (! login_data.logged_in) {
		login_data.request_id = 1;
		login_data.session_id = daap_command_login(data->host, data->port,
		                                      login_data.request_id);
		if (login_data.session_id != 0) {
			login_data.logged_in = TRUE;
		}
	}

	login_data.revision_id = daap_command_update(data->host, data->port,
	                                             login_data.session_id,
	                                             login_data.request_id);
	dbid_list = daap_command_db_list(data->host, data->port,
	                                 login_data.session_id,
	                                 login_data.revision_id,
	                                 login_data.request_id);
	g_return_val_if_fail(dbid_list != NULL, NULL);
	dbid_list_head = dbid_list;

	/* XXX i've never seen more than one db per server out in the wild,
	 *     let's hope that never changes ;)
	 *     in short, just use the first db in the list */
	db_data = (cc_list_item_t *) dbid_list->data;
	song_list = daap_command_song_list(data->host, data->port,
	                                   login_data.session_id,
	                                   login_data.revision_id,
	                                   login_data.request_id,
	                                   db_data->dbid);
	song_list_head = song_list;
	for (; NULL != song_list; song_list = g_slist_next(song_list)) {
		gchar *sid = g_malloc(G_ASCII_DTOSTR_BUF_SIZE);
		song_data = (cc_list_item_t *) song_list->data;

		g_ascii_dtostr(sid, G_ASCII_DTOSTR_BUF_SIZE, song_data->dbid);
		songurl = g_strconcat("daap://", data->host, "/", sid, ".mp3", NULL);

		h = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

		g_hash_table_insert(h, "artist",
		            xmms_object_cmd_value_str_new(song_data->song_data_artist));
		g_hash_table_insert(h, "title",
		                    xmms_object_cmd_value_str_new(song_data->iname));

		ret = xmms_xform_browse_add_entry(ret, songurl, FALSE, h);

		g_hash_table_destroy(h);
		g_free(sid);
		g_free(songurl);
	}

	g_slist_foreach(dbid_list_head, (GFunc) cc_list_item_free, NULL);
	g_slist_free(dbid_list_head);
	g_slist_foreach(song_list_head, (GFunc) cc_list_item_free, NULL);
	g_slist_free(song_list_head);

	xmms_xform_private_data_set(xform, data);
	//g_free(data);

	return ret;
}

