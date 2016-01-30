/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2005-2016 XMMS2 Team
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

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_sample.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_medialib.h>

#include <string.h>
#include <math.h>

#include <glib.h>

#ifdef HAVE_MPCDEC_OLD
# define READER_OBJ      void
# define READER_DATA(r)  (r)
#include <mpcdec/mpcdec.h>
#else
# define READER_OBJ       mpc_reader
# define READER_DATA(r)   ((r)->data)
#include <mpc/mpcdec.h>
#endif

#include "../apev2_common/apev2.c"

#ifdef HAVE_MPCDEC_OLD
typedef struct xmms_mpc_data_St {
	mpc_decoder decoder;
	mpc_reader reader;
	mpc_streaminfo info;

	GString *buffer;
} xmms_mpc_data_t;
#else
typedef struct xmms_mpc_data_St {
	mpc_demux *demux;
	mpc_reader reader;
	mpc_streaminfo info;

	GString *buffer;
} xmms_mpc_data_t;
#endif

/*
 * Function prototypes
 */
static gboolean xmms_mpc_plugin_setup (xmms_xform_plugin_t *xform_plugin);

static gboolean xmms_mpc_init (xmms_xform_t *decoder);

static void xmms_mpc_destroy (xmms_xform_t *decoder);

static void xmms_mpc_cache_streaminfo (xmms_xform_t *decoder);

static gint xmms_mpc_read (xmms_xform_t *xform, xmms_sample_t *buffer,
                           gint len, xmms_error_t *err);

static gint64 xmms_mpc_seek (xmms_xform_t *xform, gint64 offset,
                             xmms_xform_seek_mode_t whence, xmms_error_t *err);

/** These are the properties that we extract from the comments */
static const xmms_xform_metadata_basic_mapping_t basic_mappings[] = {
	{ "Album",                     XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM             },
	{ "Title",                     XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE             },
	{ "Artist",                    XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST            },
	{ "Album Artist",              XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST      },
	{ "Track",                     XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR           },
	{ "Disc",                      XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET         },
	{ "Year",                      XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR              },
	{ "Composer",                  XMMS_MEDIALIB_ENTRY_PROPERTY_COMPOSER          },
	{ "Lyricist",                  XMMS_MEDIALIB_ENTRY_PROPERTY_LYRICIST          },
	{ "Conductor",                 XMMS_MEDIALIB_ENTRY_PROPERTY_CONDUCTOR         },
	{ "Performer",                 XMMS_MEDIALIB_ENTRY_PROPERTY_PERFORMER         },
	{ "MixArtist",                 XMMS_MEDIALIB_ENTRY_PROPERTY_REMIXER           },
	{ "Arranger",                  XMMS_MEDIALIB_ENTRY_PROPERTY_ARRANGER          },
	{ "Producer",                  XMMS_MEDIALIB_ENTRY_PROPERTY_PRODUCER          },
	{ "Mixer",                     XMMS_MEDIALIB_ENTRY_PROPERTY_MIXER             },
	{ "Grouping",                  XMMS_MEDIALIB_ENTRY_PROPERTY_GROUPING          },
	{ "Compilation",               XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION       },
	{ "Comment",                   XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT           },
	{ "Genre",                     XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE             },
	{ "BPM",                       XMMS_MEDIALIB_ENTRY_PROPERTY_BPM               },
	{ "ASIN",                      XMMS_MEDIALIB_ENTRY_PROPERTY_ASIN              },
	{ "ISRC",                      XMMS_MEDIALIB_ENTRY_PROPERTY_ISRC              },
	{ "Label",                     XMMS_MEDIALIB_ENTRY_PROPERTY_PUBLISHER         },
	{ "Copyright",                 XMMS_MEDIALIB_ENTRY_PROPERTY_COPYRIGHT         },
	{ "CatalogNumber",             XMMS_MEDIALIB_ENTRY_PROPERTY_CATALOGNUMBER     },
	{ "Barcode",                   XMMS_MEDIALIB_ENTRY_PROPERTY_BARCODE           },
	{ "ALBUMSORT",                 XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_SORT        },
	{ "ALBUMARTISTSORT",           XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST_SORT },
	{ "ARTISTSORT",                XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_SORT       },
	{ "MUSICBRAINZ_TRACKID",       XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID          },
	{ "MUSICBRAINZ_ALBUMID",       XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID          },
	{ "MUSICBRAINZ_ARTISTID",      XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID         },
	{ "MUSICBRAINZ_ALBUMARTISTID", XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION       }
};

static const xmms_xform_metadata_mapping_t mappings[] = {
	{ "Cover Art (Front)", xmms_apetag_handle_tag_coverart }
};


