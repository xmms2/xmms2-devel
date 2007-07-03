#include "common.h"

xmmsc_result_t *
setup_shm (xmmsc_connection_t *c, xmmsc_vis_unixshm_t *t, int32_t id)
{ return NULL; }

void cleanup_shm (xmmsc_vis_unixshm_t *t) {}
int read_start_shm (xmmsc_vis_unixshm_t *t, int blocking, xmmsc_vischunk_t **dest) {}
void read_finish_shm (xmmsc_vis_unixshm_t *t, xmmsc_vischunk_t *dest) {}
