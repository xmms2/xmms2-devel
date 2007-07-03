#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsc/xmmsc_visualization.h"

#ifndef __VISUALIZATIONCLIENT_COMMON_H__
#define __VISUALIZATIONCLIENT_COMMON_H__

/* provided by unixshm.c / dummy.c */
xmmsc_result_t *setup_shm (xmmsc_connection_t *c, xmmsc_vis_unixshm_t *t, int32_t id);
void cleanup_shm (xmmsc_vis_unixshm_t *t);
int read_start_shm (xmmsc_vis_unixshm_t *t, int blocking, xmmsc_vischunk_t **dest);
void read_finish_shm (xmmsc_vis_unixshm_t *t, xmmsc_vischunk_t *dest);

/* provided by udp.c */
xmmsc_result_t *setup_udp (xmmsc_connection_t *c, xmmsc_vis_udp_t *t, int32_t id);
void cleanup_udp (xmmsc_vis_udp_t *t);
int read_start_udp (xmmsc_vis_udp_t *t, int blocking, xmmsc_vischunk_t **dest, int32_t id);
void read_finish_udp (xmmsc_vis_udp_t *t, xmmsc_vischunk_t *dest);

#endif
