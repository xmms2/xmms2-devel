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

/**
 * @file
 * xforms
 */

#include <glib.h>

#include <xmmspriv/xmms_xform.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_object.h>


struct xmms_stream_type_St {
	xmms_object_t obj;
	gint priority;
	gchar *name;
	GList *list;
};

typedef enum xmms_stream_type_val_type_E {
	STRING,
	INT,
} xmms_stream_type_val_type_t;

typedef struct xmms_stream_type_val_St {
	xmms_stream_type_key_t key;
	xmms_stream_type_val_type_t type;
	union {
		char *string;
		int num;
	} d;
} xmms_stream_type_val_t;


static void
xmms_stream_type_destroy (xmms_object_t *obj)
{
	xmms_stream_type_t *st = (xmms_stream_type_t *)obj;
	GList *n;

	g_free (st->name);

	for (n = st->list; n; n = g_list_next (n)) {
		xmms_stream_type_val_t *val = n->data;
		if (val->type == STRING) {
			g_free (val->d.string);
		}
		g_free (val);
	}

	g_list_free (st->list);
}

xmms_stream_type_t *
xmms_stream_type_parse (va_list ap)
{
	xmms_stream_type_t *res;

	res = xmms_object_new (xmms_stream_type_t, xmms_stream_type_destroy);
	if (!res) {
		return NULL;
	}

	res->priority = -1;
	res->name = NULL;

	for (;;) {
		xmms_stream_type_val_t *val;
		xmms_stream_type_key_t key;

		key = va_arg (ap, int);
		if (key == XMMS_STREAM_TYPE_END)
			break;

		if (key == XMMS_STREAM_TYPE_NAME) {
			res->name = g_strdup (va_arg (ap, char *));
			continue;
		}

		if (key == XMMS_STREAM_TYPE_PRIORITY) {
			res->priority = va_arg (ap, int);
			continue;
		}

		val = g_new0 (xmms_stream_type_val_t, 1);
		val->key = key;

		switch (val->key) {
		case XMMS_STREAM_TYPE_MIMETYPE:
		case XMMS_STREAM_TYPE_URL:
			val->type = STRING;
			val->d.string = g_strdup (va_arg (ap, char *));
			break;
		case XMMS_STREAM_TYPE_FMT_FORMAT:
		case XMMS_STREAM_TYPE_FMT_CHANNELS:
		case XMMS_STREAM_TYPE_FMT_SAMPLERATE:
			val->type = INT;
			val->d.num = va_arg (ap, int);
			break;
		default:
			XMMS_DBG ("UNKNOWN TYPE!!");
			g_free (val);
			xmms_object_unref (res);
			return NULL;
		}
		res->list = g_list_append (res->list, val);
	}

	if (!res->name) {
		const gchar *mime = xmms_stream_type_get_str (res, XMMS_STREAM_TYPE_MIMETYPE);
		const gchar *url = xmms_stream_type_get_str (res, XMMS_STREAM_TYPE_URL);

		if (mime && url) {
			res->name = g_strconcat (mime, ":", url, NULL);
		} else if (mime) {
			res->name = g_strdup (mime);
		} else {
			g_assert_not_reached ();
		}

		g_strdelimit (res->name, ".", '_');
	}

	if (res->priority < 0) {
		res->priority = XMMS_STREAM_TYPE_PRIORITY_DEFAULT;
	}

	return res;
}

const char *
xmms_stream_type_get_str (const xmms_stream_type_t *st, xmms_stream_type_key_t key)
{
	GList *n;

	if (key == XMMS_STREAM_TYPE_NAME) {
		return st->name;
	}

	for (n = st->list; n; n = g_list_next (n)) {
		xmms_stream_type_val_t *val = n->data;
		if (val->key == key) {
			if (val->type != STRING) {
				XMMS_DBG ("Key passed to get_str is not string");
				return NULL;
			}
			return val->d.string;
		}
	}
	return NULL;
}


gint
xmms_stream_type_get_int (const xmms_stream_type_t *st, xmms_stream_type_key_t key)
{
	GList *n;

	if (key == XMMS_STREAM_TYPE_PRIORITY) {
		return st->priority;
	}

	for (n = st->list; n; n = g_list_next (n)) {
		xmms_stream_type_val_t *val = n->data;
		if (val->key == key) {
			if (val->type != INT) {
				XMMS_DBG ("Key passed to get_int is not int");
				return -1;
			}
			return val->d.num;
		}
	}
	return -1;
}




static gboolean
match_val (xmms_stream_type_val_t *vin, xmms_stream_type_val_t *vout)
{
	if (vin->type != vout->type)
		return FALSE;
	switch (vin->type) {
	case STRING:
		return g_pattern_match_simple (vin->d.string, vout->d.string);
	case INT:
		return vin->d.num == vout->d.num;
	}
	return FALSE;
}

