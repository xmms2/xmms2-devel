#ifndef __XMMS_UTILS_H__
#define __XMMS_UTILS_H__

#include <stdio.h>

#define XMMS_STRINGIFY_NOEXPAND(x) #x
#define XMMS_STRINGIFY(x) XMMS_STRINGIFY_NOEXPAND(x)

#define x_return_if_fail(expr) if (!(expr)) { fprintf (stderr, "Failed in file " __FILE__ " on  row %d\n", __LINE__); return; }
#define x_return_val_if_fail(expr, val) if (!(expr)) { fprintf (stderr, "Failed in file " __FILE__ " on  row %d\n", __LINE__); return val; }
#define x_return_null_if_fail(expr) x_return_val_if_fail (expr, NULL)
#define x_oom() do { fprintf(stderr, "Out of memory in " __FILE__ "on row %d\n", __LINE__); } while (0)
#define x_new0(type, num) calloc (1, sizeof (type) * (num))
#define x_new(type, num) malloc (sizeof (type) * (num))
#define x_malloc0(size) calloc (1, size)
#define x_malloc(size) malloc (size)

#define XPOINTER_TO_INT(p)      ((int)   (p))
#define XPOINTER_TO_UINT(p)     ((unsigned int)  (p))

#define XINT_TO_POINTER(i)      ((void *)  (i))
#define XUINT_TO_POINTER(u)     ((void *)  (u))

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#endif

#define XMMS_PATH_MAX 255

/* 9667 is XMMS written on a phone */
#define XMMS_DEFAULT_TCP_PORT 9667

const char *xmms_userconfdir_get (char *buf, int len);
const char *xmms_usercachedir_get (char *buf, int len);
const char *xmms_default_ipcpath_get (char *buf, int len);
const char *xmms_fallback_ipcpath_get (char *buf, int len);

#endif /* __XMMS_UTILS_H__ */
