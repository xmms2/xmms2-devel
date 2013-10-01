/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2005-2014 XMMS2 Team
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

#include "../apev2_common/apev2.c"

#define APE_MIN_VERSION 3950
#define APE_MAX_VERSION 3990

#define MAC_FORMAT_FLAG_8_BIT                 1 /* is 8-bit */
#define MAC_FORMAT_FLAG_CRC                   2 /* uses the new CRC32 error detection */
#define MAC_FORMAT_FLAG_HAS_PEAK_LEVEL        4 /* uint32 nPeakLevel after the header */
#define MAC_FORMAT_FLAG_24_BIT                8 /* is 24-bit */
#define MAC_FORMAT_FLAG_HAS_SEEK_ELEMENTS    16 /* has the number of seek elements after the peak level */
#define MAC_FORMAT_FLAG_CREATE_WAV_HEADER    32 /* create the wave header on decompression (not stored) */

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

typedef struct {
	guchar magic[4];
	guint16 fileversion;

	/* descriptor block, only in new file format */
	guint16 padding;
	guint32 descriptorlength;
	guint32 headerlength;
	guint32 seektablelength;
	guint32 wavheaderlength;
	guint32 audiodatalength;
	guint32 audiodatalength_high;
	guint32 wavtaillength;
	guint8 md5[16];

	/* header block, in all versions */
	guint16 compressiontype;
	guint16 formatflags;
	guint32 blocksperframe;
	guint32 finalframeblocks;
	guint32 totalframes;
	guint32 samplebits;
	guint32 channels;
	guint32 samplerate;

	guint32 *seektable;

	/* other useful data */
	gint32 filesize;
	guint32 firstframe;
	guint32 totalsamples;
	guint32 nextframe;

	/* input buffer for reading */
	guchar *buffer;
	gint buffer_size;
	gint buffer_length;
} xmms_apefile_data_t;

/*
 * Function prototypes
 */
static gboolean xmms_apefile_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_apefile_init (xmms_xform_t *decoder);
static void xmms_apefile_destroy (xmms_xform_t *decoder);

static gboolean xmms_apefile_init_demuxer (xmms_xform_t *xform);

static gint xmms_apefile_read (xmms_xform_t *xform, xmms_sample_t *buffer,
                               gint len, xmms_error_t *err);
static gint64 xmms_apefile_seek (xmms_xform_t *xform, gint64 samples,
                                 xmms_xform_seek_mode_t whence,
                                 xmms_error_t *err);

XMMS_XFORM_PLUGIN_DEFINE ("apefile", "Monkey's Audio demuxer", XMMS_VERSION,
                          "Monkey's Audio file format demuxer",
                          xmms_apefile_plugin_setup);

static guint16
get_le16 (guchar *data)
{
	return (data[1] << 8) | data[0];
}

static void
set_le16 (guchar *data, guint16 value)
{
	data[0] = value & 0xff;
	data[1] = (value >> 8) & 0xff;
}

static guint32
get_le32 (guchar *data)
{
	return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
}

static void
set_le32 (guchar *data, guint32 value)
{
	data[0] = value & 0xff;
	data[1] = (value >> 8) & 0xff;
	data[2] = (value >> 16) & 0xff;
	data[3] = (value >> 24) & 0xff;
}

/*
 * Plugin header
 */
static gboolean
xmms_apefile_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_apefile_init;
	methods.destroy = xmms_apefile_destroy;
	methods.read = xmms_apefile_read;
	methods.seek = xmms_apefile_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_metadata_mapper_init (xform_plugin,
	                                        basic_mappings,
	                                        G_N_ELEMENTS (basic_mappings),
	                                        mappings,
	                                        G_N_ELEMENTS (mappings));

	xmms_xform_plugin_indata_add (xform_plugin, XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-ape", NULL);

	xmms_magic_add ("mpc header", "audio/x-ape", "0 string MAC ", NULL);

	return TRUE;
}

