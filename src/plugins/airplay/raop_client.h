/* RAOP Client Interface for AirTunes */

#ifndef _RAOP_CLIENT_H
#define _RAOP_CLIENT_H

#include <glib.h>

#define RAOP_DEFAULT_IP "10.0.1.1"
#define RAOP_DEFAULT_PORT 5000

#define RAOP_MIN_VOLUME -144
#define RAOP_MAX_VOLUME 0

#define RAOP_ALAC_FRAME_SIZE 4096
#define RAOP_ALAC_NUM_CHANNELS 2
#define RAOP_ALAC_BITS_PER_SAMPLE 16

/* Return/Error codes */
#define RAOP_EOK     0
#define RAOP_EFAIL  -1
#define RAOP_EPROTO -2
#define RAOP_EIO    -3
#define RAOP_ESYS   -4
#define RAOP_EINVAL -5
#define RAOP_ENOMEM -6

typedef struct raop_client_struct raop_client_t;

typedef int (*raop_client_stream_cb_func_t) (void *, guchar *, int);

typedef struct {
	raop_client_stream_cb_func_t func;
	void *data;	
} raop_client_stream_cb_t;


gint raop_client_init(raop_client_t **rc);
gint raop_client_connect(raop_client_t *rc, const gchar *host, gushort port);

gint raop_client_handle_io(raop_client_t *rc, int fd, GIOCondition cond);
gint raop_client_set_stream_cb(raop_client_t *rc, raop_client_stream_cb_func_t cb, void *data);
gint raop_client_flush(raop_client_t *rc);

gint raop_client_disconnect(raop_client_t *rc);
gint raop_client_destroy(raop_client_t *rc);

gboolean raop_client_can_read(raop_client_t *rc, gint fd);
gboolean raop_client_can_write(raop_client_t *rc, gint fd);

gint raop_client_rtsp_sock(raop_client_t *rc);
gint raop_client_stream_sock(raop_client_t *rc);

gint raop_client_set_volume(raop_client_t *rc, gdouble volume);
gint raop_client_get_volume(raop_client_t *rc, gdouble *volume);

#endif /* _RAOP_CLIENT_H */
