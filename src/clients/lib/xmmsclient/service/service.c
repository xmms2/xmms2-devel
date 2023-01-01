/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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

#include "common.h"

/* commands */
static xmmsv_t *command_call (xmmsc_connection_t *c, xmmsv_t *call);
static xmmsv_t *command_broadcast_subscribe (xmmsc_connection_t *c, xmmsv_t *name, int msgid);
static xmmsv_t *command_introspect (xmmsc_connection_t *c, xmmsv_t *args);

/* utility functions */
static int on_message_received (xmmsv_t *c2c_msg, void *userdata);
static xmmsv_t *introspect_interface_entity (xmmsc_sc_interface_entity_t *ifent);

/**
 * Set up the connection to offer service functionality.
 * @param c The connection structure.
 * @return The root namespace.
 */
xmmsc_sc_namespace_t *
xmmsc_sc_init (xmmsc_connection_t *c)
{
	xmmsc_result_t *res;

	x_check_conn (c, NULL);

	if (!c->sc_root) {
		xmmsc_sc_create_root_namespace (c);
		res = xmmsc_broadcast_c2c_message (c);
		xmmsc_result_notifier_set_c2c (res, on_message_received, c);
		xmmsc_result_unref (res);
	}

	return xmmsc_sc_interface_entity_get_namespace (c->sc_root);
}

static int
on_message_received (xmmsv_t *c2c_msg, void *userdata)
{
	int cmd, msgid;
	xmmsc_connection_t *c;
	xmmsv_t *payload, *sc_args, *reply;

	c = (xmmsc_connection_t *) userdata;

	/* Parse message payload and message id so we can reply */
	msgid = xmmsv_c2c_message_get_id (c2c_msg);
	if (msgid <= 0) {
		return true;
	}

	payload = xmmsv_c2c_message_get_payload (c2c_msg);
	if (!payload) {
		reply = xmmsv_new_error ("failed to get payload");
		goto send_reply;
	}

	/* Parse the command and arguments out of the payload */
	if (!xmmsv_dict_entry_get_int32 (payload, XMMSC_SC_CMD_KEY, &cmd)) {
		reply = xmmsv_new_error ("failed to parse command");
		goto send_reply;
	}

	if (!xmmsv_dict_get (payload, XMMSC_SC_ARGS_KEY, &sc_args)) {
		reply = xmmsv_new_error ("failed to parse arguments for command");
		goto send_reply;
	}

	/* And dispatch the command */
	switch (cmd) {
		case XMMSC_SC_CALL:
			reply = command_call (c, sc_args);
			break;

		case XMMSC_SC_BROADCAST_SUBSCRIBE:
			reply = command_broadcast_subscribe (c, sc_args, msgid);
			break;

		case XMMSC_SC_INTROSPECT:
			reply = command_introspect (c, sc_args);
			break;

		default:
			reply = xmmsv_new_error ("unrecognized command");
			break;
	}

send_reply:
	if (reply) {
		xmmsc_result_t *res;
		res = xmmsc_c2c_reply (c,
		                       msgid,
		                       XMMS_C2C_REPLY_POLICY_NO_REPLY,
		                       reply);

		xmmsc_result_unref (res);
		xmmsv_unref (reply);
	}

	return true;
}

/**
 * Implements XMMSC_SC_CALL.
 * This command expects as argument a dictionary mapping:
 * XMMSC_SC_CALL_METHOD_KEY to a method path;
 * XMMSC_SC_CALL_PARGS_KEY to a list of positional parameters;
 * and XMMSC_SC_CALL_NARGS_KEY to a dictionary of named parameters.
 */
static xmmsv_t *
command_call (xmmsc_connection_t *c, xmmsv_t *call)
{
	xmmsc_sc_interface_entity_t *method;
	xmmsv_t *method_path, *pargs, *nargs;

	if (!xmmsv_dict_get (call, XMMSC_SC_CALL_METHOD_KEY, &method_path)) {
		return xmmsv_new_error ("failed to get method path");
	}

	method = xmmsc_sc_locate_interface_entity (c, method_path);
	if (!method) {
		return xmmsv_new_error ("failed to resolve method path");
	}

	if (xmmsc_sc_interface_entity_get_type (method) !=
	    XMMSC_SC_INTERFACE_ENTITY_TYPE_METHOD) {

	    return xmmsv_new_error ("specified path is not a method");
	}

	if (!xmmsv_dict_get (call, XMMSC_SC_CALL_PARGS_KEY, &pargs)) {
		return xmmsv_new_error ("failed to get positional args");
	}

	if (!xmmsv_dict_get (call, XMMSC_SC_CALL_NARGS_KEY, &nargs)) {
		return xmmsv_new_error ("failed to get named args");
	}

	return xmmsc_sc_interface_entity_method_call (method, pargs, nargs);
}

