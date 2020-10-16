/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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
#include <ctype.h>

#include <xmmsclient/xmmsclient.h>
#include <xmmsclientpriv/xmmsclient.h>
#include <xmmsclientpriv/xmmsclient_util.h>
#include <xmmsc/xmmsc_idnumbers.h>

/**
 * Browse available media in a path.
 *
 * Retrieves a list of paths available (directly) under the specified
 * path.
 *
 */
xmmsc_result_t *
xmmsc_xform_media_browse (xmmsc_connection_t *c, const char *url)
{
	char *enc_url;
	xmmsc_result_t *res;

	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	enc_url = xmmsv_encode_url (url);
	if (!enc_url)
		return NULL;

	res = xmmsc_xform_media_browse_encoded (c, enc_url);

	free (enc_url);

	return res;
}

/**
 * Browse available media in a (already encoded) path.
 *
 * Retrieves a list of paths available (directly) under the specified
 * path.
 *
 */
xmmsc_result_t *
xmmsc_xform_media_browse_encoded (xmmsc_connection_t *c, const char *url)
{
	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	if (!_xmmsc_medialib_verify_url (url))
		x_api_error ("with a non encoded url", NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_XFORM, XMMS_IPC_COMMAND_XFORM_BROWSE,
	                       XMMSV_LIST_ENTRY_STR (url), XMMSV_LIST_END);
}
