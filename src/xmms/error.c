/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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


/**
 * @file
 * Error
 *
 */

#include <glib.h>
#include <xmms/xmms_error.h>

/** @defgroup Error Error
  * @ingroup XMMSServer
  * @brief Error reporting
  * @{
  */

static const gchar *typenames[XMMS_ERROR_COUNT] = {
	[XMMS_ERROR_NONE] = "org.xmms.Error.NoError",
	[XMMS_ERROR_GENERIC] = "org.xmms.Error.GenericError",
	[XMMS_ERROR_OOM] = "org.xmms.Error.OutofMemory",
	[XMMS_ERROR_PERMISSION] = "org.xmms.Error.PermissionProblem",
	[XMMS_ERROR_NOENT] = "org.xmms.Error.NoSuchEntry",
	[XMMS_ERROR_INVAL] = "org.xmms.Error.InvalidParameters",
	[XMMS_ERROR_EOS] = "org.xmms.Error.EndOfStream",
	[XMMS_ERROR_NO_SAUSAGE] = "org.xmms.Error.NoSausage",
};


/**
 * Get the current errortype as a string.
 */
const gchar *
xmms_error_type_get_str (xmms_error_t *err)
{
	g_return_val_if_fail (err, NULL);
	g_return_val_if_fail (XMMS_ERROR_NONE <= err->code && err->code < XMMS_ERROR_COUNT, NULL);
	return typenames[err->code];
}

/**
 * Get the human readable error
 */
const gchar *
xmms_error_message_get (xmms_error_t *err)
{
	g_return_val_if_fail (err, NULL);

	return err->message;
}

/** @} */
