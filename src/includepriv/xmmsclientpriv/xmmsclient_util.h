#ifndef __XUTILS_H__
#define __XUTILS_H__

#include <stdio.h>
#include "xmmsc/xmmsc_compiler.h"
#include "xmmsc/xmmsc_util.h"

typedef int (*XCompareFunc) (const void *a, const void *b);
typedef int (*XCompareDataFunc) (const void *a, const void *b, void *user_data);
typedef int (*XEqualFunc) (const void *a, const void *b);
typedef int (*XFunc) (void *data, void *user_data);
typedef unsigned int (*XHashFunc) (const void *key);
typedef void (*XHFunc) (const void *key, const void *value, void *user_data);
typedef int  (*XHRFunc) (void *key, void *value, void *user_data);
typedef void (*XDestroyNotify) (void *data);

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

#define x_check_conn(c, retval) do { x_api_error_if (!c, "with a NULL connection", retval); x_api_error_if (!c->ipc, "with a connection that isn't connected", retval);} while (0)

#define x_api_warning(msg) do { x_print_err (__FUNCTION__, msg); } while(0)
#define x_api_warning_if(cond, msg) do { if (cond) { x_print_err (__FUNCTION__, msg); } } while(0)
#define x_api_error(msg, retval) do { x_print_err (__FUNCTION__, msg); return retval; } while(0)
#define x_api_error_if(cond, msg, retval) do { if (cond) { x_print_err (__FUNCTION__, msg); return retval;} } while(0)
#define x_internal_error(msg) do { x_print_internal_err (__FUNCTION__, msg); } while(0)

#endif /* __XUTILS_H__ */
