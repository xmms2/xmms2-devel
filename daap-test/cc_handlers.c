
/* XXX: this code assumes the target architecture is little-endian,
 * as evidenced by the endian_swap_foo functions. daap sends its numerical
 * data (int/short/long/etc.) in big-endian format. */

#define _GNU_SOURCE
#include <string.h>

#include <stdlib.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "cc_handlers.h"
#include "daap_conn.h"

#define DMAP_BYTES_REMAINING ((gint) (data_end - current_data))

/* from daap_conn */
extern session_t login_data;

db_list_t databases;
song_list_t song_items;
pl_list_t playlists;
playlist_t playlist_items;

void endian_swap_int16(gint16 *i)
{
	gint16 tmp;
	tmp = (gint16) (((*i >>  8) & 0x00FF) |
	                ((*i <<  8) & 0xFF00));
	*i = tmp;
}

void endian_swap_int32(gint32 *i)
{
	gint32 tmp;
	tmp = (gint32) (((*i >> 24) & 0x000000FF) |
	                ((*i >>  8) & 0x0000FF00) |
	                ((*i <<  8) & 0x00FF0000) |
	                ((*i << 24) & 0xFF000000));
	*i = tmp;
}

void endian_swap_int64(gint64 *i)
{
	gint64 tmp;
	tmp = (gint64) (                ((*i >> 40) & 0x00000000000000FF) |
	                                ((*i >> 32) & 0x000000000000FF00) |
	                                ((*i >> 24) & 0x0000000000FF0000) |
	                                ((*i >>  8) & 0x00000000FF000000) |
	               G_GINT64_CONSTANT((*i <<  8) & 0x000000FF00000000) |
	               G_GINT64_CONSTANT((*i << 24) & 0x0000FF0000000000) |
	               G_GINT64_CONSTANT((*i << 32) & 0x00FF000000000000) |
	               G_GINT64_CONSTANT((*i << 40) & 0xFF00000000000000));
	*i = tmp;
}

#if 0
gboolean is_valid_cc(gchar *cc)
{
	if (NULL == cc) {
		return FALSE;
	}
	return (is_dmap_cc(cc) | is_daap_cc(cc) | is_com_cc(cc));
}

gboolean is_dmap_cc(gchar *cc)
{
	gboolean rv;

	switch (CC_TO_INT(cc[0],cc[1],cc[2],cc[3])) {
		case CC_TO_INT('m','s','r','v'):
		case CC_TO_INT('m','c','c','r'):
		case CC_TO_INT('m','l','o','g'):
		case CC_TO_INT('m','u','p','d'):
		case CC_TO_INT('m','d','c','l'):
		case CC_TO_INT('m','s','t','t'):
		case CC_TO_INT('m','i','i','d'):
		case CC_TO_INT('m','i','n','m'):
		case CC_TO_INT('m','i','k','d'):
		case CC_TO_INT('m','p','e','r'):
		case CC_TO_INT('m','c','o','n'):
		case CC_TO_INT('m','c','t','i'):
		case CC_TO_INT('m','p','c','o'):
		case CC_TO_INT('m','s','t','s'):
		case CC_TO_INT('m','i','m','c'):
		case CC_TO_INT('m','r','c','o'):
		case CC_TO_INT('m','t','c','o'):
		case CC_TO_INT('m','l','c','l'):
		case CC_TO_INT('m','l','i','t'):
		case CC_TO_INT('m','b','c','l'):
		case CC_TO_INT('m','s','a','u'):
		case CC_TO_INT('m','s','l','r'):
		case CC_TO_INT('m','p','r','o'):
		case CC_TO_INT('m','s','a','l'):
		case CC_TO_INT('m','s','u','p'):
		case CC_TO_INT('m','s','p','i'):
		case CC_TO_INT('m','s','e','x'):
		case CC_TO_INT('m','s','b','r'):
		case CC_TO_INT('m','s','q','y'):
		case CC_TO_INT('m','s','i','x'):
		case CC_TO_INT('m','s','r','s'):
		case CC_TO_INT('m','s','t','m'):
		case CC_TO_INT('m','s','d','c'):
		case CC_TO_INT('m','c','n','m'):
		case CC_TO_INT('m','c','n','a'):
		case CC_TO_INT('m','c','t','y'):
		case CC_TO_INT('m','l','i','d'):
		case CC_TO_INT('m','s','u','r'):
		case CC_TO_INT('m','u','t','y'):
		case CC_TO_INT('m','u','d','l'):
			rv = TRUE;
			break;
		default:
			rv = FALSE;
			break;
	}
	return rv;	
}

