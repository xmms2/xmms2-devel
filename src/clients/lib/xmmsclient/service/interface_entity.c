/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_util.h"

#include "interface_entity.h"

struct xmmsc_sc_namespace_St {
	x_list_t *children; /* child interface entities */
	xmmsv_t *constants;
};

/* Method description. */
typedef struct xmmsc_sc_method_desc_St {
	xmmsv_t *positional_args;
	bool va_positional;

	xmmsv_t *named_args;
	bool va_named;

	void *userdata;
	xmmsc_sc_method_t method;
} xmmsc_sc_method_desc_t;

/* C2C broadcast description. */
typedef struct xmmsc_sc_broadcast_desc_St {
	xmmsv_t *msgids;
} xmmsc_sc_broadcast_desc_t;

/* Internal representation of an entity (namespace, method or broadcast) */
struct xmmsc_sc_interface_entity_St {
	xmmsv_t *name;
    xmmsv_t *docstring;
    xmmsc_sc_interface_entity_type_t type;
    union {
        xmmsc_sc_namespace_t *namespace;
        xmmsc_sc_method_desc_t *method_desc;
        xmmsc_sc_broadcast_desc_t *broadcast_desc;
    } entity;
};

static xmmsc_sc_interface_entity_t *xmmsc_sc_interface_entity_new (const char *name, const char *docstring, xmmsc_sc_interface_entity_type_t type);

static xmmsc_sc_method_desc_t *xmmsc_sc_method_desc_new (xmmsc_sc_method_t method, xmmsv_t *positional_args, xmmsv_t *named_args, bool va_positional, bool va_named, void *userdata);
static bool xmmsc_sc_method_desc_check_args (xmmsc_sc_method_desc_t *mdesc, xmmsv_t *positional_args, xmmsv_t *named_args);
static void xmmsc_sc_method_desc_destroy (xmmsc_sc_method_desc_t *meth);

static xmmsc_sc_broadcast_desc_t *xmmsc_sc_broadcast_desc_new (void);
static void xmmsc_sc_broadcast_desc_destroy (xmmsc_sc_broadcast_desc_t *bcast);

static xmmsc_sc_namespace_t *xmmsc_sc_namespace_new_internal (void);
static xmmsc_sc_interface_entity_t *xmmsc_sc_namespace_lookup_child (xmmsc_sc_namespace_t *nms, xmmsv_t *name);

/**
 * Create a new interface entity.
 * @param name The name of the interface entity. Must not be NULL.
 * @param docstring The docstring of the interface entity.
 * @param type The type of the interface entity.
 */
static xmmsc_sc_interface_entity_t *
xmmsc_sc_interface_entity_new (const char *name, const char *docstring,
                               xmmsc_sc_interface_entity_type_t type)
{
	xmmsc_sc_interface_entity_t *ret;

	x_return_null_if_fail (name);

	ret = x_new0 (xmmsc_sc_interface_entity_t, 1);
	if (!ret) {
		x_oom ();
		return NULL;
	}

	ret->name = xmmsv_new_string (name);

	if (docstring) {
		ret->docstring = xmmsv_new_string (docstring);
	} else {
		ret->docstring = xmmsv_new_string ("");
	}

	ret->type = type;
	return ret;
}

/**
 * Destroy an interface entity.
 * @param ifent The interface entity.
 */
void
xmmsc_sc_interface_entity_destroy (xmmsc_sc_interface_entity_t *ifent)
{
	x_return_if_fail (ifent);

	xmmsv_unref (ifent->name);
	xmmsv_unref (ifent->docstring);

	switch (ifent->type) {
		case XMMSC_SC_INTERFACE_ENTITY_TYPE_NAMESPACE:
			xmmsc_sc_namespace_destroy (ifent->entity.namespace);
			break;

		case XMMSC_SC_INTERFACE_ENTITY_TYPE_METHOD:
			xmmsc_sc_method_desc_destroy (ifent->entity.method_desc);
			break;

		case XMMSC_SC_INTERFACE_ENTITY_TYPE_BROADCAST:
			xmmsc_sc_broadcast_desc_destroy (ifent->entity.broadcast_desc);
			break;

		default:
			x_internal_error ("invalid interface entity type.");
			break;
	}

	free (ifent);
	return;
}

/**
 * Create a new method interface entity.
 * @param name The name of the interface entity. Must not be NULL.
 * @param docstring The docstring of the interface entity.
 *
 * @return The new interface entity.
 */
