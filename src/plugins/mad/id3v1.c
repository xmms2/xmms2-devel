/*  XMMS2 - X Music Multiplexer System
 *
 *  Copyright (C) 2003-2012 XMMS2 Team
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

/**
 *  ID3v1
 */

#include <glib.h>
#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_medialib.h"
#include "xmms/xmms_log.h"
#include "id3v1.h"

static const gchar * const id3_genres[] = {
        "Blues", "Classic Rock", "Country", "Dance",
        "Disco", "Funk", "Grunge", "Hip-Hop",
        "Jazz", "Metal", "New Age", "Oldies",
        "Other", "Pop", "R&B", "Rap", "Reggae",
        "Rock", "Techno", "Industrial", "Alternative",
        "Ska", "Death Metal", "Pranks", "Soundtrack",
        "Euro-Techno", "Ambient", "Trip-Hop", "Vocal",
        "Jazz+Funk", "Fusion", "Trance", "Classical",
        "Instrumental", "Acid", "House", "Game",
        "Sound Clip", "Gospel", "Noise", "Alt",
        "Bass", "Soul", "Punk", "Space",
        "Meditative", "Instrumental Pop",
        "Instrumental Rock", "Ethnic", "Gothic",
        "Darkwave", "Techno-Industrial", "Electronic",
        "Pop-Folk", "Eurodance", "Dream",
        "Southern Rock", "Comedy", "Cult",
        "Gangsta Rap", "Top 40", "Christian Rap",
        "Pop/Funk", "Jungle", "Native American",
        "Cabaret", "New Wave", "Psychedelic", "Rave",
        "Showtunes", "Trailer", "Lo-Fi", "Tribal",
        "Acid Punk", "Acid Jazz", "Polka", "Retro",
        "Musical", "Rock & Roll", "Hard Rock", "Folk",
        "Folk/Rock", "National Folk", "Swing",
        "Fast-Fusion", "Bebob", "Latin", "Revival",
        "Celtic", "Bluegrass", "Avantgarde",
        "Gothic Rock", "Progressive Rock",
        "Psychedelic Rock", "Symphonic Rock", "Slow Rock",
        "Big Band", "Chorus", "Easy Listening",
        "Acoustic", "Humour", "Speech", "Chanson",
        "Opera", "Chamber Music", "Sonata", "Symphony",
        "Booty Bass", "Primus", "Porn Groove",
        "Satire", "Slow Jam", "Club", "Tango",
        "Samba", "Folklore", "Ballad", "Power Ballad",
        "Rhythmic Soul", "Freestyle", "Duet",
        "Punk Rock", "Drum Solo", "A Cappella",
        "Euro-House", "Dance Hall", "Goa",
        "Drum & Bass", "Club-House", "Hardcore",
        "Terror", "Indie", "BritPop", "Negerpunk",
        "Polsk Punk", "Beat", "Christian Gangsta Rap",
        "Heavy Metal", "Black Metal", "Crossover",
        "Contemporary Christian", "Christian Rock",
        "Merengue", "Salsa", "Thrash Metal",
        "Anime", "JPop", "Synthpop"
};

static void
xmms_id3v1_set (xmms_xform_t *xform, const char *key, const unsigned char *value,
                int len, const char *encoding)
{
	gsize readsize,writsize;
	GError *err = NULL;
	gchar *tmp;

	/* property already set? */
	/*
	if (xmms_xform_metadata_has_val (xform, key)) {
		return;
	}
	*/

	g_clear_error (&err);

	tmp = g_convert ((const char *)value, len, "UTF-8", encoding, &readsize, &writsize, &err);
	if (!tmp) {
		/* in case of not supported encoding, we try to fallback to latin1 */
		xmms_log_info ("Converting ID3v1 tag '%s' failed (check id3v1_encoding property): %s", key, err ? err->message : "Error not set");
		err = NULL;
		tmp = g_convert ((const char *)value, len, "UTF-8", "ISO8859-1", &readsize, &writsize, &err);
	}
	if (tmp) {
		g_strstrip (tmp);
		if (tmp[0] != '\0') {
			xmms_xform_metadata_set_str (xform, key, tmp);
		}
		g_free (tmp);
	}
}

static gboolean
xmms_id3v1_parse (xmms_xform_t *xform, guchar *buf)
{
	xmms_config_property_t *config;
	const char *encoding;
	const gchar *metakey;
	xmmsv_t *bb;
	unsigned char data[32];

	bb = xmmsv_bitbuffer_new_ro (buf, 128);

	xmmsv_bitbuffer_get_data (bb, data, 3);

	if (memcmp (data, "TAG", 3) != 0) {
		xmmsv_unref (bb);
		return FALSE;
	}

	XMMS_DBG ("Found ID3v1 TAG!");

	config = xmms_xform_config_lookup (xform, "id3v1_encoding");
	g_return_val_if_fail (config, FALSE);
	encoding = xmms_config_property_get_string (config);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE;
	xmmsv_bitbuffer_get_data (bb, data, 30);
	xmms_id3v1_set (xform, metakey, data, 30, encoding);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST;
	xmmsv_bitbuffer_get_data (bb, data, 30);
	xmms_id3v1_set (xform, metakey, data, 30, encoding);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM;
	xmmsv_bitbuffer_get_data (bb, data, 30);
	xmms_id3v1_set (xform, metakey, data, 30, encoding);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR;
	xmmsv_bitbuffer_get_data (bb, data, 4);
	xmms_id3v1_set (xform, metakey, data, 4, encoding);

	xmmsv_bitbuffer_get_data (bb, data, 30);

	/* v1.1 */
	if (data[28] == '\0' && data[29]) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT;
		xmms_id3v1_set (xform, metakey, data, 28, encoding);
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR;
		if (!xmms_xform_metadata_has_val (xform, metakey)) {
			xmms_xform_metadata_set_int (xform, metakey, data[29]);
		}
	} else {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT;
		xmms_id3v1_set (xform, metakey, data, 30, encoding);
	}


	xmmsv_bitbuffer_get_data (bb, data, 1);
	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE;
	if (data[0] >= G_N_ELEMENTS (id3_genres)) {
		xmms_xform_metadata_set_str (xform, metakey, "Unknown");
	} else {
		xmms_xform_metadata_set_str (xform, metakey, id3_genres[data[0]]);
	}


	xmmsv_unref (bb);
	return TRUE;
}

gint
xmms_id3v1_get_tags (xmms_xform_t *xform)
{
	xmms_error_t err;
	gint64 res;
	guchar buf[128];
	gint ret = 0;

	xmms_error_reset (&err);

	res = xmms_xform_seek (xform, -128, XMMS_XFORM_SEEK_END, &err);
	if (res == -1) {
		XMMS_DBG ("Couldn't seek - not getting id3v1 tag");
		return 0;
	}

	if (xmms_xform_read (xform, buf, 128, &err) == 128) {
		if (xmms_id3v1_parse (xform, buf)) {
			ret = 128;
		}
	} else {
		XMMS_DBG ("Read of 128 bytes failed?!");
		xmms_error_reset (&err);
	}

	res = xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_SET, &err);
	if (res == -1) {
		XMMS_DBG ("Couldn't seek after getting id3 tag?!? very bad");
		return -1;
	}

	return ret;
}


