/** @file 
 * XMMS client lib.
 *
 */

/* YES! I know that this api may change under my feet */
#define DBUS_API_SUBJECT_TO_CHANGE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <dbus/dbus.h>
#include <glib.h>

#include "xmmsclient.h"
#include "xmmsclient_int.h"
#include "xmms/signal_xmms.h"

#define XMMS_MAX_URI_LEN 1024

#define _REGULARCHAR(a) ((a>=65 && a<=90) || (a>=97 && a<=122)) || (isdigit (a))


/**
 * @internal
 */

typedef enum {
	XMMSC_TYPE_UINT32,
	XMMSC_TYPE_STRING,
	XMMSC_TYPE_NONE,
	XMMSC_TYPE_VIS,
	XMMSC_TYPE_MOVE,
	XMMSC_TYPE_MEDIAINFO,
	XMMSC_TYPE_UINT32_ARRAY,
	XMMSC_TYPE_TRANSPORT_LIST,
} xmmsc_types_t;


/**
 * @internal
 */

typedef struct xmmsc_signal_callbacks_St {
	gchar *signal_name;
	xmmsc_types_t type;
} xmmsc_signal_callbacks_t;


/**
 * @internal
 */

typedef struct xmmsc_callback_desc_St {
	void (*func)(void *, void *);
	void *userdata;
} xmmsc_callback_desc_t;


/**
 * @internal
 */

static xmmsc_signal_callbacks_t callbacks[] = {
	{ XMMS_SIGNAL_PLAYBACK_PLAYTIME, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_CORE_INFORMATION, XMMSC_TYPE_STRING },
	{ XMMS_SIGNAL_PLAYBACK_STOP, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYBACK_CURRENTID, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_CORE_DISCONNECT, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYLIST_ADD, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_PLAYLIST_MEDIAINFO, XMMSC_TYPE_MEDIAINFO },
	{ XMMS_SIGNAL_PLAYLIST_SHUFFLE, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYLIST_CLEAR, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYLIST_REMOVE, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYLIST_JUMP, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_PLAYLIST_MOVE, XMMSC_TYPE_MOVE },
	{ XMMS_SIGNAL_PLAYLIST_LIST, XMMSC_TYPE_UINT32_ARRAY },
	{ XMMS_SIGNAL_PLAYLIST_SORT, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_VISUALISATION_SPECTRUM, XMMSC_TYPE_VIS },
	{ XMMS_SIGNAL_TRANSPORT_LIST, XMMSC_TYPE_TRANSPORT_LIST },
	{ NULL, 0 },
};


/*
 * Forward declartions
 */

static xmmsc_signal_callbacks_t *get_callback (const gchar *signal);
static DBusHandlerResult handle_callback (DBusMessageHandler *handler, 
		DBusConnection *conn, DBusMessage *msg, void *user_data);
static void xmmsc_register_signal (xmmsc_connection_t *conn, gchar *signal);
static void xmmsc_send_void (xmmsc_connection_t *c, char *message);

/*
 * Public methods
 */

/**
 * @defgroup XMMSClient XMMSClient
 * @brief This functions will connect a client to a XMMS server.
 *
 * The clientlib is actually a wrapper around DBus message protocol.
 * So this is mearly for your convinence.
 *
 * @{
 */

/**
 * Initializes a xmmsc_connection_t. Returns %NULL if you
 * runned out of memory.
 *
 * @return a xmmsc_connection_t that should be freed with
 * xmmsc_deinit.
 *
 * @sa xmmsc_deinit
 */

xmmsc_connection_t *
xmmsc_init ()
{
	xmmsc_connection_t *c;
	
	c = g_new0 (xmmsc_connection_t,1);
	
	if (c) {
		c->callbacks = g_hash_table_new (g_str_hash,  g_str_equal);
	}
	
	if (!c->callbacks) {
		g_free (c);
		return NULL;
	}

	return c;
}

/**
 * Connects to the XMMS server.
 * @todo document dbuspath.
 *
 * @returns TRUE on success and FALSE if some problem
 * occured. call xmmsc_get_last_error to find out.
 *
 * @sa xmmsc_get_last_error
 */

