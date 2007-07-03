#include <math.h>

#include "xmmsc/xmmsc_visualization.h"

/* helper functions to convert timestamps */

double
tv2ts (struct timeval *t)
{
	return t->tv_sec + t->tv_usec / 1000000.0;
}

double
net2ts (int32_t* s)
{
	return (int32_t)(ntohl (s[0])) + (int32_t)(ntohl (s[1])) / 1000000.0;
}

void
ts2net (int32_t* d, double t)
{
	double s, u;
	u = modf (t, &s);
	d[0] = htonl ((int32_t)s);
	d[1] = htonl ((int32_t)(u * 1000000.0));
}

void
tv2net (int32_t* d, struct timeval *t)
{
	d[0] = htonl ((int32_t)t->tv_sec);
	d[1] = htonl ((int32_t)t->tv_usec);
}
