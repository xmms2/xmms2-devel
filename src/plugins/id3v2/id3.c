/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2011-2023 XMMS2 Team
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the XMMS2 TEAM nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL XMMS2 TEAM BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * id3v2
 */

#include <xmms/xmms_medialib.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_bindata.h>
#include "id3.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>


#define ID3v2_HEADER_FLAGS_UNSYNC 0x80
#define ID3v2_HEADER_FLAGS_EXTENDED 0x40
#define ID3v2_HEADER_FLAGS_EXPERIMENTAL 0x20
#define ID3v2_HEADER_FLAGS_FOOTER 0x10

#define ID3v2_HEADER_SUPPORTED_FLAGS (ID3v2_HEADER_FLAGS_UNSYNC | ID3v2_HEADER_FLAGS_FOOTER)

#define MUSICBRAINZ_VA_ID "89ad4ac3-39f7-470e-963a-56509c546377"

#define quad2long(a,b,c,d) ((a << 24) | (b << 16) | (c << 8) | (d))


/*
 * There are some different string-types.
 *  we only recognize the ones with '>' at the moment.
 *
 *>TALB Album/Movie/Show title
 *>TBPM BPM (beats per minute)
 *>TCMP Compilation
 *>TCOM Composer
 *>TCON Content type
 * TCOP Copyright message
 * TDAT Date (v2.3 deprecated in v2.4, replaced by TDRC)
 *>TDRC Recording time (v2.4)
 * TDLY Playlist delay
 *>TDOR Original recording year
 * TENC Encoded by
 *>TEXT Lyricist/Text writer
 * TFLT File type
 * TIME Time (v2.3 deprecated in v2.4, replaced by TDRC)
 *>TIT1 Content group description
 *>TIT2 Title/songname/content description
 *>TIT3 Subtitle/Description refinement
 * TKEY Initial key
 * TLAN Language(s)
 * TLEN Length
 * TMED Media type
 * TOAL Original album/movie/show title
 * TOFN Original filename
 * TOLY Original lyricist(s)/text writer(s)
 *>TOPE Original artist(s)/performer(s)
 *>TORY Original release year
 * TOWN File owner/licensee
 *>TPE1 Lead performer(s)/Soloist(s)
 *>TPE2 Album artist
 *>TPE3 Conductor/performer refinement
 *>TPE4 Interpreted, remixed, or otherwise modified by
 *>TPOS Part of a set
 *>TPUB Publisher
 *>TRCK Track number/Position in set
 * TRDA Recording dates (v2.3 deprecated in v2.4, replaced by TDRC)
 * TRSN Internet radio station name
 * TRSO Internet radio station owner
 * TSIZ Size
 *>TSOA Album sort
 *>TSOP Artist sort
 *>TSRC ISRC (international standard recording code)
 * TSSE Software/Hardware and settings used for encoding
 *>TYER Year (v2.3 deprecated in v2.4, replaced by TDRC)
 * WCOM Commercial information URL
 *>WCOP Copyright/Legal information URL
 *>WOAF Official audio file webpage
 *>WOAR Official artist/performer webpage
 * WOAS Official audio source webpage
 * WORS Official Internet radio station homepage
 * WPAY Payment URL
 *>WPUB Publishers official webpage
 *>XSOA Album sort
 *>XSOP Artist sort
 * TXXX User defined text information frame
 *>TXXX:ASIN                        Amazon Identification Number
 *>TXXX:BARCODE                     Barcode
 *>TXXX:CATALOGNUMBER               Record label catalog number
 *>TXXX:QuodLibet::albumartist      Album Artist Name (more to come)
 *>TXXX:REPLAYGAIN_TRACK_GAIN       Replaygain tag for a track
 *>TXXX:REPLAYGAIN_TRACK_PEAK       Replaygain tag for peak of a track
 *>TXXX:REPLAYGAIN_ALBUM_GAIN       Replaygain tag for an album
 *>TXXX:REPLAYGAIN_ALBUM_PEAK       Replaygain tag for peak of an album
 */