static gboolean
xmms_apefile_init (xmms_xform_t *xform)
{
	xmms_apefile_data_t *data;
	guchar decoder_config[6];

	data = g_new0 (xmms_apefile_data_t, 1);
	data->seektable = NULL;
	data->buffer = NULL;

	xmms_xform_private_data_set (xform, data);

	if (!xmms_apefile_init_demuxer (xform)) {
		xmms_log_error ("Couldn't initialize the demuxer, please check log");
		return FALSE;
	}

	/* Note that this function will seek around the file to another position */
	if (!xmms_apetag_read (xform)) {
		XMMS_DBG ("Couldn't read tags from the file");
	}

	xmms_xform_metadata_set_int (xform,
	                             XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
	                             data->totalsamples / data->samplerate * 1000);

	xmms_xform_auxdata_set_int (xform,
	                            "samplebits",
	                            data->samplebits);

	set_le16 (decoder_config + 0, data->fileversion);
	set_le16 (decoder_config + 2, data->compressiontype);
	set_le16 (decoder_config + 4, data->formatflags);
	xmms_xform_auxdata_set_bin (xform,
	                            "decoder_config",
	                            decoder_config,
	                            sizeof (decoder_config));

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/x-ffmpeg-ape",
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             data->samplerate,
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

static gboolean
xmms_apefile_init_demuxer (xmms_xform_t *xform)
{
	xmms_apefile_data_t *data;
	guchar buffer[512];
	xmms_error_t error;
	guint32 seektablepos;
	gint buflen, ret;

	g_return_val_if_fail (xform, FALSE);
	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	xmms_xform_metadata_get_int (xform,
	                             XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE,
	                             &data->filesize);

	ret = xmms_xform_read (xform, buffer, 16, &error);
	if (ret != 16) {
		xmms_log_error ("Reading the file descriptor failed");
		return FALSE;
	}
	buflen = ret;

	memcpy (data->magic, buffer, 4);
	if (memcmp (data->magic, "MAC ", 4)) {
		xmms_log_error ("File magic doesn't match, this is weird");
		return FALSE;
	}

	data->fileversion = get_le16 (buffer+4);
	if (data->fileversion < APE_MIN_VERSION || data->fileversion > APE_MAX_VERSION) {
		xmms_log_error ("The APE file is of unknown version, not supported!");
		return FALSE;
	}

	XMMS_DBG ("File version number %d", data->fileversion);

	if (data->fileversion >= 3980) {
		gint totallength;
		guchar *header;

		/* Descriptor length includes magic bytes and file version */
		data->padding           = get_le16 (buffer + 6);
		data->descriptorlength  = get_le32 (buffer + 8);
		data->headerlength      = get_le32 (buffer + 12);

		totallength = data->descriptorlength + data->headerlength;
		if (totallength > 512) {
			/* This doesn't fit in the buffer, maybe should make it bigger? */
			xmms_log_error ("Internal header buffer too short, please file a bug!");
			return FALSE;
		}

		/* read the rest of the header into buffer */
		ret = xmms_xform_read (xform,
		                       buffer + buflen,
		                       totallength - buflen,
		                       &error);
		if (ret != totallength - buflen) {
			xmms_log_error ("Reading the header data failed");
			return FALSE;
		}
		buflen += ret;

		/* Read rest of the descriptor data */
		data->seektablelength           = get_le32 (buffer + 16) / 4;
		data->wavheaderlength           = get_le32 (buffer + 20);
		data->audiodatalength           = get_le32 (buffer + 24);
		data->audiodatalength_high      = get_le32 (buffer + 28);
		data->wavtaillength             = get_le32 (buffer + 32);
		memcpy (data->md5, buffer + 36, 16);

		header = buffer + data->descriptorlength;

		/* Read header data */
		data->compressiontype   = get_le16 (header + 0);
		data->formatflags       = get_le16 (header + 2);
		data->blocksperframe    = get_le32 (header + 4);
		data->finalframeblocks  = get_le32 (header + 8);
		data->totalframes       = get_le32 (header + 12);
		data->samplebits        = get_le16 (header + 16);
		data->channels          = get_le16 (header + 18);
		data->samplerate        = get_le32 (header + 20);

		seektablepos = data->descriptorlength + data->headerlength;
		data->firstframe = seektablepos + data->seektablelength * 4 +
		                   data->wavheaderlength;
	} else {
		/* Header includes magic bytes and file version */
		data->headerlength = 32;

		data->compressiontype   = get_le16 (buffer+6);
		data->formatflags       = get_le16 (buffer+8);
		data->channels          = get_le16 (buffer+10);
		data->samplerate        = get_le32 (buffer+12);

		if (data->formatflags & MAC_FORMAT_FLAG_HAS_PEAK_LEVEL)
			data->headerlength += 4;
		if (data->formatflags & MAC_FORMAT_FLAG_HAS_SEEK_ELEMENTS)
			data->headerlength += 4;

		/* read the rest of the header into buffer */
		ret = xmms_xform_read (xform,
		                       buffer + buflen,
		                       data->headerlength - buflen,
		                       &error);
		if (ret != data->headerlength - buflen) {
			xmms_log_error ("Reading the header data failed");
			return FALSE;
		}
		buflen += ret;

		data->wavheaderlength   = get_le32 (buffer+16);
		data->wavtaillength     = get_le32 (buffer+20);
		data->totalframes       = get_le32 (buffer+24);
		data->finalframeblocks  = get_le32 (buffer+28);

		if (data->formatflags & MAC_FORMAT_FLAG_HAS_SEEK_ELEMENTS) {
			gint seeklenidx = data->headerlength - 4;

			data->seektablelength = get_le32 (buffer + seeklenidx);
		} else {
			data->seektablelength = data->totalframes;
		}

		if (data->formatflags & MAC_FORMAT_FLAG_8_BIT) {
			data->samplebits = 8;
		} else if (data->formatflags & MAC_FORMAT_FLAG_24_BIT) {
			data->samplebits = 24;
		} else {
			data->samplebits = 16;
		}

		if (data->fileversion >= 3950) {
			data->blocksperframe = 73728 * 4;
		} else if (data->fileversion >= 3900 || (data->fileversion >= 3800 &&
		           data->compressiontype == 4000)) {
			data->blocksperframe = 73728;
		} else {
			data->blocksperframe = 9216;
		}

		seektablepos = data->headerlength + data->wavheaderlength;
		data->firstframe = seektablepos + data->seektablelength * 4;
	}

	data->totalsamples = data->finalframeblocks;
	if (data->totalframes > 1) {
		data->totalsamples += data->blocksperframe * (data->totalframes - 1);
	}

	if (data->seektablelength > 0) {
		guchar *tmpbuf;
		gint seektablebytes, i;

		if (data->seektablelength < data->totalframes) {
			xmms_log_error ("Seektable length %d too small, frame count %d",
			                 data->seektablelength,
			                 data->totalframes);
			/* FIXME: this is not really fatal */
			return FALSE;
		}

		XMMS_DBG ("Seeking to position %d", seektablepos);

		ret = xmms_xform_seek (xform,
		                       seektablepos,
		                       XMMS_XFORM_SEEK_SET,
		                       &error);
		if (ret != seektablepos) {
			xmms_log_error ("Seeking to the beginning of seektable failed");
			/* FIXME: this is not really fatal */
			return FALSE;
		}

		seektablebytes = data->seektablelength * 4;
		tmpbuf = g_malloc (seektablebytes);
		data->seektable = g_malloc (seektablebytes);

		XMMS_DBG ("Reading %d bytes to the seek table", seektablebytes);

		/* read the seektable into buffer */
		ret = xmms_xform_read (xform,
		                       tmpbuf,
		                       seektablebytes,
		                       &error);
		if (ret != seektablebytes) {
			xmms_log_error ("Reading the seektable failed");
			/* FIXME: this is not really fatal */
			return FALSE;
		}
		buflen += ret;

		for (i = 0; i < data->seektablelength; i++) {
			data->seektable[i] = get_le32 (tmpbuf + i * 4);
		}

		g_free (tmpbuf);
	}

	return TRUE;
}

static gint
xmms_apefile_read (xmms_xform_t *xform, xmms_sample_t *buffer,
                   gint len, xmms_error_t *err)
{
	xmms_apefile_data_t *data;
	guint size;

	g_return_val_if_fail (xform, -1);
	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	size = MIN (data->buffer_length, len);

	/* There is some unhandled data in the buffer */
	if (data->buffer_length > 0) {
		memcpy (buffer, data->buffer, size);
		data->buffer_length -= size;
		g_memmove (data->buffer, data->buffer + size, data->buffer_length);

		/* the buffer is only needed in special cases, free when not used
		 * (it can be over half megabytes big so we don't want to keep it
		 * uselessly here) */
		if (data->buffer_length == 0) {
			g_free (data->buffer);
			data->buffer = NULL;
			data->buffer_size = 0;
		}

		return size;
	}

	while (size == 0) {
		gint framepos, framelength, framealign, nblocks, ret;

		/* this is the beginning of a new frame */
		xmms_xform_auxdata_barrier (xform);

		if (data->nextframe >= data->totalframes) {
			/* EOF reached, no more frames */
			return 0;
		}

		/* look up the position of next frame */
		framepos = data->seektable[data->nextframe];

		/* last frame needs a special handling since it differs */
		if (data->nextframe < (data->totalframes - 1)) {
			framelength = data->seektable[data->nextframe+1] -
			              data->seektable[data->nextframe];
			nblocks = data->blocksperframe;
		} else {
			if (data->filesize > data->seektable[data->nextframe]) {
				framelength = data->filesize - data->seektable[data->nextframe];
			} else {
				/* unknown or invalid file size, just read a lot */
				framelength = data->finalframeblocks * 4;
			}
			nblocks = data->finalframeblocks;
		}

		/* the data is aligned in 32-bit words, so need to fix alignment */
		framealign = (data->seektable[data->nextframe] -
		             data->seektable[0]) & 3;
		framepos -= framealign;
		framelength += framealign;

		ret = xmms_xform_seek (xform,
		                       framepos,
		                       XMMS_XFORM_SEEK_SET,
		                       err);
		if (ret != framepos) {
			xmms_log_error ("Seeking to the beginning of next frame failed");
			/* FIXME: this is not really fatal */
			return -1;
		}

		/* the required data doesn't fit into the buffer, allocate new input buffer
		 * 8 bytes are required for the libavcodec metadata passing */
		if ((framelength + 8) > len) {
			data->buffer = g_realloc (data->buffer, framelength + 8 - len);
			data->buffer_size = framelength + 8 - len;
		}

		/* calculate how much data will go directly into the buffer */
		size = MIN (framelength, len - 8);

		/* read the actual data */
		ret = xmms_xform_read (xform, buffer + 8, size, err);
		if (ret < 0) {
			xmms_log_error ("Reading the frame data failed");
			return ret;
		}

		/* set frame metadata required by avcodec decoder */
		set_le32 (buffer + 0, nblocks);
		set_le32 (buffer + 4, framealign);
		size = size + 8;

		if ((framelength + 8) > len) {
			/* read the data that didn't fit the original buffer */
			ret = xmms_xform_read (xform,
			                       data->buffer,
			                       framelength + 8 - len,
			                       err);
			if (ret < 0) {
				xmms_log_error ("Reading the frame data failed");
				return ret;
			}

			data->buffer_length = ret;
		}

		data->nextframe++;
	}

	return size;
}

static gint64
xmms_apefile_seek (xmms_xform_t *xform, gint64 samples,
                   xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_apefile_data_t *data;

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);
	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);
	g_return_val_if_fail (data->seektable, -1);

	if (samples < 0 || samples > data->totalsamples) {
		/* trying to seek outside file bounds */
		xmms_error_set (err, XMMS_ERROR_GENERIC,
		                "Seek index out of bounds, only seek within the file");
		return -1;
	}

	/* update the position of the next frame */
	data->nextframe = samples / data->blocksperframe;

	/* free possibly temporary buffer, it is useless now */
	g_free (data->buffer);
	data->buffer = NULL;
	data->buffer_length = 0;

	return (data->nextframe * data->blocksperframe);
}

void
xmms_apefile_destroy (xmms_xform_t *xform)
{
	xmms_apefile_data_t *data;

	g_return_if_fail (xform);
	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	g_free (data->seektable);
	g_free (data->buffer);
	g_free (data);
}