gboolean
xmms_stream_type_match (const xmms_stream_type_t *in_type, const xmms_stream_type_t *out_type)
{
	GList *in;

	for (in = in_type->list; in; in = g_list_next (in)) {
		xmms_stream_type_val_t *inval = in->data;
		GList *n;

		for (n = out_type->list; n; n = g_list_next (n)) {
			xmms_stream_type_val_t *outval = n->data;
			if (inval->key == outval->key) {
				if (!match_val (inval, outval))
					return FALSE;
				break;
			}

		}
		if (!n) {
			/* didn't exist in out */
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * Find the best pair of formats
 */
xmms_stream_type_t *
xmms_stream_type_coerce (const xmms_stream_type_t *in, const GList *goal_types)
{
	xmms_stream_type_t *best = NULL;
	const GList *on;
/*	gint bestscore = GINT_MAX;*/
	gint bestscore = 100000;
	gint format, samplerate, channels;
	gint gformat, gsamplerate, gchannels;
	const gchar *gmime;

	format = xmms_stream_type_get_int (in, XMMS_STREAM_TYPE_FMT_FORMAT);
	samplerate = xmms_stream_type_get_int (in, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	channels = xmms_stream_type_get_int (in, XMMS_STREAM_TYPE_FMT_CHANNELS);

	if (format == -1 || samplerate == -1 || channels == -1) {
		xmms_log_info ("In-type lacks format, samplerate or channels");
		return NULL;
	}

	for (on = goal_types ; on; on = g_list_next (on)) {
		xmms_stream_type_t *goal = on->data;
		const gchar *mime;
		gint score = 0;

		mime = xmms_stream_type_get_str (goal, XMMS_STREAM_TYPE_MIMETYPE);
		if (strcmp (mime, "audio/pcm") != 0) {
			continue;
		}

		gformat = xmms_stream_type_get_int (goal, XMMS_STREAM_TYPE_FMT_FORMAT);
		gsamplerate = xmms_stream_type_get_int (goal, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
		gchannels = xmms_stream_type_get_int (goal, XMMS_STREAM_TYPE_FMT_CHANNELS);
		if (gsamplerate == -1) {
			gsamplerate = samplerate;
		}
		if (gformat == -1 || gchannels == -1) {
			continue;
		}


		if (gchannels > channels) {
			/* we loose no quality, just cputime */
			score += gchannels - channels;
		} else if (gchannels < channels) {
			/* quality loss! */
			score += 10 * (channels - gchannels);
		}

		/* the format enum should be ordered in
		   quality order */
		if (gformat > format) {
			/* we loose no quality, just cputime */
			score += gformat - format;
		} else if (gformat < format) {
			/* quality loss! */
			score += 10 * (format - gformat);
		}


		if (gsamplerate > samplerate) {
			/* we loose no quality, just cputime */
			score += 2 * gsamplerate / samplerate;
		} else if (gsamplerate < samplerate) {
			/* quality loss! */
			score += 20 * samplerate / gsamplerate;
		}

		if (score < bestscore) {
			best = goal;
			bestscore = score;
		}

	}

	if (!best) {
		xmms_log_error ("Couldn't convert sample format to any of the %d goal formats",
		                g_list_length ((GList *)goal_types));
		return NULL;
	}

	gmime = xmms_stream_type_get_str (best, XMMS_STREAM_TYPE_MIMETYPE);
	gformat = xmms_stream_type_get_int (best, XMMS_STREAM_TYPE_FMT_FORMAT);
	gchannels = xmms_stream_type_get_int (best, XMMS_STREAM_TYPE_FMT_CHANNELS);
	gsamplerate = xmms_stream_type_get_int (best, XMMS_STREAM_TYPE_FMT_SAMPLERATE);

	/* Use the requested samplerate if target accepts any. */
	if (gsamplerate == -1) {
		gsamplerate = samplerate;
	}

	best = _xmms_stream_type_new (XMMS_STREAM_TYPE_BEGIN,
	                              XMMS_STREAM_TYPE_MIMETYPE, gmime,
	                              XMMS_STREAM_TYPE_FMT_FORMAT, gformat,
	                              XMMS_STREAM_TYPE_FMT_CHANNELS, gchannels,
	                              XMMS_STREAM_TYPE_FMT_SAMPLERATE, gsamplerate,
	                              XMMS_STREAM_TYPE_END);

	return best;
}



/*
	XMMS_DBG ("Looking for xform with intypes matching:");
	for (n = prev->out_types; n; n = g_list_next (n)) {
		xmms_stream_type_val_t *val = n->data;
		switch (val->type) {
		case INT:
			XMMS_DBG (" - %d = %d", val->key, val->d.num);
			break;
		case STRING:
			XMMS_DBG (" - %d = '%s'", val->key, val->d.string);
			break;
		default:
			XMMS_DBG (" - ????");
			break;
		}
	}

*/

xmms_stream_type_t *
_xmms_stream_type_new (const gchar *begin, ...)
{
	xmms_stream_type_t *res;
	va_list ap;

	va_start (ap, begin);
	res = xmms_stream_type_parse (ap);
	va_end (ap);

	return res;
}