xmmsc_sc_interface_entity_t *
xmmsc_sc_interface_entity_new_method (const char *name, const char *docstring,
                                      xmmsc_sc_method_t method,
                                      xmmsv_t *positional_args,
                                      xmmsv_t *named_args,
                                      bool va_positional, bool va_named,
                                      void *userdata)
{
	xmmsc_sc_interface_entity_t *ret;
	xmmsc_sc_method_desc_t *mdesc;

	ret = xmmsc_sc_interface_entity_new (name, docstring,
	                                     XMMSC_SC_INTERFACE_ENTITY_TYPE_METHOD);
	if (!ret) {
		return NULL;
	}

	mdesc = xmmsc_sc_method_desc_new (method,
	                                  positional_args,
	                                  named_args,
	                                  va_positional,
	                                  va_named,
	                                  userdata);
	if (!mdesc) {
		xmmsc_sc_interface_entity_destroy (ret);
		return NULL;
	}

	ret->entity.method_desc = mdesc;
	return ret;
}

/**
 * Create a new broadcast interface entity.
 * @param name The name of the interface entity.
 * @param docstring The docstring of the interface entity.
 *
 * @return The new interface entity.
 */
xmmsc_sc_interface_entity_t *
xmmsc_sc_interface_entity_new_broadcast (const char *name,
                                         const char *docstring)
{
	xmmsc_sc_interface_entity_t *ret;
	xmmsc_sc_broadcast_desc_t *bdesc;

	ret = xmmsc_sc_interface_entity_new (name, docstring,
	                                     XMMSC_SC_INTERFACE_ENTITY_TYPE_BROADCAST);
	if (!ret) {
		return NULL;
	}

	bdesc = xmmsc_sc_broadcast_desc_new ();
	if (!bdesc) {
		xmmsc_sc_interface_entity_destroy (ret);
		return NULL;
	}

	ret->entity.broadcast_desc = bdesc;
	return ret;
}

/**
 * Create a new namespace interface entity.
 * @param name The name of the interface entity.
 * @param docstring The docstring of the interface entity.
 *
 * @return The new interface entity.
 */
xmmsc_sc_interface_entity_t *
xmmsc_sc_interface_entity_new_namespace (const char *name,
                                         const char *docstring)
{
	xmmsc_sc_interface_entity_t *ret;
	xmmsc_sc_namespace_t *nms;

	ret = xmmsc_sc_interface_entity_new (name, docstring,
	                                     XMMSC_SC_INTERFACE_ENTITY_TYPE_NAMESPACE);
	if (!ret) {
		return NULL;
	}

	nms = xmmsc_sc_namespace_new_internal ();
	if (!nms) {
		xmmsc_sc_interface_entity_destroy (ret);
		return NULL;
	}

	ret->entity.namespace = nms;
	return ret;
}

/**
 * Remove a symbol from a namespace. The relative path can point to a
 * subnamespace, a method or a broadcast.
 * @param nms The parent namespace.
 * @param path The relative path of the symbol.
 */
void
xmmsc_sc_namespace_remove (xmmsc_sc_namespace_t *nms, xmmsv_t *path)
{
	x_list_t *child;
	xmmsc_sc_interface_entity_t *ifent;
	xmmsc_sc_namespace_t *parent = NULL;

	x_return_if_fail (nms);
	x_api_error_if (!path, "with NULL path.", );
	x_api_error_if (xmmsv_is_type(path, XMMSV_TYPE_LIST),
	                "with invalid path (list expected).", );
	x_api_error_if (!xmmsv_list_restrict_type (path, XMMSV_TYPE_STRING),
	                "with invalid type in path (string expected).", );

	ifent = xmmsc_sc_namespace_resolve_path (nms, path, &parent);
	x_return_if_fail (ifent);

	child = x_list_find (parent->children, ifent);
	nms->children = x_list_delete_link (nms->children, child);
	xmmsc_sc_interface_entity_destroy (ifent);
}

/**
 * Get the namespace from an interface entity.
 *
 * The namespace still belongs to the interface entity and
 * should not be destroyed by the caller.
 * @param ifent The interface entity.
 */
xmmsc_sc_namespace_t *
xmmsc_sc_interface_entity_get_namespace (xmmsc_sc_interface_entity_t *ifent)
{
	return ifent->entity.namespace;
}

/**
 * Get the type of an interface entity.
 * @param ifent The interface entity.
 */
