#ifndef __XMMS_UTIL_H__

#include <stdio.h>

#include "xmms/log.h"

#define XMMS_PATH_MAX 255

#define XMMS_STRINGIFY_NOEXPAND(x) #x
#define XMMS_STRINGIFY(x) XMMS_STRINGIFY_NOEXPAND(x)

#define DEBUG

#ifdef DEBUG
#define XMMS_DBG(fmt, args...) xmms_log_debug (__FILE__ ": " fmt, ## args)
#else
#define XMMS_DBG(fmt,...)
#endif

gchar *xmms_util_decode_path (const gchar *path);

#endif
