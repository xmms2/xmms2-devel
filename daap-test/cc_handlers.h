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

#define DMAP_SIZE_LEN 4
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
	/* placeholder */
} db_list_t;

typedef struct {
	/* placeholder */
} song_list_t;

typedef struct {
	/* placeholder */
} pl_list_t;

typedef struct {
	/* placeholder */
} playlist_t;

void endian_swap_int32(gint32 *i);

/*
static gint is_dmap_cc(gchar *cc);
static gint is_daap_cc(gchar *cc);
static gint is_com_cc(gchar *cc);
gint is_valid_cc(gchar *cc);
*/

gint grab_data(void *container, gchar *data, content_type ct);
gint grab_data_string(gchar **container, gchar *data, gint str_len);
gint grab_data_version(gint16 *cont_upper, gint16 *cont_lower, gchar *data);

gint cc_handler(gchar *data, gint data_len);

gint cc_handler_mtco(gchar *current_data);
gint cc_handler_mrco(gchar *current_data);
gint cc_handler_muty(gchar *current_data);
gint cc_handler_mstt(gchar *current_data);

gint cc_handler_adbs(gchar *data, gint data_len);
gint cc_handler_msrv(gchar *data, gint data_len);
gint cc_handler_mccr(gchar *data, gint data_len);
gint cc_handler_mlog(gchar *data, gint data_len);
gint cc_handler_mupd(gchar *data, gint data_len);
gint cc_handler_avdb(gchar *data, gint data_len);
gint cc_handler_apso(gchar *data, gint data_len);
gint cc_handler_aply(gchar *data, gint data_len);
gint cc_handler_mlcl(gchar *data, gint data_len);
gint cc_handler_mlit(gchar *data, gint data_len);

#endif
/* vim:noexpandtab:shiftwidth=4:set tabstop=4: */
