#ifndef __XMMS_UTILS_H__
#define __XMMS_UTILS_H__

#include <stdio.h>
#include <stdlib.h>

#include "xmmsc/xmmsc_stdbool.h"

#define XMMS_STRINGIFY_NOEXPAND(x) #x
#define XMMS_STRINGIFY(x) XMMS_STRINGIFY_NOEXPAND(x)

void xmms_dump_stack (void);

#define x_return_if_fail(expr) if (!(expr)) { fprintf (stderr, "Failed in file " __FILE__ " on  row %d\n", __LINE__); xmms_dump_stack (); return; }
#define x_return_val_if_fail(expr, val) if (!(expr)) { fprintf (stderr, "Failed in file " __FILE__ " on  row %d\n", __LINE__); xmms_dump_stack (); return val; }
#define x_return_null_if_fail(expr) x_return_val_if_fail (expr, NULL)
#define x_oom() do { fprintf(stderr, "Out of memory in " __FILE__ "on row %d\n", __LINE__); xmms_dump_stack (); } while (0)
#define x_new0(type, num) calloc (1, sizeof (type) * (num))
#define x_new(type, num) malloc (sizeof (type) * (num))
#define x_malloc0(size) calloc (1, size)
#define x_malloc(size) malloc (size)

/* This is not nice but there's no very clean way around the ugly warnings,
 * glibc does about the same but on compile time (this could be moved to waf?) */
#if defined(__x86_64__)
#  define XPOINTER_TO_INT(p)      ((int)  (long)  (p))
#  define XPOINTER_TO_UINT(p)     ((unsigned int)  (unsigned long)  (p))
#  define XINT_TO_POINTER(i)      ((void *)  (long)  (i))
#  define XUINT_TO_POINTER(u)     ((void *)  (unsigned long)  (u))
#else
#  define XPOINTER_TO_INT(p)      ((int)  (p))
#  define XPOINTER_TO_UINT(p)     ((unsigned int)  (p))
#  define XINT_TO_POINTER(i)      ((void *)  (i))
#  define XUINT_TO_POINTER(u)     ((void *)  (u))
#endif

#ifndef MIN
#  define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef X_N_ELEMENTS
#  define X_N_ELEMENTS(a)  (sizeof (a) / sizeof ((a)[0]))
#endif

#define XMMS_PATH_MAX 255

/* 9667 is XMMS written on a phone */
#define XMMS_DEFAULT_TCP_PORT 9667

const char *xmms_userconfdir_get (char *buf, int len);
const char *xmms_usercachedir_get (char *buf, int len);
const char *xmms_default_ipcpath_get (char *buf, int len);
const char *xmms_fallback_ipcpath_get (char *buf, int len);
bool xmms_sleep_ms (int n);

#endif /* __XMMS_UTILS_H__ */