gboolean is_daap_cc(gchar *cc)
{
	gboolean rv;

	switch (CC_TO_INT(cc[0],cc[1],cc[2],cc[3])) {
		case CC_TO_INT('a','v','d','b'):
		case CC_TO_INT('a','p','s','o'):
		case CC_TO_INT('a','p','l','y'):
		case CC_TO_INT('a','p','r','o'):
		case CC_TO_INT('a','b','r','o'):
		case CC_TO_INT('a','b','a','l'):
		case CC_TO_INT('a','b','a','r'):
		case CC_TO_INT('a','b','c','p'):
		case CC_TO_INT('a','b','g','n'):
		case CC_TO_INT('a','d','b','s'):
		case CC_TO_INT('a','s','a','l'):
		case CC_TO_INT('a','s','a','r'):
		case CC_TO_INT('a','s','b','t'):
		case CC_TO_INT('a','s','b','r'):
		case CC_TO_INT('a','s','c','m'):
		case CC_TO_INT('a','s','c','o'):
		case CC_TO_INT('a','s','d','a'):
		case CC_TO_INT('a','s','d','m'):
		case CC_TO_INT('a','s','d','c'):
		case CC_TO_INT('a','s','d','n'):
		case CC_TO_INT('a','s','d','b'):
		case CC_TO_INT('a','s','e','q'):
		case CC_TO_INT('a','s','f','m'):
		case CC_TO_INT('a','s','g','n'):
		case CC_TO_INT('a','s','d','t'):
		case CC_TO_INT('a','s','r','v'):
		case CC_TO_INT('a','s','s','r'):
		case CC_TO_INT('a','s','s','z'):
		case CC_TO_INT('a','s','s','t'):
		case CC_TO_INT('a','s','s','p'):
		case CC_TO_INT('a','s','t','m'):
		case CC_TO_INT('a','s','t','c'):
		case CC_TO_INT('a','s','t','n'):
		case CC_TO_INT('a','s','u','r'):
		case CC_TO_INT('a','s','y','r'):
		case CC_TO_INT('a','s','d','k'):
		case CC_TO_INT('a','s','u','l'):
		case CC_TO_INT('a','b','p','l'):
		case CC_TO_INT('a','r','s','v'):
		case CC_TO_INT('a','r','i','f'):
			rv = TRUE;
			break;
		default:
			rv = FALSE;
			break;
	}
	return rv;	
}

gboolean is_com_cc(gchar *cc)
{
	gboolean rv;

	switch (CC_TO_INT(cc[0],cc[1],cc[2],cc[3])) {
		case CC_TO_INT('a','e','N','V'):
		case CC_TO_INT('a','e','S','P'):
			rv = TRUE;
			break;
		default:
			rv = FALSE;
			break;
	}
	return rv;	
}
#endif

