#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/util.h"
#include "xmms/playlist.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>

/* 
 * Type definition
 */

struct id3v1tag_t {
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
};


/*
 * Function prototypes
 */

static gboolean xmms_id3_can_handle (const gchar *mimetype);
static gboolean xmms_id3_get_info (xmms_playlist_entry_t *entry);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_MEDIAINFO, 
			"ID3 (v1/v2) mediainfo " VERSION,
			"Plugin for extracting info from mpeg files.");
	
	xmms_plugin_method_add (plugin, "can_handle", xmms_id3_can_handle);
	xmms_plugin_method_add (plugin, "get_info", xmms_id3_get_info);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_id3_can_handle (const gchar *mimetype)
{
	g_return_val_if_fail (mimetype, FALSE);

	if (g_strcasecmp (mimetype, "audio/mpeg") == 0)
		return TRUE;

	return FALSE;
}

static gboolean
xmms_id3_get_info (xmms_playlist_entry_t *entry)
{
	FILE *fp;
	struct id3v1tag_t tag;

	fp = fopen (entry->uri, "rb");
	if (!fp)
		return FALSE;

	g_snprintf (entry->title, XMMS_PL_PROPERTY, "%s", tag.title);
	g_snprintf (entry->artist, XMMS_PL_PROPERTY, "%s", tag.artist);
	g_snprintf (entry->album, XMMS_PL_PROPERTY, "%s", tag.album);
	/*FIXME: comment?*/
	entry->year = strtol (tag.year, NULL, 10);
	entry->genre = (gint) tag.genre;

	return TRUE;
}