gboolean
xmmsc_connect (xmmsc_connection_t *c, const char *dbuspath)
{
	DBusConnection *conn;
	DBusError err;
	DBusMessageHandler *hand;
	gint i = 0;

	dbus_error_init (&err);

	if (!c || !dbuspath)
		return FALSE;

	conn = dbus_connection_open (dbuspath, &err);
	
	if (!conn) {
		int ret;
		
		g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Couldn't connect to xmms2d, startning it...\n");
		ret = system ("xmms2d -d");

		if (ret != 0) {
			c->error = "Error starting xmms2d";
			return FALSE;
		}
		g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "started!\n");

		dbus_error_init (&err);
		conn = dbus_connection_open (dbuspath, &err);
		if (!conn) {
			c->error = "Couldn't connect to xmms2d even tough I started it... Bad, very bad.";
			return FALSE;
		}
	}


	while (callbacks[i].signal_name) {
		hand = dbus_message_handler_new (handle_callback, c, NULL);
		dbus_connection_register_handler (conn, hand, &callbacks[i].signal_name, 1);

		i++;
	}
	
	c->conn=conn;
	return TRUE;
}


/**
 * Encodes a path for use with xmmsc_playlist_add
 *
 * @sa xmmsc_playlist_add
 */
gchar *
xmmsc_encode_path (gchar *path) {
	gchar *out, *outreal;
	gint i;
	gint len;

	len = strlen (path);
	outreal = out = (gchar *)g_malloc0 (len * 3 + 1);

	for ( i = 0; i < len; i++) {
		if (path[i] == '/' || 
			_REGULARCHAR ((gint) path[i]) || 
			path[i] == '_' ||
			path[i] == '-' ){
			*(out++) = path[i];
		} else if (path[i] == ' '){
			*(out++) = '+';
		} else {
			g_snprintf (out, 4, "%%%02x", (guchar) path[i]);
			out += 3;
		}
	}

	return outreal;
}


/**
 * Returns a string that descibes the last error.
 */
gchar *
xmmsc_get_last_error (xmmsc_connection_t *c)
{
	return c->error;
}


/**
 * Frees up any resources used by xmmsc_connection_t
 */

void
xmmsc_deinit (xmmsc_connection_t *c)
{
	dbus_connection_flush (c->conn);
	dbus_connection_disconnect (c->conn);
	dbus_connection_unref (c->conn);
}

/** 
 * @defgroup ClientCallbacks ClientCallbacks
 * @brief This callbacks can be set with xmmsc_set_callback
 * and will be called with one argument.
 * @{
 */

/**
 * @def XMMS_SIGNAL_PLAYBACK_PLAYTIME
 *
 * This callback is called after each sample
 * played by core. This should update the playtime.
 * The argument will be a UINT32 with the number of
 * milliseconds played. The latency is already accounted
 * for in the value you get.
 */

/**
 * @def XMMS_SIGNAL_PLAYBACK_STOP
 *
 * This callback is called after someone pressed
 * "stop". The core will remain "stopped" until 
 * XMMS_SIGNAL_PLAYBACK_CURRENTID is called. This
 * callback is called with %NULL argument.
 *
 * @sa xmmsc_playback_stop
 */

/**
 * @def XMMS_SIGNAL_PLAYBACK_CURRENTID
 *
 * This callback will be called when playback changed.
 * If playback is stopped this means that someone pressed
 * play. The argument to this function is a UINT32 with the
 * id of the song that is being played. Call xmmsc_playlist_mediainfo
 * to get the information on the song.
 *
 * @sa xmmsc_playlist_get_mediainfo
 * @sa xmmsc_playback_current_id
 */

/**
 * @def XMMS_SIGNAL_CORE_INFORMATION
 *
 * This callback will be called when there is information from
 * the server that should be presented to the user.
 * The argument is a zero terminated string.
 */

