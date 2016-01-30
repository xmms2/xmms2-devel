/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
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

#include <fnmatch.h>

#define XMMSC_SC_ENTITY_NAME_PATTERN "[_a-zA-Z][_a-zA-Z0-9]*"

static xmmsc_result_t *introspect_internal (xmmsc_connection_t *c, int dest, xmmsv_t *entity, bool enforce_type, xmmsc_sc_interface_entity_type_t type, xmmsv_t *keyfilter);
static bool validate_entity_name (const char *name);

/**
 * Call a method in another client.
 * @param c The connection structure.
 * @param dest The destination client's id.
 * @param method A non-empty list of strings forming the path to the method.
 * @param pargs A list of positional arguments. May be NULL.
 * @param nargs A dict of named arguments. May be NULL.
 */
xmmsc_result_t *
xmmsc_sc_call (xmmsc_connection_t *c, int dest, xmmsv_t *method,
               xmmsv_t *pargs, xmmsv_t *nargs)
{
	xmmsc_result_t *res;
	xmmsv_t *msg, *call;

	x_check_conn (c, NULL);
	x_api_error_if (!dest, "with 0 as destination client.", NULL);
	x_api_error_if (!method, "with NULL method path.", NULL);
	x_api_error_if (!xmmsv_list_get_size (method), "with empty method.", NULL);
	x_api_error_if (pargs && xmmsv_get_type (pargs) != XMMSV_TYPE_LIST,
	                "with a non-list of positional arguments.", NULL);
	x_api_error_if (nargs && xmmsv_get_type (nargs) != XMMSV_TYPE_DICT,
	                "with a non-dict of named arguments.", NULL);
	x_api_error_if (!xmmsv_list_restrict_type (method, XMMSV_TYPE_STRING),
	                "with non-string in method path", NULL);

	/* Normalize NULL to empty values. */
	pargs = pargs ? xmmsv_ref (pargs) : xmmsv_new_list ();
	nargs = nargs ? xmmsv_ref (nargs) : xmmsv_new_dict ();

	/* Pack the call information in a dict.
	 * Let the dict steal a reference to pargs and nargs since either
	 * we created new values or we referenced the old ones above.
	 */
	call = xmmsv_build_dict (XMMSV_DICT_ENTRY (XMMSC_SC_CALL_METHOD_KEY,
	                                           xmmsv_ref (method)),
	                         XMMSV_DICT_ENTRY (XMMSC_SC_CALL_PARGS_KEY,
	                                           pargs),
	                         XMMSV_DICT_ENTRY (XMMSC_SC_CALL_NARGS_KEY,
	                                           nargs),
	                         XMMSV_DICT_END);

	/* And send that dict as the argument to the destination CALL command */
	msg = xmmsv_build_dict (XMMSV_DICT_ENTRY_INT (XMMSC_SC_CMD_KEY,
	                                              XMMSC_SC_CALL),
	                        XMMSV_DICT_ENTRY (XMMSC_SC_ARGS_KEY, call),
	                        XMMSV_DICT_END);

	res = xmmsc_c2c_send (c, dest, XMMS_C2C_REPLY_POLICY_SINGLE_REPLY, msg);
	xmmsv_unref (msg);

	return res;
}

/**
 * Subscribe to a broadcast from another client.
 * The returned result can be used to set up notifiers as usual.
 * @param c The connection structuer.
 * @param dest The client that issues the broadcast.
 * @param broadcast A non-empty list of strings forming the path
 * to the broadcast.
 *
 * \sa xmmsc_result_notifier_set_default and others.
 */
