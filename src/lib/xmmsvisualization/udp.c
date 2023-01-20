#include <stdlib.h>
#include <string.h>

#include <xmmsc/xmmsc_visualization.h>

/* helper functions to send/receive upd packages */

char*
packet_init_data (xmmsc_vis_udp_data_t *p)
{
	size_t sz = 1 + sizeof (uint16_t) + sizeof (xmmsc_vischunk_t);
	char* buffer = malloc (sz);
	if (buffer) {
		memset(buffer, 0, sz);

		buffer[0] = 'V';
		p->__unaligned_type = &buffer[0];
		p->__unaligned_grace = (uint16_t*)&buffer[1];
		p->__unaligned_data = (xmmsc_vischunk_t*)&buffer[1 + sizeof (uint16_t)];
		p->size = sz;
	}
	return buffer;
}

char*
packet_init_timing (xmmsc_vis_udp_timing_t *p)
{
	size_t sz = 1 + 5*sizeof (int32_t);
	char* buffer = malloc (sz);
	if (buffer) {
		memset(buffer, 0, sz);

		buffer[0] = 'T';
		p->__unaligned_type = &buffer[0];
		p->__unaligned_id = (int32_t*)&buffer[1];
		p->__unaligned_clientstamp = (int32_t*)&buffer[1 + sizeof (int32_t)];
		p->__unaligned_serverstamp = (int32_t*)&buffer[1 + 3*sizeof (int32_t)];
		p->size = sz;
	}
	return buffer;
}