static const gchar * const id3_genres[] =
{
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

/**
 * do the actual convertion to UTF-8 from enc
 */
static gchar *
convert_id3_text (const gchar *enc, const gchar *txt, gint len, gsize *out_len)
{
	gchar *nval = NULL;
	GError *err = NULL;

	if (len < 1)
		return NULL;

	g_return_val_if_fail (txt, NULL);

	nval = g_convert (txt, len, "UTF-8", enc, NULL, out_len, &err);
	if (err) {
		xmms_log_error ("Couldn't convert field from %s", enc);
		return NULL;
	}

	return nval;
}

/**
 * This function takes the binary field and returns the
 * a string that describes how the text field was encoded.
 * this is supposed to be feed directly to g_convert and
 * friends.
 */
static const gchar *
binary_to_enc (guchar val)
{
	const gchar *retval;

	if (val == 0x00) {
		retval = "ISO8859-1";
	} else if (val == 0x01) {
		retval = "UTF-16";
	} else if (val == 0x02) {
		retval = "UTF-16BE";
	} else if (val == 0x03) {
		retval = "UTF-8";
	} else {
		xmms_log_error ("UNKNOWN id3v2.4 encoding (%02x)!", val);
		retval = NULL;
	}
	return retval;
}

static const gchar *
find_nul (const gchar *buf, gsize *len)
{
	gsize l = *len;
	while (l) {
		if (*buf == '\0' && l > 1) {
			*len = l - 1;
			return buf + 1;
		}
		buf++;
		l--;
	}
	return NULL;
}

static void
add_to_entry (xmms_xform_t *xform,
              xmms_id3v2_header_t *head,
              const gchar *key,
              gchar *val,
              gint len)
{
	gchar *nval;
	const gchar *tmp;

	if (len < 1)
		return;

	tmp = binary_to_enc (val[0]);
	nval = convert_id3_text (tmp, &val[1], len - 1, NULL);
	if (nval) {
		xmms_xform_metadata_set_str (xform, key, nval);
		g_free (nval);
	}
}

static void
handle_id3v2_tcon (xmms_xform_t *xform, xmms_id3v2_header_t *head,
                   const gchar *key, gchar *buf, gsize len)
{
	gint res;
	guint genre_id;
	gchar *val;
	const gchar *tmp;
	const gchar *metakey;

	/* XXX - we should handle it differently v4 separates them with NUL instead of using () */
	/*
	if (head->ver == 4) {
		buf++;
		len -= 1;
	}
	*/
	tmp = binary_to_enc (buf[0]);
	val = convert_id3_text (tmp, &buf[1], len - 1, NULL);
	if (!val)
		return;

	if (head->ver >= 4) {
		res = sscanf (val, "%u", &genre_id);
	} else {
		res = sscanf (val, "(%u)", &genre_id);
	}

	if (res > 0 && genre_id < G_N_ELEMENTS (id3_genres)) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE;
		xmms_xform_metadata_set_str (xform, metakey,
		                             (gchar *) id3_genres[genre_id]);
	} else {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE;
		xmms_xform_metadata_set_str (xform, metakey, val);
	}

	g_free (val);
}

