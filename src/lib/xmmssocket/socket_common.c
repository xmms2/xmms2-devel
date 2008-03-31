#include "xmmsc/xmmsc_sockets.h"
#include "xmmsc/xmmsc_stdbool.h"

#ifdef HAVE_WSPIAPI
#include <wspiapi.h>
#endif

bool
xmms_socket_error_recoverable ()
{
	if (xmms_socket_errno () == XMMS_EAGAIN ||
	    xmms_socket_errno () == XMMS_EINTR) {
		return true;
	}
	return false;
}

int
xmms_getaddrinfo (const char *node, const char *service,
                  const struct addrinfo *hints, struct addrinfo **res)
{
       return getaddrinfo (node, service, hints, res);
}

void
xmms_freeaddrinfo (struct addrinfo *res)
{
       return freeaddrinfo (res);
}