/**
 * @def XMMS_SIGNAL_CORE_DISCONNECT
 *
 * This callback will be called when the server wants us
 * to disconnect. This usely means that server exits.
 * Argument is %NULL.
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_ADD
 *
 * This callback will be called when something is added to
 * the playlist. 
 * 
 * The argument is a UINT32 with the id of the added song.
 *
 * @sa xmmsc_playlist_add
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_MEDIAINFO
 *
 * This will be called with information about a specific
 * song. The arugment is a pointer to GHashTable with 
 * information about the song.
 *
 * @sa xmmsc_playlist_get_mediainfo
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_SHUFFLE
 *
 * This is called when the playlist is shuffled.
 * You will have to reread the playlist with xmmsc_playlist_list.
 * The callback is called with %NULL as argument.
 *
 * @sa xmmsc_playlist_list
 * @sa xmmsc_playlist_shuffle
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_CLEAR
 *
 * This callback is called when the playlist is cleared.
 * 
 * @sa xmmsc_playlist_clear
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_REMOVE
 *
 * This is called when a song is removed from the playlist.
 * The argument is a UINT32 with the removed songs id.
 *
 * @sa xmmsc_playlist_remove
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_JUMP
 *
 * This is called when someone moves the current song pointer
 * in the playlist. 
 * The argument is a UINT32 with the new current entry.
 *
 * @sa xmmsc_playlist_jump
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_MOVE
 *
 * This is called when someone is moving something in the playlist
 * @todo figure out how the argument should work.
 *
 * @sa xmmsc_playlist_move
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_LIST
 *
 * This will be called with a list of ids that is in the playlist.
 * The argument is a UINT32 Array that will be zeroterminated.
 *
 * @sa xmmsc_playlist_list
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_SORT
 *
 * This will be called when someone has sorted the playlist.
 * You will have to reread the playlist to ensure you have
 * the correct data.
 *
 * @sa xmmsc_playlist_sort
 */

/**
 * @def XMMS_SIGNAL_VISUALISATION_SPECTRUM
 *
 * This will be called for each sample with the fft data for that
 * sample. The argument is a array with 10 doubles. This can be
 * used for output a spectrumanalyzer.
 */

/**
 * @def XMMS_SIGNAL_TRANSPORT_LIST
 *
 * This will be called when a file list is ready.
 * This is generated by a xmmsc_file_list call.
 *
 * @sa xmmsc_file_list
 */

/**
 * Set a callback for the signal you want to handle.
 * the callback function should have the following prototype
 * void callback (void *userdata, void *callbackdata)
 * What callbackdata points to is diffrent for each callback.
 */

void
xmmsc_set_callback (xmmsc_connection_t *conn, 
		    gchar *callback, 
		    void (*func) (void *, void*), 
		    void *userdata)
{
	GList *l = NULL;
	xmmsc_callback_desc_t *desc = g_new0 (xmmsc_callback_desc_t, 1);

	desc->func = func;
	desc->userdata = userdata;

	/** @todo more than one callback of each type */
	l = g_hash_table_lookup (conn->callbacks, callback);
	if (!l) {
		xmmsc_register_signal (conn, callback);
		l = g_list_append (l, desc);
		g_hash_table_insert (conn->callbacks, g_strdup (callback), l);
	} else {
		l = g_list_append (l, desc);
	}

}

/** @} */

/**
 * Disconnects you from the current XMMS server.
 */

void
xmmsc_quit (xmmsc_connection_t *c)
{
	xmmsc_send_void(c, XMMS_SIGNAL_CORE_QUIT);
}

/**
 * Decodes a path. All paths that are recived from
 * the server are encoded, to make them userfriendly
 * they need to be called with this function.
 *
 * @param path encoded path.
 *
 * @returns a gchar * that needs to be freed with g_free.
 */