static void
handle_id3v2_txxx (xmms_xform_t *xform, xmms_id3v2_header_t *head,
                   const gchar *_key, gchar *buf, gsize len)
{
	const gchar *enc;
	gchar *cbuf;
	const gchar *key, *val;
	const gchar *metakey;
	gsize clen;

	enc = binary_to_enc (buf[0]);
	cbuf = convert_id3_text (enc, &buf[1], len - 1, &clen);
	if (!cbuf)
		return;

	key = cbuf;
	val = find_nul (cbuf, &clen);
	if (!val) {
		g_free (cbuf);
		return;
	}

	if (g_ascii_strcasecmp (key, "MusicBrainz Album Id") == 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID;
		xmms_xform_metadata_set_str (xform, metakey, val);
	} else if (g_ascii_strcasecmp (key, "MusicBrainz Artist Id") == 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID;
		xmms_xform_metadata_set_str (xform, metakey, val);
	} else if ((g_ascii_strcasecmp (key, "MusicBrainz Album Artist Id") == 0) &&
	           (g_ascii_strcasecmp (val, MUSICBRAINZ_VA_ID) == 0)) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION;
		xmms_xform_metadata_set_int (xform, metakey, 1);
	} else if (g_ascii_strcasecmp (key, "ASIN") == 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_ASIN;
		xmms_xform_metadata_set_str (xform, metakey, val);
	} else if (g_ascii_strcasecmp (key, "QuodLibet::albumartist") == 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST;
		xmms_xform_metadata_set_str (xform, metakey, val);
	} else if (g_ascii_strcasecmp (key, "ALBUMARTISTSORT") == 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST_SORT;
		xmms_xform_metadata_set_str (xform, metakey, val);
	} else if (g_ascii_strcasecmp (key, "ALBUMARTISTSORTORDER") == 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST_SORT;
		xmms_xform_metadata_set_str (xform, metakey, val);
	} else if (g_ascii_strcasecmp (key, "BARCODE") == 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_BARCODE;
		xmms_xform_metadata_set_str (xform, metakey, val);
	} else if (g_ascii_strcasecmp (key, "CATALOGNUMBER") == 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_CATALOGNUMBER;
		xmms_xform_metadata_set_str (xform, metakey, val);
	} else if (g_ascii_strcasecmp (key, "replaygain_track_gain") == 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK;
		xmms_xform_metadata_parse_replay_gain (xform, metakey, val, 0);
	} else if (g_ascii_strcasecmp (key, "replaygain_album_gain") == 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM;
		xmms_xform_metadata_parse_replay_gain (xform, metakey, val, 0);
	} else if (g_ascii_strcasecmp (key, "replaygain_track_peak") == 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK;
		xmms_xform_metadata_set_str (xform, metakey, val);
	} else if (g_ascii_strcasecmp (key, "replaygain_album_peak") == 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM;
		xmms_xform_metadata_set_str (xform, metakey, val);
	} else {
		XMMS_DBG ("Unhandled tag 'TXXX:%s' = '%s'", key, val);
	}

	g_free (cbuf);
}

static void
handle_int_field (xmms_xform_t *xform, xmms_id3v2_header_t *head,
                  const gchar *key, gchar *buf, gsize len)
{

	gchar *nval;
	const gchar *tmp;
	gint i;

	tmp = binary_to_enc (buf[0]);
	nval = convert_id3_text (tmp, &buf[1], len - 1, NULL);
	if (nval) {
		i = strtol (nval, NULL, 10);
		xmms_xform_metadata_set_int (xform, key, i);
		g_free (nval);
	}

}

/* Parse a string like '1/10' and return how many integers were found */
static gint
parse_number_slash_number (const gchar *value, gint *first, gint *second)
{
	gchar *endptr = NULL;
	gint ivalue;

	*first = *second = -1;

	ivalue = strtol (value, &endptr, 10);
	if (!(endptr > value)) {
		return 0;
	}

	*first = ivalue;

	if (*endptr != '/') {
		return 1;
	}

	value = (const gchar *) endptr + 1;

	ivalue = strtol (value, &endptr, 10);
	if (!(endptr > value)) {
		return 1;
	}

	*second = ivalue;

	return 2;
}

