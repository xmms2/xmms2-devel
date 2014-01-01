#ifndef __XMMSC_XMMSC_LOG_H__
#define __XMMSC_XMMSC_LOG_H__

#include "xmmsc/xmmsc_idnumbers.h"

typedef void (*xmmsc_log_handler_t) (const char *, xmmsc_log_level_t, const char *, void *);

void xmmsc_log_handler_set (xmmsc_log_handler_t, void *);
void xmmsc_log_handler_get (xmmsc_log_handler_t *, void **);

void xmmsc_log_default_handler (const char *domain, xmmsc_log_level_t level, const char *msg, void *);

#endif /* __XMMSC_XMMSC_LOG_H__ */
