
/* XXX: this code assumes the target architecture is little-endian,
 * as evidenced by the endian_swap_foo functions. daap sends its numerical
 * data (int/short/long/etc.) in big-endian format. */
/* TODO: fix the problem w/o the need for a patch */

#define _GNU_SOURCE
#include <string.h>

#include <stdlib.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "cc_handlers.h"
#include "daap_conn.h"

#define DMAP_BYTES_REMAINING ((gint) (data_end - current_data))

static void endian_swap_int16(gint16 *i)
{
	gint16 tmp;
	tmp = (gint16) (((*i >>  8) & 0x00FF) |
	                ((*i <<  8) & 0xFF00));
	*i = tmp;
}

static void endian_swap_int32(gint32 *i)
{
	gint32 tmp;
	tmp = (gint32) (((*i >> 24) & 0x000000FF) |
	                ((*i >>  8) & 0x0000FF00) |
	                ((*i <<  8) & 0x00FF0000) |
	                ((*i << 24) & 0xFF000000));
	*i = tmp;
}

static void endian_swap_int64(gint64 *i)
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

static gint grab_data_string(gchar **container, gchar *data, gint str_len)
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

static gint grab_data_version(gint16 *cont_upper, gint16 *cont_lower, gchar *data)
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

static gint grab_data(void *container, gchar *data, content_type ct)
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

static gint cc_handler_mtco(cc_data_t *fields, gchar *current_data)
{
	gint offset = grab_data(&(fields->n_rec_matches), current_data, DMAP_CTYPE_INT);
	//g_printf("Number of matching records: %d\n", n_rec_matches);
	return offset;
}

static gint cc_handler_mrco(cc_data_t *fields, gchar *current_data)
{
	gint offset = grab_data(&(fields->n_ret_items), current_data, DMAP_CTYPE_INT);
	//g_printf("Number of returned items: %d\n", n_ret_items);
	return offset;
}

static gint cc_handler_muty(cc_data_t *fields, gchar *current_data)
{
	gint offset = grab_data(&(fields->updt_type), current_data, DMAP_CTYPE_BYTE);
	//g_printf("Update type: %d\n", updt_type);
	return offset;
}

static gint cc_handler_mstt(cc_data_t *fields, gchar *current_data)
{
	gint offset = grab_data(&(fields->status), current_data, DMAP_CTYPE_INT);
	//g_printf("Server status: %d\n", status);
	return offset;
}

