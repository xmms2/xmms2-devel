/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
 * 
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 */




/**
 * id3 and id3v2
 */

#include "xmms/util.h"
#include "xmms/playlist.h"
#include "id3.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>


#define ID3v2_HEADER_FLAGS_UNSYNC 0x80
#define ID3v2_HEADER_FLAGS_EXTENDED 0x40
#define ID3v2_HEADER_FLAGS_EXPERIMENTAL 0x20

#define quad2long(a,b,c,d) ((a << 24) | (b << 16) | (c << 8) | (d))


/*
 * There are some different string-types.
 *  we only recognize the ones with '>' at the moment.
 *
 *>TALB Album/Movie/Show title
 *>TBPM BPM (beats per minute)
 * TCOM Composer
 *>TCON Content type
 * TCOP Copyright message
 * TDAT Date
 * TDLY Playlist delay
 * TENC Encoded by
 * TEXT Lyricist/Text writer
 * TFLT File type
 * TIME Time
 * TIT1 Content group description
 *>TIT2 Title/songname/content description
 * TIT3 Subtitle/Description refinement
 * TKEY Initial key
 * TLAN Language(s)
 * TLEN Length
 * TMED Media type
 * TOAL Original album/movie/show title
 * TOFN Original filename
 * TOLY Original lyricist(s)/text writer(s)
 * TOPE Original artist(s)/performer(s)
 * TORY Original release year
 * TOWN File owner/licensee
 *>TPE1 Lead performer(s)/Soloist(s)
 * TPE2 Band/orchestra/accompaniment
 * TPE3 Conductor/performer refinement
 * TPE4 Interpreted, remixed, or otherwise modified by
 * TPOS Part of a set
 * TPUB Publisher
 *>TRCK Track number/Position in set
 * TRDA Recording dates
 * TRSN Internet radio station name
 * TRSO Internet radio station owner
 * TSIZ Size
 * TSRC ISRC (international standard recording code)
 * TSSE Software/Hardware and settings used for encoding
 *>TYER Year
 * TXXX User defined text information frame
 */

static void
add_to_entry (xmms_medialib_entry_t entry, gchar *key, guchar *val, gint len)
{
	gchar *nval;
	gsize readsize,writsize;
	GError *err = NULL;

	g_return_if_fail (len>0);

	if (len > 2 && ((val[0]==0xFF && val[1]==0xFE) || (val[0]==0xFE && val[1]==0xFF))) {
		nval = g_convert (val, len, "UTF-8", "USC-2", &readsize, &writsize, &err);
	} else {
		nval = g_convert (val, len, "UTF-8", "ISO-8859-1", &readsize, &writsize, &err);
	}

	if (err) {
		xmms_log_error ("Couldn't convert: %s", err->message);
		g_error_free (err);
		return;
	}
	XMMS_DBG ("%s=%s", key, nval);
	xmms_medialib_entry_property_set (entry, key, nval);	
	g_free (nval);
}

static void
xmms_mad_handle_id3v2_text (guint32 type, gchar *buf, guint flags, gint len, xmms_medialib_entry_t entry)
{

	if (len < 1) {
		XMMS_DBG ("Skipping short id3v2 text-frame");
		return;
	}
	
	if (!buf[0]){ /** Is this really correct? */
		buf++;
		len--;
		if (len < 1) {
			XMMS_DBG ("Skipping empty text-frame");
			return;
		}
	}

	switch (type) {
	case quad2long('T','Y','E',0):
	case quad2long('T','Y','E','R'): {
		add_to_entry(entry, XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR, buf, len);
		break;
	}
	case quad2long('T','A','L',0):
	case quad2long('T','A','L','B'): {
		add_to_entry(entry, XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM, buf, len);
		break;
	}
	case quad2long('T','T','2',0):
	case quad2long('T','I','T','2'): {
		add_to_entry(entry, XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, buf, len);
		break;
	}
	case quad2long('T','P','1',0):
	case quad2long('T','P','E','1'): {
		add_to_entry(entry, XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST, buf, len);
		break;
	}
	case quad2long('T','R','K',0):
	case quad2long('T','R','C','K'): {
		add_to_entry(entry, XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR, buf, len);
		break;
	}
	case quad2long('T','C','O','N'): {
		/* @todo we could resolve numeric types here */
		add_to_entry(entry, XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE, buf, len);
		break;
	}
	case quad2long('T','X','X','X'): {
		/* User defined, lets search for musicbrainz */
		guint32 l2 = strlen (buf);
		if (g_strcasecmp (buf, "MusicBrainz Album Id") == 0)
			add_to_entry(entry, XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID, buf+l2+1, len-l2-1);
		else if (g_strcasecmp (buf, "MusicBrainz Artist Id") == 0)
			add_to_entry(entry, XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID, buf+l2+1, len-l2-1);
		break;

	}
	case quad2long('U','F','I','D'): {
		guint32 l2 = strlen (buf);
		if (g_strcasecmp (buf, "http://musicbrainz.org") == 0)
			add_to_entry(entry, XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID, buf+l2+1, len-l2-1);
		break;
	}
	case quad2long('T','B','P',0):
	case quad2long('T','B','P','M'): {
		add_to_entry(entry, "bpm", buf, len);
		break;
	}
	}
}

