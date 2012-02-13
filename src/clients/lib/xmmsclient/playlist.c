/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_ipc.h"
#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmsc/xmmsc_stdbool.h"
#include "xmmsc/xmmsc_stringport.h"

/**
 * @defgroup PlaylistControl PlaylistControl
 * @ingroup XMMSClient
 * @brief This controls the playlist.
 *
 * @{
 */

/**
 * Retrive the current position in the playlist
 */
xmmsc_result_t *
xmmsc_playlist_current_pos (xmmsc_connection_t *c, const char *playlist)
{
	x_check_conn (c, NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST,
	                       XMMS_IPC_CMD_CURRENT_POS,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_END);
}

/**
 * Retrive the name of the active playlist
 */
xmmsc_result_t *
xmmsc_playlist_current_active (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_CURRENT_ACTIVE);
}

/**
 * List the existing playlists.
 */
xmmsc_result_t *
xmmsc_playlist_list (xmmsc_connection_t *c)
{
	return xmmsc_coll_list (c, XMMS_COLLECTION_NS_PLAYLISTS);
}

/**
 * Create a new empty playlist.
 */
xmmsc_result_t *
xmmsc_playlist_create (xmmsc_connection_t *c, const char *playlist)
{
	xmmsc_result_t *res;
	xmmsv_coll_t *plcoll;

	x_check_conn (c, NULL);
	x_api_error_if (!playlist, "playlist name cannot be NULL", NULL);

	plcoll = xmmsv_coll_new (XMMS_COLLECTION_TYPE_IDLIST);
	xmmsv_coll_attribute_set (plcoll, "type", "list");
	res = xmmsc_coll_save (c, plcoll, playlist, XMMS_COLLECTION_NS_PLAYLISTS);

	xmmsv_coll_unref (plcoll);

	return res;
}

/**
 * Shuffles the current playlist.
 */
xmmsc_result_t *
xmmsc_playlist_shuffle (xmmsc_connection_t *c, const char *playlist)
{
	x_check_conn (c, NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_SHUFFLE,
	                       XMMSV_LIST_ENTRY_STR (playlist), XMMSV_LIST_END);
}

/**
 * Sorts the playlist according to the list of properties
 * (#xmmsv_t containing a list of strings).
 */