gint cc_handler(gchar *data, gint data_len)
{
	gint offset = 0;

	switch (CC_TO_INT(data[0],data[1],data[2],data[3])) {
		case CC_TO_INT('a','d','b','s'):
			offset = cc_handler_adbs(data, data_len);
			break;
		case CC_TO_INT('m','s','r','v'):
			offset = cc_handler_msrv(data, data_len);
			break;
		case CC_TO_INT('m','c','c','r'):
			offset = cc_handler_mccr(data, data_len);
			break;
		case CC_TO_INT('m','l','o','g'):
			offset = cc_handler_mlog(data, data_len);
			break;
		case CC_TO_INT('m','u','p','d'):
			offset = cc_handler_mupd(data, data_len);
			break;
		case CC_TO_INT('a','v','d','b'):
			offset = cc_handler_avdb(data, data_len);
			break;
		case CC_TO_INT('a','p','s','o'):
			offset = cc_handler_apso(data, data_len);
			break;
		case CC_TO_INT('a','p','l','y'):
			offset = cc_handler_aply(data, data_len);
			break;
		default:
			offset = DMAP_UNKNOWN_CC;
			break;
	}

	return offset;
}

gint cc_handler_adbs(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	g_printf("Requested entire list of songs.\n");

	while ((current_data < data_end) && !do_break) {
		g_print("  ");
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(current_data);
				break;
			case CC_TO_INT('m','u','t','y'):
				offset += cc_handler_muty(current_data);
				break;
			case CC_TO_INT('m','t','c','o'):
				offset += cc_handler_mtco(current_data);
				break;	
			case CC_TO_INT('m','r','c','o'):
				offset += cc_handler_mrco(current_data);
				break;	
			case CC_TO_INT('m','l','c','l'):
				offset += cc_handler_mlcl(current_data, DMAP_BYTES_REMAINING);
				break;
			default:
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}
	
	return (gint) (current_data - data);
}

