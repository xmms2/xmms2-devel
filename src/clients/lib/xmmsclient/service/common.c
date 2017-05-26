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

#include "common.h"

/**
 * Resolve a path to a registered interface entity.
 * An empty path resolves to the root namespace.
 */
xmmsc_sc_interface_entity_t *
xmmsc_sc_locate_interface_entity (xmmsc_connection_t *c, xmmsv_t *path)
{
	xmmsc_sc_namespace_t *root;

	if (!xmmsv_list_get_size (path)) {
		return c->sc_root;
	}

	root = xmmsc_sc_interface_entity_get_namespace (c->sc_root);
	return xmmsc_sc_namespace_resolve_path (root, path, NULL);
}

void
xmmsc_sc_create_root_namespace (xmmsc_connection_t *c)
{
	xmmsv_t *clientname;
	xmmsc_sc_namespace_t *new_nms;

	if (c->sc_root) {
		x_internal_error ("Attempted to re-create root namespace.");
		return;
	}

	c->sc_root = xmmsc_sc_interface_entity_new_namespace ("", "");

	/* Register client name as constant */
	new_nms = xmmsc_sc_interface_entity_get_namespace (c->sc_root);

	clientname = xmmsv_new_string (c->clientname);
	xmmsc_sc_namespace_add_constant (new_nms, "name", clientname);
	xmmsv_unref (clientname);
}
