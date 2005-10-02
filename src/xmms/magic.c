/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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



#include <glib.h>
#include <string.h>
#include <stdlib.h>

#include "xmms/xmms_log.h"
#include "xmmspriv/xmms_magic.h"

#define SWAP16(v, endian) \
	if (endian == G_LITTLE_ENDIAN) { \
		v = GUINT16_TO_LE (v); \
	} else if (endian == G_BIG_ENDIAN) { \
		v = GUINT16_TO_BE (v); \
	}

#define SWAP32(v, endian) \
	if (endian == G_LITTLE_ENDIAN) { \
		v = GUINT32_TO_LE (v); \
	} else if (endian == G_BIG_ENDIAN) { \
		v = GUINT32_TO_BE (v); \
	}

#define CMP(v1, entry, v2) \
	if (entry->pre_test_and_op) { \
		v1 &= entry->pre_test_and_op; \
	} \
\
	switch (entry->oper) { \
		case XMMS_MAGIC_ENTRY_OPERATOR_EQUAL: \
			return v1 == v2; \
		case XMMS_MAGIC_ENTRY_OPERATOR_LESS_THAN: \
			return v1 < v2; \
		case XMMS_MAGIC_ENTRY_OPERATOR_GREATER_THAN: \
			return v1 > v2; \
		case XMMS_MAGIC_ENTRY_OPERATOR_AND: \
			return (v1 & v2) == v2; \
		case XMMS_MAGIC_ENTRY_OPERATOR_NAND: \
			return (v1 & v2) != v2; \
	} \

typedef enum xmms_magic_entry_type_St {
	XMMS_MAGIC_ENTRY_TYPE_UNKNOWN = 0,
	XMMS_MAGIC_ENTRY_TYPE_BYTE,
	XMMS_MAGIC_ENTRY_TYPE_INT16,
	XMMS_MAGIC_ENTRY_TYPE_INT32,
	XMMS_MAGIC_ENTRY_TYPE_STRING
} xmms_magic_entry_type_t;

typedef enum xmms_magic_entry_operator_St {
	XMMS_MAGIC_ENTRY_OPERATOR_EQUAL = 0,
	XMMS_MAGIC_ENTRY_OPERATOR_LESS_THAN,
	XMMS_MAGIC_ENTRY_OPERATOR_GREATER_THAN,
	XMMS_MAGIC_ENTRY_OPERATOR_AND,
	XMMS_MAGIC_ENTRY_OPERATOR_NAND
} xmms_magic_entry_operator_t;

typedef struct xmms_magic_entry_St {
	guint offset;
	xmms_magic_entry_type_t type;
	gint endian;
	guint len;
	guint pre_test_and_op;
	xmms_magic_entry_operator_t oper;

	union {
		guint8 i8;
		guint16 i16;
		guint32 i32;
		gchar s[32];
	} value;
} xmms_magic_entry_t;

static void
xmms_magic_entry_free (xmms_magic_entry_t *e)
{
	g_free (e);
}

static xmms_magic_entry_type_t
parse_type (gchar **s, gint *endian)
{
	struct {
		const gchar *string;
		xmms_magic_entry_type_t type;
		gint endian;
	} *t, types[] = {
		{"byte", XMMS_MAGIC_ENTRY_TYPE_BYTE, G_BYTE_ORDER},
		{"short", XMMS_MAGIC_ENTRY_TYPE_INT16, G_BYTE_ORDER},
		{"long", XMMS_MAGIC_ENTRY_TYPE_INT16, G_BYTE_ORDER},
		{"beshort", XMMS_MAGIC_ENTRY_TYPE_INT16, G_BIG_ENDIAN},
		{"belong", XMMS_MAGIC_ENTRY_TYPE_INT32, G_BIG_ENDIAN},
		{"leshort", XMMS_MAGIC_ENTRY_TYPE_INT16, G_LITTLE_ENDIAN},
		{"lelong", XMMS_MAGIC_ENTRY_TYPE_INT32, G_LITTLE_ENDIAN},
		{"string", XMMS_MAGIC_ENTRY_TYPE_STRING, G_BYTE_ORDER},
		{NULL, XMMS_MAGIC_ENTRY_TYPE_UNKNOWN, G_BYTE_ORDER}
	};

	for (t = types; t; t++) {
		int l = t->string ? strlen (t->string) : 0;

		if (!l || !strncmp (*s, t->string, l)) {
			*s += l;
			*endian = t->endian;

			return t->type;
		}
	}

	g_assert_not_reached ();
}


