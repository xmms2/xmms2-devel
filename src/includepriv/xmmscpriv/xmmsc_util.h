#ifndef __XMMSCPRIV_XMMSC_UTIL_H__
#define __XMMSCPRIV_XMMSC_UTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <xmmsc/xmmsc_util.h>
#include <xmmsc/xmmsc_compiler.h>

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
void xmms_dump_stack (void);

static inline void
x_print_err (const char *func, const char *msg)
{
	fprintf (stderr, " ******\n");
	fprintf (stderr, " * %s was called %s\n", func, msg);
	fprintf (stderr, " * This is probably an error in the application using libxmmsclient\n");
	fprintf (stderr, " ******\n");
	xmms_dump_stack ();
}

static inline void
x_print_internal_err (const char *func, const char *msg)
{
	fprintf (stderr, " ******\n");
	fprintf (stderr, " * %s raised a fatal error: %s\n", func, msg);
	fprintf (stderr, " * This is probably a bug in XMMS2\n");
	fprintf (stderr, " ******\n");
	xmms_dump_stack ();
}

#define x_return_if_fail(expr) if (!(expr)) { fprintf (stderr, "Failed in file " __FILE__ " on  row %d\n", __LINE__); xmms_dump_stack (); return; }
#define x_return_val_if_fail(expr, val) if (!(expr)) { fprintf (stderr, "Failed in file " __FILE__ " on  row %d\n", __LINE__); xmms_dump_stack (); return val; }
#define x_return_null_if_fail(expr) x_return_val_if_fail (expr, NULL)
#define x_oom() do { fprintf(stderr, "Out of memory in " __FILE__ "on row %d\n", __LINE__); xmms_dump_stack (); } while (0)

#define x_api_warning(msg) do { x_print_err (__FUNCTION__, msg); } while(0)
#define x_api_warning_if(cond, msg) do { if (cond) { x_print_err (__FUNCTION__, msg); } } while(0)
#define x_api_error(msg, retval) do { x_print_err (__FUNCTION__, msg); return retval; } while(0)
#define x_api_error_if(cond, msg, retval) do { if (cond) { x_print_err (__FUNCTION__, msg); return retval;} } while(0)
#define x_internal_error(msg) do { x_print_internal_err (__FUNCTION__, msg); } while(0)

/* allocation */
#define x_new0(type, num) calloc (1, sizeof (type) * (num))
#define x_new(type, num) malloc (sizeof (type) * (num))
#define x_malloc0(size) calloc (1, size)
#define x_malloc(size) malloc (size)

/* utility functions */
char *x_vasprintf (const char *fmt, va_list args) XMMS_FORMAT(printf, 1, 0);
char *x_asprintf (const char *fmt, ...) XMMS_FORMAT(printf, 1, 2);

#endif /* __XMMSCPRIV_XMMSC_UTIL_H__ */