/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN_DEFINE ("musepack", "Musepack decoder", XMMS_VERSION,
                          "Musepack Living Audio Compression",
                          xmms_mpc_plugin_setup);

static gboolean
xmms_mpc_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_mpc_init;
	methods.destroy = xmms_mpc_destroy;
	methods.read = xmms_mpc_read;
	methods.seek = xmms_mpc_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_metadata_mapper_init (xform_plugin,
	                                        basic_mappings,
	                                        G_N_ELEMENTS (basic_mappings),
	                                        mappings,
	                                        G_N_ELEMENTS (mappings));

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-mpc",
	                              XMMS_STREAM_TYPE_END);

	xmms_magic_add ("mpc header", "audio/x-mpc", "0 string MP+", NULL);

#ifndef HAVE_MPCDEC_OLD
	/* The old API doesn't support sv8 bitstream, so we add it only for the
	 * new version */
	xmms_magic_add ("mpc header", "audio/x-mpc", "0 string MPCK", NULL);
#endif

	return TRUE;
}

static mpc_int32_t
xmms_mpc_callback_read (READER_OBJ *p_obj, void *buffer, mpc_int32_t size)
{
	xmms_xform_t *xform = READER_DATA (p_obj);
	xmms_error_t err;

	g_return_val_if_fail (xform, -1);

	xmms_error_reset (&err);

	return xmms_xform_read (xform, (gchar *) buffer, size, &err);
}


static mpc_bool_t
xmms_mpc_callback_seek (READER_OBJ *p_obj, mpc_int32_t offset)
{
	xmms_xform_t *xform = READER_DATA (p_obj);
	xmms_error_t err;
	gint ret;

	g_return_val_if_fail (xform, -1);

	xmms_error_reset (&err);

	ret = xmms_xform_seek (xform, (gint64) offset, XMMS_XFORM_SEEK_SET, &err);

	return (ret == -1) ? FALSE : TRUE;
}


static mpc_int32_t
xmms_mpc_callback_tell (READER_OBJ *p_obj)
{
	xmms_xform_t *xform = READER_DATA (p_obj);
	xmms_error_t err;

	g_return_val_if_fail (xform, -1);

	xmms_error_reset (&err);

	return xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_CUR, &err);
}


static mpc_bool_t
xmms_mpc_callback_canseek (READER_OBJ *p_obj)
{
	xmms_xform_t *xform = READER_DATA (p_obj);

	g_return_val_if_fail (xform, FALSE);

	return FALSE;
}


static mpc_int32_t
xmms_mpc_callback_get_size (READER_OBJ *p_obj)
{
	xmms_xform_t *xform = READER_DATA (p_obj);
	const gchar *metakey;
	gint ret;

	g_return_val_if_fail (xform, -1);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	if (xmms_xform_metadata_get_int (xform, metakey, &ret)) {
		return ret;
	}

	return -1;
}



static gboolean
xmms_mpc_init (xmms_xform_t *xform)
{
	xmms_mpc_data_t *data;
	xmms_error_t error;

	data = g_new0 (xmms_mpc_data_t, 1);
	xmms_xform_private_data_set (xform, data);

	if (!xmms_apetag_read (xform)) {
		XMMS_DBG ("Failed to read APEv2 tag");
	}

	/* Reset to start after reading the tags */
	xmms_error_reset (&error);
	xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_SET, &error);

	data->buffer = g_string_new (NULL);

	data->reader.read = xmms_mpc_callback_read;
	data->reader.seek = xmms_mpc_callback_seek;
	data->reader.tell = xmms_mpc_callback_tell;
	data->reader.canseek = xmms_mpc_callback_canseek;
	data->reader.get_size = xmms_mpc_callback_get_size;

	data->reader.data = xform;

#ifdef HAVE_MPCDEC_OLD
	mpc_streaminfo_init (&data->info);
	if (mpc_streaminfo_read (&data->info, &data->reader) != ERROR_CODE_OK)
		return FALSE;

	mpc_decoder_setup (&data->decoder, &data->reader);

	if (mpc_decoder_initialize (&data->decoder, &data->info) == FALSE)
		return FALSE;
#else
	data->demux = mpc_demux_init (&data->reader);
	if (!data->demux) return FALSE;

	mpc_demux_get_info (data->demux,  &data->info);
#endif

	xmms_mpc_cache_streaminfo (xform);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_FLOAT,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->info.channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             data->info.sample_freq,
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

#ifdef HAVE_MPCDEC_OLD

static inline double
xmms_mpc_normalize_gain (gdouble gain)
{
	return pow (10.0, gain / 2000.0);
}

static inline double
xmms_mpc_normalize_peak (gdouble peak)
{
	return (peak / 32768.0);
}
#else