gchar *
xmmsc_decode_path (const gchar *path)
{
	gchar *qstr;
	gchar tmp[3];
	gint c1, c2;

	c1 = c2 = 0;

	qstr = (gchar *)g_malloc0 (strlen (path) + 1);

	tmp[2] = '\0';
	while (path[c1] != '\0'){
		if (path[c1] == '%'){
			gint l;
			tmp[0] = path[c1+1];
			tmp[1] = path[c1+2];
			l = strtol(tmp,NULL,16);
			if (l!=0){
				qstr[c2] = (gchar)l;
				c1+=2;
			} else {
				qstr[c2] = path[c1];
			}
		} else if (path[c1] == '+') {
			qstr[c2] = ' ';
		} else {
			qstr[c2] = path[c1];
		}
		c1++;
		c2++;
	}
	qstr[c2] = path[c1];

	return qstr;
}

/** @} */

/**
 * @defgroup PlaybackControl PlaybackControl
 * @ingroup XMMSClient
 * @brief This controls the playback.
 *
 * @{
 */

/**
 * Plays the next track in playlist.
 */

void
xmmsc_play_next (xmmsc_connection_t *c)
{
	xmmsc_send_void(c, XMMS_SIGNAL_PLAYBACK_NEXT);
}

/**
 * Plays the previos track in the playlist.
 */

void
xmmsc_play_prev (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,XMMS_SIGNAL_PLAYBACK_PREV);
}

/**
 * Stops the current playback. This will make the server
 * idle.
 */

void
xmmsc_playback_stop (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,XMMS_SIGNAL_PLAYBACK_STOP);
}

/**
 * Starts playback if server is idle.
 */

void
xmmsc_playback_start (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,XMMS_SIGNAL_PLAYBACK_PLAY);
}


/**
 * Seek to a absolute time in the current playback.
 *
 * @param c The connection structure.
 * @param milliseconds The total number of ms where
 * playback should continue.
 */

void
xmmsc_playback_seek_ms (xmmsc_connection_t *c, guint milliseconds)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_PLAYBACK_SEEK_MS, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, milliseconds);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/**
 * Seek to a absoulte number of samples in the current playback.
 *
 * @param c The connection structure.
 * @param samples the total number of samples where playback
 * should continue.
 */

void
xmmsc_playback_seek_samples (xmmsc_connection_t *c, guint samples)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_PLAYBACK_SEEK_SAMPLES, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, samples);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/**
 * Make server emit the current id.
 */

void
xmmsc_playback_current_id (xmmsc_connection_t *c)
{
	xmmsc_send_void (c,XMMS_SIGNAL_PLAYBACK_CURRENTID);
}

/** @} */

/**
 * @defgroup PlaylistControl PlaylistControl
 * @ingroup XMMSClient
 * @brief This controls the playlist.
 *
 * @{
 */

/**
 * Shuffles the current playlist.
 */

void
xmmsc_playlist_shuffle (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,XMMS_SIGNAL_PLAYLIST_SHUFFLE);
}

/**
 * Sorts the playlist according to the property
 */

void
xmmsc_playlist_sort (xmmsc_connection_t *c, char *property)
{
	DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_SORT, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, property);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/**
 * Clears the current playlist.
 */

void
xmmsc_playlist_clear (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,XMMS_SIGNAL_PLAYLIST_CLEAR);
}

/**
 * Saves the playlist to the specified filename. The
 * format of the playlist is detemined by checking the
 * extension of the filename.
 *
 * @param c The connection structure.
 * @param filename file on server-side to save the playlist
 * in.
 */

void
xmmsc_playlist_save (xmmsc_connection_t *c, gchar *filename)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_SAVE, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, filename);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/**
 * This will make the server list the current playlist.
 * The entries will be feed to the XMMS_SIGNAL_PLAYLIST_LIST
 * callback.
 */

void
xmmsc_playlist_list (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,XMMS_SIGNAL_PLAYLIST_LIST);
}

/**
 * Set the current song in the playlist.
 *
 * @param c The connection structure.
 * @param id The id that should be the current song
 * in the playlist.
 *
 * @sa xmmsc_playlist_list
 */

void
xmmsc_playlist_jump (xmmsc_connection_t *c, guint id)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_JUMP, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);

}