xmmsc_sc_interface_entity_type_t
xmmsc_sc_interface_entity_get_type (xmmsc_sc_interface_entity_t *ifent)
{
	return ifent->type;
}

/**
 * Create a new method description.
 * @param method The underlying function that will be called.
 * @param positional_args A list of positional arguments.
 * @param named_args A list of named arguments.
 * @param va_positional Whether or not this method accepts a variable number
 * of positional arguments (in addition to positional_args).
 * @param va_named Whether or not this method accepts a extra named arguments
 * (in addition to named_args).
 * @param userdata Will be passed to the underlying function.
 *
 * \sa xmmsv_sc_argument_new
 */
static xmmsc_sc_method_desc_t *
xmmsc_sc_method_desc_new (xmmsc_sc_method_t method,
                          xmmsv_t *positional_args, xmmsv_t *named_args,
                          bool va_positional, bool va_named,
                          void *userdata)
{
	xmmsc_sc_method_desc_t *meth;

	meth = x_new0 (xmmsc_sc_method_desc_t, 1);
	if (!meth) {
		x_oom ();
		return NULL;
	}

	if (positional_args && xmmsv_is_type (positional_args, XMMSV_TYPE_LIST)) {
		meth->positional_args = xmmsv_ref (positional_args);
	} else {
		meth->positional_args = xmmsv_new_list ();
	}

	if (named_args && xmmsv_is_type (named_args, XMMSV_TYPE_DICT)) {
		meth->named_args = xmmsv_ref (named_args);
	} else {
		meth->named_args = xmmsv_new_list ();
	}

	meth->method = method;
	meth->va_positional = va_positional;
	meth->va_named = va_named;
	meth->userdata = userdata;

	return meth;
}

/**
 * Destroy a method description.
 * @param meth The method description.
 */
static void
xmmsc_sc_method_desc_destroy (xmmsc_sc_method_desc_t *meth)
{
	x_return_if_fail (meth);

	xmmsv_unref (meth->positional_args);
	xmmsv_unref (meth->named_args);

	free (meth);
	return;
}

/**
 * Check whether the arguments passed to a method call are correct.
 * @param mdesc The description of the method.
 * @param positional_args A list of positional arguments to be checked.
 * @param named_args A dict of named arguments to be checked.
 *
 * @return true if the arguments are valid and false otherwise.
 * In either case, positional_args and named_args may have been modified.
 */
static bool
xmmsc_sc_method_desc_check_args (xmmsc_sc_method_desc_t *mdesc,
                                 xmmsv_t *positional_args, xmmsv_t *named_args)
{
	const char *name;
	xmmsv_t *arg, *value;
	xmmsv_type_t expected_type;
	xmmsv_list_iter_t *itsig, *itrecv;

	/* Check positional arguments.
	 * Traverse the list of positional arguments in the signature
	 * and the list of args we received, checking if types match.
	 */

	xmmsv_get_list_iter (mdesc->positional_args, &itsig);
	xmmsv_get_list_iter (positional_args, &itrecv);

	while (xmmsv_list_iter_valid (itsig) && xmmsv_list_iter_valid (itrecv)) {
		xmmsv_list_iter_entry (itsig, &arg);
		xmmsv_list_iter_entry (itrecv, &value);

		expected_type = xmmsv_sc_argument_get_type (arg);
		if (expected_type == XMMSV_TYPE_NONE ||
		    expected_type != xmmsv_get_type (value)) {

			/* Wrong type! */
			return false;
		}

		xmmsv_list_iter_next (itsig);
		xmmsv_list_iter_next (itrecv);
	}

	if (xmmsv_list_iter_valid (itrecv) && !mdesc->va_positional) {
		/* Too many positional arguments! */
		return false;
	}

	while (xmmsv_list_iter_valid (itsig)) {
		/* Less positional arguments received than declared.
		 * See if we can fill up the rest with the default values.
		 */

		xmmsv_list_iter_entry (itsig, &arg);
		value = xmmsv_sc_argument_get_default_value (arg);

		if (!value) {
			/* Missing positional arguments! */
			return false;
		}

		xmmsv_list_append (positional_args, value);
		xmmsv_list_iter_next (itsig);
	}

	/* Check named arguments.
	 * Traverse the list of named arguments in the signature
	 * and the dict of named arguments we received, checking names
	 * and types.
	 */

	xmmsv_get_list_iter (mdesc->named_args, &itsig);

	while (xmmsv_list_iter_valid (itsig)) {
		xmmsv_list_iter_entry (itsig, &arg);
		name = xmmsv_sc_argument_get_name (arg);

		if (!xmmsv_dict_get (named_args, name, &value)) {
			/* Missing named argument.
			 * Try to use the default value or fail if
			 * there is none.
			 */

			value = xmmsv_sc_argument_get_default_value (arg);
			if (!value) {
				return false;
			}

			xmmsv_dict_set (named_args, name, value);
		}

		expected_type = xmmsv_sc_argument_get_type (arg);
		if (expected_type == XMMSV_TYPE_NONE ||
		    expected_type != xmmsv_get_type (value)) {

			/* Wrong type! */
			return false;
		}

		xmmsv_list_iter_next (itsig);
	}

	if (!mdesc->va_named &&
	    xmmsv_list_get_size (mdesc->named_args) <
	    xmmsv_dict_get_size (named_args)) {

	    /* Too many named arguments! */
	    return false;
	}

	return true;
}