xmmsc_result_t *
xmmsc_playlist_sort (xmmsc_connection_t *c, const char *playlist, xmmsv_t *properties)
{
	int contains_strings_only;

	x_check_conn (c, NULL);
	x_api_error_if (!properties, "with a NULL property", NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	contains_strings_only = xmmsv_list_restrict_type (properties,
	                                                  XMMSV_TYPE_STRING);
	x_api_error_if (!contains_strings_only,
	                "property list may only contain strings", NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_SORT,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_ENTRY (xmmsv_ref (properties)),
	                       XMMSV_LIST_END);
}

/**
 * Clears the current playlist.
 */
xmmsc_result_t *
xmmsc_playlist_clear (xmmsc_connection_t *c, const char *playlist)
{
	x_check_conn (c, NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_CLEAR,
	                       XMMSV_LIST_ENTRY_STR (playlist), XMMSV_LIST_END);
}

/**
 * Remove the given playlist.
 */
xmmsc_result_t *
xmmsc_playlist_remove (xmmsc_connection_t *c, const char *playlist)
{
	return xmmsc_coll_remove (c, playlist, XMMS_COLLECTION_NS_PLAYLISTS);
}


/**
 * List current playlist.
 */
xmmsc_result_t *
xmmsc_playlist_list_entries (xmmsc_connection_t *c, const char *playlist)
{
	x_check_conn (c, NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_LIST,
	                       XMMSV_LIST_ENTRY_STR (playlist), XMMSV_LIST_END);
}

/**
 * Insert a medialib id at given position in playlist. 
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to insert the media.
 * @param pos A position in the playlist
 * @param id A medialib id.
 *
 */
xmmsc_result_t *
xmmsc_playlist_insert_id (xmmsc_connection_t *c, const char *playlist, int pos, int id)
{
	x_check_conn (c, NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_INSERT_ID,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_ENTRY_INT (pos),
	                       XMMSV_LIST_ENTRY_INT (id),
	                       XMMSV_LIST_END);
}

/**
 * Insert entry at given position in playlist.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to insert the media.
 * @param pos A position in the playlist
 * @param url The URL to insert
 *
 */
xmmsc_result_t *
xmmsc_playlist_insert_url (xmmsc_connection_t *c, const char *playlist, int pos, const char *url)
{
	return xmmsc_playlist_insert_full (c, playlist, pos, url, NULL);
}

/**
 * Insert a directory recursivly at a given position in the playlist.
 *
 * The url should be absolute to the server-side. Note that you will
 * have to include the protocol for the url to. ie:
 * file://mp3/my_mp3s/first.mp3.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to add the media.
 * @param pos A position in the playlist
 * @param url path.
 */
xmmsc_result_t *
xmmsc_playlist_rinsert (xmmsc_connection_t *c, const char *playlist, int pos, const char *url)
{
	xmmsc_result_t *res;
	char *enc_url;

	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	enc_url = xmmsc_medialib_encode_url (url);
	if (!enc_url)
		return NULL;

	res = xmmsc_playlist_rinsert_encoded (c, playlist, pos, enc_url);

	free (enc_url);

	return res;
}

/**
 * Insert a directory recursivly at a given position in the playlist.
 *
 * The url should be absolute to the server-side and url encoded. Note
 * that you will have to include the protocol for the url to. ie:
 * file://mp3/my_mp3s/first.mp3. You probably want to use
 * #xmmsc_playlist_radd unless you want to add a string that comes as
 * a result from the daemon, such as from #xmmsc_xform_media_browse
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to add the media.
 * @param pos A position in the playlist
 * @param url Encoded path.
 */
xmmsc_result_t *
xmmsc_playlist_rinsert_encoded (xmmsc_connection_t *c, const char *playlist, int pos, const char *url)
{
	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	if (!_xmmsc_medialib_verify_url (url))
		x_api_error ("with a non encoded url", NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_RINSERT,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_ENTRY_INT (pos),
	                       XMMSV_LIST_ENTRY_STR (url),
	                       XMMSV_LIST_END);
}

/**
 * Insert entry at given position in playlist with args.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to insert the media.
 * @param pos A position in the playlist
 * @param url The URL to insert
 * @param numargs The number of arguments
 * @param args array of numargs strings used as arguments
 */
xmmsc_result_t *
xmmsc_playlist_insert_args (xmmsc_connection_t *c, const char *playlist, int pos, const char *url, int numargs, const char **args)
{
	xmmsc_result_t *res;
	char *enc_url;

	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	enc_url = _xmmsc_medialib_encode_url_old (url, numargs, args);
	if (!enc_url)
		return NULL;

	res = xmmsc_playlist_insert_encoded (c, playlist, pos, enc_url);
	free (enc_url);

	return res;
}

/**
 * Insert entry at given position in playlist with args.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to insert the media.
 * @param pos A position in the playlist
 * @param url The URL to insert
 * @param numargs The number of arguments
 * @param args array of numargs strings used as arguments
 */
xmmsc_result_t *
xmmsc_playlist_insert_full (xmmsc_connection_t *c, const char *playlist, int pos, const char *url, xmmsv_t *args)
{
	xmmsc_result_t *res;
	char *enc_url;

	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	enc_url = xmmsc_medialib_encode_url_full (url, args);
	if (!enc_url)
		return NULL;

	res = xmmsc_playlist_insert_encoded (c, playlist, pos, enc_url);
	free (enc_url);

	return res;
}

/**
 * Insert entry at given position in playlist.
 * Same as #xmmsc_playlist_insert_url but takes an encoded
 * url instead.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to insert the media.
 * @param pos A position in the playlist
 * @param url The URL to insert
 *
 */
xmmsc_result_t *
xmmsc_playlist_insert_encoded (xmmsc_connection_t *c, const char *playlist, int pos, const char *url)
{
	if (!_xmmsc_medialib_verify_url (url))
		x_api_error ("with a non encoded url", NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST,
	                       XMMS_IPC_CMD_INSERT_URL,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_ENTRY_INT (pos),
	                       XMMSV_LIST_ENTRY_STR (url),
	                       XMMSV_LIST_END);
}

/**
 * Queries the medialib for media and inserts the matching ones to
 * the current playlist at the given position.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to insert the media.
 * @param pos A position in the playlist
 * @param coll The collection to find media in the medialib.
 * @param order The list of properties by which to order the matching
 *              media, passed as an #xmmsv_t list of strings.
 */
xmmsc_result_t *
xmmsc_playlist_insert_collection (xmmsc_connection_t *c, const char *playlist,
                                  int pos, xmmsv_coll_t *coll,
                                  xmmsv_t *order)
{
	x_check_conn (c, NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST,
	                       XMMS_IPC_CMD_INSERT_COLL,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_ENTRY_INT (pos),
	                       XMMSV_LIST_ENTRY_COLL (coll),
	                       XMMSV_LIST_ENTRY (xmmsv_ref (order)),
	                       XMMSV_LIST_END);
}


/**
 * Add a medialib id to the playlist. 
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to add the media.
 * @param id A medialib id.
 *
 */
xmmsc_result_t *
xmmsc_playlist_add_id (xmmsc_connection_t *c, const char *playlist, int id)
{
	x_check_conn (c, NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_ADD_ID,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_ENTRY_INT (id),
	                       XMMSV_LIST_END);
}

/**
 * Add the url to the playlist. The url should be absolute to the
 * server-side. Note that you will have to include the protocol for
 * the url to. ie: file://mp3/my_mp3s/first.mp3.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to add the media.
 * @param url path.
 *
 */
xmmsc_result_t *
xmmsc_playlist_add_url (xmmsc_connection_t *c, const char *playlist, const char *url)
{
	return xmmsc_playlist_add_full (c, playlist, url, NULL);
}

/**
 * Adds a directory recursivly to the playlist. The url should be absolute to the
 * server-side. Note that you will have to include the protocol for
 * the url to. ie: file://mp3/my_mp3s/first.mp3.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to add the media.
 * @param url path.
 *
 */
xmmsc_result_t *
xmmsc_playlist_radd (xmmsc_connection_t *c, const char *playlist, const char *url)
{
	xmmsc_result_t *res;
	char *enc_url;

	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	enc_url = xmmsc_medialib_encode_url (url);
	if (!enc_url)
		return NULL;

	res = xmmsc_playlist_radd_encoded (c, playlist, enc_url);

	free (enc_url);

	return res;
}

/**
 * Adds a directory recursivly to the playlist.
 *
 * The url should be absolute to the server-side and url encoded. Note
 * that you will have to include the protocol for the url to. ie:
 * file://mp3/my_mp3s/first.mp3. You probably want to use
 * #xmmsc_playlist_radd unless you want to add a string that comes as
 * a result from the daemon, such as from #xmmsc_xform_media_browse
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to add the media.
 * @param url Encoded path.
 *
 */
xmmsc_result_t *
xmmsc_playlist_radd_encoded (xmmsc_connection_t *c, const char *playlist, const char *url)
{
	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	if (!_xmmsc_medialib_verify_url (url))
		x_api_error ("with a non encoded url", NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_RADD,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_ENTRY_STR (url),
	                       XMMSV_LIST_END);
}

/**
 * Add the url to the playlist with arguments.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to add the media.
 * @param url path.
 * @param nargs The number of arguments
 * @param args array of numargs strings used as arguments
 */
xmmsc_result_t *
xmmsc_playlist_add_args (xmmsc_connection_t *c, const char *playlist, const char *url, int nargs, const char **args)
{
	xmmsc_result_t *res;
	char *enc_url;

	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	enc_url = _xmmsc_medialib_encode_url_old (url, nargs, args);
	if (!enc_url)
		return NULL;

	res = xmmsc_playlist_add_encoded (c, playlist, enc_url);
	free (enc_url);

	return res;
}

/**
 * Add the url to the playlist with arguments.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to add the media.
 * @param url path.
 * @param nargs The number of arguments
 * @param args array of numargs strings used as arguments
 */
xmmsc_result_t *
xmmsc_playlist_add_full (xmmsc_connection_t *c, const char *playlist, const char *url, xmmsv_t *args)
{
	xmmsc_result_t *res;
	char *enc_url;

	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	enc_url = xmmsc_medialib_encode_url_full (url, args);
	if (!enc_url)
		return NULL;

	res = xmmsc_playlist_add_encoded (c, playlist, enc_url);
	free (enc_url);

	return res;
}


/**
 * Add the url to the playlist. The url should be absolute to the
 * server-side AND encoded.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to add the media.
 * @param url path.
 *
 */
xmmsc_result_t *
xmmsc_playlist_add_encoded (xmmsc_connection_t *c, const char *playlist, const char *url)
{
	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	if (!_xmmsc_medialib_verify_url (url))
		x_api_error ("with a non encoded url", NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_ADD_URL,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_ENTRY_STR (url),
	                       XMMSV_LIST_END);
}

/**
 * Adds media in idlist to a playlist.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to add the media.
 * @param coll The collection to find media in the medialib.
 */
xmmsc_result_t *
xmmsc_playlist_add_idlist (xmmsc_connection_t *c, const char *playlist,
                           xmmsv_coll_t *coll)
{
	x_check_conn (c, NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST,
	                       XMMS_IPC_CMD_ADD_IDLIST,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_ENTRY_COLL (coll),
	                       XMMSV_LIST_END);
}

/**
 * Queries the medialib for media and adds the matching ones to
 * the current playlist.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to add the media.
 * @param coll The collection to find media in the medialib.
 * @param order The list of properties by which to order the matching
 *              media, passed as an #xmmsv_t list of strings.
 */
xmmsc_result_t *
xmmsc_playlist_add_collection (xmmsc_connection_t *c, const char *playlist,
                               xmmsv_coll_t *coll, xmmsv_t *order)
{
	x_check_conn (c, NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_ADD_COLL,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_ENTRY_COLL (coll),
	                       XMMSV_LIST_ENTRY (xmmsv_ref (order)),
	                       XMMSV_LIST_END);
}

/**
 * Move a playlist entry to a new position (absolute move)
 */
xmmsc_result_t *
xmmsc_playlist_move_entry (xmmsc_connection_t *c, const char *playlist,
                           int cur_pos, int new_pos)
{
	x_check_conn (c, NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST,
	                       XMMS_IPC_CMD_MOVE_ENTRY,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_ENTRY_INT (cur_pos),
	                       XMMSV_LIST_ENTRY_INT (new_pos),
	                       XMMSV_LIST_END);
}

/**
 * Remove an entry from the playlist.
 *
 * @param c The connection structure.
 * @param playlist The playlist in which to add the media.
 * @param pos The position that should be removed from the playlist.
 *
 * @sa xmmsc_playlist_list
 */
xmmsc_result_t *
xmmsc_playlist_remove_entry (xmmsc_connection_t *c, const char *playlist,
                             int pos)
{
	x_check_conn (c, NULL);

	/* default to the active playlist */
	if (playlist == NULL) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST,
	                       XMMS_IPC_CMD_REMOVE_ENTRY,
	                       XMMSV_LIST_ENTRY_STR (playlist),
	                       XMMSV_LIST_ENTRY_INT (pos),
	                       XMMSV_LIST_END);

}

