#ifndef XMMS_URL_H
#define XMMS_URL_H

struct xmms_url_St {
	char *protocol;
	char *username, *password;

	int ipv6_host;
	char *host, *port;

	char *path;
};

typedef struct xmms_url_St xmms_url_t;

xmms_url_t *parse_url(const char *);
void free_url(xmms_url_t *);

#endif /* XMMS_URL_H */
