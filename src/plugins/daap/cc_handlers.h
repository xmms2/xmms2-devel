#ifndef CC_HANDLERS_H
#define CC_HANDLERS_H

#include <glib.h>

#define CC_TO_INT(a,b,c,d) ((gint) ((a << 24) | \
                                    (b << 16) | \
                                    (c <<  8) | \
                                    (d      )    ))

#define DMAP_CC_SZ              (sizeof(gchar) * 4)
#define DMAP_BYTE_SZ            sizeof(gint8)
#define DMAP_SHORT_SZ           sizeof(gint16)
#define DMAP_INT_SZ             sizeof(gint32)
#define DMAP_LONG_SZ            sizeof(gint64)
#define DMAP_VERSION_SZ         sizeof(gint16)

#define DMAP_UNKNOWN_CC -1

typedef enum {
	DMAP_CTYPE_BYTE          = 1,
	/* unconfirmed */
	DMAP_CTYPE_UNSIGNEDBYTE  = 2,

	DMAP_CTYPE_SHORT         = 3,
	/* unconfirmed */
	DMAP_CTYPE_UNSIGNEDSHORT = 4,

	DMAP_CTYPE_INT           = 5,
	/* unconfirmed */
	DMAP_CTYPE_UNSIGNEDINT   = 6,

	DMAP_CTYPE_LONG          = 7,
	/* unconfirmed */
	DMAP_CTYPE_UNSIGNEDLONG  = 8,
	
	DMAP_CTYPE_STRING        = 9,
	DMAP_CTYPE_DATE          = 10,
	DMAP_CTYPE_VERSION       = 11,
	DMAP_CTYPE_LIST          = 12,
} content_type;

typedef struct {
	/* common to most response types */
	gint8 updt_type;

	gint32 n_rec_matches;
	gint32 n_ret_items;
	gint32 status;

	/* for results containing mlcl */
	GSList *record_list;

	/* msrv - server info */
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

	gchar *server_name;

	/* mccr - content codes */
	/* none */

	/* mlog - login */
	guint32 session_id;

	/* mupd - update */
	guint32 revision_id;

	/* avdb - db list */
	/* none */

	/* apso - items in playlist */
	/* none */

	/* aply - playlist list */
	/* none */

} cc_data_t;

/* mlit -- used in a item listing */
typedef struct {
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

	guint64 persistent_id;
	
	gchar *iname;
	gchar *song_data_url;
	gchar *song_data_album;
	gchar *song_data_artist;
	gchar *song_comment;
	gchar *song_description;
	gchar *song_genre;
	gchar *song_format;
	gchar *song_composer;
	gchar *song_grouping;
	
	/* db list specific */
	gint32 db_n_items;
	gint32 db_n_playlist;

} cc_list_item_t;

cc_data_t * cc_data_new();
void cc_data_free(cc_data_t *fields);
cc_data_t * cc_handler(gchar *data, gint data_len);

/* return type is the appropriate value */
/*
static cc_data_t * cc_handler_adbs(gchar *data, gint data_len);
static cc_data_t * cc_handler_msrv(gchar *data, gint data_len);
static cc_data_t * cc_handler_mccr(gchar *data, gint data_len);
static cc_data_t * cc_handler_mlog(gchar *data, gint data_len);
static cc_data_t * cc_handler_mupd(gchar *data, gint data_len);
static cc_data_t * cc_handler_avdb(gchar *data, gint data_len);
static cc_data_t * cc_handler_apso(gchar *data, gint data_len);
static cc_data_t * cc_handler_aply(gchar *data, gint data_len);
*/

/* return type must be int for offset calculation */
/*
static gint cc_handler_mtco(cc_data_t *fields, gchar *current_data);
static gint cc_handler_mrco(cc_data_t *fields, gchar *current_data);
static gint cc_handler_muty(cc_data_t *fields, gchar *current_data);
static gint cc_handler_mstt(cc_data_t *fields, gchar *current_data);
static gint cc_handler_mlcl(cc_data_t *fields, gchar *data, gint data_len);
static gint cc_handler_mlit(cc_data_t *fields, gchar *data, gint data_len);

static int grab_data(void *container, gchar *data, content_type ct);
static gint grab_data_string(gchar **container, gchar *data, gint str_len);
static gint grab_data_version(gint16 *cont_upper, gint16 *cont_lower, gchar *data);
*/

#endif
