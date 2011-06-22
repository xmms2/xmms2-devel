/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"

#include <libxml/xmlreader.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

typedef struct xmms_xspf_track_attr_St {
	const gchar *key;
	xmmsv_t *value;
} xmms_xspf_track_attr_t;

typedef struct xmms_xspf_track_St {
	GList *attrs;
	gchar *location;
} xmms_xspf_track_t;

typedef enum {
	XMMS_XSPF_ATTR_LOCATION,
	XMMS_XSPF_ATTR_PROP
} xmms_xspf_track_data_type_t;

typedef enum {
	XMMS_XSPF_TRACK_ATTR_TYPE_INT32,
	XMMS_XSPF_TRACK_ATTR_TYPE_STRING
} xmms_xspf_track_attr_type_t;

typedef struct xmms_xspf_track_prop_St {
	const gchar *name;
	xmms_xspf_track_data_type_t type;
	xmms_xspf_track_attr_type_t attr_type;
} xmms_xspf_track_prop_t;

static const xmms_xspf_track_prop_t xmms_xspf_track_props[] = {
	{ "location",   XMMS_XSPF_ATTR_LOCATION, XMMS_XSPF_TRACK_ATTR_TYPE_STRING },
	{ "identifier", XMMS_XSPF_ATTR_PROP,     XMMS_XSPF_TRACK_ATTR_TYPE_STRING },
	{ "title",      XMMS_XSPF_ATTR_PROP,     XMMS_XSPF_TRACK_ATTR_TYPE_STRING },
	{ "creator",    XMMS_XSPF_ATTR_PROP,     XMMS_XSPF_TRACK_ATTR_TYPE_STRING },
	{ "annotation", XMMS_XSPF_ATTR_PROP,     XMMS_XSPF_TRACK_ATTR_TYPE_STRING },
	{ "info",       XMMS_XSPF_ATTR_PROP,     XMMS_XSPF_TRACK_ATTR_TYPE_STRING },
	{ "image",      XMMS_XSPF_ATTR_PROP,     XMMS_XSPF_TRACK_ATTR_TYPE_STRING },
	{ "album",      XMMS_XSPF_ATTR_PROP,     XMMS_XSPF_TRACK_ATTR_TYPE_STRING },
	{ "trackNum",   XMMS_XSPF_ATTR_PROP,     XMMS_XSPF_TRACK_ATTR_TYPE_INT32  },
	{ "duration",   XMMS_XSPF_ATTR_PROP,     XMMS_XSPF_TRACK_ATTR_TYPE_INT32  },
	{ NULL, 0, 0 } /* TODO: link, meta, extension */
};

static gboolean xmms_xspf_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_xspf_init (xmms_xform_t *xform);
static gboolean xmms_xspf_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error);

XMMS_XFORM_PLUGIN ("xspf",
                   "reader for xspf playlists",
                   XMMS_VERSION,
                   "reader for xspf playlists",
                   xmms_xspf_plugin_setup);

static gboolean
xmms_xspf_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_xspf_init;
	methods.browse = xmms_xspf_browse;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-xmms2-xml+playlist",
	                              XMMS_STREAM_TYPE_END);

	xmms_magic_extension_add ("application/x-xmms2-xml+playlist", "*.xspf");

	return TRUE;
}