static gint cc_handler_mlit(cc_data_t *fields, gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	cc_list_item_t *item_fields;
	item_fields = g_malloc0(sizeof(cc_list_item_t));

	while (current_data < data_end && !do_break) {
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','i','i','d'):
				offset += grab_data(&(item_fields->dbid), current_data,
				                    DMAP_CTYPE_INT);
				//g_printf("Database ID: %d\n", dbid);
				break;
			case CC_TO_INT('m','p','e','r'):
				offset += grab_data(&(item_fields->persistent_id), current_data,
				                    DMAP_CTYPE_LONG);
				//g_printf("Persistent ID: %lld\n", persistent_id);
				break;
			case CC_TO_INT('m','i','n','m'):
				offset += grab_data(&(item_fields->iname), current_data,
				                    DMAP_CTYPE_STRING);
				//g_printf("Name: %s\n", iname);
				//g_free(iname);
				break;

			/* song list specific */
			case CC_TO_INT('m','i','k','d'):
				offset += grab_data(&(item_fields->item_kind), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Song/item kind: %d\n", item_kind);
				break;
			case CC_TO_INT('a','s','d','k'):
				offset += grab_data(&(item_fields->song_data_kind), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Song data kind: %d\n", song_data_kind);
				break;	
			case CC_TO_INT('a','s','u','l'):
				offset += grab_data(&(item_fields->song_data_url), current_data,
				                    DMAP_CTYPE_STRING);
				//g_printf("Song data URL: %s\n", song_data_url);
				//g_free(song_data_url);
				break;
			case CC_TO_INT('a','s','a','l'):
				offset += grab_data(&(item_fields->song_data_album),
				                    current_data,
				                    DMAP_CTYPE_STRING);
				//g_printf("Song data album: %s\n", song_data_album);
				//g_free(song_data_album);
				break;
			case CC_TO_INT('a','s','a','r'):
				offset += grab_data(&(item_fields->song_data_artist),
				                    current_data,
				                    DMAP_CTYPE_STRING);
				//g_printf("Song data artist: %s\n", song_data_artist);
				//g_free(song_data_artist);
				break;
			case CC_TO_INT('a','s','b','r'):
				offset += grab_data(&(item_fields->song_bitrate), current_data,
				                    DMAP_CTYPE_SHORT);
				//g_printf("Song bitrate: %d\n", song_bitrate);
				break;
			case CC_TO_INT('a','s','c','m'):
				offset += grab_data(&(item_fields->song_comment), current_data,
				                    DMAP_CTYPE_STRING);
				//g_printf("Song comment: %s\n", song_comment);
				//g_free(song_comment);
				break;
			case CC_TO_INT('a','s','d','a'):
				offset += grab_data(&(item_fields->song_date), current_data,
				                    DMAP_CTYPE_DATE);
				//g_printf("Song date added: %d\n", song_date);
				break;
			case CC_TO_INT('a','s','d','m'):
				offset += grab_data(&(item_fields->song_date_mod), current_data,
				                    DMAP_CTYPE_DATE);
				//g_printf("Song date modified: %d\n", song_date_mod);
				break;
			case CC_TO_INT('a','s','g','n'):
				offset += grab_data(&(item_fields->song_genre), current_data,
				                    DMAP_CTYPE_STRING);
				//g_printf("Song genre: %s\n", song_genre);
				//g_free(song_genre);
				break;
			case CC_TO_INT('a','s','f','m'):
				offset += grab_data(&(item_fields->song_format), current_data,
				                    DMAP_CTYPE_STRING);
				//g_printf("Song format: %s\n", song_format);
				//g_free(song_format);
				break;
			case CC_TO_INT('a','s','d','t'):
				offset += grab_data(&(item_fields->song_description), current_data,
				                    DMAP_CTYPE_STRING);
				//g_printf("Song description: %s\n", song_description);
				//g_free(song_description);
				break;
			case CC_TO_INT('a','s','s','r'):
				offset += grab_data(&(item_fields->sample_rate), current_data, DMAP_CTYPE_INT);
				//g_printf("Song sample rate: %d\n", sample_rate);
				break;
			case CC_TO_INT('a','s','s','z'):
				offset += grab_data(&(item_fields->song_size), current_data, DMAP_CTYPE_INT);
				//g_printf("Song size: %d\n", song_size);
				break;
			case CC_TO_INT('a','s','s','t'):
				offset += grab_data(&(item_fields->song_start_time), current_data,
				                    DMAP_CTYPE_INT);
				//g_printf("Song start time (ms): %d\n", song_start_time);
				break;
			case CC_TO_INT('a','s','s','p'):
				offset += grab_data(&(item_fields->song_stop_time), current_data,
				                    DMAP_CTYPE_INT);
				//g_printf("Song stop time (ms): %d\n", song_stop_time);
				break;
			case CC_TO_INT('a','s','t','m'):
				offset += grab_data(&(item_fields->song_total_time), current_data,
				                    DMAP_CTYPE_INT);
				//g_printf("Song total time (ms): %d\n", song_total_time);
				break;
			case CC_TO_INT('a','s','y','r'):
				offset += grab_data(&(item_fields->song_year), current_data, DMAP_CTYPE_SHORT);
				//g_printf("Song year: %d\n", song_year);
				break;
			case CC_TO_INT('a','s','t','n'):
				offset += grab_data(&(item_fields->song_track_no), current_data,
				                    DMAP_CTYPE_SHORT);
				//g_printf("Song track number: %d\n", song_track_no);
				break;
			case CC_TO_INT('a','s','c','p'):
				offset += grab_data(&(item_fields->song_composer), current_data,
				                    DMAP_CTYPE_STRING);
				//g_printf("Song composer: %s\n", song_composer);
				//g_free(song_composer);
				break;
			case CC_TO_INT('a','s','t','c'):
				offset += grab_data(&(item_fields->song_track_count), current_data,
				                    DMAP_CTYPE_SHORT);
				//g_printf("Song track count: %d\n", song_track_count);
				break;
			case CC_TO_INT('a','s','d','c'):
				offset += grab_data(&(item_fields->song_disc_count), current_data,
				                    DMAP_CTYPE_SHORT);
				//g_printf("Song disc count: %d\n", song_disc_count);
				break;
			case CC_TO_INT('a','s','d','n'):
				offset += grab_data(&(item_fields->song_disc_no), current_data,
				                    DMAP_CTYPE_SHORT);
				//g_printf("Song disc number: %d\n", song_disc_no);
				break;
			case CC_TO_INT('a','s','c','o'):
				offset += grab_data(&(item_fields->song_compilation), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Song compilation: %d\n", song_compilation);
				break;
			case CC_TO_INT('a','s','b','t'):
				offset += grab_data(&(item_fields->song_bpm), current_data,
				                    DMAP_CTYPE_SHORT);
				//g_printf("Song BPM: %d\n", song_bpm);
				break;
			case CC_TO_INT('a','g','r','p'):
				offset += grab_data(&(item_fields->song_grouping), current_data,
				                    DMAP_CTYPE_STRING);
				//g_printf("Song grouping: %s\n", song_grouping);
				//g_free(song_grouping);
				break;
			case CC_TO_INT('m','c','t','i'):
				offset += grab_data(&(item_fields->container_id), current_data,
				                    DMAP_CTYPE_INT);
				//g_printf("Item's container ID: %d\n", container_id);
				break;
			case CC_TO_INT('m','u','d','l'):
				offset += grab_data(&(item_fields->deleted_id), current_data,
				                    DMAP_CTYPE_INT);
				//g_printf("Deleted ID listing: %d\n", deleted_id);
				break;

			/* db list specific */
			case CC_TO_INT('m','i','m','c'):
				offset += grab_data(&(item_fields->db_n_items), current_data,
				                    DMAP_CTYPE_INT);
				//g_printf("Number of db items: %d\n", db_n_items);
				break;
			case CC_TO_INT('m','c','t','c'):
				offset += grab_data(&(item_fields->db_n_playlist), current_data,
				                    DMAP_CTYPE_INT);
				//g_printf("Number of playlists: %d\n", db_n_playlist);
				break;
			case CC_TO_INT('a','e','S','P'):
				offset += grab_data(&(item_fields->is_smart_pl), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Is a smart playlist: %d\n", is_smart_pl);
				break;
			case CC_TO_INT('a','b','p','l'):
				offset += grab_data(&(item_fields->is_base_pl), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Is the base playlist: %d\n", is_base_pl);
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

	fields->record_list = g_slist_prepend(fields->record_list, item_fields);
	
	return (gint) (current_data - data);
}

static gint cc_handler_mlcl(cc_data_t *fields, gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	while (current_data < data_end && !do_break) {
		if (CC_TO_INT(current_data[0], current_data[1],
		              current_data[2], current_data[3]) ==
		    CC_TO_INT('m','l','i','t')) {

			offset += cc_handler_mlit(fields, current_data,
			                          DMAP_BYTES_REMAINING);
		} else {
			break;
		}

		current_data += offset;
		offset = 0;
	}

	return (gint)(current_data - data);
}

static cc_data_t * cc_handler_adbs(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	cc_data_t *fields = cc_data_new();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(fields, current_data);
				break;
			case CC_TO_INT('m','u','t','y'):
				offset += cc_handler_muty(fields, current_data);
				break;
			case CC_TO_INT('m','t','c','o'):
				offset += cc_handler_mtco(fields, current_data);
				break;	
			case CC_TO_INT('m','r','c','o'):
				offset += cc_handler_mrco(fields, current_data);
				break;	
			case CC_TO_INT('m','l','c','l'):
				offset += cc_handler_mlcl(fields, current_data,
				                          DMAP_BYTES_REMAINING);
				break;
			default:
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}
	
	return fields;
}

static cc_data_t * cc_handler_msrv(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	cc_data_t *fields = cc_data_new();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(fields, current_data);
				break;
			case CC_TO_INT('a','p','r','o'):
				offset += grab_data_version(&(fields->daap_proto_major),
				                            &(fields->daap_proto_minor),
				                            current_data);
				/*g_printf("daap protocol version: %d.%d\n", daap_proto_major,
				                                           daap_proto_minor);
				*/
				break;
			case CC_TO_INT('m','p','r','o'):
				offset += grab_data_version(&(fields->dmap_proto_major),
				                            &(fields->dmap_proto_minor),
				                            current_data);
				/*g_printf("dmap protocol version: %d.%d\n", dmap_proto_major,
				                                           dmap_proto_minor);
				*/
				break;
			case CC_TO_INT('m','s','t','m'):
				offset += grab_data(&(fields->timeout_interval), current_data,
				                    DMAP_CTYPE_INT);
				//g_printf("Server timeout interval: %d\n", timeout_interval);
				break;	
			case CC_TO_INT('m','s','i','x'):
				offset += grab_data(&(fields->has_indexing), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Server supports indexing: %d\n", has_indexing);
				break;	
			case CC_TO_INT('m','s','e','x'):
				offset += grab_data(&(fields->has_extensions), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Server supports extensions: %d\n", has_extensions);
				break;	
			case CC_TO_INT('m','s','u','p'):
				offset += grab_data(&(fields->has_update), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Server supports updates: %d\n", has_update);
				break;	
			case CC_TO_INT('m','s','a','l'):
				offset += grab_data(&(fields->has_autologout), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Server supports autologout: %d\n", has_autologout);
				break;	
			case CC_TO_INT('m','s','l','r'):
				offset += grab_data(&(fields->login_required), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Login required: %d\n", login_required);
				break;	
			case CC_TO_INT('m','s','q','y'):
				offset += grab_data(&(fields->has_queries), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Server supports queries: %d\n", has_queries);
				break;	
			case CC_TO_INT('m','s','r','s'):
				offset += grab_data(&(fields->has_resolve), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Server supports resolve: %d\n", has_resolve);
				break;	
			case CC_TO_INT('m','s','b','r'):
				offset += grab_data(&(fields->has_browsing), current_data,
				                    DMAP_CTYPE_BYTE);
				//g_printf("Server supports browsing: %d\n", has_browsing);
				break;	
			case CC_TO_INT('m','s','p','i'):
				offset += grab_data(&(fields->has_persistent), current_data,
				                    DMAP_CTYPE_BYTE);
				/*g_printf("Server supports persistent IDs: %d\n",
				         has_persistent);*/
				break;	
			case CC_TO_INT('m','s','a','s'):
				offset += grab_data(&(fields->auth_type), current_data, DMAP_CTYPE_BYTE);
				/*g_printf("Server authentication type: %d\n",
				         auth_type);*/
				break;	
			case CC_TO_INT('m','s','a','u'):
				offset += grab_data(&(fields->auth_method), current_data,
				                    DMAP_CTYPE_BYTE);
				/*g_printf("Server authentication method: %d\n",
				         auth_method);*/
				break;	
			case CC_TO_INT('a','e','S','V'):
				offset += grab_data(&(fields->sharing_version), current_data,
				                    DMAP_CTYPE_INT);
				/*g_printf("Server music sharing version: %d\n",
				         sharing_version);*/
				break;	
			case CC_TO_INT('m','s','d','c'):
				offset += grab_data(&(fields->db_count), current_data, DMAP_CTYPE_INT);
				//g_printf("Number of databases: %d\n", db_count);
				break;	
			case CC_TO_INT('m','i','n','m'):
				offset += grab_data(&(fields->server_name), current_data,
				                    DMAP_CTYPE_STRING);
				//g_printf("Server name: %s\n", server_name);
				//g_free(server_name);
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

	return fields;
}

static cc_data_t * cc_handler_mccr(gchar *data, gint data_len)
{
	/* XXX not implemented */
	return NULL;
}

static cc_data_t * cc_handler_mlog(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	cc_data_t *fields = cc_data_new();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(fields, current_data);
				break;
			case CC_TO_INT('m','l','i','d'):
				offset += grab_data(&(fields->session_id), current_data, DMAP_CTYPE_INT);
				//g_printf("session id: %d\n", session_id);
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

	return fields;
}

static cc_data_t * cc_handler_mupd(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	cc_data_t *fields = cc_data_new();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','u','s','r'):
				offset += grab_data(&(fields->revision_id), current_data,
				                    DMAP_CTYPE_INT);
				//g_printf("Revision ID: %d\n", revision_id);
				break;
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(fields, current_data);
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

	return fields;
}

static cc_data_t * cc_handler_avdb(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	cc_data_t *fields = cc_data_new();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(fields, current_data);
				break;
			case CC_TO_INT('m','u','t','y'):
				offset += cc_handler_muty(fields, current_data);
				break;
			case CC_TO_INT('m','t','c','o'):
				offset += cc_handler_mtco(fields, current_data);
				break;	
			case CC_TO_INT('m','r','c','o'):
				offset += cc_handler_mrco(fields, current_data);
				break;	
			case CC_TO_INT('m','l','c','l'):
				offset += cc_handler_mlcl(fields, current_data,
				                          DMAP_BYTES_REMAINING);
				break;
			default:
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}
	
	return fields;
}

static cc_data_t * cc_handler_apso(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	cc_data_t *fields = cc_data_new();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(fields, current_data);
				break;
			case CC_TO_INT('m','u','t','y'):
				offset += cc_handler_muty(fields, current_data);
				break;
			case CC_TO_INT('m','t','c','o'):
				offset += cc_handler_mtco(fields, current_data);
				break;	
			case CC_TO_INT('m','r','c','o'):
				offset += cc_handler_mrco(fields, current_data);
				break;	
			case CC_TO_INT('m','l','c','l'):
				offset += cc_handler_mlcl(fields, current_data, DMAP_BYTES_REMAINING);
				break;
			default:
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}
	
	return fields;
}

static cc_data_t * cc_handler_aply(gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	cc_data_t *fields = cc_data_new();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT(current_data[0], current_data[1],
		                  current_data[2], current_data[3])) {
			case CC_TO_INT('m','s','t','t'):
				offset += cc_handler_mstt(fields, current_data);
				break;
			case CC_TO_INT('m','u','t','y'):
				offset += cc_handler_muty(fields, current_data);
				break;
			case CC_TO_INT('m','t','c','o'):
				offset += cc_handler_mtco(fields, current_data);
				break;	
			case CC_TO_INT('m','r','c','o'):
				offset += cc_handler_mrco(fields, current_data);
				break;	
			case CC_TO_INT('m','l','c','l'):
				offset += cc_handler_mlcl(fields, current_data,
				                          DMAP_BYTES_REMAINING);
				break;
			default:
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}
	
	return fields;
}

cc_data_t * cc_data_new()
{
	cc_data_t *retval;

	retval = g_malloc0(sizeof(cc_data_t));
	retval->record_list = NULL;

	return retval;
}

void cc_data_free(cc_data_t *fields)
{
	/* FIXME */
}

cc_data_t * cc_handler(gchar *data, gint data_len)
{
	cc_data_t *retval;

	switch (CC_TO_INT(data[0],data[1],data[2],data[3])) {
		case CC_TO_INT('a','d','b','s'):
			retval = cc_handler_adbs(data, data_len);
			break;
		case CC_TO_INT('m','s','r','v'):
			retval = cc_handler_msrv(data, data_len);
			break;
		case CC_TO_INT('m','c','c','r'):
			retval = cc_handler_mccr(data, data_len);
			break;
		case CC_TO_INT('m','l','o','g'):
			retval = cc_handler_mlog(data, data_len);
			break;
		case CC_TO_INT('m','u','p','d'):
			retval = cc_handler_mupd(data, data_len);
			break;
		case CC_TO_INT('a','v','d','b'):
			retval = cc_handler_avdb(data, data_len);
			break;
		case CC_TO_INT('a','p','s','o'):
			retval = cc_handler_apso(data, data_len);
			break;
		case CC_TO_INT('a','p','l','y'):
			retval = cc_handler_aply(data, data_len);
			break;
		default:
			//offset = DMAP_UNKNOWN_CC;
			break;
	}

	return retval;
}


