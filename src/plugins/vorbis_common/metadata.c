/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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

#include <xmms/xmms_medialib.h>
#include <xmms/xmms_xformplugin.h>

#include <glib.h>

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
