#ifndef __XUTILS_H__
#define __XUTILS_H__


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


#endif /* __XUTILS_H__ */
