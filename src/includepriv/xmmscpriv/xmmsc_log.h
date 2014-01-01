#ifndef __XMMSCPRIV_XMMSC_LOG_H__
#define __XMMSCPRIV_XMMSC_LOG_H__

#include "xmmsc/xmmsc_log.h"
#include "xmmsc/xmmsc_compiler.h"

void xmmsc_log (const char *domain, xmmsc_log_level_t level, const char *fmt, ...) XMMS_FORMAT(printf, 3, 4);

#ifndef XMMSC_LOG_DOMAIN
#define XMMSC_LOG_DOMAIN ((const char *) NULL)
#endif

#define xmmsc_log_fail(...) xmmsc_log (XMMSC_LOG_DOMAIN, XMMS_LOG_LEVEL_FAIL, __VA_ARGS__)
#define xmmsc_log_error(...) xmmsc_log (XMMSC_LOG_DOMAIN, XMMS_LOG_LEVEL_ERROR, __VA_ARGS__)

#endif /* __XMMSCPRIV_XMMSC_LOG_H__ */
