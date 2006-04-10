/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 */




/**
 * id3v2
 */

#include "xmms/xmms_medialib.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_xformplugin.h"
#include "id3.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define ID3v2_HEADER_FLAGS_UNSYNC 0x80
#define ID3v2_HEADER_FLAGS_EXTENDED 0x40
#define ID3v2_HEADER_FLAGS_EXPERIMENTAL 0x20
#define ID3v2_HEADER_FLAGS_FOOTER 0x10

#define MUSICBRAINZ_VA_ID "89ad4ac3-39f7-470e-963a-56509c546377"

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

const gchar *id3_genres[] =
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

static gchar *
convert_id3_text (xmms_id3v2_header_t *head, 
		  guchar *val, 
		  gint len)
{
	gchar *nval = NULL;
	gsize readsize,writsize;
	GError *err = NULL;

	g_return_val_if_fail (len>0, NULL);

	if (head->ver == 4) {
		if (len <= 1) {
			/* we don't want to handle this at all, probably an empty tag */
			return NULL;
		} else if (val[0] == 0x00) {
			/* ISO-8859-1 */
			nval = g_convert ((gchar *)val+1, len-1, "UTF-8", "ISO-8859-1", &readsize, &writsize, &err);
		} else if (len > 3 && val[0] == 0x01 && ((val[1] == 0xFF && val[2] == 0xFE) || (val[1] == 0xFE && val[2] == 0xFF))) {
			/* UTF-16 with BOM */
			nval = g_convert ((gchar *)val+1, len-1, "UTF-8", "UTF-16", &readsize, &writsize, &err);
		} else if (val[0] == 0x02) {
			/* UTF-16 without BOM */
			nval = g_convert ((gchar *)val+1, len-1, "UTF-8", "UTF-16BE", &readsize, &writsize, &err);
		} else if (val[0] == 0x03) {
			/* UTF-8 */
			nval = g_strndup ((gchar *)val+1, len-1);
		} else {
			XMMS_DBG ("UNKNOWN id3v2.4 encoding (%02x)!", val[0]);
			return NULL;
		}
	} else if (head->ver == 2 || head->ver == 3) {
		if (len <= 1) {
			/* we don't want to handle this at all, probably an empty tag */
			return NULL;
		} else if (val[0] == 0x00) {
			nval = g_convert ((gchar *)val+1, len-1, "UTF-8", "ISO-8859-1", &readsize, &writsize, &err);
		} else if (val[0] == 0x01) {
			if (len > 2 && val[1] == 0xFF && val[2] == 0xFE) {
				nval = g_convert ((gchar *)val+3, len-3, "UTF-8", "UCS-2LE", &readsize, &writsize, &err);
			} else if (len > 2 && val[1] == 0xFE && val[2] == 0xFF) {
				nval = g_convert ((gchar *)val+3, len-3, "UTF-8", "UCS-2BE", &readsize, &writsize, &err);
			} else {
				XMMS_DBG ("Missing/bad boom in id3v2 tag!");
				return NULL;
			}
		} else {
			XMMS_DBG ("UNKNOWN id3v2.2/2.3 encoding (%02x)!", val[0]);
			return NULL;
		}
	}

	if (err) {
		xmms_log_error ("Couldn't convert: %s", err->message);
		g_error_free (err);
		return NULL;
	}

	g_assert (nval);

	return nval;
}


static void
add_to_entry (xmms_xform_t *xform,
              xmms_id3v2_header_t *head,
              gchar *key,
              guchar *val,
              gint len)
{
	gchar *nval;

	nval = convert_id3_text (head, val, len);
	if (nval) {
		xmms_xform_metadata_set_str (xform, key, nval);
		g_free (nval);
	}
}

static void
xmms_mad_handle_id3v2_tcon (xmms_xform_t *xform,
                            xmms_id3v2_header_t *head,
                            gchar *key,
                            guchar *buf,
                            gint len)
{
	gint res;
	guint genre_id;
	gchar *val;

	if (head->ver == 4) {
		buf++;
		len -= 1; /* total len of buffer */
	}

	val = convert_id3_text (head, buf, len);
	if (!val)
		return;
	res = sscanf (val, "(%u)", &genre_id);

	if (res > 0 && genre_id < G_N_ELEMENTS(id3_genres)) {
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE, 
		                             (gchar *)id3_genres[genre_id]);
	} else {
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE, 
		                             val);
	}

	g_free (val);
}

static void
xmms_mad_handle_id3v2_txxx (xmms_xform_t *xform,
                            xmms_id3v2_header_t *head, 
                            gchar *key, 
                            guchar *buf, 
                            gint len)
{

	guint32 l2;
	gchar *val;

	if (head->ver == 4) {
		buf++;
		len -= 1; /* total len of buffer */
	}

	l2 = strlen ((gchar *)buf);

	val = g_strndup ((gchar *)(buf+l2+1), len-l2-1);

	if ((len - l2 - 1) < 1) {
		g_free (val);
		return;
	}

	if (g_strcasecmp ((gchar *)buf, "MusicBrainz Album Id") == 0)
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID,
		                             val);
	else if (g_strcasecmp ((gchar *)buf, "MusicBrainz Artist Id") == 0)
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID,
		                             val);
	else if ((g_strcasecmp ((gchar *)buf, "MusicBrainz Album Artist Id") == 0) &&
		 (g_strncasecmp ((gchar *)(buf+l2+1), MUSICBRAINZ_VA_ID, len-l2-1) == 0)) {
		xmms_xform_metadata_set_int (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION, 1);
	}

	g_free (val);
}