static void
handle_tracknr_field (xmms_xform_t *xform, xmms_id3v2_header_t *head,
                      const gchar *key, gchar *buf, gsize len)
{
	const gchar *enc;
	gchar *nval;
	gsize clen;

	enc = binary_to_enc (buf[0]);
	nval = convert_id3_text (enc, &buf[1], len - 1, &clen);
	if (nval) {
		gint tracknr, tracktotal;

		parse_number_slash_number (nval, &tracknr, &tracktotal);
		if (tracknr > 0) {
			const gchar *metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR;
			xmms_xform_metadata_set_int (xform, metakey, tracknr);
		}
		if (tracktotal > 0) {
			const gchar *metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TOTALTRACKS;
			xmms_xform_metadata_set_int (xform, metakey, tracktotal);
		}

		g_free (nval);
	}
}

static void
handle_partofset_field (xmms_xform_t *xform, xmms_id3v2_header_t *head,
                        const gchar *key, gchar *buf, gsize len)
{
	const gchar *enc;
	gchar *nval;
	gsize clen;

	enc = binary_to_enc (buf[0]);
	nval = convert_id3_text (enc, &buf[1], len - 1, &clen);
	if (nval) {
		gint partofset, totalset;

		parse_number_slash_number (nval, &partofset, &totalset);
		if (partofset > 0) {
			const gchar *metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET;
			xmms_xform_metadata_set_int (xform, metakey, partofset);
		}
		if (totalset > 0) {
			const gchar *metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TOTALSET;
			xmms_xform_metadata_set_int (xform, metakey, totalset);
		}
		g_free (nval);
	}
}

static void
handle_id3v2_ufid (xmms_xform_t *xform, xmms_id3v2_header_t *head,
                   const gchar *key, gchar *buf, gsize len)
{
	const gchar *val;

	val = find_nul (buf, &len);
	if (!val)
		return;

	if (g_ascii_strcasecmp (buf, "http://musicbrainz.org") == 0) {
		const gchar *metakey;
		gchar *val0;
		/* make sure it is NUL terminated */
		val0 = g_strndup (val, len);

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID,
		xmms_xform_metadata_set_str (xform, metakey, val0);

		g_free (val0);
	}
}

static void
handle_id3v2_apic (xmms_xform_t *xform, xmms_id3v2_header_t *head,
                   const gchar *key, gchar *buf, gsize len)
{
	const gchar *typ, *desc, *data, *mime;
	gchar hash[33];

	/* skip encoding */
	buf++;
	len--;
	mime = buf;
	typ = find_nul (buf, &len);

	if (!typ || len == 0) {
		XMMS_DBG ("Unable to read APIC frame, malformed tag?");
		return;
	}

	if (typ[0] != 0x00 && typ[0] != 0x03) {
		XMMS_DBG ("Picture type %02x not handled", typ[0]);
		return;
	}

	desc = typ + 1;
	len--;

	/* XXX desc might be UCS2 and find_nul will not do what we want */
	data = find_nul (desc, &len);

	if (data && xmms_bindata_plugin_add ((const guchar *)data, len, hash)) {
		const gchar *metakey;

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT;
		xmms_xform_metadata_set_str (xform, metakey, hash);

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT_MIME;
		xmms_xform_metadata_set_str (xform, metakey, mime);
	}
}

static void
handle_id3v2_comm (xmms_xform_t *xform, xmms_id3v2_header_t *head,
                   const gchar *key, gchar *buf, gsize len)
{
	/* COMM is weird but it's like this:
	 * $xx enc
	 * $xx xx xx lang
	 * $text $0 desc according to enc
	 * $text $0 comment according to enc
	 */
	const gchar *enc, *desc, *comm;
	gchar *cbuf;
	gsize clen;

	enc = binary_to_enc (buf[0]);
	buf++;
	len--;

	/* Language is always three _bytes_ - we currently don't care */
	buf += 3;
	len -= 3;

	cbuf = convert_id3_text (enc, buf, len, &clen);
	if (!cbuf)
		return;

	desc = cbuf;
	comm = find_nul (cbuf, &clen);

	if (comm && comm[0]) {
		const gchar *metakey;
		gchar *tmp;

		if (desc && desc[0]) {
			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT;
			tmp = g_strdup_printf ("%s_%s", metakey, desc);
			xmms_xform_metadata_set_str (xform, tmp, comm);
			g_free (tmp);
		} else {
			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT;
			xmms_xform_metadata_set_str (xform, metakey, comm);
		}
	}

	g_free (cbuf);

}

