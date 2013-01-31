#ifndef __XMMS_UTILS_H__
#define __XMMS_UTILS_H__

#include "xmmsc/xmmsc_stdbool.h"

#define XMMS_STRINGIFY_NOEXPAND(x) #x
#define XMMS_STRINGIFY(x) XMMS_STRINGIFY_NOEXPAND(x)

#define XMMS_PATH_MAX 255

/* 9667 is XMMS written on a phone */
#define XMMS_DEFAULT_TCP_PORT 9667

const char *xmms_userconfdir_get (char *buf, int len);
const char *xmms_usercachedir_get (char *buf, int len);
const char *xmms_default_ipcpath_get (char *buf, int len);
const char *xmms_fallback_ipcpath_get (char *buf, int len);
bool xmms_sleep_ms (int n);

#endif /* __XMMS_UTILS_H__ */
