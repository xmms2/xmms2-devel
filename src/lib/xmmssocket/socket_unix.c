#include <errno.h>
#include <xmmsc/xmmsc_sockets.h>


int xmms_sockets_initialize () {
	return 1;
}

/**
 * Tries to set socket to non-blocking mode.
 * @param socket Socket to make non-blocking.
 * On success, returns 1.
 * On failure, closes socket and returns 0.
 */
int xmms_socket_set_nonblock (xmms_socket_t socket) {

	int flags;
	flags = fcntl (socket, F_GETFL, 0);

	if (flags == -1) {
		close (socket);
		return 0;
	}

	flags |= O_NONBLOCK;

	flags = fcntl (socket, F_SETFL, flags);
	if (flags == -1) {
		close (socket);
		return 0;
	}
	return 1;
}


int xmms_socket_valid (xmms_socket_t socket) {
	if (socket < 0) {
		return 0;
	}
	return 1;
}

void xmms_socket_invalidate (xmms_socket_t *socket) {
	*socket = -1;
}

void xmms_socket_close (xmms_socket_t socket) {
	close (socket);
}

int xmms_socket_errno () {
	return errno;
}
