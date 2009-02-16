#ifndef __XMMS_PRIV_UTILS_H__
#define __XMMS_PRIV_UTILS_H__

#define XMMS_BUILD_PATH(...) xmms_build_path (__VA_ARGS__, NULL)

char *xmms_build_path (const char *name, ...);
gint xmms_natcmp_len (const gchar *str1, gint len1, const gchar *str2, gint len2);
gint xmms_natcmp (const gchar *str1, const gchar *str2);

#endif /* __XMMS_PRIV_UTILS_H__ */
