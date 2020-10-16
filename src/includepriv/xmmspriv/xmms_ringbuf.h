/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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




#ifndef __XMMS_RINGBUF_H__
#define __XMMS_RINGBUF_H__

#include <glib.h>

typedef struct xmms_ringbuf_St xmms_ringbuf_t;

xmms_ringbuf_t *xmms_ringbuf_new (guint size);
void xmms_ringbuf_destroy (xmms_ringbuf_t *ringbuf);
void xmms_ringbuf_clear (xmms_ringbuf_t *ringbuf);
guint xmms_ringbuf_bytes_free (const xmms_ringbuf_t *ringbuf);
guint xmms_ringbuf_bytes_used (const xmms_ringbuf_t *ringbuf);
guint xmms_ringbuf_size (xmms_ringbuf_t *ringbuf);

guint xmms_ringbuf_read (xmms_ringbuf_t *ringbuf, gpointer data, guint length);
guint xmms_ringbuf_read_wait (xmms_ringbuf_t *ringbuf, gpointer data, guint length, GMutex *mtx);
guint xmms_ringbuf_peek (xmms_ringbuf_t *ringbuf, gpointer data, guint length);
guint xmms_ringbuf_peek_wait (xmms_ringbuf_t *ringbuf, gpointer data, guint length, GMutex *mtx);
void xmms_ringbuf_hotspot_set (xmms_ringbuf_t *ringbuf, gboolean (*cb) (void *), void (*destroy) (void *), void *arg);
guint xmms_ringbuf_write (xmms_ringbuf_t *ringbuf, gconstpointer data, guint length);
guint xmms_ringbuf_write_wait (xmms_ringbuf_t *ringbuf, gconstpointer data, guint length, GMutex *mtx);

void xmms_ringbuf_wait_free (xmms_ringbuf_t *ringbuf, guint len, GMutex *mtx);
void xmms_ringbuf_wait_used (xmms_ringbuf_t *ringbuf, guint len, GMutex *mtx);

gboolean xmms_ringbuf_iseos (const xmms_ringbuf_t *ringbuf);
void xmms_ringbuf_set_eos (xmms_ringbuf_t *ringbuf, gboolean eos);
void xmms_ringbuf_wait_eos (xmms_ringbuf_t *ringbuf, GMutex *mtx);

#endif /* __XMMS_RINGBUF_H__ */
