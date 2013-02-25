#include <xmmsc/xmmsc_sockets.h>

int xmms_sockets_initialize () {
	WSADATA wsaData;
	int res = WSAStartup (MAKEWORD (2,2), &wsaData);
	if (res != NO_ERROR) {
		return 0;
	}
	return 1;
}

/**
 * Tries to set socket to non-blocking mode.
 * @param socket Socket to make non-blocking.
 * On success, returns 1.
 * On failure, closes socket and returns 0.
 */
int xmms_socket_set_nonblock (xmms_socket_t socket) {
	unsigned long yes = 1;
	int err = ioctlsocket (socket, FIONBIO, &yes);
	if (err == SOCKET_ERROR) {
		closesocket (socket);
		return 0;
	}
	return 1;

}
int xmms_socket_valid (xmms_socket_t socket) {
	if (socket == INVALID_SOCKET) {
		return 0;
	}
	return 1;
}

void xmms_socket_invalidate (xmms_socket_t *socket) {
	*socket = INVALID_SOCKET;
}

void xmms_socket_close (xmms_socket_t socket) {
	closesocket (socket);
}

int xmms_socket_errno () {
	return WSAGetLastError ();
}
