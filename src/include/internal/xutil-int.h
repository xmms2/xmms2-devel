#ifndef __XUTILS_H__
#define __XUTILS_H__

#define x_return_if_fail(expr) if (!expr) { fprintf (stderr, "Failed in file " __FILE__ " on  row %d\n", __LINE__); return; }
#define x_return_val_if_fail(expr, val) if (!expr) { fprintf (stderr, "Failed in file " __FILE__ " on  row %d\n", __LINE__); return val; }
#define x_return_null_if_fail(expr) x_return_val_if_fail (expr, NULL)
#define x_new0(type, num) calloc (1, sizeof (type) * num);
#define x_malloc0(size) calloc (1, size);

#define XPOINTER_TO_INT(p)      ((int)   (p))
#define XPOINTER_TO_UINT(p)     ((unsigned int)  (p))
                                                                                
#define XINT_TO_POINTER(i)      ((void *)  (i))
#define XUINT_TO_POINTER(u)     ((void *)  (u))

typedef int (*XCompareFunc) (const void *a, const void *b);
typedef int (*XCompareDataFunc) (const void *a, const void *b, void *user_data);
typedef int (*XEqualFunc) (const void *a, const void *b);
typedef int (*XFunc) (void *data, void *user_data);
typedef unsigned int (*XHashFunc) (const void *key);
typedef void (*XHFunc) (const void *key, const void *value, void *user_data);
typedef int  (*XHRFunc) (void *key, void *value, void *user_data);
typedef void (*XDestroyNotify) (void *data);

#endif /* __XUTILS_H__ */