/**
 * Call a method represented by an interface entity.
 * @param method The method interface entity.
 * @param pargs The list of positional arguments.
 * @param nargs The dictionary of named arguments.
 *
 * @return The value returned by the method or an error if the arguments
 * don't match the method's signature.
 */
xmmsv_t *
xmmsc_sc_interface_entity_method_call (xmmsc_sc_interface_entity_t *method,
                                       xmmsv_t *pargs, xmmsv_t *nargs)
{
	xmmsv_t *ret;
	xmmsc_sc_method_desc_t *mdesc;

	mdesc = method->entity.method_desc;

	if (!xmmsc_sc_method_desc_check_args (mdesc, pargs, nargs)) {
		return xmmsv_new_error ("arguments don't match method signature");
	}

	ret = mdesc->method (pargs, nargs, mdesc->userdata);
	if (!ret) {
		ret = xmmsv_new_none ();
	}

	return ret;
}

/**
 * Introspect into a method interface entity.
 *
 * The method description is a dictionary of the form:\n
 * "name" : the method's name.\n
 * "docstring" : the method's docstring.\n
 * "positional-arguments" : a list of positional arguments.\n
 * "named-arguments" : a list of named arguments.\n
 * "va-positional" : whether or not this method accepts a variable number of
 * positional-arguments.\n
 * "va-named" : whether or not this method accepts a variable number of named
 * arguments.\n
 *
 * @param method The method interface entity.
 * @return A dictionary describing the method.
 */
xmmsv_t *
xmmsc_sc_interface_entity_method_introspect (xmmsc_sc_interface_entity_t *method)
{
	xmmsc_sc_method_desc_t *mdesc;

	mdesc = method->entity.method_desc;

	return xmmsv_build_dict (XMMSV_DICT_ENTRY ("name",
	                                           xmmsv_ref (method->name)),
	                         XMMSV_DICT_ENTRY ("docstring",
	                                           xmmsv_ref (method->docstring)),
	                         XMMSV_DICT_ENTRY ("positional-arguments",
	                                           xmmsv_ref (mdesc->positional_args)),
	                         XMMSV_DICT_ENTRY ("named-arguments",
	                                           xmmsv_ref (mdesc->named_args)),
	                         XMMSV_DICT_ENTRY_INT ("va-positional",
	                                               mdesc->va_positional),
	                         XMMSV_DICT_ENTRY_INT ("va-named",
	                                               mdesc->va_named),
	                         XMMSV_DICT_END);
}

/**
 * Add a message id to a broadcast.
 * @param bcast The broadcast interface entity.
 * @param id The message id.
 */
void
xmmsc_sc_interface_entity_broadcast_add_id (xmmsc_sc_interface_entity_t *bcast,
                                            int id)
{
	xmmsv_t *idval;

	idval = xmmsv_new_int (id);
	xmmsv_list_append (bcast->entity.broadcast_desc->msgids, idval);
	xmmsv_unref (idval);

	return;
}

/**
 * Borrow a reference to the list of message ids in a broadcast.
 * @param bcast The broadcast interface entity.
 * @return A list of integer message ids.
 */
xmmsv_t *
xmmsc_sc_interface_entity_broadcast_get_ids (xmmsc_sc_interface_entity_t *bcast)
{
	return bcast->entity.broadcast_desc->msgids;
}

/**
 * Introspect into a broadcast interface entity.
 *
 * A broadcast description dictionary has the form:\n
 * "name" : the broadcast's name.\n
 * "docstring" : the broadcast's docstring.\n
 *
 * @param broadcast The broadcast interface entity.
 */