struct id3tags_t {
	guint32 type;
	const gchar *prop;
	void (*fun)(xmms_xform_t *, xmms_id3v2_header_t *, const gchar *, gchar *, gsize); /* Instead of add_to_entry */
};

static struct id3tags_t tags[] = {
	{ quad2long ('T','Y','E',0),   XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR, NULL },
	{ quad2long ('T','Y','E','R'), XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR, NULL },
	{ quad2long ('T','D','R','C'), XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR, NULL },
	{ quad2long ('T','D','O','R'), XMMS_MEDIALIB_ENTRY_PROPERTY_ORIGINALYEAR, NULL },
	{ quad2long ('T','O','R','Y'), XMMS_MEDIALIB_ENTRY_PROPERTY_ORIGINALYEAR, NULL },
	{ quad2long ('T','A','L',0),   XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM, NULL },
	{ quad2long ('T','A','L','B'), XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM, NULL },
	{ quad2long ('T','S','O','A'), XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_SORT, NULL },
	{ quad2long ('X','S','O','A'), XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_SORT, NULL },
	{ quad2long ('T','T','2',0),   XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, NULL },
	{ quad2long ('T','I','T','2'), XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, NULL },
	{ quad2long ('T','R','K',0),   XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR, handle_tracknr_field },
	{ quad2long ('T','R','C','K'), XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR, handle_tracknr_field },
	{ quad2long ('T','P','1',0),   XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST, NULL },
	{ quad2long ('T','P','E','1'), XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST, NULL },
	{ quad2long ('T','S','O','P'), XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_SORT, NULL },
	{ quad2long ('X','S','O','P'), XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_SORT, NULL },
	{ quad2long ('T','S','O','2'), XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST_SORT, NULL },
	{ quad2long ('T','C','O','N'), NULL, handle_id3v2_tcon },
	{ quad2long ('T','B','P',0),   XMMS_MEDIALIB_ENTRY_PROPERTY_BPM, handle_int_field },
	{ quad2long ('T','B','P','M'), XMMS_MEDIALIB_ENTRY_PROPERTY_BPM, handle_int_field },
	{ quad2long ('T','P','O','S'), XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET, handle_partofset_field },
	{ quad2long ('T','C','M','P'), XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION, handle_int_field },
	{ quad2long ('T','X','X','X'), NULL, handle_id3v2_txxx },
	{ quad2long ('U','F','I','D'), NULL, handle_id3v2_ufid },
	{ quad2long ('A','P','I','C'), NULL, handle_id3v2_apic },
	{ quad2long ('C','O','M','M'), NULL, handle_id3v2_comm },
	{ quad2long ('T','I','T','1'), XMMS_MEDIALIB_ENTRY_PROPERTY_GROUPING, NULL },
	{ quad2long ('T','I','T','3'), XMMS_MEDIALIB_ENTRY_PROPERTY_DESCRIPTION, NULL },
	{ quad2long ('T','P','E','2'), XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST, NULL },
	{ quad2long ('T','P','E','3'), XMMS_MEDIALIB_ENTRY_PROPERTY_CONDUCTOR, NULL },
	{ quad2long ('T','P','E','4'), XMMS_MEDIALIB_ENTRY_PROPERTY_REMIXER, NULL },
	{ quad2long ('T','O','P','E'), XMMS_MEDIALIB_ENTRY_PROPERTY_ORIGINAL_ARTIST, NULL },
	{ quad2long ('T','P','U','B'), XMMS_MEDIALIB_ENTRY_PROPERTY_PUBLISHER, NULL },
	{ quad2long ('T','C','O','M'), XMMS_MEDIALIB_ENTRY_PROPERTY_COMPOSER, NULL },
	{ quad2long ('T','E','X','T'), XMMS_MEDIALIB_ENTRY_PROPERTY_LYRICIST, NULL },
	{ quad2long ('T','S','R','C'), XMMS_MEDIALIB_ENTRY_PROPERTY_ISRC, NULL },
	{ quad2long ('T','C','O','P'), XMMS_MEDIALIB_ENTRY_PROPERTY_COPYRIGHT, NULL },
	{ quad2long ('W','O','A','R'), XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_ARTIST, NULL },
	{ quad2long ('W','O','A','F'), XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_FILE, NULL },
	{ quad2long ('W','P','U','B'), XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_PUBLISHER, NULL },
	{ quad2long ('W','C','O','P'), XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_COPYRIGHT, NULL },
	{ 0, NULL, NULL }
};

