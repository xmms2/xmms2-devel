#ifndef __XMMS_DBUS_H__
#define __XMMS_DBUS_H__


gboolean xmms_dbus_init (const gchar *path);
void xmms_dbus_register_object (const gchar *objectpath, xmms_object_t *object);

#endif
