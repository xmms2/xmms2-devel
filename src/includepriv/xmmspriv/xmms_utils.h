#ifndef __XMMS_PRIV_UTILS_H__
#define __XMMS_PRIV_UTILS_H__

#include "xmmsc/xmmsc_compiler.h"

#define XMMS_BUILD_PATH(...) xmms_build_path (__VA_ARGS__, NULL)

char *xmms_build_path (const char *name, ...) XMMS_SENTINEL(0);
gint xmms_natcmp_len (const gchar *str1, gint len1, const gchar *str2, gint len2);
gint xmms_natcmp (const gchar *str1, const gchar *str2);
gboolean xmms_strcase_equal (gconstpointer v1, gconstpointer v2);
guint xmms_strcase_hash (gconstpointer v);

#endif /* __XMMS_PRIV_UTILS_H__ */
