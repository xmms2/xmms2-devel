#ifndef __XMMS_MEDIAINFO_H__
#define __XMMS_MEDIAINFO_H__

#include "xmms/playlist.h"

typedef struct xmms_mediainfo_thread_St xmms_mediainfo_thread_t;

xmms_mediainfo_thread_t *xmms_mediainfo_thread_start (xmms_playlist_t *playlist);
void xmms_mediainfo_thread_add (xmms_mediainfo_thread_t *mthread, guint entryid);

#endif /* __XMMS_MEDIAINFO_H__ */
