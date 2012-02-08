/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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
  * @file vorbisfile decoder.
  * @url http://www.xiph.org/ogg/vorbis/doc/vorbisfile/
  */


#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_medialib.h"
#include "xmms/xmms_bindata.h"

#include <glib.h>

#include <math.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct xmms_vorbis_data_St {
	OggVorbis_File vorbisfile;
	ov_callbacks callbacks;
	gint current;
} xmms_vorbis_data_t;


/*
 * Function prototypes
 */

static void xmms_vorbis_set_duration (xmms_xform_t *xform, guint dur);
static gulong xmms_vorbis_ov_read (OggVorbis_File *vf, gchar *buf, gint len,
                                   gint bigendian, gint sampsize, gint signd,
                                   gint *outbuf);
static gboolean xmms_vorbis_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_vorbis_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static gboolean xmms_vorbis_init (xmms_xform_t *decoder);
static void xmms_vorbis_destroy (xmms_xform_t *decoder);
static gint64 xmms_vorbis_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err);
static gboolean handle_image_comment (xmms_xform_t *xform, const gchar *key, const gchar *encoded_value, gsize length);

/** These are the properties that we extract from the comments */
static const xmms_xform_metadata_basic_mapping_t basic_mappings[] = {
	{ "album",                     XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM             },
	{ "title",                     XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE             },
	{ "artist",                    XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST            },
	{ "albumartist",               XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST      },
	{ "date",                      XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR              },
	{ "originaldate",              XMMS_MEDIALIB_ENTRY_PROPERTY_ORIGINALYEAR      },
	{ "composer",                  XMMS_MEDIALIB_ENTRY_PROPERTY_COMPOSER          },
	{ "lyricist",                  XMMS_MEDIALIB_ENTRY_PROPERTY_LYRICIST          },
	{ "conductor",                 XMMS_MEDIALIB_ENTRY_PROPERTY_CONDUCTOR         },
	{ "performer",                 XMMS_MEDIALIB_ENTRY_PROPERTY_PERFORMER         },
	{ "remixer",                   XMMS_MEDIALIB_ENTRY_PROPERTY_REMIXER           },
	{ "arranger",                  XMMS_MEDIALIB_ENTRY_PROPERTY_ARRANGER          },
	{ "producer",                  XMMS_MEDIALIB_ENTRY_PROPERTY_PRODUCER          },
	{ "mixer",                     XMMS_MEDIALIB_ENTRY_PROPERTY_MIXER             },
	{ "grouping",                  XMMS_MEDIALIB_ENTRY_PROPERTY_GROUPING          },
	{ "grouping",                  XMMS_MEDIALIB_ENTRY_PROPERTY_GROUPING          },
	{ "tracknumber",               XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR           },
	{ "tracktotal",                XMMS_MEDIALIB_ENTRY_PROPERTY_TOTALTRACKS       },
	{ "totaltracks",               XMMS_MEDIALIB_ENTRY_PROPERTY_TOTALTRACKS       },
	{ "discnumber",                XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET         },
	{ "disctotal",                 XMMS_MEDIALIB_ENTRY_PROPERTY_TOTALSET          },
	{ "totaldiscs",                XMMS_MEDIALIB_ENTRY_PROPERTY_TOTALSET          },
	{ "compilation",               XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION       },
	{ "comment",                   XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT           },
	{ "genre",                     XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE             },
	{ "bpm",                       XMMS_MEDIALIB_ENTRY_PROPERTY_BPM               },
	{ "isrc",                      XMMS_MEDIALIB_ENTRY_PROPERTY_ISRC              },
	{ "copyright",                 XMMS_MEDIALIB_ENTRY_PROPERTY_COPYRIGHT         },
	{ "catalognumber",             XMMS_MEDIALIB_ENTRY_PROPERTY_CATALOGNUMBER     },
	{ "barcode",                   XMMS_MEDIALIB_ENTRY_PROPERTY_BARCODE           },
	{ "albumsort",                 XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_SORT        },
	{ "albumartistsort",           XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST_SORT },
	{ "artistsort",                XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_SORT       },
	{ "description",               XMMS_MEDIALIB_ENTRY_PROPERTY_DESCRIPTION       },
	{ "label",                     XMMS_MEDIALIB_ENTRY_PROPERTY_PUBLISHER         },
	{ "asin",                      XMMS_MEDIALIB_ENTRY_PROPERTY_ASIN              },
	{ "conductor",                 XMMS_MEDIALIB_ENTRY_PROPERTY_CONDUCTOR         },
	{ "musicbrainz_albumid",       XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID          },
	{ "musicbrainz_artistid",      XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID         },
	{ "musicbrainz_trackid",       XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID          },
	{ "musicbrainz_albumartistid", XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION       },
	{ "replaygain_track_gain",     XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK        },
	{ "replaygain_album_gain",     XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM        },
	{ "replaygain_track_peak",     XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK        },
	{ "replaygain_album_peak",     XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM        },
};