xmmsc_result_t *
xmmsc_sc_broadcast_subscribe (xmmsc_connection_t *c, int dest,
                              xmmsv_t *broadcast)
{
	xmmsv_t *msg;
	xmmsc_result_t *res;

	x_check_conn (c, NULL);
	x_api_error_if (!dest, "with 0 as destination client.", NULL);
	x_api_error_if (!broadcast, "with NULL broadcast path.", NULL);
	x_api_error_if (!xmmsv_list_get_size (broadcast),
	                "with empty broadcast.", NULL);
	x_api_error_if (!xmmsv_list_restrict_type (broadcast, XMMSV_TYPE_STRING),
	                "with non-string in broadcast path", NULL);

	msg = xmmsv_build_dict (XMMSV_DICT_ENTRY_INT (XMMSC_SC_CMD_KEY,
	                                              XMMSC_SC_BROADCAST_SUBSCRIBE),
	                        XMMSV_DICT_ENTRY (XMMSC_SC_ARGS_KEY,
	                                          xmmsv_ref (broadcast)),
	                        XMMSV_DICT_END);

	res = xmmsc_c2c_send (c, dest, XMMS_C2C_REPLY_POLICY_MULTI_REPLY, msg);
	xmmsv_unref (msg);

	return res;
}

/**
 * Emit a broadcast.
 * @param c The connection structure.
 * @param broadcast A non-empty list of strings specifying the broadcast
 * path.
 * @param value The value to emit. Must not be NULL.
 */
bool
xmmsc_sc_broadcast_emit (xmmsc_connection_t *c, xmmsv_t *broadcast,
                         xmmsv_t *value)
{
	int id;
	xmmsv_t *idlist, *v;
	xmmsv_list_iter_t *it;
	xmmsc_sc_interface_entity_t *entity;

	x_check_conn (c, false);
	x_api_error_if (!broadcast, "with NULL broadcast path.", false);
	x_api_error_if (!xmmsv_list_get_size (broadcast),
	                "with empty broadcast.", false);
	x_api_error_if (!xmmsv_list_restrict_type (broadcast, XMMSV_TYPE_STRING),
	                "with non-string in broadcast path", false);
	x_api_error_if (!value, "with NULL value", false);

	entity = xmmsc_sc_locate_interface_entity (c, broadcast);
	x_api_error_if (!entity ||
	                xmmsc_sc_interface_entity_get_type (entity) !=
	                XMMSC_SC_INTERFACE_ENTITY_TYPE_BROADCAST,
	                "with a path that doesn't resolve to a broadcast",
	                false);

	/* Go through the list of msgids and reply to each sending 'value'.
	 * Don't expect counter-replies.
	 */
	idlist = xmmsc_sc_interface_entity_broadcast_get_ids (entity);
	xmmsv_get_list_iter (idlist, &it);
	while (xmmsv_list_iter_valid (it)) {
		if (xmmsv_list_iter_entry (it, &v) && xmmsv_get_int (v, &id)) {
			xmmsc_result_t *res;

			res = xmmsc_c2c_reply (c, id, XMMS_C2C_REPLY_POLICY_NO_REPLY, value);
			xmmsc_result_unref (res);
		}

		xmmsv_list_iter_next (it);
	}

	return true;
}