gboolean
xmms_mad_id3v2_header (guchar *buf, xmms_id3v2_header_t *header)
{
	if (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') {
		header->ver = buf[3];
		header->rev = buf[4];
		
		if (header->ver == 3 || header->ver == 2) {
			gint32 len;
			
			header->flags = buf[5];
			
			len = (buf[6] << 21) | (buf[7] << 14) |
				(buf[8] << 7) | buf[9];

			if ((buf[6] | buf[7] | buf[8] | buf[9]) & 0x80) {
				XMMS_DBG ("WARNING: id3v2 tag having lenpath with msb set! Probably broken tag/tag-writer. %02x %02x %02x %02x",
					  buf[6], buf[7], buf[8], buf[9]);
			}
			
			header->len = len;

			return TRUE;
		}
		XMMS_DBG ("Unsupported id3v2 version (%d)", header->ver);
	}
	return FALSE;
}
/**
 * 
 */
gboolean
xmms_mad_id3v2_parse (guchar *buf, xmms_id3v2_header_t *head, xmms_medialib_entry_t entry)
{
	gint len=head->len;

	if (head->flags != 0) {
		/** @todo must handle unsync-flag */
		XMMS_DBG ("ID3v2 contain unsupported flags, skipping tag");
		return FALSE;
	}

	while (len>0) {
		guint size;
		guint flags;
		guint32 type;


		if (head->ver == 3) {
			if ( len < 10) {
				XMMS_DBG ("B0rken frame in ID3v2tag (len=%d)", len);
				return FALSE;
			}
			
			type = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | (buf[3]);
			
			size = (buf[4]<<24) | (buf[5]<<16) | (buf[6]<<8) | (buf[7]);
			
			if (size+10 > len) {
				XMMS_DBG ("B0rken frame in ID3v2tag (size=%d,len=%d)", size, len);
				return FALSE;
			}
			
			flags = buf[8] | buf[9];

			if (buf[0] == 'T' || buf[0] == 'U') {
				xmms_mad_handle_id3v2_text (type, buf + 10, flags, size, entry);
			}
			
			if (buf[0] == 0) { /* padding */
				return TRUE;
			}
			
			buf += size+10;
			len -= size+10;
		} else if (head->ver == 2) {
			if ( len < 6) {
				XMMS_DBG ("B0rken frame in ID3v2tag (len=%d)", len);
				return FALSE;
			}
			
			type = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8);
			size = (buf[3]<<16) | (buf[4]<<8) | buf[5];

			if (size+6 > len) {
				XMMS_DBG ("B0rken frame in ID3v2tag (size=%d,len=%d)", size, len);
				return FALSE;
			}

			if (buf[0] == 'T') {
				xmms_mad_handle_id3v2_text (type, buf + 6, 0, size, entry);
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

/**
 *  ID3 
 */

#define GENRE_MAX 0x94

const gchar *id3_genres[GENRE_MAX] =
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

typedef struct id3v1tag_St {
        char tag[3]; /* always "TAG": defines ID3v1 tag 128 bytes before EOF */
        char title[30];
        char artist[30];
        char album[30];
        char year[4];
        union {
                struct {
                        char comment[30];
                } v1_0;
                struct {
                        char comment[28];
                        char __zero;
                        unsigned char track_number;
                } v1_1;
        } u;
        unsigned char genre;
} id3v1tag_t;

/**
 * Samma, på svenska.
 */
gboolean
xmms_mad_id3_parse (guchar *buf, xmms_medialib_entry_t entry)
{
	id3v1tag_t *tag = (id3v1tag_t *) buf;
	gsize readsize,writsize;
	GError *err = NULL;
	gchar *tmp;

	if (strncmp (tag->tag, "TAG", 3) != 0) {
		return FALSE;
	}

	XMMS_DBG ("Found ID3v1 TAG!");

	tmp = g_convert (tag->artist, 30, "UTF-8", "ISO-8859-1", &readsize, &writsize, &err);
	if (!tmp) {
		g_clear_error (&err);
	} else {
		g_strstrip (tmp);
		xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST, tmp);
		g_free (tmp);
	}
	
	tmp = g_convert (tag->album, 30, "UTF-8", "ISO-8859-1", &readsize, &writsize, &err);
	if (!tmp) {
		g_clear_error (&err);
	} else {
		g_strstrip (tmp);
		xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM, tmp);
		g_free (tmp);
	}
	
	tmp = g_convert (tag->title, 30, "UTF-8", "ISO-8859-1", &readsize, &writsize, &err);
	if (!tmp) {
		g_clear_error (&err);
	} else {
		g_strstrip (tmp);
		xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, tmp);
		g_free (tmp);
	}
	
	tmp = g_convert (tag->year, 4, "UTF-8", "ISO-8859-1", &readsize, &writsize, &err);
	if (!tmp) {
		g_clear_error (&err);
	} else {
		g_strstrip (tmp);
		xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR, tmp);
		g_free (tmp);
	}

	if (tag->genre > GENRE_MAX) {
		xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE, "Unknown");
	} else {
		tmp = g_strdup ((gchar *)id3_genres[tag->genre]);
		xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE, tmp);
		g_free (tmp);
	}
	
	if (atoi (&tag->u.v1_1.track_number) > 0) {
		/* V1.1 */
		tmp = g_convert (tag->u.v1_1.comment, 28, "UTF-8", "ISO-8859-1", &readsize, &writsize, &err);
		g_strstrip (tmp);
		xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT, tmp);
		g_free (tmp);
		
		tmp = g_strdup_printf ("%d", (gint) tag->u.v1_1.track_number);
		xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR, tmp);
		g_free (tmp);
	} else {
		tmp = g_convert (tag->u.v1_1.comment, 30, "UTF-8", "ISO-8859-1", &readsize, &writsize, &err);
		g_strstrip (tmp);
		xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT, tmp);
		g_free (tmp);
	}

	return TRUE;
}
