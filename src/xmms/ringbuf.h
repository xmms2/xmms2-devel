#ifndef __XMMS_RINGBUF_H__
#define __XMMS_RINGBUF_H__

#include <glib.h>

typedef struct
{
	guint8 *buffer;
	gint buffer_size;
	gint rd_index, wr_index;
	gboolean eos;

	GCond *free_cond, *used_cond, *eos_cond;
} xmms_ringbuf_t;

xmms_ringbuf_t *xmms_ringbuf_new (guint size);
void xmms_ringbuf_destroy (xmms_ringbuf_t *ringbuf);
void xmms_ringbuf_clear (xmms_ringbuf_t *ringbuf);
guint xmms_ringbuf_free (const xmms_ringbuf_t *ringbuf);
guint xmms_ringbuf_used (const xmms_ringbuf_t *ringbuf);

guint xmms_ringbuf_read (xmms_ringbuf_t *ringbuf, gpointer data, guint length);
guint xmms_ringbuf_write (xmms_ringbuf_t *ringbuf, gconstpointer data, guint length);
guint xmms_ringbuf_unread (xmms_ringbuf_t *ringbuf, guint length);

void xmms_ringbuf_wait_free (const xmms_ringbuf_t *ringbuf, guint len, GMutex *mtx);
void xmms_ringbuf_wait_used (const xmms_ringbuf_t *ringbuf, guint len, GMutex *mtx);

gboolean xmms_ringbuf_eos (const xmms_ringbuf_t *ringbuf);
void xmms_ringbuf_set_eos (xmms_ringbuf_t *ringbuf, gboolean eos);
void xmms_ringbuf_wait_eos (const xmms_ringbuf_t *ringbuf, GMutex *mtx);

#endif /* __XMMS_RINGBUF_H__ */
