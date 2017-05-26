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


#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>
#include <stdlib.h>

#include <xmms/xmms_log.h>
#include <xmmspriv/xmms_xform.h>

static GList *magic_list, *ext_list;

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
	XMMS_MAGIC_ENTRY_TYPE_STRING,
	XMMS_MAGIC_ENTRY_TYPE_STRINGC,
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

typedef struct xmms_magic_checker_St {
	xmms_xform_t *xform;
	gchar *buf;
	guint alloc;
	guint read;
	guint offset;
	gint dumpcount;
} xmms_magic_checker_t;

typedef struct xmms_magic_ext_data_St {
	gchar *type;
	gchar *pattern;
} xmms_magic_ext_data_t;

static void xmms_magic_tree_free (GNode *tree);

static gchar *xmms_magic_match (xmms_magic_checker_t *c, const gchar *u);
static guint xmms_magic_complexity (GNode *tree);

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
		{"string/c", XMMS_MAGIC_ENTRY_TYPE_STRINGC, G_BYTE_ORDER},
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
	switch (entry->type) {
		case XMMS_MAGIC_ENTRY_TYPE_STRING:
		case XMMS_MAGIC_ENTRY_TYPE_STRINGC:
			break;
		default:
			entry->oper = parse_oper (&end);
			break;
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
		case XMMS_MAGIC_ENTRY_TYPE_STRINGC:
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

static void
xmms_magic_tree_free (GNode *tree)
{
	g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
	                 (GNodeTraverseFunc) free_node, NULL);
}

static GNode *
xmms_magic_add_node (GNode *tree, const gchar *s, GNode *prev_node)
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

	return xmms_xform_peek (c->xform, c->buf, needed, &e);
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
		case XMMS_MAGIC_ENTRY_TYPE_STRINGC:
			return !g_ascii_strncasecmp (ptr, entry->value.s, entry->len);
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
		if (node_match (c, n) && tree_match (c, n)) {
			return TRUE;
		}
	}

	return FALSE;
}

static gchar *
xmms_magic_match (xmms_magic_checker_t *c, const gchar *uri)
{
	const GList *l;
	gchar *u, *dump;
	int i;

	g_return_val_if_fail (c, NULL);

	/* only one of the contained sets has to match */
	for (l = magic_list; l; l = g_list_next (l)) {
		GNode *tree = l->data;

		if (tree_match (c, tree)) {
			gpointer *data = tree->data;
			XMMS_DBG ("magic plugin detected '%s' (%s)",
			          (char *)data[1], (char *)data[0]);
			return (char *) (data[1]);
		}
	}

	if (!uri)
		return NULL;

	u = g_ascii_strdown (uri, -1);
	for (l = ext_list; l; l = g_list_next (l)) {
		xmms_magic_ext_data_t *e = l->data;
		if (g_pattern_match_simple (e->pattern, u)) {
			XMMS_DBG ("magic plugin detected '%s' (by extension '%s')", e->type, e->pattern);
			g_free (u);
			return e->type;
		}
	}
	g_free (u);

	if (c->dumpcount > 0) {
		dump = g_malloc ((MIN (c->read, c->dumpcount) * 3) + 1);
		u = dump;

		XMMS_DBG ("Magic didn't match anything...");
		for (i = 0; i < c->dumpcount && i < c->read; i++) {
			g_sprintf (u, "%02X ", (unsigned char)c->buf[i]);
			u += 3;
		}
		XMMS_DBG ("%s", dump);

		g_free (dump);
	}

	return NULL;
}

static guint
xmms_magic_complexity (GNode *tree)
{
	return g_node_n_nodes (tree, G_TRAVERSE_ALL);
}

static gint
cb_sort_magic_list (GNode *a, GNode *b)
{
	guint n1, n2;

	n1 = xmms_magic_complexity (a);
	n2 = xmms_magic_complexity (b);

	if (n1 > n2) {
		return -1;
	} else if (n1 < n2) {
		return 1;
	} else {
		return 0;
	}
}


gboolean
xmms_magic_extension_add (const gchar *mime, const gchar *ext)
{
	xmms_magic_ext_data_t *e;

	g_return_val_if_fail (mime, FALSE);
	g_return_val_if_fail (ext, FALSE);

	e = g_new0 (xmms_magic_ext_data_t, 1);
	e->pattern = g_strdup (ext);
	e->type = g_strdup (mime);

	ext_list = g_list_prepend (ext_list, e);

	return TRUE;
}

gboolean
xmms_magic_add (const gchar *desc, const gchar *mime, ...)
{
	GNode *tree, *node = NULL;
	va_list ap;
	gchar *s;
	gpointer *root_props;
	gboolean ret = TRUE;

	g_return_val_if_fail (desc, FALSE);
	g_return_val_if_fail (mime, FALSE);

	/* now process the magic specs in the argument list */
	va_start (ap, mime);

	s = va_arg (ap, gchar *);
	if (!s) { /* no magic specs passed -> failure */
		va_end (ap);
		return FALSE;
	}

	/* root node stores the description and the mimetype */
	root_props = g_new0 (gpointer, 2);
	root_props[0] = g_strdup (desc);
	root_props[1] = g_strdup (mime);
	tree = g_node_new (root_props);

	do {
		if (!*s) {
			ret = FALSE;
			xmms_log_error ("invalid magic spec: '%s'", s);
			break;
		}

		s = g_strdup (s); /* we need our own copy */
		node = xmms_magic_add_node (tree, s, node);

		if (!node) {
			xmms_log_error ("invalid magic spec: '%s'", s);
			ret = FALSE;

			g_free (s);
			break;
		}
		g_free (s);
	} while ((s = va_arg (ap, gchar *)));

	va_end (ap);

	/* only add this tree to the list if all spec chunks are valid */
	if (ret) {
		magic_list =
			g_list_insert_sorted (magic_list, tree,
			                      (GCompareFunc) cb_sort_magic_list);
	} else {
		xmms_magic_tree_free (tree);
	}

	return ret;
}

static gboolean
xmms_magic_plugin_init (xmms_xform_t *xform)
{
	xmms_magic_checker_t c;
	gchar *res;
	const gchar *url;
	xmms_config_property_t *cv;

	c.xform = xform;
	c.read = c.offset = 0;
	c.alloc = 128; /* start with a 128 bytes buffer */
	c.buf = g_malloc (c.alloc);

	cv = xmms_xform_config_lookup (xform, "dumpcount");
	c.dumpcount = xmms_config_property_get_int (cv);

	url = xmms_xform_indata_find_str (xform, XMMS_STREAM_TYPE_URL);

	res = xmms_magic_match (&c, url);
	if (res) {
		xmms_xform_metadata_set_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_MIME, res);
		xmms_xform_outdata_type_add (xform,
		                             XMMS_STREAM_TYPE_MIMETYPE,
		                             res,
		                             XMMS_STREAM_TYPE_END);
	}

	g_free (c.buf);

	return !!res;
}

static gboolean
xmms_magic_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_magic_plugin_init;
	methods.read = xmms_xform_read;
	methods.seek = xmms_xform_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/octet-stream",
	                              XMMS_STREAM_TYPE_END);

	xmms_xform_plugin_config_property_register (xform_plugin, "dumpcount",
	                                            "16", NULL, NULL);

	return TRUE;
}

XMMS_XFORM_BUILTIN_DEFINE (magic,
                           "Magic file identifier",
                           XMMS_VERSION,
                           "Magic file identifier",
                           xmms_magic_plugin_setup);
