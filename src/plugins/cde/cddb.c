/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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

/** @file A nice CDDB query tool */

#include "xmms/plugin.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/output.h"
#include "xmms/transport.h"
#include "xmms/xmms.h"

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

#include "cdae.h"

#define CDDB_READ "/~cddb/cddb.cgi?cmd=cddb+read+"
#define CDDB_QUERY "/~cddb/cddb.cgi?cmd=cddb+query+"

static gint 
cddb_sum (gint in)
{
        gint retval = 0;
                                                                                
        while (in > 0)
        {
                retval += in % 10;
                in /= 10;
        }
        return retval;
}

static guint32 
xmms_cdae_cddb_discid (xmms_cdae_toc_t *toc)
{
        gint i;
        guint high = 0, low;
                                                                                
        for (i = toc->first_track; i <= toc->last_track; i++)
                high += cddb_sum (toc->track[i].minute * 60 + toc->track[i].second);
                                                                                
        low = (toc->leadout.minute * 60 + toc->leadout.second) -
                (toc->track[toc->first_track].minute * 60 +
                 toc->track[toc->first_track].second);
                                                                                
        return ((high % 0xff) << 24 | low << 8 | (toc->last_track - toc->first_track + 1));
}

static gchar *
xmms_cdae_cddb_offsets (xmms_cdae_toc_t *toc)
{
        gchar *buffer;
        gint i;
        
	buffer = g_malloc (toc->last_track * 7 + 1);
        
	sprintf (buffer, "%d", LBA (toc->track[toc->first_track]));
        
	for (i = toc->first_track + 1; i <= toc->last_track; i++)
                sprintf (buffer, "%s+%d", buffer, LBA (toc->track[i]));
        
	return buffer;
}

static gchar *
xmms_cdae_cddb_url (xmms_cdae_toc_t *toc)
{
	gchar *ret;
	gchar *tmp;

	tmp = xmms_cdae_cddb_offsets (toc);

	ret = g_strdup_printf ("%08x+%d+%s+%d&hello=nobody+localhost+XMMS+%s&proto=1",
				xmms_cdae_cddb_discid (toc),
				toc->last_track - toc->first_track + 1,
				tmp,
				(toc->leadout.minute * 60) + toc->leadout.second,
				XMMS_VERSION);

	g_free (tmp);

	return ret;

}

static gchar *
xmms_cdae_cddb_read_url (guint discid, gchar *category)
{
	gchar *ret;

	ret = g_strdup_printf ("%s+%08x&hello=nobody+localhost+XMMS+%s&proto=1",
				category,
				discid,
				XMMS_VERSION);

	return ret;
}

#ifdef HAVE_CURL

typedef struct {
	xmms_playlist_entry_t *entry;
	gint state; /* 1 query, 2 read */
	gchar *category;
	guint discid;
	gboolean ok;
	FILE *fp;
} xmms_cdae_cddb_info_t;

static size_t
xmms_cdae_cddb_cwrite (void *ptr, size_t size, size_t nmemb, void *stream)
{
	xmms_cdae_cddb_info_t *info = stream;


	if (info->state == 1) {
		gchar **l;

		l = g_strsplit ((gchar *)ptr, " ", 4);
		if (g_strcasecmp (l[0], "200") == 0) {
			/* One match */
			if (!l[1] || !l[2]) {
				XMMS_DBG ("p00p");
				g_strfreev (l);
				return size * nmemb;
			}
			info->category = g_strdup (l[1]);
			info->discid = strtoul (l[2], NULL, 16);
			info->ok = TRUE;
		}

		g_strfreev (l);
	} else if (info->state == 2) {
		gchar *p;

		p = strchr ((gchar *)ptr, '\n');
		if (p) {
			p++;
			fwrite (p, (size*nmemb)-(p-(gchar*)ptr), 1, info->fp);
			return size*nmemb;
		}
		return -1;
	}

	return size * nmemb;
}

