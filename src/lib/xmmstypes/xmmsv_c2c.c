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

/*
 * FIXME: due to missing definition of NULL in the following headers.
 * The right headers should definitely be set in xmmsv_general.h though.
 */
#include <stddef.h>

#include "xmmsc/xmmsv.h"
#include "xmmsc/xmmsc_util.h"
#include "xmmscpriv/xmmsv_c2c.h"


/* Macro magic to define metadata extractor functions */
#define GEN_C2C_FIELD_GET_FUNC(field, getter, ctype, failval) \
ctype \
xmmsv_c2c_message_get_##field (xmmsv_t *msg) \
{ \
	ctype ret; \
	if (!getter (msg, #field, &ret)) \
		return failval; \
	return ret; \
}

/**
 * Extract the id from a c2c message.
 * @param msg The client-to-client message.
 *
 * @return The id upon success, -1 upon failure.
 */

GEN_C2C_FIELD_GET_FUNC (id, xmmsv_dict_entry_get_int32, int, -1)

/**
 * Extract the sender id from a c2c message.
 * @param msg The client-to-client message.
 *
 * @return The sender id upon success, -1 upon failure.
 */

GEN_C2C_FIELD_GET_FUNC (sender, xmmsv_dict_entry_get_int32, int, -1)

/**
 * Extract the destination id from a c2c message.
 * @param msg The client-to-client-message.
 *
 * @return The destination id upon success, -1 upon failure.
 */

GEN_C2C_FIELD_GET_FUNC (destination, xmmsv_dict_entry_get_int32, int, -1)

/**
 * Extract the payload from a c2c message.
 * @param msg The client-to-client message.
 *
 * @return The payload upon success, NULL upon failure.
 */

GEN_C2C_FIELD_GET_FUNC (payload, xmmsv_dict_get, xmmsv_t *, NULL)

/**
 * Format a client-to-client message.
 *
 * Messages are dictionaries of the form:\n
 * "sender" : id of the sender client\n
 * "destination" : id of the destination client\n
 * "id" : 0 if the message doesn't expect reply, else a message id\n
 * "payload" : the contents of the message
 *
 * @param sender the id of the sender client
 * @param dest the id of the destination client
 * @param id the id of the message
 * @param payload the contents of the message
 *
 * @return the formatted message
 *
 * @note Increases the refcount of payload.
 */
xmmsv_t *
xmmsv_c2c_message_format (int sender, int dest, int id,
                          xmmsv_t *payload)
{
	xmmsv_ref (payload);
	return xmmsv_build_dict (XMMSV_DICT_ENTRY_INT ("sender", sender),
	                         XMMSV_DICT_ENTRY_INT ("destination", dest),
	                         XMMSV_DICT_ENTRY_INT ("id", id),
	                         XMMSV_DICT_ENTRY ("payload", payload),
	                         XMMSV_DICT_END);
}