#define GEN_SC_INTROSPECT_FUNC(entity, type) \
xmmsc_result_t * \
xmmsc_sc_introspect_##entity (xmmsc_connection_t *c, int dest, \
                              xmmsv_t *entity) \
{ \
	x_check_conn (c, NULL); \
	x_api_error_if (!dest, "with 0 as destination client.", NULL); \
	x_api_error_if (!entity, "with NULL " #entity " path.", NULL); \
	x_api_error_if (!xmmsv_list_restrict_type (entity, XMMSV_TYPE_STRING), \
	                "with non-string in " #entity " path", NULL); \
\
	return introspect_internal (c, dest, entity, true, type, NULL); \
}

/**
 * Introspect into a namespace.
 *
 * The result will carry a dictionary of the form:\n
 * "name" : the namespace's name.\n
 * "docstring" : the namespace's docstring.\n
 * "constants" : a dictionary of constants.\n
 * "namespaces" : a list of names of subnamespaces.\n
 * "methods" : a list of descriptions of the methods in the namespace.\n
 * "broadcasts" : a list of descriptions of the broadcasts in the namespace.\n
 *
 * @param c The connection structure.
 * @param dest The non-zero id of the destination client.
 * @param namespace A list of strings forming the path to the remote namespace.
 * Use an empty list to refer to the root namespace.
 */
GEN_SC_INTROSPECT_FUNC(namespace, XMMSC_SC_INTERFACE_ENTITY_TYPE_NAMESPACE);

/**
 * Introspect into a method.
 *
 * The result will carry a dictionary of the form:\n
 * "name" : the method's name.\n
 * "docstring" : the method's docstring.\n
 * "positional arguments" : a list of positional arguments.\n
 * "named arguments" : a list of named arguments.\n
 * "va positional" : whether or not the method accepts a variable number of
 * positional arguments.\n
 * "va named" : whether or not the method accepts a variable number of named
 * arguments.\n
 *
 * @param c The connection structure.
 * @param dest The non-zero id of the destination client.
 * @param method A list of strings forming the path to the remote method.
 */
GEN_SC_INTROSPECT_FUNC(method, XMMSC_SC_INTERFACE_ENTITY_TYPE_METHOD);

/**
 * Introspect into a client-to-client broadcast.
 *
 * The result will carry a dict of the form:\n
 * "name" : the broadcast's name.\n
 * "docstring" : the broadcast's docstring.\n
 *
 * @param c The connection structure.
 * @param dest The non-zero id of the destination client.
 * @param broadcast A list of strings forming the path to the remote broadcast.
 */
GEN_SC_INTROSPECT_FUNC(broadcast, XMMSC_SC_INTERFACE_ENTITY_TYPE_BROADCAST);

/**
 * Convenience function to retrieve a constant from a namespace directly.
 *
 * @param c The connection structure.
 * @param dest The nonzero destination client's id.
 * @param nms The path to the namespace in the remote client. Use an empty list
 * to refer to the root namespace.
 * @param key The key to the constant.
 */
xmmsc_result_t *
xmmsc_sc_introspect_constant (xmmsc_connection_t *c, int dest, xmmsv_t *nms,
                              const char *key)
{
	xmmsv_t *keyfilter;
	xmmsc_result_t *res;

	x_check_conn (c, NULL);
	x_api_error_if (!dest, "with 0 as destination client.", NULL);
	x_api_error_if (!nms, "with NULL namespace path.", NULL);
	x_api_error_if (!key, "with NULL key.", NULL);
	x_api_error_if (!xmmsv_list_restrict_type (nms, XMMSV_TYPE_STRING),
	                "with non-string in namespace path", NULL);


	keyfilter = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("constants"),
	                              XMMSV_LIST_ENTRY_STR (key),
	                              XMMSV_LIST_END);

	res = introspect_internal (c, dest, nms, true,
	                           XMMSC_SC_INTERFACE_ENTITY_TYPE_NAMESPACE,
	                           keyfilter);
	xmmsv_unref (keyfilter);

	return res;
}

/**
 * Get the docstring from a method, broadcast or namespace.
 *
 * @param c The connection structure.
 * @param dest The nonzero id of the destination client.
 * @param path The path to the remote entity.
 */
xmmsc_result_t *
xmmsc_sc_introspect_docstring (xmmsc_connection_t *c, int dest, xmmsv_t *path)
{
	xmmsv_t *keyfilter;
	xmmsc_result_t *res;

	x_check_conn (c, NULL);
	x_api_error_if (!dest, "with 0 as destination client.", NULL);
	x_api_error_if (!path, "with NULL path.", NULL);
	x_api_error_if (!xmmsv_list_restrict_type (path, XMMSV_TYPE_STRING),
	                "with non-string in namespace path", NULL);


	keyfilter = xmmsv_new_list ();
	xmmsv_list_append_string (keyfilter, "docstring");

	res = introspect_internal (c, dest, path, false, 0, keyfilter);
	xmmsv_unref (keyfilter);

	return res;
}

/**
 * Get the root namespace.
 * @param c The connection structure.
 *
 * @return The root namespace.
 */
xmmsc_sc_namespace_t *
xmmsc_sc_namespace_root (xmmsc_connection_t *c)
{
	return xmmsc_sc_interface_entity_get_namespace(c->sc_root);
}

/**
 * Get an existing namespace from its full path.
 * @param c The connection structure.
 * @param nms The path to the namespace.
 *
 * @return A namespace.
 */
xmmsc_sc_namespace_t *
xmmsc_sc_namespace_lookup (xmmsc_connection_t *c, xmmsv_t *nms)
{
	xmmsc_sc_interface_entity_t *ifent;

	x_check_conn (c, NULL);
	x_api_error_if (!nms, "with NULL path.", NULL);
	x_api_error_if (xmmsv_is_type(nms, XMMSV_TYPE_LIST),
	                "with invalid path (list expected).", NULL);
	x_api_error_if (!xmmsv_list_restrict_type (nms, XMMSV_TYPE_STRING),
	                "with invalid type in path (string expected).", NULL);

	ifent = xmmsc_sc_locate_interface_entity (c, nms);
	x_return_null_if_fail (ifent);

	return xmmsc_sc_interface_entity_get_namespace (ifent);
}

/**
 * Create a new namespace.
 * @param parent The parent namespace.
 * @param name The name of the new namespace. Must match
 * ::XMMSC_SC_ENTITY_NAME_PATTERN.
 * @param docstring The docstring of the namespace.
 *
 * @return The new namespace.
 */
xmmsc_sc_namespace_t *
xmmsc_sc_namespace_new (xmmsc_sc_namespace_t *parent,
                        const char *name, const char *docstring)
{
	xmmsc_sc_interface_entity_t *ifent;

	x_api_error_if (!name, "with NULL name.", false);
	x_api_error_if (!validate_entity_name (name), "with invalid name", false);

	x_return_null_if_fail (parent);

	ifent = xmmsc_sc_interface_entity_new_namespace (name, docstring);
	x_return_null_if_fail (ifent);

	if (!xmmsc_sc_namespace_add_child (parent, ifent)) {
		xmmsc_sc_interface_entity_destroy (ifent);
		return NULL;
	}

	return xmmsc_sc_interface_entity_get_namespace (ifent);
}

/**
 * Get an existing sub-namespace.
 * @param parent The parent namespace.
 * @param nms The name of the namespace.
 *
 * @return A namespace.
 */
xmmsc_sc_namespace_t *
xmmsc_sc_namespace_get (xmmsc_sc_namespace_t *parent, const char *name)
{
	xmmsc_sc_interface_entity_t *ifent;
	xmmsv_t *relpath;

	x_api_error_if (!parent, "with NULL parent namespace", NULL);
	x_api_error_if (!name, "with NULL name", NULL);
	x_api_error_if (!validate_entity_name (name), "with invalid name", NULL);

	relpath = xmmsv_build_list (XMMSV_LIST_ENTRY_STR (name), XMMSV_LIST_END);
	ifent = xmmsc_sc_namespace_resolve_path (parent, relpath, NULL);
	xmmsv_unref (relpath);
	x_return_null_if_fail (ifent);

	return xmmsc_sc_interface_entity_get_namespace (ifent);
}

/**
 * Create a new method.
 * @param nms The parent namespace.
 * @param method The underlying function that will be called.
 * @param name The name of the method. Must not be NULL. Must match
 * ::XMMSC_SC_ENTITY_NAME_PATTERN.
 * @param docstring The docstring of the method.
 * @param positional_args A list of positional arguments, or NULL to declare
 * none.
 * @param named_args A list of named arguments, or NULL to declare none.
 * @param va_positional Whether or not this method accepts a variable number
 * of positional arguments (in addition to positional_args).
 * @param va_named Whether or not this method accepts a extra named arguments
 * (in addition to named_args).
 * @param userdata Will be passed to the underlying function.
 *
 * \sa xmmsv_sc_argument_new
 */
bool
xmmsc_sc_namespace_add_method (xmmsc_sc_namespace_t *nms,
                               xmmsc_sc_method_t method,
                               const char *name, const char *docstring,
                               xmmsv_t *positional_args, xmmsv_t *named_args,
                               bool va_positional, bool va_named,
                               void *userdata)
{
	xmmsc_sc_interface_entity_t *ifent;

	x_return_val_if_fail (nms, false);

	x_api_error_if (!method, "with NULL method.", false);
	x_api_error_if (!name, "with NULL name.", false);
	x_api_error_if (!validate_entity_name (name), "with invalid name", false);
	x_api_error_if (positional_args &&
	                !xmmsv_is_type (positional_args, XMMSV_TYPE_LIST),
	                "with invalid type (list of positional arguments expected).", false);
	x_api_error_if (named_args &&
	                !xmmsv_is_type (named_args, XMMSV_TYPE_LIST),
	                "with invalid type (list of named arguments expected).", false);

	ifent = xmmsc_sc_interface_entity_new_method (name,
	                                              docstring,
	                                              method,
	                                              positional_args,
	                                              named_args,
	                                              va_positional,
	                                              va_named,
	                                              userdata);
	x_return_val_if_fail (ifent, false);

	if (!xmmsc_sc_namespace_add_child (nms, ifent)) {
		xmmsc_sc_interface_entity_destroy (ifent);
		return false;
	}

	return true;
}

/**
 * Convenience wrapper around xmmsc_sc_method_new to declare
 * a method that accepts no arguments.
 *
 * \sa xmmsc_sc_namespace_add_method
 */
bool
xmmsc_sc_namespace_add_method_noarg (xmmsc_sc_namespace_t *parent,
                                     xmmsc_sc_method_t method,
                                     const char *name, const char *docstring,
                                     void *userdata)
{
	return xmmsc_sc_namespace_add_method (parent, method, name, docstring,
	                                      NULL, NULL, false, false,
	                                      userdata);
}

/**
 * Create a new client-to-client broadcast.
 * @param nms The parent namespace.
 * @param name The name of the broadcast. Must not be NULL. Must match
 * ::XMMSC_SC_ENTITY_NAME_PATTERN.
 * @param docstring The docstring for the broadcast.
 *
 * \sa xmmsc_sc_broadcast_emit
 */
bool
xmmsc_sc_namespace_add_broadcast (xmmsc_sc_namespace_t *nms,
                                  const char *name, const char *docstring)
{
	xmmsc_sc_interface_entity_t *ifent;

	x_api_error_if (!name, "with NULL name.", false);
	x_api_error_if (!validate_entity_name (name), "with invalid name", false);

	x_return_val_if_fail (nms, false);

	ifent = xmmsc_sc_interface_entity_new_broadcast (name, docstring);

	if (!xmmsc_sc_namespace_add_child (nms, ifent)) {
		xmmsc_sc_interface_entity_destroy (ifent);
		return false;
	}

	return true;
}

/**
 * Build and send an introspection message.
 *
 * @param c The connection structure.
 * @param dest The destination client's id.
 * @param entity The path to the introspected entity.
 * @param enforce_type If true, the interface entity's type will be checked.
 * @param type If enforce_type is true, the type of the interface entity.
 * Ignored otherwise.
 * @param keyfilter Either NULL or a list of keys to be followed in
 * the resulting introspection value.
 *
 * \sa command_introspect
 */
static xmmsc_result_t *
introspect_internal (xmmsc_connection_t *c, int dest, xmmsv_t *entity,
                     bool enforce_type, xmmsc_sc_interface_entity_type_t type,
                     xmmsv_t *keyfilter)
{
	xmmsv_t *args, *msg;
	xmmsc_result_t *res;

	args = xmmsv_new_dict ();
	xmmsv_dict_set (args, XMMSC_SC_INTROSPECT_PATH_KEY, entity);

	if (keyfilter) {
		xmmsv_dict_set (args, XMMSC_SC_INTROSPECT_KEYFILTER_KEY, keyfilter);
	}

	if (enforce_type) {
		xmmsv_dict_set_int (args, XMMSC_SC_INTROSPECT_TYPE_KEY, type);
	}

	msg = xmmsv_build_dict (XMMSV_DICT_ENTRY_INT (XMMSC_SC_CMD_KEY,
	                                              XMMSC_SC_INTROSPECT),
	                        XMMSV_DICT_ENTRY (XMMSC_SC_ARGS_KEY, args),
	                        XMMSV_DICT_END);

	res = xmmsc_c2c_send (c, dest, XMMS_C2C_REPLY_POLICY_MULTI_REPLY, msg);
	xmmsv_unref (msg);

	return res;
}

/**
 * Check whether a given name qualifies as a namespace, broadcast
 * or method name.
 * The name must match ::XMMSC_SC_ENTITY_NAME_PATTERN.
 */
static bool
validate_entity_name (const char *name)
{
	return !fnmatch (XMMSC_SC_ENTITY_NAME_PATTERN, name, 0);
}
