#include <xmmsclientpriv/visualization/common.h>

xmmsc_result_t *
setup_shm_prepare (xmmsc_connection_t *c, int32_t vv)
{
	return NULL;
}

bool
setup_shm_handle (xmmsc_result_t *res)
{
	return 0;
}

void
cleanup_shm (xmmsc_vis_unixshm_t *t)
{
}

int
read_do_shm (xmmsc_vis_unixshm_t *t, xmmsc_visualization_t *v, short *buffer, int drawtime, unsigned int blocking)
{
	return -1;
}