/**
 * Add the url to the playlist. The url should be encoded with
 * xmmsc_encode_path and be absolute to the server-side. Note that
 * you will have to include the protocol for the url to. ie:
 * file://mp3/my_mp3s/first.mp3.
 *
 * @param c The connection structure.
 * @param url an encoded path.
 *
 * @sa xmmsc_encode_path
 */

void
xmmsc_playlist_add (xmmsc_connection_t *c, char *url)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_ADD, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, url);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);

}

/**
 * Remove an entry from the playlist.
 *
 * param id the id to remove from the playlist.
 *
 * @sa xmmsc_playlist_list
 */

void
xmmsc_playlist_remove (xmmsc_connection_t *c, guint id)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_REMOVE, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);

}

/**
 * Retrives information about a certain entry.
 */

void
xmmsc_playlist_get_mediainfo (xmmsc_connection_t *c, guint id)
{
	DBusMessage *msg;
	DBusMessageIter itr;
	DBusError err;
	guint cserial;

	dbus_error_init (&err);

	msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_MEDIAINFO, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);
	dbus_connection_send (c->conn, msg, &cserial);

	dbus_message_unref (msg);
	dbus_connection_flush (c->conn);
}

static gboolean
free_str (gpointer key, gpointer value, gpointer udata)
{
	gchar *k = (gchar *)key;

	if (g_strcasecmp (k, "id") == 0) {
		g_free (key);
		return TRUE;
	}

	if (key)
		g_free (key);
	if (value)
		g_free (value);

	return TRUE;
}

/**
 * Free all strings in a GHashTable.
 */
void
xmmsc_playlist_entry_free (GHashTable *entry)
{
	g_return_if_fail (entry);

	g_hash_table_foreach_remove (entry, free_str, NULL);

	g_hash_table_destroy (entry);
}

static void
send_mode (xmmsc_connection_t *c, guint m)
{
	DBusMessage *msg;
	DBusMessageIter itr;
	DBusError err;
	guint cserial;

	dbus_error_init (&err);

	msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_MODE_SET, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, m);
	dbus_connection_send (c->conn, msg, &cserial);

	dbus_message_unref (msg);
	dbus_connection_flush (c->conn);
}


/**
 * @todo fix the integers here, this will b0rk b0rk if we
 * change the enum in core.
 */

/**
 * Sets the current playlist to mode none, or normal.
 */

void
xmmsc_playlist_mode_set_none (xmmsc_connection_t *c)
{
	send_mode (c, 0);
}

/**
 * Sets the current playlist to mode repeat all.
 * This makes the playlist loop after it's finished.
 */

void
xmmsc_playlist_mode_set_repeatall (xmmsc_connection_t *c)
{
	send_mode (c, 1);
}

/**
 * Sets the current playlist to mode repeat one.
 * This makes the same song to play over and over again.
 */

void
xmmsc_playlist_mode_set_repeatone (xmmsc_connection_t *c)
{
	send_mode (c, 2);
}

/**
 * Sets the current playlist to mode stop.
 * This will stop playback after the current song.
 */

void
xmmsc_playlist_mode_set_stop (xmmsc_connection_t *c)
{
	send_mode (c, 3);
}

/** @} */

/**
 * @defgroup OtherControl OtherControl
 * @ingroup XMMSClient
 * @brief This controls various other functions of the XMMS server.
 *
 * @{
 */


/**
 * Sets a configvalue in the server.
 */
void
xmmsc_configval_set (xmmsc_connection_t *c, gchar *key, gchar *val)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_CONFIG_VALUE_CHANGE, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, key);
	dbus_message_iter_append_string (&itr, val);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/**
 * Retrives a list of files from url.
 */
void
xmmsc_file_list (xmmsc_connection_t *c, gchar *url) 
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_TRANSPORT_LIST, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, url);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/** @} */

/*
 * Static utils
 */

static xmmsc_signal_callbacks_t *
get_callback (const gchar *signal)
{
	gint i = 0;

	while (callbacks[i].signal_name) {

		if (g_strcasecmp (callbacks[i].signal_name, signal) == 0)
			return &callbacks[i];
		i++;
	}

	return NULL;
}