/**
 * Request the playlist changed broadcast from the server. Everytime someone
 * manipulate the playlist this will be emitted.
 */
xmmsc_result_t *
xmmsc_broadcast_playlist_changed (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);
}

/**
 * Request the playlist current pos broadcast. When the position
 * in the playlist is changed this will be called.
 */
xmmsc_result_t *
xmmsc_broadcast_playlist_current_pos (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS);
}

/**
 * Set next entry in the playlist. Alter the position pointer.
 */
xmmsc_result_t *
xmmsc_playlist_set_next (xmmsc_connection_t *c, int pos)
{
	x_check_conn (c, NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_SET_POS,
	                       XMMSV_LIST_ENTRY_INT (pos), XMMSV_LIST_END);
}

/**
 * Same as #xmmsc_playlist_set_next but relative to the current postion.
 * -1 will back one and 1 will move to the next.
 */
xmmsc_result_t *
xmmsc_playlist_set_next_rel (xmmsc_connection_t *c, int pos)
{
	x_check_conn (c, NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST,
	                       XMMS_IPC_CMD_SET_POS_REL,
	                       XMMSV_LIST_ENTRY_INT (pos),
	                       XMMSV_LIST_END);
}

/**
 * Load a playlist as the current active playlist
 */
xmmsc_result_t *
xmmsc_playlist_load (xmmsc_connection_t *c, const char *name)
{
	x_check_conn (c, NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_LOAD,
	                       XMMSV_LIST_ENTRY_STR (name), XMMSV_LIST_END);
}

/**
 * Request the playlist_loaded broadcast. This will be called
 * if a playlist is loaded server-side. The argument will be a string
 * with the playlist name.
 */
xmmsc_result_t *
xmmsc_broadcast_playlist_loaded (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_PLAYLIST_LOADED);
}

/** @} */