static xmms_magic_entry_operator_t
parse_oper (gchar **s)
{
	gchar c = **s;
	struct {
		gchar c;
		xmms_magic_entry_operator_t o;
	} *o, opers[] = {
		{'=', XMMS_MAGIC_ENTRY_OPERATOR_EQUAL},
		{'<', XMMS_MAGIC_ENTRY_OPERATOR_LESS_THAN},
		{'>', XMMS_MAGIC_ENTRY_OPERATOR_GREATER_THAN},
		{'&', XMMS_MAGIC_ENTRY_OPERATOR_AND},
		{'^', XMMS_MAGIC_ENTRY_OPERATOR_NAND},
		{'\0', XMMS_MAGIC_ENTRY_OPERATOR_EQUAL}
	};

	for (o = opers; o; o++) {
		if (!o->c) {
			/* no operator found */
			return o->o;
		} else if (c == o->c) {
			(*s)++; /* skip operator */
			return o->o;
		}
	}

	g_assert_not_reached ();
}

static gboolean
parse_pre_test_and_op (xmms_magic_entry_t *entry, gchar **end)
{
	gboolean ret = FALSE;

	if (**end == ' ') {
		(*end)++;
		return TRUE;
	}

	switch (entry->type) {
		case XMMS_MAGIC_ENTRY_TYPE_BYTE:
		case XMMS_MAGIC_ENTRY_TYPE_INT16:
		case XMMS_MAGIC_ENTRY_TYPE_INT32:
			if (**end == '&') {
				(*end)++;
				entry->pre_test_and_op = strtoul (*end, end, 0);
				ret = TRUE;
			}
		default:
			break;
	}

	return ret;
}

static xmms_magic_entry_t *
parse_entry (const gchar *s)
{
	xmms_magic_entry_t *entry;
	gchar *end = NULL;

	entry = g_new0 (xmms_magic_entry_t, 1);
	entry->endian = G_BYTE_ORDER;
	entry->oper = XMMS_MAGIC_ENTRY_OPERATOR_EQUAL;
	entry->offset = strtoul (s, &end, 0);

	end++;

	entry->type = parse_type (&end, &entry->endian);
	if (entry->type == XMMS_MAGIC_ENTRY_TYPE_UNKNOWN) {
		g_free (entry);
		return NULL;
	}

	if (!parse_pre_test_and_op (entry, &end)) {
		g_free (entry);
		return NULL;
	}

	/* @todo Implement string operators */
	if (entry->type != XMMS_MAGIC_ENTRY_TYPE_STRING) {
		entry->oper = parse_oper (&end);
	}

	switch (entry->type) {
		case XMMS_MAGIC_ENTRY_TYPE_BYTE:
			entry->value.i8 = strtoul (end, &end, 0);
			entry->len = 1;
			break;
		case XMMS_MAGIC_ENTRY_TYPE_INT16:
			entry->value.i16 = strtoul (end, &end, 0);
			entry->len = 2;
			break;
		case XMMS_MAGIC_ENTRY_TYPE_INT32:
			entry->value.i32 = strtoul (end, &end, 0);
			entry->len = 4;
			break;
		case XMMS_MAGIC_ENTRY_TYPE_STRING:
			g_strlcpy (entry->value.s, end, sizeof (entry->value.s));
			entry->len = strlen (entry->value.s);
			break;
		default:
			break; /* won't get here, handled above */
	}

	return entry;
}

static gboolean
free_node (GNode *node, xmms_magic_entry_t *entry)
{
	if (G_NODE_IS_ROOT (node)) {
		gpointer *data = node->data;

		/* this isn't a magic entry, but the description of the tree */
		g_free (data[0]); /* desc */
		g_free (data[1]); /* mime */
		g_free (data);
	} else {
		xmms_magic_entry_free (entry);
	}

	return FALSE; /* continue traversal */
}

void
xmms_magic_tree_free (GNode *tree)
{
	g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
	                 (GNodeTraverseFunc) free_node, NULL);
}