static void
xmms_mad_handle_int_field (xmms_xform_t *xform,
                           xmms_id3v2_header_t *head, 
                           gchar *key, 
                           guchar *buf, 
                           gint len)
{

	gchar *nval;
	gint i;

	nval = convert_id3_text (head, buf, len);
	if (nval) {
		i = strtol (nval, NULL, 10);
		xmms_xform_metadata_set_int (xform, key, i);
		g_free (nval);
	}

}

static void
xmms_mad_handle_id3v2_ufid (xmms_xform_t *xform,
                            xmms_id3v2_header_t *head, 
                            gchar *key, 
                            guchar *buf, 
                            gint len)
{
	gchar *val;
	guint32 l2 = strlen ((gchar *)buf);
	val = g_strndup ((gchar *)(buf+l2+1), len-l2-1);
	if (g_strcasecmp ((gchar *)buf, "http://musicbrainz.org") == 0)
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID, val);
	g_free (val);
}

struct id3tags_t {
	guint32 type;
	gchar *prop;
	void (*fun)(xmms_xform_t *, xmms_id3v2_header_t *, gchar *, guchar *, gint); /* Instead of add_to_entry */
};

static struct id3tags_t tags[] = {
	{ quad2long('T','Y','E',0), XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR, NULL },
	{ quad2long('T','Y','E','R'), XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR, NULL },
	{ quad2long('T','A','L',0), XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM, NULL },
	{ quad2long('T','A','L','B'), XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM, NULL },
	{ quad2long('T','T','2',0), XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, NULL },
	{ quad2long('T','I','T','2'), XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, NULL },
	{ quad2long('T','R','K',0), XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR, xmms_mad_handle_int_field },
	{ quad2long('T','R','C','K'), XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR, xmms_mad_handle_int_field },
	{ quad2long('T','P','1',0), XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST, NULL },
	{ quad2long('T','P','E','1'), XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST, NULL },
	{ quad2long('T','C','O','N'), NULL, xmms_mad_handle_id3v2_tcon },
	{ quad2long('T','B','P',0), XMMS_MEDIALIB_ENTRY_PROPERTY_BPM, xmms_mad_handle_int_field },
	{ quad2long('T','B','P','M'), XMMS_MEDIALIB_ENTRY_PROPERTY_BPM, xmms_mad_handle_int_field },
	{ quad2long('T','X','X','X'), NULL, xmms_mad_handle_id3v2_txxx },
	{ quad2long('U','F','I','D'), NULL, xmms_mad_handle_id3v2_ufid },
	{ 0, NULL, NULL }
};


static void
xmms_mad_handle_id3v2_text (xmms_xform_t *xform,
                            xmms_id3v2_header_t *head, 
                            guint32 type, guchar *buf, 
                            guint flags, 
                            gint len)
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
	XMMS_DBG ("Unhandled tag %c%c%c%c", (type >> 24) & 0xff, (type >> 16) & 0xff, (type >> 8) & 0xff, (type) & 0xff);
}


gboolean
xmms_mad_id3v2_header (guchar *buf, xmms_id3v2_header_t *header)
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
	
	if (strncmp((gchar *)id3head->id, "ID3", 3)) return FALSE;

	if (id3head->ver > 4 || id3head->ver < 2) {
		XMMS_DBG ("Unsupported id3v2 version (%d)", id3head->ver);
		return FALSE;
	}
	
	if ((id3head->size[0] | id3head->size[1] | id3head->size[2] |
	     id3head->size[3]) & 0x80) {
		xmms_log_error ("id3v2 tag having lenpath with msb set "
		                "(%02x %02x %02x %02x)!  Probably broken "
		                "tag/tag-writer. Skipping Tag.",
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
		header->len += sizeof(id3head_t);
	}

	XMMS_DBG ("Found id3v2 header (version=%d, rev=%d, len=%d, flags=%x)",
	          header->ver, header->rev, header->len, header->flags);

	return TRUE;
}


/**
 * 
 */
gboolean
xmms_mad_id3v2_parse (xmms_xform_t *xform,
                      guchar *buf, xmms_id3v2_header_t *head)
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


		if (head->ver == 3 || head->ver == 4) {
			if ( len < 10) {
				XMMS_DBG ("B0rken frame in ID3v2tag (len=%d)", len);
				return FALSE;
			}
			
			type = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | (buf[3]);
			if (head->ver == 3) {
				size = (buf[4]<<24) | (buf[5]<<16) | (buf[6]<<8) | (buf[7]);
			} else {
				size = (buf[4]<<21) | (buf[5]<<14) | (buf[6]<<7) | (buf[7]);
			}

			if (size+10 > len) {
				XMMS_DBG ("B0rken frame in ID3v2tag (size=%d,len=%d)", size, len);
				return FALSE;
			}
			
			flags = buf[8] | buf[9];

			if (buf[0] == 'T' || buf[0] == 'U') {
				xmms_mad_handle_id3v2_text (xform, head, type, buf + 10, flags, size);
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
				XMMS_DBG ("B0rken frame in ID3v2tag (size=%d,len=%d)", size, len);
				return FALSE;
			}

			if (buf[0] == 'T' || buf[0] == 'U') {
				xmms_mad_handle_id3v2_text (xform, head, type, buf + 6, 0, size);
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

