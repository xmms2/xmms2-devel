#ifndef __XMMSCPRIV_XMMSC_UTIL_H__
#define __XMMSCPRIV_XMMSC_UTIL_H__

#include <stdlib.h>
#include <stdarg.h>

#include <xmmsc/xmmsc_util.h>
#include <xmmsc/xmmsc_compiler.h>
#include <xmmscpriv/xmmsc_log.h>

void xmms_dump_stack (void);

#ifndef MIN
#  define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#  define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

#ifndef X_N_ELEMENTS
#  define X_N_ELEMENTS(a)  (sizeof (a) / sizeof ((a)[0]))
#endif

#define INT64_TO_INT32(val) MAX (INT32_MIN, MIN (INT32_MAX, val))

typedef int (*XCompareFunc) (const void *a, const void *b);
typedef int (*XCompareDataFunc) (const void *a, const void *b, void *user_data);
typedef int (*XEqualFunc) (const void *a, const void *b);
typedef int (*XFunc) (void *data, void *user_data);
typedef unsigned int (*XHashFunc) (const void *key);
typedef void (*XHFunc) (const void *key, const void *value, void *user_data);
typedef int  (*XHRFunc) (void *key, void *value, void *user_data);
typedef void (*XDestroyNotify) (void *data);

/* errors and warnings */
#define x_return_if_fail(expr) if (!(expr)) { xmmsc_log_fail ("Check '%s' failed in %s at %s:%d", XMMS_STRINGIFY (expr), __FUNCTION__, __FILE__, __LINE__); return; }
#define x_return_val_if_fail(expr, val) if (!(expr)) { xmmsc_log_fail ("Check '%s' failed in %s at %s:%d", XMMS_STRINGIFY (expr), __FUNCTION__, __FILE__, __LINE__); return (val); }
#define x_return_null_if_fail(expr) x_return_val_if_fail (expr, NULL)
#define x_oom() xmmsc_log_fail ("Out of memory in %s at %s:%d", __FUNCTION__, __FILE__, __LINE__)

#define x_api_warning(msg) xmmsc_log_fail ("%s was called %s", __FUNCTION__, (msg))
#define x_api_warning_if(cond, msg) if (cond) { x_api_warning (msg); }
#define x_api_error(msg, retval) do { x_api_warning (msg); return retval; } while(0)
#define x_api_error_if(cond, msg, retval) if (cond) { x_api_warning (msg); return retval; }
#define x_internal_error(msg) xmmsc_log_fail("%s raised an error: %s", __FUNCTION__, (msg))

/* allocation */
#define x_new0(type, num) calloc (1, sizeof (type) * (num))
#define x_new(type, num) malloc (sizeof (type) * (num))
#define x_malloc0(size) calloc (1, size)
#define x_malloc(size) malloc (size)

/* utility functions */
char *x_vasprintf (const char *fmt, va_list args) XMMS_FORMAT(printf, 1, 0);
char *x_asprintf (const char *fmt, ...) XMMS_FORMAT(printf, 1, 2);

#endif /* __XMMSCPRIV_XMMSC_UTIL_H__ */