xmmsv_t *
xmmsc_sc_interface_entity_broadcast_introspect (xmmsc_sc_interface_entity_t *bcast)
{
	return xmmsv_build_dict (XMMSV_DICT_ENTRY ("name",
	                                           xmmsv_ref (bcast->name)),
	                         XMMSV_DICT_ENTRY ("docstring",
	                                           xmmsv_ref (bcast->docstring)),
	                         XMMSV_DICT_END);
}

/**
 * Create a new broadcast description.
 */
static xmmsc_sc_broadcast_desc_t *
xmmsc_sc_broadcast_desc_new (void)
{
	xmmsc_sc_broadcast_desc_t *bcast;

	bcast = x_new0 (xmmsc_sc_broadcast_desc_t, 1);
	if (!bcast) {
		x_oom ();
		return NULL;
	}

	bcast->msgids = xmmsv_new_list ();
	return bcast;
}

/**
 * Destroy a broadcast description.
 * @param bcas The broadcast description.
 */
static void
xmmsc_sc_broadcast_desc_destroy (xmmsc_sc_broadcast_desc_t *bcast)
{
	x_return_if_fail (bcast);

	xmmsv_unref (bcast->msgids);

	free (bcast);
	return;
}

/**
 * Introspect into a namespace interface entity.
 *
 * The namespace description is a dictionary of the form:\n
 * "name" : the namespace's name.\n
 * "docstring" : the namespace's docstring.\n
 * "constants" : a dictionary of constants.\n
 * "namespaces" : a list of names of subnamespaces.\n
 * "methods" : a list of descriptions of the methods in the namespace.\n
 * "broadcasts" : a list of descriptions of the broadcasts in the namespace.\n
 *
 * @param nms The namespace interface entity.
 * @return A dictionary describing the namespace.
 */
xmmsv_t *
xmmsc_sc_interface_entity_namespace_introspect (xmmsc_sc_interface_entity_t *nms)
{
	x_list_t *node;
	xmmsc_sc_namespace_t *namespace;
	xmmsc_sc_interface_entity_t *child;
	xmmsv_t *methods, *subnamespaces, *broadcasts, *desc;

	methods = xmmsv_new_list ();
	subnamespaces = xmmsv_new_list ();
	broadcasts = xmmsv_new_list ();

	namespace = nms->entity.namespace;

	/* Traverse the list of children */
	node = namespace->children;
	while (node) {
		child = node->data;

		/* Introspect each child and add it to the corresponding list */
		switch (xmmsc_sc_interface_entity_get_type (child)) {
			case XMMSC_SC_INTERFACE_ENTITY_TYPE_NAMESPACE:
				xmmsv_list_append (subnamespaces, child->name);
				break;

			case XMMSC_SC_INTERFACE_ENTITY_TYPE_METHOD:
				desc = xmmsc_sc_interface_entity_method_introspect (child);
				xmmsv_list_append (methods, desc);
				xmmsv_unref (desc);
				break;

			case XMMSC_SC_INTERFACE_ENTITY_TYPE_BROADCAST:
				desc = xmmsc_sc_interface_entity_broadcast_introspect (child);
				xmmsv_list_append (broadcasts, desc);
				xmmsv_unref (desc);
				break;

			default:
				x_internal_error ("invalid interface entity type.");
				break;

		}

		node = x_list_next (node);
	}

	return xmmsv_build_dict (XMMSV_DICT_ENTRY ("name", xmmsv_ref (nms->name)),
	                         XMMSV_DICT_ENTRY ("docstring",
	                                           xmmsv_ref (nms->docstring)),
	                         XMMSV_DICT_ENTRY ("constants",
	                                           xmmsv_ref (namespace->constants)),
	                         XMMSV_DICT_ENTRY ("namespaces", subnamespaces),
	                         XMMSV_DICT_ENTRY ("methods", methods),
	                         XMMSV_DICT_ENTRY ("broadcasts", broadcasts),
	                         XMMSV_DICT_END);
}

/**
 * Create a new namespace structure.
 * Not to be confused with the client API function xmmsc_sc_namespace_new.
 */
static xmmsc_sc_namespace_t *
xmmsc_sc_namespace_new_internal (void)
{
	xmmsc_sc_namespace_t *nms;

	nms = x_new0 (xmmsc_sc_namespace_t, 1);
	if (!nms) {
		x_oom ();
		return NULL;
	}

	nms->children = NULL; /* Initialize later via append() */
	nms->constants = xmmsv_new_dict ();

	return nms;
}

