#ifndef __XMMS_LOG_H__
#define __XMMS_LOG_H__

#include <glib.h>

gint xmms_log_initialize (const char *filename);
void xmms_log_shutdown (void);
void xmms_log_daemonize (void);

#define XMMS_LOG_DEBUG 1
#define XMMS_LOG_INFORMATION 2
#define XMMS_LOG_FATAL 3 
#define XMMS_LOG_THOMAS 3

#ifdef __GNUC__
void xmms_log_debug (const gchar *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void xmms_log (const gchar *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void xmms_log_fatal (const gchar *fmt, ...) __attribute__ ((format (printf, 1, 2)));
#else
void xmms_log_debug (const gchar *fmt, ...);
void xmms_log (const gchar *fmt, ...);
void xmms_log_fatal (const gchar *fmt, ...);
#endif

#endif