static const xmms_xform_metadata_mapping_t mappings[] = {
	{ "METADATA_BLOCK_PICTURE", handle_image_comment }
};

/*
 * Plugin header
 */

static gboolean
xmms_vorbis_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_vorbis_init;
	methods.destroy = xmms_vorbis_destroy;
	methods.read = xmms_vorbis_read;
	methods.seek = xmms_vorbis_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_metadata_mapper_init (xform_plugin,
	                                        basic_mappings,
	                                        G_N_ELEMENTS (basic_mappings),
	                                        mappings,
	                                        G_N_ELEMENTS (mappings));

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/ogg",
	                              NULL);


	xmms_magic_add ("ogg/vorbis header",
	                "application/ogg",
	                "0 string OggS", ">4 byte 0",
	                ">>28 string \x01vorbis", NULL);

	xmms_magic_extension_add ("application/ogg", "*.ogg");

	return TRUE;
}

static void
xmms_vorbis_destroy (xmms_xform_t *xform)
{
	xmms_vorbis_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	ov_clear (&data->vorbisfile);
	g_free (data);
}

static size_t
vorbis_callback_read (void *ptr, size_t size, size_t nmemb,
                      void *datasource)
{
	xmms_vorbis_data_t *data;
	xmms_xform_t *xform = datasource;
	xmms_error_t error;
	size_t ret;

	g_return_val_if_fail (xform, 0);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, 0);

	ret = xmms_xform_read (xform, ptr, size * nmemb, &error);

	return ret / size;
}

static int
vorbis_callback_seek (void *datasource, ogg_int64_t offset, int whence)
{
	xmms_xform_t *xform = datasource;
	xmms_error_t err;
	gint ret;

	g_return_val_if_fail (xform, -1);

	xmms_error_reset (&err);

	if (whence == SEEK_CUR) {
		whence = XMMS_XFORM_SEEK_CUR;
	} else if (whence == SEEK_SET) {
		whence = XMMS_XFORM_SEEK_SET;
	} else if (whence == SEEK_END) {
		whence = XMMS_XFORM_SEEK_END;
	}

	ret = xmms_xform_seek (xform, (gint64) offset, whence, &err);

	return (ret == -1) ? -1 : 0;
}

static int
vorbis_callback_close (void *datasource)
{
	return 0;
}

static long
vorbis_callback_tell (void *datasource)
{
	xmms_xform_t *xform = datasource;
	xmms_error_t err;

	g_return_val_if_fail (xform, -1);

	xmms_error_reset (&err);

	return xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_CUR, &err);
}
static guint32
decode_uint32 (guchar **pos)
{
	guint32 value;
	memcpy (&value, *pos, sizeof (guint32));
	(*pos) += sizeof (guint32);
	return GUINT32_FROM_BE (value);
}

static gboolean
handle_image_comment (xmms_xform_t *xform, const gchar *key,
                      const gchar *encoded_value, gsize length)
{
	gsize len;
	guchar *value;

	guint32 typ, mime_len, desc_len, img_len;
	guchar *pos, *end, *mime_data, *img_data;
	gchar hash[33];

#if GLIB_CHECK_VERSION(2,12,0)
	value = g_base64_decode (encoded_value, &len);
#else
	/* TODO: Implement/backport base64 decoding */
	return;
#endif

	pos = value;
	end = value + len;

	if (pos + sizeof (guint32) > end) {
		XMMS_DBG ("Malformed picture comment");
		goto finish;
	}

	typ = decode_uint32 (&pos);
	if (typ != 0 && typ != 3) {
		XMMS_DBG ("Picture type %d not handled", typ);
		goto finish;
	}

	if (pos + sizeof (guint32) > end) {
		XMMS_DBG ("Malformed picture comment");
		goto finish;
	}

	mime_len = decode_uint32 (&pos);
	mime_data = pos;
	pos += mime_len;

	if (pos + sizeof (guint32) > end) {
		XMMS_DBG ("Malformed picture comment");
		goto finish;
	}

	desc_len = decode_uint32 (&pos);
	pos += desc_len;

	decode_uint32 (&pos); /* width */
	decode_uint32 (&pos); /* height */
	decode_uint32 (&pos); /* depth */
	decode_uint32 (&pos); /* indexed palette length */

	if (pos + sizeof (guint32) > end) {
		XMMS_DBG ("Malformed picture comment");
		goto finish;
	}

	img_len = decode_uint32 (&pos);
	img_data = pos;

	if (img_data + img_len > end) {
		XMMS_DBG ("Malformed picture comment");
		goto finish;
	}

	if (xmms_bindata_plugin_add (img_data, img_len, hash)) {
		const gchar *metakey;

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT;
		xmms_xform_metadata_set_str (xform, metakey, hash);

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT_MIME;
		mime_data[mime_len] = '\0';
		xmms_xform_metadata_set_str (xform, metakey, (gchar *)mime_data);
	}

finish:
	g_free (value);
	return TRUE;
}