static inline double
xmms_mpc_normalize_gain (gdouble gain)
{
	return pow (10.0, 20.0 * (MPC_OLD_GAIN_REF - gain / 256.0));
}

static inline double
xmms_mpc_normalize_peak (gdouble peak)
{
	return pow (10.0, peak / (20.0 * 256.0) / 32768.0);
}
#endif

static void
xmms_mpc_cache_streaminfo (xmms_xform_t *xform)
{
	xmms_mpc_data_t *data;
	gint bitrate, duration, filesize;
	gchar buf[8];
	const gchar *metakey;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	XMMS_DBG ("stream version = %d", data->info.stream_version);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	if (xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
		duration = mpc_streaminfo_get_length (&data->info) * 1000;
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
		xmms_xform_metadata_set_int (xform, metakey, duration);
	}

	bitrate = (data->info.bitrate) ? data->info.bitrate :
	                                 data->info.average_bitrate;

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE;
	xmms_xform_metadata_set_int (xform, metakey, bitrate);

	if (data->info.gain_album) {
		g_snprintf (buf, sizeof (buf), "%f",
		            xmms_mpc_normalize_gain ((gdouble) data->info.gain_album));
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM;
		xmms_xform_metadata_set_str (xform, metakey, buf);
	}

	if (data->info.gain_title) {
		g_snprintf (buf, sizeof (buf), "%f",
		            xmms_mpc_normalize_gain ((gdouble) data->info.gain_title));
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK;
		xmms_xform_metadata_set_str (xform, metakey, buf);
	}

	if (data->info.peak_album) {
		g_snprintf (buf, sizeof (buf), "%f",
		            xmms_mpc_normalize_peak ((gdouble) data->info.peak_album));
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM;
		xmms_xform_metadata_set_str (xform, metakey, buf);
	}

	if (data->info.peak_title) {
		g_snprintf (buf, sizeof (buf), "%f",
		            xmms_mpc_normalize_peak ((gdouble) data->info.peak_title));
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK;
		xmms_xform_metadata_set_str (xform, metakey, buf);
	}
}

static gint
xmms_mpc_read (xmms_xform_t *xform, xmms_sample_t *buffer,
               gint len, xmms_error_t *err)
{
	MPC_SAMPLE_FORMAT internal[MPC_DECODER_BUFFER_LENGTH];
	xmms_mpc_data_t *data;
	mpc_uint32_t ret;
	guint size;

	data = xmms_xform_private_data_get (xform);

	size = MIN (data->buffer->len, len);

#ifdef HAVE_MPCDEC_OLD
	if (size <= 0) {
		ret = mpc_decoder_decode (&data->decoder, internal, NULL, NULL);
		if (ret == -1) {
			xmms_error_set (err, XMMS_ERROR_GENERIC, "Musepack decoder failed");
			return -1;
		}

		ret *= xmms_sample_size_get (XMMS_SAMPLE_FORMAT_FLOAT);
		ret *= data->info.channels;

		g_string_append_len (data->buffer, (gchar *) internal, ret);
	}
#else
	if (size <= 0) {
		mpc_frame_info frame;

		frame.buffer = internal;
		do {
			ret = mpc_demux_decode (data->demux, &frame);
		} while (frame.bits != -1 && frame.samples == 0);

		if (frame.bits == -1 && ret != MPC_STATUS_OK) {
			xmms_error_set (err, XMMS_ERROR_GENERIC, "Musepack decoder failed");
			return -1;
		}
		ret = frame.samples;

		ret *= xmms_sample_size_get (XMMS_SAMPLE_FORMAT_FLOAT);
		ret *= data->info.channels;

		g_string_append_len (data->buffer, (gchar *) internal, ret);
	}
#endif

	/* Update the current size of available data */
	size = MIN (data->buffer->len, len);

	memcpy (buffer, data->buffer->str, size);
	g_string_erase (data->buffer, 0, size);

	return size;
}

static gint64
xmms_mpc_seek (xmms_xform_t *xform, gint64 offset,
               xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_mpc_data_t *data;
	data = xmms_xform_private_data_get (xform);

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);

#ifdef HAVE_MPCDEC_OLD
	mpc_decoder_seek_sample (&data->decoder, offset);
#else
	mpc_demux_seek_sample (data->demux, offset);
#endif
	g_string_erase (data->buffer, 0, data->buffer->len);

	return offset;
}

void
xmms_mpc_destroy (xmms_xform_t *xform)
{
	xmms_mpc_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

#ifndef HAVE_MPCDEC_OLD
	if (data->demux)
		mpc_demux_exit (data->demux);
#endif

	if (data->buffer) {
		g_string_free (data->buffer, TRUE);
	}

	g_free (data);
}