static DBusHandlerResult
handle_callback (DBusMessageHandler *handler, 
		DBusConnection *conn, DBusMessage *msg, 
		void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
	xmmsc_signal_callbacks_t *c;
	GList *cb_list;
        DBusMessageIter itr;
	guint tmp[2]; /* used by MOVE */
	void *arg = NULL;

	c = get_callback (dbus_message_get_name (msg));

	/* This shouldnt happen, this means that we have registered a signal
	   there is no callback for. */
	if (!c) {
		return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	dbus_message_iter_init (msg, &itr);


	switch (c->type) {
		case XMMSC_TYPE_STRING:
			if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING)
				arg = dbus_message_iter_get_string (&itr);
			break;
		case XMMSC_TYPE_UINT32:
			if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32)
				arg = GUINT_TO_POINTER (dbus_message_iter_get_uint32 (&itr));
			break;
		case XMMSC_TYPE_VIS:
			{
				int len=0;
				dbus_message_iter_get_double_array (&itr, (double *) &arg, &len);
			}
			break;

		case XMMSC_TYPE_MOVE:
			if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
				tmp[0] = dbus_message_iter_get_uint32 (&itr);
				tmp[1] = dbus_message_iter_get_uint32 (&itr);
				arg = &tmp;
			}
			break;

		case XMMSC_TYPE_UINT32_ARRAY:
			if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_ARRAY &&
				dbus_message_iter_get_array_type (&itr) == DBUS_TYPE_UINT32) {
				guint32 *arr;
				gint len;
				guint32 *tmp;

				dbus_message_iter_get_uint32_array (&itr, &tmp, &len);

				arr = g_new0 (guint32, len+1);
				memcpy (arr, tmp, len * sizeof(guint32));
				arr[len] = '\0';
				
				arg = arr;
			}
			break;
		case XMMSC_TYPE_MEDIAINFO:
			if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_DICT) {
				DBusMessageIter dictitr;
				GHashTable *tab;
				dbus_message_iter_init_dict_iterator (&itr, &dictitr);

				tab = g_hash_table_new (g_str_hash, g_str_equal);

				while (42) {
					gchar *key = dbus_message_iter_get_dict_key (&dictitr);
					if (g_strcasecmp (key, "id") == 0) {
						g_hash_table_insert (tab, key, GUINT_TO_POINTER (dbus_message_iter_get_uint32 (&dictitr)));
					} else {
						g_hash_table_insert (tab, key, dbus_message_iter_get_string (&dictitr));
					}
					if (!dbus_message_iter_has_next (&dictitr))
						break;
					dbus_message_iter_next (&dictitr);
				}
				arg = tab;
			}
			break;

		case XMMSC_TYPE_TRANSPORT_LIST:
			{
				GList *list = NULL;

				while (42) {
					if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
						xmmsc_file_t *f;
					
						f = g_new (xmmsc_file_t, 1);
					
						f->path = dbus_message_iter_get_string (&itr);
						dbus_message_iter_next (&itr);
						f->file = dbus_message_iter_get_boolean (&itr);
						list = g_list_append (list, f);
					}

					if (!dbus_message_iter_has_next (&itr))
						break;

					dbus_message_iter_next (&itr);
				}

				arg = list;
			}

			break;

		case XMMSC_TYPE_NONE:
			/* don't really know how to handle this yet. */
			break;
	}

	cb_list = g_hash_table_lookup (xmmsconn->callbacks, dbus_message_get_name (msg));
	if (cb_list) {
		GList *node;
		for (node = cb_list; node; node = g_list_next (node)) {
			xmmsc_callback_desc_t *cb = node->data;
			cb->func(cb->userdata, arg);
		}
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static void
xmmsc_register_signal (xmmsc_connection_t *conn, gchar *signal)
{
	DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_CORE_SIGNAL_REGISTER, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, signal);
	dbus_connection_send (conn->conn, msg, &cserial);
	dbus_message_unref (msg);

}

static void
xmmsc_send_void (xmmsc_connection_t *c, char *message)
{
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (message, NULL);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}