static void
handle_id3v2_text (xmms_xform_t *xform, xmms_id3v2_header_t *head,
                   guint32 type, gchar *buf, guint flags, gint len)
{
	gint i = 0;

	if (len < 1) {
		XMMS_DBG ("Skipping short id3v2 text-frame");
		return;
	}

	while (tags[i].type != 0) {
		if (tags[i].type == type) {
			if (tags[i].fun) {
				tags[i].fun (xform, head, tags[i].prop, buf, len);
			} else {
				add_to_entry (xform, head, tags[i].prop, buf, len);
			}
			return;
			break;
		}
		i++;
	}
	XMMS_DBG ("Unhandled tag '%c%c%c%c'", (type >> 24) & 0xff, (type >> 16) & 0xff, (type >> 8) & 0xff, (type) & 0xff);
}


gboolean
xmms_id3v2_is_header (guchar *buf, xmms_id3v2_header_t *header)
{
	typedef struct {
		/* All members are defined in terms of chars so padding does not
		 * occur. Is there a cleaner way to keep the compiler from
		 * padding? */

		guchar     id[3];
		guchar     ver;
		guchar     rev;
		guchar     flags;
		guchar     size[4];
	} id3head_t;

	id3head_t *id3head;

	id3head = (id3head_t *) buf;

	if (strncmp ((gchar *)id3head->id, "ID3", 3)) return FALSE;

	if (id3head->ver > 4 || id3head->ver < 2) {
		XMMS_DBG ("Unsupported id3v2 version (%d)", id3head->ver);
		return FALSE;
	}

	if ((id3head->size[0] | id3head->size[1] | id3head->size[2] |
	     id3head->size[3]) & 0x80) {
		xmms_log_error ("id3v2 tag having lenbyte with msb set "
		                "(%02x %02x %02x %02x)!  Probably broken "
		                "tag/tag-writer. Skipping tag.",
		                id3head->size[0], id3head->size[1],
		                id3head->size[2], id3head->size[3]);
		return FALSE;
	}

	header->ver = id3head->ver;
	header->rev = id3head->rev;
	header->flags = id3head->flags;

	header->len = id3head->size[0] << 21 | id3head->size[1] << 14 |
	              id3head->size[2] << 7 | id3head->size[3];

	if (id3head->flags & ID3v2_HEADER_FLAGS_FOOTER) {
		/* footer is copy of header */
		header->len += sizeof (id3head_t);
	}

	XMMS_DBG ("Found id3v2 header (version=%d, rev=%d, len=%d, flags=%x)",
	          header->ver, header->rev, header->len, header->flags);

	return TRUE;
}


/**
 *
 */
