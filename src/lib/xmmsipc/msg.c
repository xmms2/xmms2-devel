#include <glib.h>
#include <string.h>
#include "xmms/ipc_msg.h"
#include "xmms/ringbuf.h"
#include "xmms/util.h"

xmms_ipc_msg_t *
xmms_ipc_msg_stringlist_new (guint32 cmd, GList *strlist)
{
	xmms_ipc_msg_t *msg;
	GList *n;

	msg = xmms_ipc_msg_new (cmd);
	
	for (n = strlist; n; n = g_list_next (n)) {
		xmms_ipc_msg_put_string (msg, (gchar *)n->data);
	}

	return msg;

}

xmms_ipc_msg_t *
xmms_ipc_msg_string_new (guint32 cmd, gchar *string)
{
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (cmd);
	xmms_ipc_msg_put_string (msg, string);
	return msg;
}

xmms_ipc_msg_t *
xmms_ipc_msg_uint_new (guint32 cmd, guint32 val)
{
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (cmd);
	xmms_ipc_msg_put_uint32 (msg, val);
	return msg;
}

xmms_ipc_msg_t *
xmms_ipc_msg_int_new (guint32 cmd, gint32 val)
{
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (cmd);
	xmms_ipc_msg_put_int32 (msg, val);
	return msg;
}

xmms_ipc_msg_t *
xmms_ipc_msg_uintlist_new (guint32 cmd, GList *uintlist)
{
	xmms_ipc_msg_t *msg;
	GList *n;

	msg = xmms_ipc_msg_new (cmd);
	for (n = uintlist; n; n = g_list_next (n)) {
		xmms_ipc_msg_put_uint32 (msg, GPOINTER_TO_UINT (n->data));
	}

	return msg;
}

xmms_ipc_msg_t *
xmms_ipc_msg_intlist_new (guint32 cmd, GList *intlist)
{
	xmms_ipc_msg_t *msg;
	GList *n;

	msg = xmms_ipc_msg_new (cmd);
	for (n = intlist; n; n = g_list_next (n)) {
		xmms_ipc_msg_put_int32 (msg, GPOINTER_TO_INT (n->data));
	}

	return msg;
}

/** Use when you add a hash to a msg from g_hash_table_foreach() */
void
xmms_ipc_msg_hash_callback (gpointer key, gpointer value, gpointer userdata)
{
	xmms_ipc_msg_t *msg = userdata;
	xmms_ipc_msg_put_string (msg, (gchar *)key);
	xmms_ipc_msg_put_string (msg, (gchar *)value);
}

xmms_ipc_msg_t *
xmms_ipc_msg_hash_new (guint32 cmd, GHashTable *hash)
{
	xmms_ipc_msg_t *msg;
	msg = xmms_ipc_msg_new (cmd);
	g_hash_table_foreach (hash, xmms_ipc_msg_hash_callback, msg);
	return msg;
}

xmms_ipc_msg_t *
xmms_ipc_msg_hashlist_new (guint32 cmd, GList *list)
{
	xmms_ipc_msg_t *msg;
	GList *n;

	msg = xmms_ipc_msg_new (cmd);
	for (n = list; n; n = g_list_next (n)) {
		g_hash_table_foreach ((GHashTable *)n->data, xmms_ipc_msg_hash_callback, msg);
	}

	return msg;
}

/****** LOWLEVEL *******/

xmms_ipc_msg_t *
xmms_ipc_msg_new (guint32 cmd)
{
	xmms_ipc_msg_t *msg;
	
	msg = g_new0 (xmms_ipc_msg_t, 1);
	msg->cmd = cmd;
	msg->get_pos = 0;
	msg->data_length = 0;

	return msg;
}

void
xmms_ipc_msg_destroy (xmms_ipc_msg_t *msg)
{
	g_return_if_fail (msg);
	g_free (msg);
}

xmms_ipc_msg_t *
xmms_ipc_msg_read (xmms_ringbuf_t *ringbuf)
{
	xmms_ipc_msg_t *msg;
	guint32 cmd;

	if (xmms_ringbuf_bytes_used (ringbuf) < 6)
		return NULL;

	xmms_ringbuf_read (ringbuf, &cmd, sizeof (guint32));
	cmd = g_ntohl (cmd);
	msg = xmms_ipc_msg_new (cmd);

	g_return_val_if_fail (msg, NULL);
	
	xmms_ringbuf_read (ringbuf, &msg->data_length, sizeof (guint16));
	msg->data_length = g_ntohs (msg->data_length);
	
	g_return_val_if_fail (xmms_ringbuf_bytes_used (ringbuf) > msg->data_length, NULL);

	xmms_ringbuf_read (ringbuf, msg->data, msg->data_length);
	return msg;
}

gboolean
xmms_ipc_msg_write (xmms_ringbuf_t *ringbuf, const xmms_ipc_msg_t *msg)
{
	guint32 cmd;
	guint16 len;

	g_return_val_if_fail (xmms_ringbuf_bytes_free (ringbuf) > (msg->data_length +6), FALSE);

	cmd = g_htonl (msg->cmd);
	len = g_htons (msg->data_length);
	xmms_ringbuf_write (ringbuf, &cmd, sizeof (cmd));
	xmms_ringbuf_write (ringbuf, &len, sizeof (len));
	xmms_ringbuf_write (ringbuf, msg->data, msg->data_length);

	return TRUE;
}