GNode *
xmms_magic_add (GNode *tree, const gchar *s, GNode *prev_node)
{
	xmms_magic_entry_t *entry;
	gpointer *data = tree->data;
	guint indent = 0, prev_indent;

	g_assert (s);

	XMMS_DBG ("adding magic spec to tree '%s'", (gchar *) data[0]);

	/* indent level is number of leading '>' characters */
	while (*s == '>') {
		indent++;
		s++;
	}

	entry = parse_entry (s);
	if (!entry) {
		XMMS_DBG ("cannot parse magic entry");
		return NULL;
	}

	if (!indent) {
		return g_node_append_data (tree, entry);
	}

	if (!prev_node) {
		XMMS_DBG ("invalid indent level");
		xmms_magic_entry_free (entry);
		return NULL;
	}

	prev_indent = g_node_depth (prev_node) - 2;

	if (indent > prev_indent) {
		/* larger jumps are invalid */
		if (indent != prev_indent + 1) {
			XMMS_DBG ("invalid indent level");
			xmms_magic_entry_free (entry);
			return NULL;
		}

		return g_node_append_data (prev_node, entry);
	} else {
		while (indent < prev_indent) {
			prev_indent--;
			prev_node = prev_node->parent;
		}

		return g_node_insert_after (prev_node->parent, prev_node,
		                            g_node_new (entry));
	}
}

static gint
read_data (xmms_magic_checker_t *c, guint needed)
{
	xmms_error_t e;

	if (needed > c->alloc) {
		c->alloc = needed;
		c->buf = g_realloc (c->buf, c->alloc);
	}

	xmms_error_reset (&e);

	return xmms_transport_peek (c->transport, c->buf, needed, &e);
}

static gboolean
node_match (xmms_magic_checker_t *c, GNode *node)
{
	xmms_magic_entry_t *entry = node->data;
	guint needed = c->offset + entry->offset + entry->len;
	guint8 i8;
	guint16 i16;
	guint32 i32;
	gint tmp;
	gchar *ptr;

	/* do we have enough data ready for this check?
	 * if not, read some more
	 */
	if (c->read < needed) {
		tmp = read_data (c, needed);
		if (tmp == -1) {
			return FALSE;
		}

		c->read = tmp;
		if (c->read < needed) {
			/* couldn't read enough data */
			return FALSE;
		}
	}

	ptr = &c->buf[c->offset + entry->offset];

	switch (entry->type) {
		case XMMS_MAGIC_ENTRY_TYPE_BYTE:
			memcpy (&i8, ptr, sizeof (i8));
			CMP (i8, entry, entry->value.i8); /* returns */
		case XMMS_MAGIC_ENTRY_TYPE_INT16:
			memcpy (&i16, ptr, sizeof (i16));
			SWAP16 (i16, entry->endian);
			CMP (i16, entry, entry->value.i16); /* returns */
		case XMMS_MAGIC_ENTRY_TYPE_INT32:
			memcpy (&i32, ptr, sizeof (i32));
			SWAP32 (i32, entry->endian);
			CMP (i32, entry, entry->value.i32); /* returns */
		case XMMS_MAGIC_ENTRY_TYPE_STRING:
			return !strncmp (ptr, entry->value.s, entry->len);
		default:
			return FALSE;
	}
}

static gboolean
tree_match (xmms_magic_checker_t *c, GNode *tree)
{
	GNode *n;

	/* empty subtrees match anything */
	if (!tree->children) {
		return TRUE;
	}

	for (n = tree->children; n; n = n->next) {
		if (!node_match (c, n) || !tree_match (c, n)) {
			return FALSE;
		}
	}

	return TRUE;
}

guint
tree_bytes_max_needed (xmms_magic_checker_t *c, GNode *tree)
{
	GNode *n;
	guint ret = 0;

	for (n = tree->children; n; n = n->next) {
		xmms_magic_entry_t *entry = n->data;

		ret = MAX (ret, c->offset + entry->offset + entry->len);
		ret = MAX (ret, tree_bytes_max_needed (c, n));
	}

	return ret;
}

GNode *
xmms_magic_match (xmms_magic_checker_t *c, const GList *magic)
{
	const GList *l;

	g_return_val_if_fail (c, NULL);
	g_return_val_if_fail (magic, NULL);

	/* only one of the contained sets has to match */
	for (l = magic; l; l = g_list_next (l)) {
		GNode *tree = l->data;
		gpointer *data = tree->data;
		guint needed;

		XMMS_DBG ("matching tree '%s'", (gchar *) data[0]);

		/* will we be able to read enough data to do all of the checks
		 * specified in this tree?
		 */
		needed = tree_bytes_max_needed (c, tree);
		if (needed > xmms_transport_buffersize (c->transport)) {
			xmms_log ("magic check requires a minimum transport "
			          "buffer size of %i bytes", needed);
		} else if (tree_match (c, tree)) {
			return tree;
		}
	}

	return NULL;
}

guint
xmms_magic_complexity (const GList *magic)
{
	const GList *l;
	guint ret = 0;

	for (l = magic; l; l = g_list_next (l)) {
		ret = MAX (ret, g_node_n_nodes (l->data, G_TRAVERSE_ALL));
	}

	return ret;
}