gint cc_handler_msrv(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	gint8 has_indexing;
	gint8 has_extensions;
	gint8 has_update;
	gint8 has_autologout;
	gint8 has_queries;
	gint8 has_resolve;
	gint8 has_browsing;
	gint8 has_persistent;
	gint8 auth_type;
	gint8 auth_method;
	gint8 login_required;

	gint16 daap_proto_major;
	gint16 daap_proto_minor;
	gint16 dmap_proto_major;
	gint16 dmap_proto_minor;

	gint32 timeout_interval;
	gint32 sharing_version;
	gint32 db_count;

	gchar *server_name = NULL;

	g_printf("Requested server info.\n");

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(current_data);
				break;
			case CC_TO_INT('a','p','r','o'):
				offset += grab_data_version(&daap_proto_major,
				                            &daap_proto_minor, current_data);
				g_printf("daap protocol version: %d.%d\n", daap_proto_major,
				                                           daap_proto_minor);
				break;
			case CC_TO_INT('m','p','r','o'):
				offset += grab_data_version(&dmap_proto_major,
				                            &dmap_proto_minor, current_data);
				g_printf("dmap protocol version: %d.%d\n", dmap_proto_major,
				                                           dmap_proto_minor);
				break;
			case CC_TO_INT('m','s','t','m'):
				offset += grab_data(&timeout_interval, current_data,
				                    DMAP_CTYPE_INT);
				g_printf("Server timeout interval: %d\n", timeout_interval);
				break;	
			case CC_TO_INT('m','s','i','x'):
				offset += grab_data(&has_indexing, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Server supports indexing: %d\n", has_indexing);
				break;	
			case CC_TO_INT('m','s','e','x'):
				offset += grab_data(&has_extensions, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Server supports extensions: %d\n", has_extensions);
				break;	
			case CC_TO_INT('m','s','u','p'):
				offset += grab_data(&has_update, current_data, DMAP_CTYPE_BYTE);
				g_printf("Server supports updates: %d\n", has_update);
				break;	
			case CC_TO_INT('m','s','a','l'):
				offset += grab_data(&has_autologout, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Server supports autologout: %d\n", has_autologout);
				break;	
			case CC_TO_INT('m','s','l','r'):
				offset += grab_data(&login_required, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Login required: %d\n", login_required);
				break;	
			case CC_TO_INT('m','s','q','y'):
				offset += grab_data(&has_queries, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Server supports queries: %d\n", has_queries);
				break;	
			case CC_TO_INT('m','s','r','s'):
				offset += grab_data(&has_resolve, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Server supports resolve: %d\n", has_resolve);
				break;	
			case CC_TO_INT('m','s','b','r'):
				offset += grab_data(&has_browsing, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Server supports browsing: %d\n", has_browsing);
				break;	
			case CC_TO_INT('m','s','p','i'):
				offset += grab_data(&has_persistent, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Server supports persistent IDs: %d\n",
				         has_persistent);
				break;	
			case CC_TO_INT('m','s','a','s'):
				offset += grab_data(&auth_type, current_data, DMAP_CTYPE_BYTE);
				g_printf("Server authentication type: %d\n",
				         auth_type);
				break;	
			case CC_TO_INT('m','s','a','u'):
				offset += grab_data(&auth_method, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Server authentication method: %d\n",
				         auth_method);
				break;	
			case CC_TO_INT('a','e','S','V'):
				offset += grab_data(&sharing_version, current_data,
				                    DMAP_CTYPE_INT);
				g_printf("Server music sharing version: %d\n",
				         sharing_version);
				break;	
			case CC_TO_INT('m','s','d','c'):
				offset += grab_data(&db_count, current_data, DMAP_CTYPE_INT);
				g_printf("Number of databases: %d\n", db_count);
				break;	
			case CC_TO_INT('m','i','n','m'):
				offset += grab_data(&server_name, current_data,
				                    DMAP_CTYPE_STRING);
				if (NULL == server_name) {
					g_printf("Server name: n/a\n");
				} else {
					g_printf("Server name: %s\n", server_name);
					g_free(server_name);
				}
				break;	
			default:
				g_printf("Unrecognized content code or end of data: %s\n",
				         current_data);
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}
	
	return (gint) (current_data - data);
}

gint cc_handler_mccr(gchar *data, gint data_len)
{
	return 0;
}

gint cc_handler_mlog(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	gint32 login_id;

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(current_data);
				break;
			case CC_TO_INT('m','l','i','d'):
				offset += grab_data(&login_id, current_data, DMAP_CTYPE_INT);
				g_printf("session id: %d\n", login_id);
				login_data.session_id = login_id;
				break;
			default:
				g_printf("Unrecognized content code or end of data: %s\n",
				         current_data);
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}
	login_data.request_id = 1;

	return (gint) (current_data - data);
}

gint cc_handler_mupd(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	gint32 revision_id;

	g_printf("Requested an update.\n");

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','u','s','r'):
				offset += grab_data(&revision_id, current_data, DMAP_CTYPE_INT);
				g_printf("Revision ID: %d\n", revision_id);
				login_data.revision_id = revision_id;
				break;
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(current_data);
				break;
			default:
				g_printf("Unrecognized content code or end of data: %s\n",
				         current_data);
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}

	return (gint) (current_data - data);
}

gint cc_handler_avdb(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	g_printf("Requested entire list of songs.\n");

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(current_data);
				break;
			case CC_TO_INT('m','u','t','y'):
				offset += cc_handler_muty(current_data);
				break;
			case CC_TO_INT('m','t','c','o'):
				offset += cc_handler_mtco(current_data);
				break;	
			case CC_TO_INT('m','r','c','o'):
				offset += cc_handler_mrco(current_data);
				break;	
			case CC_TO_INT('m','l','c','l'):
				offset += cc_handler_mlcl(current_data, DMAP_BYTES_REMAINING);
				break;
			default:
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}
	
	return (gint) (current_data - data);
}

gint cc_handler_apso(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;
	
	g_printf("requested playlists\n");

	while ((current_data < data_end) && !do_break) {
		g_print("  ");
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(current_data);
				break;
			case CC_TO_INT('m','u','t','y'):
				offset += cc_handler_muty(current_data);
				break;
			case CC_TO_INT('m','t','c','o'):
				offset += cc_handler_mtco(current_data);
				break;	
			case CC_TO_INT('m','r','c','o'):
				offset += cc_handler_mrco(current_data);
				break;	
			case CC_TO_INT('m','l','c','l'):
				offset += cc_handler_mlcl(current_data, DMAP_BYTES_REMAINING);
				break;
			default:
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}
	
	return (gint) (current_data - data);
}

gint cc_handler_aply(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;
	
	g_printf("requested playlists\n");

	while ((current_data < data_end) && !do_break) {
		g_print("  ");
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(current_data);
				break;
			case CC_TO_INT('m','u','t','y'):
				offset += cc_handler_muty(current_data);
				break;
			case CC_TO_INT('m','t','c','o'):
				offset += cc_handler_mtco(current_data);
				break;	
			case CC_TO_INT('m','r','c','o'):
				offset += cc_handler_mrco(current_data);
				break;	
			case CC_TO_INT('m','l','c','l'):
				offset += cc_handler_mlcl(current_data, DMAP_BYTES_REMAINING);
				break;
			default:
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}
	
	return (gint) (current_data - data);
}

gint cc_handler_mlcl(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	g_printf("List of records:\n");

	while (current_data < data_end && !do_break) {
		if (CC_TO_INT(current_data[0], current_data[1],
		              current_data[2], current_data[3]) ==
		    CC_TO_INT('m','l','i','t')) {

			offset += cc_handler_mlit(current_data, DMAP_BYTES_REMAINING);
		} else {
			break;
		}

		current_data += offset;
		offset = 0;
	}

	g_printf("End of record list.\n");

	return (gint)(current_data - data);
}

gint cc_handler_mlit(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	/* song list specific */
	gint8 item_kind;
	gint8 song_data_kind;
	gint8 song_compilation;
	gint8 is_smart_pl;
	gint8 is_base_pl;
	
	gint16 song_bitrate;
	gint16 song_year;
	gint16 song_track_no;
	gint16 song_track_count;
	gint16 song_disc_count;
	gint16 song_disc_no;
	gint16 song_bpm;

	gint32 dbid;
	gint32 sample_rate;
	gint32 song_size;
	gint32 song_start_time;
	gint32 song_stop_time;
	gint32 song_total_time;
	gint32 song_date;
	gint32 song_date_mod;
	gint32 container_id;

	/* FIXME? */
	gint32 deleted_id;

	gint64 persistent_id;
	
	gchar *iname = NULL;
	gchar *song_data_url = NULL;
	gchar *song_data_album = NULL;
	gchar *song_data_artist = NULL;
	gchar *song_comment = NULL;
	gchar *song_description = NULL;
	gchar *song_genre = NULL;
	gchar *song_format = NULL;
	gchar *song_composer = NULL;
	gchar *song_grouping = NULL;
	
	/* db list specific */
	gint32 db_n_items;
	gint32 db_n_playlist;

	g_printf("\nList item:\n");

	while (current_data < data_end && !do_break) {
		g_printf("  ");
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','i','i','d'):
				offset += grab_data(&dbid, current_data, DMAP_CTYPE_INT);
				g_printf("Database ID: %d\n", dbid);
				break;
			case CC_TO_INT('m','p','e','r'):
				offset += grab_data(&persistent_id, current_data,
				                    DMAP_CTYPE_LONG);
				g_printf("Persistent ID: %lld\n", persistent_id);
				break;
			case CC_TO_INT('m','i','n','m'):
				offset += grab_data(&iname, current_data, DMAP_CTYPE_STRING);
				g_printf("Name: %s\n", iname);
				g_free(iname);
				break;

			/* song list specific */
			case CC_TO_INT('m','i','k','d'):
				offset += grab_data(&item_kind, current_data, DMAP_CTYPE_BYTE);
				g_printf("Song/item kind: %d\n", item_kind);
				break;
			case CC_TO_INT('a','s','d','k'):
				offset += grab_data(&song_data_kind, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Song data kind: %d\n", song_data_kind);
				break;	
			case CC_TO_INT('a','s','u','l'):
				offset += grab_data(&song_data_url, current_data,
				                    DMAP_CTYPE_STRING);
				if (NULL != song_data_url) {
					g_printf("Song data URL: %s\n", song_data_url);
					g_free(song_data_url);
				} else {
					g_printf("Song data URL: n/a\n");
				}
				break;
			case CC_TO_INT('a','s','a','l'):
				offset += grab_data(&song_data_album, current_data,
				                    DMAP_CTYPE_STRING);
				g_printf("Song data album: %s\n", song_data_album);
				g_free(song_data_album);
				break;
			case CC_TO_INT('a','s','a','r'):
				offset += grab_data(&song_data_artist, current_data,
				                    DMAP_CTYPE_STRING);
				g_printf("Song data artist: %s\n", song_data_artist);
				g_free(song_data_artist);
				break;
			case CC_TO_INT('a','s','b','r'):
				offset += grab_data(&song_bitrate, current_data,
				                    DMAP_CTYPE_SHORT);
				g_printf("Song bitrate: %d\n", song_bitrate);
				break;
			case CC_TO_INT('a','s','c','m'):
				offset += grab_data(&song_comment, current_data,
				                    DMAP_CTYPE_STRING);
				g_printf("Song comment: %s\n", song_comment);
				g_free(song_comment);
				break;
			case CC_TO_INT('a','s','d','a'):
				offset += grab_data(&song_date, current_data, DMAP_CTYPE_DATE);
				g_printf("Song date added: %d\n", song_date);
				break;
			case CC_TO_INT('a','s','d','m'):
				offset += grab_data(&song_date_mod, current_data,
				                    DMAP_CTYPE_DATE);
				g_printf("Song date modified: %d\n", song_date_mod);
				break;
			case CC_TO_INT('a','s','g','n'):
				offset += grab_data(&song_genre, current_data,
				                    DMAP_CTYPE_STRING);
				g_printf("Song genre: %s\n", song_genre);
				g_free(song_genre);
				break;
			case CC_TO_INT('a','s','f','m'):
				offset += grab_data(&song_format, current_data,
				                    DMAP_CTYPE_STRING);
				g_printf("Song format: %s\n", song_format);
				g_free(song_format);
				break;
			case CC_TO_INT('a','s','d','t'):
				offset += grab_data(&song_description, current_data,
				                    DMAP_CTYPE_STRING);
				g_printf("Song description: %s\n", song_description);
				g_free(song_description);
				break;
			case CC_TO_INT('a','s','s','r'):
				offset += grab_data(&sample_rate, current_data, DMAP_CTYPE_INT);
				g_printf("Song sample rate: %d\n", sample_rate);
				break;
			case CC_TO_INT('a','s','s','z'):
				offset += grab_data(&song_size, current_data, DMAP_CTYPE_INT);
				g_printf("Song size: %d\n", song_size);
				break;
			case CC_TO_INT('a','s','s','t'):
				offset += grab_data(&song_start_time, current_data,
				                    DMAP_CTYPE_INT);
				g_printf("Song start time (ms): %d\n", song_start_time);
				break;
			case CC_TO_INT('a','s','s','p'):
				offset += grab_data(&song_stop_time, current_data,
				                    DMAP_CTYPE_INT);
				g_printf("Song stop time (ms): %d\n", song_stop_time);
				break;
			case CC_TO_INT('a','s','t','m'):
				offset += grab_data(&song_total_time, current_data,
				                    DMAP_CTYPE_INT);
				g_printf("Song total time (ms): %d\n", song_total_time);
				break;
			case CC_TO_INT('a','s','y','r'):
				offset += grab_data(&song_year, current_data, DMAP_CTYPE_SHORT);
				g_printf("Song year: %d\n", song_year);
				break;
			case CC_TO_INT('a','s','t','n'):
				offset += grab_data(&song_track_no, current_data,
				                    DMAP_CTYPE_SHORT);
				g_printf("Song track number: %d\n", song_track_no);
				break;
			case CC_TO_INT('a','s','c','p'):
				offset += grab_data(&song_composer, current_data,
				                    DMAP_CTYPE_STRING);
				g_printf("Song composer: %s\n", song_composer);
				g_free(song_composer);
				break;
			case CC_TO_INT('a','s','t','c'):
				offset += grab_data(&song_track_count, current_data,
				                    DMAP_CTYPE_SHORT);
				g_printf("Song track count: %d\n", song_track_count);
				break;
			case CC_TO_INT('a','s','d','c'):
				offset += grab_data(&song_disc_count, current_data,
				                    DMAP_CTYPE_SHORT);
				g_printf("Song disc count: %d\n", song_disc_count);
				break;
			case CC_TO_INT('a','s','d','n'):
				offset += grab_data(&song_disc_no, current_data,
				                    DMAP_CTYPE_SHORT);
				g_printf("Song disc number: %d\n", song_disc_no);
				break;
			case CC_TO_INT('a','s','c','o'):
				offset += grab_data(&song_compilation, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Song compilation: %d\n", song_compilation);
				break;
			case CC_TO_INT('a','s','b','t'):
				offset += grab_data(&song_bpm, current_data,
				                    DMAP_CTYPE_SHORT);
				g_printf("Song BPM: %d\n", song_bpm);
				break;
			case CC_TO_INT('a','g','r','p'):
				offset += grab_data(&song_grouping, current_data,
				                    DMAP_CTYPE_STRING);
				if (NULL != song_grouping) {
					g_printf("Song grouping: %s\n", song_grouping);
					g_free(song_grouping);
				} else {
					g_printf("Song grouping: n/a\n");
				}
				break;
			case CC_TO_INT('m','c','t','i'):
				offset += grab_data(&container_id, current_data,
				                    DMAP_CTYPE_INT);
				g_printf("Item's container ID: %d\n", container_id);
				break;
			case CC_TO_INT('m','u','d','l'):
				offset += grab_data(&deleted_id, current_data,
				                    DMAP_CTYPE_INT);
				g_printf("Deleted ID listing: %d\n", deleted_id);
				break;

			/* db list specific */
			case CC_TO_INT('m','i','m','c'):
				offset += grab_data(&db_n_items, current_data,
				                    DMAP_CTYPE_INT);
				g_printf("Number of db items: %d\n", db_n_items);
				break;
			case CC_TO_INT('m','c','t','c'):
				offset += grab_data(&db_n_playlist, current_data,
				                    DMAP_CTYPE_INT);
				g_printf("Number of playlists: %d\n", db_n_playlist);
				break;
			case CC_TO_INT('a','e','S','P'):
				offset += grab_data(&is_smart_pl, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Is a smart playlist: %d\n", is_smart_pl);
				break;
			case CC_TO_INT('a','b','p','l'):
				offset += grab_data(&is_base_pl, current_data,
				                    DMAP_CTYPE_BYTE);
				g_printf("Is the base playlist: %d\n", is_base_pl);
				break;

			/* exit conditions */
			case CC_TO_INT('m','l','i','t'):
				do_break = TRUE;
				break;
			default:
				g_printf("Warning: Unrecognized content code "
				         "or end of data: %s\n", current_data);
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}
	
	return (gint) (current_data - data);
}

gint cc_handler_mtco(gchar *current_data)
{
	gint32 n_rec_matches;
	gint offset = grab_data(&n_rec_matches, current_data, DMAP_CTYPE_INT);
	g_printf("Number of matching records: %d\n", n_rec_matches);
	return offset;
}

gint cc_handler_mrco(gchar *current_data)
{
	gint32 n_ret_items;
	gint offset = grab_data(&n_ret_items, current_data, DMAP_CTYPE_INT);
	g_printf("Number of returned items: %d\n", n_ret_items);
	return offset;
}

gint cc_handler_muty(gchar *current_data)
{
	gint8 updt_type;
	gint offset = grab_data(&updt_type, current_data, DMAP_CTYPE_BYTE);
	g_printf("Update type: %d\n", updt_type);
	return offset;
}

gint cc_handler_mstt(gchar *current_data)
{
	gint32 status;
	gint offset = grab_data(&status, current_data, DMAP_CTYPE_INT);
	g_printf("Server status: %d\n", status);
	return offset;
}

gint grab_data(void *container, gchar *data, content_type ct)
{
	gint offset;
	gint data_size;

	offset = DMAP_CC_SZ;
	memcpy(&data_size, data + offset, DMAP_INT_SZ);
	endian_swap_int32((gint32 *)&data_size);
	offset += DMAP_INT_SZ;
	
	switch (ct) {
		case DMAP_CTYPE_BYTE:
		case DMAP_CTYPE_UNSIGNEDBYTE:
			memcpy(container, data + offset, DMAP_BYTE_SZ);
			offset += DMAP_BYTE_SZ;
			break;
		case DMAP_CTYPE_SHORT:
		case DMAP_CTYPE_UNSIGNEDSHORT:
			memcpy(container, data + offset, DMAP_SHORT_SZ);
			endian_swap_int16(container);
			offset += DMAP_SHORT_SZ;
			break;
		case DMAP_CTYPE_INT:
		case DMAP_CTYPE_UNSIGNEDINT:
			memcpy(container, data + offset, DMAP_INT_SZ);
			endian_swap_int32(container);
			offset += DMAP_INT_SZ;
			break;
		case DMAP_CTYPE_LONG:
		case DMAP_CTYPE_UNSIGNEDLONG:
			memcpy(container, data + offset, DMAP_LONG_SZ);
			endian_swap_int64(container);
			offset += DMAP_LONG_SZ;
			break;
		case DMAP_CTYPE_STRING:
			offset += grab_data_string((gchar **) container, data, data_size);
			break;
		case DMAP_CTYPE_DATE:
			memcpy(container, data + offset, DMAP_INT_SZ);
			endian_swap_int32(container);
			offset += DMAP_INT_SZ;
			break;
		/*
		case DMAP_CTYPE_VERSION:
			memcpy(container, data + offset, DMAP_VERSION_SZ);
			endian_swap_int16(container);
			offset += DMAP_VERSION_SZ;
			break;
		case DMAP_CTYPE_LIST:
			break;
		*/
		default:
			g_printf("Warning: Unrecognized content type (%d).\n", ct);
			break;
	}

	return offset;
}

gint grab_data_string(gchar **container, gchar *data, gint str_len)
{
	gint offset = 0;
	
	if (0 != str_len) {
		*container = (gchar *) malloc(sizeof(gchar) * (str_len+1));
	
		memcpy(*container, data + DMAP_CC_SZ + DMAP_INT_SZ, str_len);
		(*container)[str_len] = '\0';
	
		offset += str_len;
	}

	return offset;
}

gint grab_data_version(gint16 *cont_upper, gint16 *cont_lower, gchar *data)
{
	gint offset = DMAP_CC_SZ;
	
	memcpy(cont_lower, data + offset, DMAP_INT_SZ);
	endian_swap_int16(cont_lower);
	offset += DMAP_INT_SZ;

	memcpy(cont_upper, data + offset, DMAP_INT_SZ);
	endian_swap_int16(cont_upper);
	offset += DMAP_INT_SZ;

	return offset;
}
/* vim:noexpandtab:shiftwidth=4:set tabstop=4: */
