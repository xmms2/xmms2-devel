#ifndef __XMMS_SQLITE_H__
#define __XMMS_SQLITE_H__


gchar *xmms_sqlite_korv_encode (const guint8 *korv_data, guint korv_length);
gchar *xmms_sqlite_korv_encode_string (const gchar *string);
guint8 * xmms_sqlite_korv_decode (const gchar *korv_data);


#endif 