gboolean
xmms_ipc_msg_write_fd (gint fd, const xmms_ipc_msg_t *msg)
{
	guint32 cmd;
	guint16 len;
	
	cmd = g_htonl (msg->cmd);
	len = g_htons (msg->data_length);

	if (write (fd, &cmd, sizeof (cmd)) != sizeof (cmd))
		return FALSE;
	if (write (fd, &len, sizeof (len)) != sizeof (len))
		return FALSE;
	if (write (fd, msg->data, msg->data_length) != msg->data_length)
		return FALSE;
	
	return TRUE;
}

gpointer
xmms_ipc_msg_put_data (xmms_ipc_msg_t *msg, gconstpointer data, guint len)
{
	XMMS_DBG ("len = %d msg->data_length = %d", len, msg->data_length);

	g_return_val_if_fail (msg, NULL);
	g_return_val_if_fail ((msg->data_length + len) < XMMS_IPC_MSG_DATA_SIZE, NULL);
	
	memcpy (&msg->data[msg->data_length], data, len);
	msg->data_length += len;

	return &msg->data[msg->data_length - len];
}

gpointer
xmms_ipc_msg_put_uint32 (xmms_ipc_msg_t *msg, guint32 v)
{
	v = g_htonl (v);
	return xmms_ipc_msg_put_data (msg, &v, sizeof (v));
}

gpointer
xmms_ipc_msg_put_int32 (xmms_ipc_msg_t *msg, gint32 v)
{
	v = g_htonl (v);
	return xmms_ipc_msg_put_data (msg, &v, sizeof (v));
}

gpointer
xmms_ipc_msg_put_float (xmms_ipc_msg_t *msg, gfloat v)
{
	/** @todo do we need to convert ? */
	return xmms_ipc_msg_put_data (msg, &v, sizeof (v));
}

gpointer
xmms_ipc_msg_put_string (xmms_ipc_msg_t *msg, const char *str)
{
	if (!msg)
		return NULL;

	if (!str)
		return xmms_ipc_msg_put_uint32 (msg, 0);
	
	if ((msg->data_length + strlen (str) + 5) > XMMS_IPC_MSG_DATA_SIZE)
		return NULL;

	xmms_ipc_msg_put_uint32 (msg, strlen (str) + 1);

	return xmms_ipc_msg_put_data (msg, str, strlen (str) + 1);
}

void
xmms_ipc_msg_get_reset (xmms_ipc_msg_t *msg)
{
	msg->get_pos = 0;
}

gboolean
xmms_ipc_msg_get_data (xmms_ipc_msg_t *msg, gpointer buf, guint len)
{
	if (!msg || ((msg->get_pos + len) > msg->data_length))
		return FALSE;

	if (buf)
		memcpy (buf, &msg->data[msg->get_pos], len);
	msg->get_pos += len;

	return TRUE;
}

gboolean
xmms_ipc_msg_get_uint32 (xmms_ipc_msg_t *msg, guint32 *v)
{
	gboolean ret;
	ret = xmms_ipc_msg_get_data (msg, v, sizeof (*v));
	if (v)
		*v = g_ntohl (*v);
	return ret;
}

gboolean
xmms_ipc_msg_get_int32 (xmms_ipc_msg_t *msg, gint32 *v)
{
	gboolean ret;
	ret = xmms_ipc_msg_get_data (msg, v, sizeof (*v));
	if (v)
		*v = g_ntohl (*v);
	return ret;
}

gboolean
xmms_ipc_msg_get_float (xmms_ipc_msg_t *msg, gfloat *v)
{
	gboolean ret;
	ret = xmms_ipc_msg_get_data (msg, v, sizeof (*v));
	/** @todo do we need to convert? */
	return ret;
}


gboolean
xmms_ipc_msg_get_string (xmms_ipc_msg_t *msg, char *buf, guint maxlen)
{
	guint32 len;

	if (buf) {
		buf[maxlen - 1] = '\0';
		maxlen--;
	}
	if (!xmms_ipc_msg_get_uint32 (msg, &len))
		return FALSE;
	if (!xmms_ipc_msg_get_data (msg, buf, MIN (maxlen, len)))
		return FALSE;
	if (maxlen < len) {
		xmms_ipc_msg_get_data (msg, NULL, len - maxlen);
	}
	return TRUE;
}

gboolean
xmms_ipc_msg_get (xmms_ipc_msg_t *msg, ...)
{
	va_list ap;
	void *dest;
	xmms_ipc_msg_arg_type_t type;
	gint len;

	va_start (ap, msg);

	while (42) {
		type = va_arg (ap, xmms_ipc_msg_arg_type_t);
		switch (type){
			case XMMS_IPC_MSG_ARG_TYPE_UINT32:
				dest = va_arg (ap, guint32 *);
				if (!xmms_ipc_msg_get_uint32 (msg, dest)) {
					return FALSE;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_INT32:
				dest = va_arg (ap, gint32 *);
				if (!xmms_ipc_msg_get_int32 (msg, dest)) {
					return FALSE;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_FLOAT:
				dest = va_arg (ap, gfloat *);
				if (!xmms_ipc_msg_get_float (msg, dest)) {
					return FALSE;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_STRING:
				len = va_arg (ap, gint);
				dest = va_arg (ap, char *);
				if (!xmms_ipc_msg_get_string (msg, dest, len)) {
					return FALSE;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_DATA:
				len = va_arg (ap, gint);
				dest = va_arg (ap, void *);
				if (!xmms_ipc_msg_get_data (msg, dest, len)) {
					return FALSE;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_END:
				va_end (ap);
				return TRUE;
		}
		
	}
}
