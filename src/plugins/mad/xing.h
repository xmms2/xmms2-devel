#ifndef __XING_H_
#define __XING_H_

#include <mad.h>

typedef enum {
	XMMS_XING_FRAMES = 1UL << 0,
	XMMS_XING_BYTES  = 1UL << 1,
	XMMS_XING_TOC    = 1UL << 2,
	XMMS_XING_SCALE  = 1UL << 3,
} xmms_xing_flags_t;

struct xmms_xing_St;
typedef struct xmms_xing_St xmms_xing_t;

gboolean xmms_xing_has_flag (xmms_xing_t *xing, xmms_xing_flags_t flag);
guint xmms_xing_get_frames (xmms_xing_t *xing);
guint xmms_xing_get_bytes (xmms_xing_t *xing);
guint xmms_xing_get_toc (xmms_xing_t *xing, gint index);
xmms_xing_t *xmms_xing_parse (struct mad_bitptr ptr);

#endif
