#ifndef __XMMS_OBJECT_H__
#define __XMMS_OBJECT_H__

#include <glib.h>

#define XMMS_OBJECT_MID 0x00455574


typedef struct xmms_object_St {
	guint32 id;
	GHashTable *signals;
	GMutex *mutex;
	struct xmms_object_St *parent;
	GHashTable *methods;
} xmms_object_t;


typedef enum {
	XMMS_OBJECT_METHOD_ARG_NONE,
	XMMS_OBJECT_METHOD_ARG_UINT32,
	XMMS_OBJECT_METHOD_ARG_INT32,
	XMMS_OBJECT_METHOD_ARG_STRING,
	XMMS_OBJECT_METHOD_ARG_PLAYLIST_ENTRY,
	XMMS_OBJECT_METHOD_ARG_PLAYLIST,
} xmms_object_method_arg_type_t;

#define XMMS_OBJECT_METHOD_MAX_ARGS 2
typedef struct {
	xmms_object_method_arg_type_t types[XMMS_OBJECT_METHOD_MAX_ARGS];
	union {
		gint32 int32;
		guint uint32;
		char *string;
	} values[XMMS_OBJECT_METHOD_MAX_ARGS];
	xmms_object_method_arg_type_t rettype;
	union {
		guint32 uint32;
		struct xmms_playlist_entry_St *playlist_entry; /* reffed */
		GList *playlist; /* GList of entries, reffed */
	} retval;
} xmms_object_method_arg_t;

typedef void (*xmms_object_method_func_t) (xmms_object_t *object, xmms_object_method_arg_t *arg);

typedef void (*xmms_object_handler_t) (xmms_object_t *object, gconstpointer data, gpointer userdata);

#define XMMS_OBJECT(p) ((xmms_object_t *)p)
#define XMMS_IS_OBJECT(p) (XMMS_OBJECT (p)->id == XMMS_OBJECT_MID)

void xmms_object_init (xmms_object_t *object);
void xmms_object_cleanup (xmms_object_t *object);

void xmms_object_parent_set (xmms_object_t *object, xmms_object_t *parent);

void xmms_object_connect (xmms_object_t *object, const gchar *signal,
						  xmms_object_handler_t handler, gpointer userdata);
void xmms_object_disconnect (xmms_object_t *object, const gchar *signal,
							 xmms_object_handler_t handler);

void xmms_object_emit (xmms_object_t *object, const gchar *signal,
					   gconstpointer data);




void xmms_object_method_add (xmms_object_t *object, char *method, xmms_object_method_func_t func);

void xmms_object_method_call (xmms_object_t *object, const char *method, xmms_object_method_arg_t *arg);

/* Some minor macro-magic. XMMS_METHOD_DEFINE and XMMS_METHOD_FUNC
 * are the only ones to be used directly */

#define __XMMS_METHOD_DO_ARG_NONE(a)
#define __XMMS_METHOD_DO_ARG_STRING(a) ,arg->values[a].string
#define __XMMS_METHOD_DO_ARG_UINT32(a) ,arg->values[a].uint32
#define __XMMS_METHOD_DO_ARG_INT32(a) ,arg->values[a].int32

#define __XMMS_METHOD_DO_RETVAL_NONE()
#define __XMMS_METHOD_DO_RETVAL_PLAYLIST_ENTRY() arg->retval.playlist_entry = 
#define __XMMS_METHOD_DO_RETVAL_UINT32() arg->retval.uint32 = 
#define __XMMS_METHOD_DO_RETVAL_PLAYLIST() arg->retval.playlist = 

#define XMMS_METHOD_DEFINE(methodname, realfunc, argtype0, _rettype, argtype1, argtype2) static void \
__int_xmms_method_##methodname (xmms_object_t *object, xmms_object_method_arg_t *arg) \
{ \
g_return_if_fail (XMMS_IS_OBJECT (object)); \
g_return_if_fail (arg->types[0] == XMMS_OBJECT_METHOD_ARG_##argtype1); \
g_return_if_fail (arg->types[1] == XMMS_OBJECT_METHOD_ARG_##argtype2); \
__XMMS_METHOD_DO_RETVAL_##_rettype() realfunc ((argtype0)object __XMMS_METHOD_DO_ARG_##argtype1(0) __XMMS_METHOD_DO_ARG_##argtype2(1)); \
arg->rettype = XMMS_OBJECT_METHOD_ARG_##_rettype; \
}

#define XMMS_METHOD_FUNC(methodname) __int_xmms_method_##methodname


#endif /* __XMMS_OBJECT_H__ */