/**
 * Destroy a namespace.
 * @param nms The namespace.
 */
void
xmmsc_sc_namespace_destroy (xmmsc_sc_namespace_t *nms)
{
	x_list_t *c;

	x_return_if_fail (nms);

	/* Destroy all children of this namespace */
	for (c = nms->children; c; c = x_list_next (c)) {
		xmmsc_sc_interface_entity_destroy (c->data);
	}

	x_list_free (nms->children);
	xmmsv_unref (nms->constants);

	free (nms);
	return;
}

/**
 * Add a child to a namespace.
 * The child's name must not already be registered in the namespace.
 * @param nms The namespace.
 * @param child The child interface entity.
 */
bool
xmmsc_sc_namespace_add_child (xmmsc_sc_namespace_t *nms,
                              xmmsc_sc_interface_entity_t *child)
{
	x_return_val_if_fail (nms, false);
	x_return_val_if_fail (!xmmsc_sc_namespace_lookup_child (nms, child->name),
	                      false);

	nms->children = x_list_append (nms->children, (void *) child);
	return true;
}

/**
 * Add a constant key, value pair to a namespace.
 * @param nms The namespace.
 * @param key The key to the constant.
 * @param value The value of the constant.
 */
bool
xmmsc_sc_namespace_add_constant (xmmsc_sc_namespace_t *nms, const char *key,
                                 xmmsv_t *value)
{
	x_return_val_if_fail (nms, false);

	return xmmsv_dict_set (nms->constants, key, value);
}

/**
 * Remove a symbol from a namespace. The relative path can point to a
 * subnamespace, a method or a broadcast.
 * @param nms The parent namespace.
 * @param path The relative path of the symbol.
 */
void
xmmsc_sc_namespace_remove_constant (xmmsc_sc_namespace_t *nms, const char *key)
{
	x_return_if_fail (nms);
	x_return_if_fail (key);

	xmmsv_dict_remove (nms->constants, key);
}

/**
 * Look up a child by name in a namespace.
 * @param nms The namespace.
 * @param name The name of the child.
 * @return The interface entity for the child, or NULL if not found.
 */
static xmmsc_sc_interface_entity_t *
xmmsc_sc_namespace_lookup_child (xmmsc_sc_namespace_t *nms, xmmsv_t *name)
{
	x_list_t *child;
	xmmsc_sc_interface_entity_t *ifent;
	const char *childname, *targetname;

	x_return_null_if_fail (nms);
	x_return_null_if_fail (name);

	xmmsv_get_string (name, &targetname);
	for (child = nms->children; child; child = x_list_next (child)) {
		ifent = child->data;

		xmmsv_get_string (ifent->name, &childname);
		if (!strcmp (targetname, childname)) {
			return ifent;
		}
	}

	return NULL;
}

/**
 * Resolve a path into an interface entity.
 * @param root The namespace from which to start.
 * @param path A list of strings for each entity along the path.
 * @param parent A return location to store the parent namespace, or NULL.
 * @return The interface entity, or NULL if not found.
 */
xmmsc_sc_interface_entity_t *
xmmsc_sc_namespace_resolve_path (xmmsc_sc_namespace_t *root,
                                 xmmsv_t *path,
                                 xmmsc_sc_namespace_t **parent)
{
	xmmsv_t *name;
	xmmsv_list_iter_t *it;
	xmmsc_sc_interface_entity_t *ifent = NULL;
	xmmsc_sc_namespace_t *last_ns = NULL;

	x_return_null_if_fail (root);
	x_return_null_if_fail (path);

	xmmsv_get_list_iter (path, &it);
	while (xmmsv_list_iter_valid (it)) {
		last_ns = root;

		xmmsv_list_iter_entry (it, &name);
		xmmsv_list_iter_next (it);

		ifent = xmmsc_sc_namespace_lookup_child (root, name);
		if (!ifent) {
			return NULL;
		}

		if (ifent->type == XMMSC_SC_INTERFACE_ENTITY_TYPE_NAMESPACE) {
			root = ifent->entity.namespace;
		} else {
			break;
		}
	}

	if (xmmsv_list_iter_valid (it)) {
		/* Path too large */
		last_ns = NULL;
		ifent = NULL;
	}

	if (parent) {
		*parent = last_ns;
	}

	return ifent;
}