static void
xmms_cdae_cddb_request (xmms_cdae_toc_t *toc, gchar *server)
{
	xmms_cdae_cddb_info_t *info;
	CURL *curl;
	gchar *url;
	gchar *cddburl;
	gchar error[CURL_ERROR_SIZE];
	gchar *file;

	curl = curl_easy_init ();

	url = xmms_cdae_cddb_url (toc);

	cddburl = g_strdup_printf ("http://%s%s%s", server, CDDB_QUERY, url);
	info = g_new0 (xmms_cdae_cddb_info_t, 1);
	info->state = 1;

	g_free (url);

	XMMS_DBG ("CDDB URL: %s ", cddburl);

        curl_easy_setopt (curl, CURLOPT_URL, cddburl );
        curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, xmms_cdae_cddb_cwrite);
        curl_easy_setopt (curl, CURLOPT_WRITEDATA, info);
        curl_easy_setopt (curl, CURLOPT_HTTPGET, 1);
        curl_easy_setopt (curl, CURLOPT_USERAGENT, "XMMS/" XMMS_VERSION);
	curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, error);

	if (curl_easy_perform (curl) != 0) {
		XMMS_DBG ("Error: %s when fetching CDDB information", error);
		curl_easy_cleanup (curl);
		g_free (info);
		g_free (cddburl);
		return;
	}

	g_free (cddburl);

	if (!info->ok) {
		curl_easy_cleanup (curl);
		g_free (info);
		return;
	}

	url = xmms_cdae_cddb_read_url (info->discid, info->category);

	cddburl = g_strdup_printf ("http://%s%s%s", server, CDDB_READ, url);
	info->state = 2;

	file = g_strdup_printf ("%s/.cddb/%08x", g_get_home_dir (), info->discid);

	info->fp = fopen (file, "wb");
	g_free (file);

	if (!info->fp) {
		curl_easy_cleanup (curl);
		g_free (info);
		return;
	}

	fwrite ("# Added by XMMS/" XMMS_VERSION "\n", 17+strlen (XMMS_VERSION), 1, info->fp);

        curl_easy_setopt (curl, CURLOPT_URL, cddburl );
        curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, xmms_cdae_cddb_cwrite);
        curl_easy_setopt (curl, CURLOPT_WRITEDATA, info);
        curl_easy_setopt (curl, CURLOPT_HTTPGET, 1);
        curl_easy_setopt (curl, CURLOPT_USERAGENT, "XMMS/" XMMS_VERSION);
	curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, error);

	if (curl_easy_perform (curl) != 0) {
		XMMS_DBG ("Error: %s when fetching CDDB information", error);
		curl_easy_cleanup (curl);
		fclose (info->fp);
		g_free (info);
		g_free (cddburl);
		return;
	}

	fclose (info->fp);
	curl_easy_cleanup (curl);
	g_free (info);
	g_free (cddburl);
	return;

}

xmms_playlist_entry_t *
xmms_cdae_cddb_parse (FILE *fp, gint track)
{
	xmms_playlist_entry_t *entry;
	gchar buffer[2046];
	gchar **lines;
	gint ret;
	gint i=0;
	gint t=1;

	ret = fread (buffer, 1, 2046, fp);

	entry = xmms_playlist_entry_new (NULL);

	buffer[ret] = '\0';

	lines = g_strsplit (buffer, "\r\n", 0);
	while (lines[i]) {
		gchar **kv;

		if (lines[i][0] == '#') {
			i++;
			continue;
		}

		kv = g_strsplit (lines[i], "=", 2);
		if (kv && kv[0] && kv[1]) {
			if (g_strcasecmp (kv[0], "DTITLE") == 0) {
				gchar *p = strchr (kv[1], '/');
				*(p-1)='\0';
				xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST, g_strdup(kv[1]));
				xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM, g_strdup(p+2));
			} else if (g_strncasecmp (kv[0], "TTITLE", 6) == 0) {
				if (t == track) {
					xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE, g_strdup (kv[1]));
				}
				t++;
			}
		} else {
			if (kv)
				g_strfreev (kv);
			i++;
			continue;
		}

		i++;
		g_strfreev (kv);

	}

	g_strfreev (lines);

	return entry;
}

xmms_playlist_entry_t *
xmms_cdae_cddb_query (xmms_cdae_toc_t *toc, gchar *server, gint track)
{
	FILE *fp;
	gchar *file;
	gchar *dir;
	xmms_playlist_entry_t *entry;

	dir = g_strdup_printf ("%s/.cddb/", g_get_home_dir ());
	file = g_strdup_printf ("%s/.cddb/%08x", g_get_home_dir (), xmms_cdae_cddb_discid (toc));

	fp = NULL;

	if (!g_file_test (dir, G_FILE_TEST_IS_DIR)) {
		mkdir (dir, 0755);
		xmms_cdae_cddb_request (toc, server);
	} else {
		if (!(fp = fopen (file, "rb"))) {
			xmms_cdae_cddb_request (toc, server);
		}
	}

	g_free (dir);

	if (!fp) {
		if (!(fp = fopen (file, "rb"))) {
			g_free (file);
			return NULL;
		}
	}

	g_free (file);

	entry = xmms_cdae_cddb_parse (fp, track);

	return entry;

}


#else
xmms_playlist_entry_t *
xmms_cdae_cddb_query (xmms_cdae_toc_t *toc, gchar *server, gint track)
{
	return NULL;
}
#endif

