#ifndef __SIDPLAY_WRAPPER_H
#define __SIDPLAY_WRAPPER_H

#include <glib.h>

struct sidplay_wrapper;
struct sidplay_wrapper *sidplay_wrapper_init(void);
gint sidplay_wrapper_play(struct sidplay_wrapper *, void *, gint);
gint sidplay_wrapper_load(struct sidplay_wrapper *, void *, gint);
const char *sidplay_wrapper_error(struct sidplay_wrapper *wrap);

#endif