static gboolean
xmms_xspf_init (xmms_xform_t *xform)
{
	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/x-xmms2-playlist-entries",
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

static xmms_xspf_track_attr_t *
xmms_xspf_track_attr_from_node (const xmms_xspf_track_prop_t *prop, xmlNodePtr node)
{
	xmmsv_t *value;
	xmms_xspf_track_attr_t *attr;

	switch (prop->attr_type) {
		case XMMS_XSPF_TRACK_ATTR_TYPE_STRING:
			value = xmmsv_new_string ((char *)node->children->content);
			break;
		case XMMS_XSPF_TRACK_ATTR_TYPE_INT32: {
			/* TODO: check for errors */
			gint32 val = strtol ((char *)node->children->content, (char **)NULL, 10);
			value = xmmsv_new_int (val);
			break;
		}
	}

	if (!value) {
		return NULL;
	}

	attr = g_new0 (xmms_xspf_track_attr_t, 1);
	attr->key = prop->name;
	attr->value = value;

	return attr;
}

static xmms_xspf_track_t *
xmms_xspf_parse_track_node (xmms_xform_t *xform, xmlNodePtr node, xmms_error_t *error)
{
	xmlNodePtr cur;
	xmms_xspf_track_t *track;

	track = g_new0 (xmms_xspf_track_t, 1);

	cur = node->children;
	while (cur != NULL) {
		const xmms_xspf_track_prop_t *prop;

		for (prop = xmms_xspf_track_props; prop->name != NULL; prop++) {
			if (xmlStrEqual (cur->name, BAD_CAST prop->name)) {
				switch (prop->type) {
					case XMMS_XSPF_ATTR_LOCATION:
						track->location = (char *)cur->children->content;
						break;
					case XMMS_XSPF_ATTR_PROP: {
						xmms_xspf_track_attr_t *attr;
						attr = xmms_xspf_track_attr_from_node (prop, cur);

						if (attr) {
							track->attrs = g_list_prepend (track->attrs, attr);
						}

						break;
					}
				}
			}
		}

		cur = cur->next;
	}

	return track;
}

static gboolean
xmms_xspf_check_valid_xspf (xmlDocPtr doc, xmlXPathContextPtr xpath,
                            xmms_error_t *error)
{
	xmlXPathObjectPtr obj;

	obj = xmlXPathEvalExpression (BAD_CAST "/xspf:playlist[@version<=1]",
	                              xpath);

	if (!obj) {
		xmms_error_set (error, XMMS_ERROR_INVAL,
		                "unable to evaluate xpath expression");
		return FALSE;
	}

	if (xmlXPathNodeSetGetLength (obj->nodesetval) != 1) {
		xmms_error_set (error, XMMS_ERROR_INVAL,
		                "xspf document doesn't contain a version 0 or 1 "
		                "playlist");
		xmlXPathFreeObject (obj);
		return FALSE;
	}

	xmlXPathFreeObject (obj);
	obj = xmlXPathEvalExpression (BAD_CAST "/xspf:playlist[@version<=1]/xspf:trackList",
	                              xpath);

	if (!obj) {
		xmms_error_set (error, XMMS_ERROR_INVAL,
		                "unable to evaluate xpath expression");
		return FALSE;
	}

	if (xmlXPathNodeSetGetLength (obj->nodesetval) != 1) {
		xmms_error_set (error, XMMS_ERROR_INVAL,
		                "invalid xspf: document doesn't contain exactly one trackList");
		xmlXPathFreeObject (obj);
		return FALSE;
	}

	return TRUE;
}

static gboolean
xmms_xspf_browse_add_entries (xmms_xform_t *xform, xmlDocPtr doc,
                              xmms_error_t *error)
{
	int i;
	xmlXPathContextPtr xpath;
	xmlXPathObjectPtr obj;
	const xmlChar *playlist_image = NULL;

	xpath = xmlXPathNewContext (doc);
	xmlXPathRegisterNs (xpath,
	                    BAD_CAST "xspf",
	                    BAD_CAST "http://xspf.org/ns/0/");

	if (!xmms_xspf_check_valid_xspf (doc, xpath, error)) {
		xmlXPathFreeContext (xpath);
		return FALSE;
	}

	obj = xmlXPathEvalExpression (BAD_CAST "/xspf:playlist[@version<=1]/xspf:image/text()/..",
	                              xpath);

	if (!obj) {
		xmms_error_set (error, XMMS_ERROR_INVAL,
		                "unable to evaluate xpath expression");
		xmlXPathFreeContext (xpath);
		return FALSE;
	}

	if (xmlXPathNodeSetGetLength (obj->nodesetval) == 1) {
		playlist_image = xmlXPathNodeSetItem (obj->nodesetval, 0)->children->content;
	}

	xmlXPathFreeObject (obj);
	obj = xmlXPathEvalExpression (BAD_CAST "/xspf:playlist[@version<=1]/xspf:trackList/xspf:track/xspf:location/text()/../..",
	                              xpath);

	if (!obj) {
		xmms_error_set (error, XMMS_ERROR_INVAL,
		                "unable to evaluate xpath expression");
		xmlXPathFreeContext (xpath);
		return FALSE;
	}

	for (i = 0; i < xmlXPathNodeSetGetLength (obj->nodesetval); i++) {
		xmms_xspf_track_t *track;
		GList *attr;

		track = xmms_xspf_parse_track_node (xform,
		                                    xmlXPathNodeSetItem (obj->nodesetval, i),
		                                    error);

		if (!track) {
			continue;
		}

		xmms_xform_browse_add_symlink (xform, NULL, track->location);

		if (playlist_image) {
			xmms_xform_browse_add_entry_property_str (xform, "image", (char *)playlist_image);
		}

		for (attr = track->attrs; attr; attr = g_list_next (attr)) {
			xmms_xform_browse_add_entry_property (xform,
			                                      ((xmms_xspf_track_attr_t *)attr->data)->key,
			                                      ((xmms_xspf_track_attr_t *)attr->data)->value);
		}

		g_list_free (track->attrs);
		g_free (track);
	}

	xmlXPathFreeObject (obj);
	xmlXPathFreeContext (xpath);
	return TRUE;
}

static gboolean
xmms_xspf_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error)
{
	int ret;
	char buf[4096];
	xmlParserCtxtPtr ctx;
	xmlDocPtr doc;

	g_return_val_if_fail (xform, FALSE);

	xmms_error_reset (error);

	ctx = xmlCreatePushParserCtxt (NULL, NULL, buf, 0, NULL);

	if (!ctx) {
		xmms_error_set (error, XMMS_ERROR_OOM, "Could not allocate xml parser");
		return FALSE;
	}

	while ((ret = xmms_xform_read (xform, buf, sizeof (buf), error)) > 0) {
		if ((xmlParseChunk (ctx, buf, ret, 0)) != 0) {
			break;
		}
	}

	if (ret < 0) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "failed to read data from previous xform");
		xmlFreeParserCtxt (ctx);
		return FALSE;
	}

	xmlParseChunk (ctx, buf, 0, 1);

	if (ctx->lastError.message) {
		xmms_error_set (error, XMMS_ERROR_INVAL, ctx->lastError.message);
		xmlFreeParserCtxt (ctx);
		return FALSE;
	}

	doc = ctx->myDoc;

	if (!xmms_xspf_browse_add_entries (xform, doc, error)) {
		xmlFreeParserCtxt (ctx);
		return FALSE;
	}

	xmms_error_reset (error);
	xmlFreeParserCtxt (ctx);

	return TRUE;
}