gboolean
xmms_id3v2_parse (xmms_xform_t *xform,
                  guchar *buf, xmms_id3v2_header_t *head)
{
	gint len=head->len;
	gboolean broken_version4_frame_size_hack = FALSE;

	if ((head->flags & ~ID3v2_HEADER_SUPPORTED_FLAGS) != 0) {
		XMMS_DBG ("ID3v2 contain unsupported flags, skipping tag");
		return FALSE;
	}

	if (head->flags & ID3v2_HEADER_FLAGS_UNSYNC) {
		int i, j;
		XMMS_DBG ("Removing false syncronisations from id3v2 tag");
		for (i = 0, j = 0; i < len; i++, j++) {
			buf[i] = buf[j];
			if (i < len-1 && buf[i] == 0xff && buf[i+1] == 0x00) {
				XMMS_DBG (" - false sync @%d", i);
				/* skip next byte */
				i++;
			}
		}
		len = j;
		XMMS_DBG ("Removed %d false syncs", i-j);
	}

	while (len>0) {
		gsize size;
		guint flags;
		guint32 type;

		if (head->ver == 3 || head->ver == 4) {
			if ( len < 10) {
				XMMS_DBG ("B0rken frame in ID3v2tag (len=%d)", len);
				return FALSE;
			}

			type = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | (buf[3]);
			if (head->ver == 3) {
				size = (buf[4]<<24) | (buf[5]<<16) | (buf[6]<<8) | (buf[7]);
			} else {
				guchar *tmp;
				guint next_size;

				if (!broken_version4_frame_size_hack) {
					size = (buf[4]<<21) | (buf[5]<<14) | (buf[6]<<7) | (buf[7]);
					/* The specs say the above, but many taggers (inluding iTunes)
					 * don't follow the spec and writes the int without synchsafe.
					 * Since we want to be according to the spec we try correct
					 * behaviour first, if that doesn't work out we try the former
					 * behaviour. Yay for specficiations.
					 *
					 * Identification of "erronous" frames aren't that pretty. We
					 * just check if the next frame seems to have a vaild size.
					 * This should probably be done better in the future.
					 * FIXME
					 */
					if (size + 18 <= len) {
						tmp = buf+10+size;
						next_size = (tmp[4]<<21) | (tmp[5]<<14) | (tmp[6]<<7) | (tmp[7]);

						if (next_size+10 > (len-size)) {
							XMMS_DBG ("Uho, seems like someone isn't using synchsafe integers here...");
							broken_version4_frame_size_hack = TRUE;
						}
					}
				}

				if (broken_version4_frame_size_hack) {
					size = (buf[4]<<24) | (buf[5]<<16) | (buf[6]<<8) | (buf[7]);
				}
			}

			if (size+10 > len) {
				XMMS_DBG ("B0rken frame in ID3v2tag (size=%d,len=%d)", (int)size, len);
				return FALSE;
			}

			flags = buf[8] | buf[9];

			if (buf[0] == 'T' || buf[0] == 'U' || buf[0] == 'A' || buf[0] == 'C') {
				handle_id3v2_text (xform, head, type, (gchar *)(buf + 10), flags, size);
			}

			if (buf[0] == 0) { /* padding */
				return TRUE;
			}

			buf += size+10;
			len -= size+10;
		} else if (head->ver == 2) {
			if (len < 6) {
				XMMS_DBG ("B0rken frame in ID3v2tag (len=%d)", len);
				return FALSE;
			}

			type = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8);
			size = (buf[3]<<16) | (buf[4]<<8) | buf[5];

			if (size+6 > len) {
				XMMS_DBG ("B0rken frame in ID3v2tag (size=%d,len=%d)", (int)size, len);
				return FALSE;
			}

			if (buf[0] == 'T' || buf[0] == 'U' || buf[0] == 'C') {
				handle_id3v2_text (xform, head, type, (gchar *)(buf + 6), 0, size);
			}

			if (buf[0] == 0) { /* padding */
				return TRUE;
			}

			buf += size+6;
			len -= size+6;
		}

	}

	return TRUE;
}
