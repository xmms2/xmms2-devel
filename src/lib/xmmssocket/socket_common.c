#include "xmmsc/xmmsc_sockets.h"
#include "xmmsc/xmmsc_stdbool.h"

bool xmms_socket_error_recoverable () {
	if (xmms_socket_errno () == XMMS_EAGAIN ||
	    xmms_socket_errno () == XMMS_EINTR) {
		return true;
	}
	return false;
}