/**
 * Implements XMMSC_SC_BROADCAST_SUBSCRIBE.
 * This command expects a path to a broadcast as its argument.
 */
static xmmsv_t *
command_broadcast_subscribe (xmmsc_connection_t *c, xmmsv_t *path, int msgid)
{
	xmmsc_sc_interface_entity_t *broadcast;

	broadcast = xmmsc_sc_locate_interface_entity (c, path);
	if (!broadcast) {
		return xmmsv_new_error ("failed to resolve broadcast path");
	}

	xmmsc_sc_interface_entity_broadcast_add_id (broadcast, msgid);
	return NULL;
}

/**
 * Implements XMMSC_SC_INTROSPECT.
 * This command expects as its argument a dictionary mapping:
 * XMMSC_SC_INTROSPECT_PATH_KEY to a path to an interface entity;
 * XMMSC_SC_INTROSPECT_TYPE_KEY optionally, to the expected type of the
 * interface entity;
 * XMMSC_SC_INTROSPECT_KEYFILTER_KEY optionally, to a keyfilter.
 *
 * A  keyfilter specifies a list of string keys whose values will be
 * followed in sequence in the interface entity's description dictionary
 * (and nested dictionaries) to yield the value to return.
 *
 * For instance, in a dict argument where:
 * XMMSC_SC_INTROSPECT_PATH_KEY : ["some", "namespace", "path"]
 * XMMSC_SC_INTROSPECT_TYPE_KEY : XMMSC_SC_INTERFACE_ENTITY_TYPE_NAMESPACE
 * XMMSC_SC_INTROSPECT_KEYFILTER_KEY : ["constants", "some_constant_key"]
 * the return value will be the value of the constant referenced by the
 * key "some_constant_key" in the namespace.
 *
 * Likewise, when:
 * XMMSC_SC_INTROSPECT_PATH_KEY : ["some", "method", "path"]
 * XMMSC_SC_INTROSPECT_TYPE_KEY : XMMSC_SC_INTERFACE_ENTITY_METHOD
 * XMMSC_SC_INTROSPECT_KEYFILTER_KEY : ["docstring"]
 * the return value for the command will be the method's docstring,
 * rather than its full description.
 */
static xmmsv_t *
command_introspect (xmmsc_connection_t *c, xmmsv_t *args)
{
	int type;
	const char *key;
	xmmsv_list_iter_t *it;
	xmmsv_t *path, *ret, *keyfilter, *v;
	xmmsc_sc_interface_entity_t *ifent;

	if (!xmmsv_dict_get (args, XMMSC_SC_INTROSPECT_PATH_KEY, &path)) {
		return xmmsv_new_error ("failed to get path from arguments");
	}

	ifent = xmmsc_sc_locate_interface_entity (c, path);
	if (!ifent) {
		return xmmsv_new_error ("failed to resolve path");
	}

	if (xmmsv_dict_entry_get_int (args, XMMSC_SC_INTROSPECT_TYPE_KEY, &type) &&
		xmmsc_sc_interface_entity_get_type (ifent) != type) {

		return xmmsv_new_error ("unexpected type");
	}

	ret = introspect_interface_entity (ifent);

	/* Follow trail of keys specified in keyfilter */
	if (xmmsv_dict_get (args, XMMSC_SC_INTROSPECT_KEYFILTER_KEY, &keyfilter)) {
		xmmsv_get_list_iter (keyfilter, &it);

		while (xmmsv_list_iter_valid (it)) {
			xmmsv_list_iter_entry_string (it, &key);
			xmmsv_dict_get (ret, key, &v);

			/* Get a reference to the value and unref the
			 * parent dict since we won't need it anymore.
			 */
			xmmsv_ref (v);
			xmmsv_unref (ret);
			ret = v;

			xmmsv_list_iter_next (it);
		}
	}

	return ret;
}

/**
 * Introspect an interface entity regardless of type.
 */
static xmmsv_t *
introspect_interface_entity (xmmsc_sc_interface_entity_t *ifent)
{
	switch (xmmsc_sc_interface_entity_get_type (ifent)) {
		case XMMSC_SC_INTERFACE_ENTITY_TYPE_METHOD:
			return xmmsc_sc_interface_entity_method_introspect (ifent);

		case XMMSC_SC_INTERFACE_ENTITY_TYPE_NAMESPACE:
			return xmmsc_sc_interface_entity_namespace_introspect (ifent);

		case XMMSC_SC_INTERFACE_ENTITY_TYPE_BROADCAST:
			return xmmsc_sc_interface_entity_broadcast_introspect (ifent);
	}
	return NULL;
}
