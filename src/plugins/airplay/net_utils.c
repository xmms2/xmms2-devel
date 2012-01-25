#include "net_utils.h"

char *
get_local_addr (int fd)
{
	struct sockaddr_in addr;
	int ret;
	socklen_t len = sizeof (addr);

	ret = getsockname (fd, (struct sockaddr *) &addr, &len);

	if (ret) {
		return NULL;
	}

	return inet_ntoa (addr.sin_addr);
}

int
set_sock_nonblock (int sockfd)
{
	int flags;
	flags = fcntl (sockfd, F_GETFL, 0);
	return fcntl (sockfd, F_SETFL, flags | O_NONBLOCK);
}

int
tcp_open ()
{
	int sock_fd;
	sock_fd = socket (AF_INET, SOCK_STREAM, 0);
	return sock_fd;
}

int
tcp_connect (int sock_fd, const char *host, unsigned int port)
{
	struct sockaddr_in addr;
	struct hostent *h;
	int ret;

	h = gethostbyname (host);
	if (h) {
		addr.sin_family = h->h_addrtype;
		memcpy ((char *) &addr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
	}else{
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr (host);
		if (addr.sin_addr.s_addr == 0xFFFFFFFF) {
			return -1;
		}
	}
	addr.sin_port = htons (port);
	ret = connect (sock_fd, (struct sockaddr *)&addr, sizeof (struct sockaddr));

	return ret;
}

int
tcp_write (int fd, const char *buf, int n)
{
	size_t nleft;
	ssize_t nwritten = 0;
	const char *ptr;
	int ret;

	ptr = buf;
	nleft = n;
	while (nleft > 0) {
		ret = write (fd, ptr, nleft);
		if (ret <= 0) {
			if (ret < 0 && errno == EWOULDBLOCK) {
				break;
			} else if (ret < 0 && errno == EINTR) {
				continue;
			} else {
				return -1;
			}
		}
		nwritten += ret;
		nleft -= ret;
		ptr   += ret;
	}
	return nwritten;
}
