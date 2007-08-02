#ifndef __XMMS_PRIV_UTILS_H__
#define __XMMS_PRIV_UTILS_H__

#define XMMS_BUILD_PATH(...) xmms_build_path (__VA_ARGS__, NULL)

char *xmms_build_path (const char *name, ...);

#endif /* __XMMS_PRIV_UTILS_H__ */