static void
xmms_vorbis_read_metadata (xmms_xform_t *xform, xmms_vorbis_data_t *data)
{
	vorbis_comment *vc;
	gint i;

	vc = ov_comment (&data->vorbisfile, -1);
	if (!vc)
		return;

	for (i = 0; i < vc->comments; i++) {
		const gchar *ptr, *entry;
		gsize length;
		gchar key[64];

		entry = vc->user_comments[i];
		length = vc->comment_lengths[i];

		if (entry == NULL || *entry == '\0')
			continue;

		/* check whether it's a valid comment */
		ptr = memchr (entry, '=', length);
		if (ptr == NULL)
			continue;

		ptr++;

		g_strlcpy (key, entry, MIN (ptr - entry, sizeof (key)));

		if (!xmms_xform_metadata_mapper_match (xform, key, ptr, length - (ptr - entry))) {
			XMMS_DBG ("Unhandled tag '%s'", entry);
		}
	}
}

static gboolean
xmms_vorbis_init (xmms_xform_t *xform)
{
	xmms_vorbis_data_t *data;
	vorbis_info *vi;
	gint ret;
	guint playtime;
	const gchar *metakey;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_vorbis_data_t, 1),

	data->callbacks.read_func = vorbis_callback_read;
	data->callbacks.close_func = vorbis_callback_close;
	data->callbacks.tell_func = vorbis_callback_tell;
	data->callbacks.seek_func = vorbis_callback_seek;

	data->current = -1;

	xmms_xform_private_data_set (xform, data);

	ret = ov_open_callbacks (xform, &data->vorbisfile, NULL, 0,
	                         data->callbacks);
	if (ret) {
		return FALSE;
	}

	vi = ov_info (&data->vorbisfile, -1);

	playtime = ov_time_total (&data->vorbisfile, -1);
	if (playtime != OV_EINVAL) {
		gint filesize;

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
		if (xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
			xmms_vorbis_set_duration (xform, playtime);
		}
	}

	if (vi && vi->bitrate_nominal) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE;
		xmms_xform_metadata_set_int (xform, metakey,
		                             (gint) vi->bitrate_nominal);
	}

	xmms_vorbis_read_metadata (xform, data);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S16,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             vi->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             vi->rate,
	                             XMMS_STREAM_TYPE_END);


	return TRUE;
}

static gint
xmms_vorbis_read (xmms_xform_t *xform, gpointer buf, gint len,
                  xmms_error_t *err)
{
	gint ret = 0;
	gint c;
	xmms_vorbis_data_t *data;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	ret = xmms_vorbis_ov_read (&data->vorbisfile, (gchar *) buf, len,
	                           G_BYTE_ORDER == G_BIG_ENDIAN,
	                           xmms_sample_size_get (XMMS_SAMPLE_FORMAT_S16),
	                           1, &c);

	if (ret < 0) {
		return -1;
	}

	if (ret && c != data->current) {
		xmms_vorbis_read_metadata (xform, data);
		data->current = c;
	}

	return ret;
}

static gint64
xmms_vorbis_seek (xmms_xform_t *xform, gint64 samples,
                  xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_vorbis_data_t *data;

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);
	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	if (samples > ov_pcm_total (&data->vorbisfile, -1)) {
		xmms_log_error ("Trying to seek past end of stream");
		return -1;
	}

	ov_pcm_seek (&data->vorbisfile, samples);

	return samples;
}
