#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "url.h"

static int strstrsplit (const char *, const char *, char **, char **);
static int strchrsplit (const char *, const char, char **, char **);
static int strrchrsplit (const char *, const char, char **, char **);
static int strpchrsplit (const char *, const char *, const char, char **, char **);


/**
 * Split a URL into its respective parts
 * @param url The URL to split
 */
xmms_url_t *parse_url (const char *url)
{
	char *tmp1, *tmp2, *tmp3, *tmp4;
	char *end;
	char *protocol;
	char *username, *password;
	char *host, *port;
	char *path;

	xmms_url_t *result;


	result = calloc (1, sizeof (xmms_url_t));
	if (!result)
		return NULL;

	if (strstrsplit (url, "://", &protocol, &tmp1)) {
		protocol = strdup ("");
		tmp1 = strdup (url);
	}

	if (strchrsplit (tmp1, '/', &tmp2, &path)) {
		tmp2 = strdup (tmp1);
		path = strdup ("");
	}

	if (strchrsplit (tmp2, '@', &tmp3, &tmp4)) {
		tmp3 = strdup ("");
		tmp4 = strdup (tmp2);
	}

	if (strchrsplit (tmp3, ':', &username, &password)) {
		username = strdup (tmp3);
		password = strdup ("");
	}

	/* Parse IPv4 and IPv6 host+port fields differently */
	if (tmp4[0] == '[') {
		result->ipv6_host = 1;

		end = strchr (tmp4 + 1, ']');
		if (end) {
			if (strpchrsplit (tmp4, end, ':', &host, &port)) {
				host = strdup (tmp4);
				port = strdup ("");
			}

			memmove (host, host + 1, end - tmp4 - 1);
			host[end - tmp4 - 1] = '\0';
		} else {
			host = strdup (tmp4 + 1);
			port = strdup ("");
		}
	} else {
		result->ipv6_host = 0;

		if (strrchrsplit (tmp4, ':', &host, &port)) {
			host = strdup (tmp4);
			port = strdup ("");
		}
	}

	free (tmp1);
	free (tmp2);
	free (tmp3);
	free (tmp4);

	result->protocol = protocol;
	result->username = username;
	result->password = password;
	result->host = host;
	result->port = port;
	result->path = path;

	return result;
}

void free_url (xmms_url_t *url)
{
	free (url->protocol);
	free (url->username);
	free (url->password);
	free (url->host);
	free (url->port);
	free (url->path);
	free (url);
}


/**
 * Split a string by the given substring.
 * @param str The string to split.
 * @param sep The separator substring.
 * @param former_result The first part (before the separator).
 * @param latter_result The last part (after the separator).
 * @return True on error, otherwise false.
 */
static int strstrsplit (const char *str, const char *sep, char **former_result, char **latter_result)
{
	char *split;
	char *former, *latter;

	split = strstr (str, sep);
	if (!split) {
		return 1;
	}

	former = malloc (split - str + 1);
	if (!former) {
		return 1;
	}

	strncpy (former, str, split - str);
	former[split - str] = '\0';

	latter = strdup (split + strlen (sep));

	*former_result = former;
	*latter_result = latter;
	return 0;
}

/**
 * Split a string by the first occurence of the given character.
 * @param str The string to split.
 * @param sep The separator character.
 * @param former_result The first part (before the separator).
 * @param latter_result The last part (after the separator).
 * @return True on error, otherwise false.
 */
static int strchrsplit (const char *str, const char sep, char **former_result, char **latter_result)
{
	char *split;
	char *former, *latter;

	split = strchr (str, sep);
	if (!split) {
		return 1;
	}

	former = malloc (split - str + 1);
	if (!former) {
		return 1;
	}

	strncpy (former, str, split - str);
	former[split - str] = '\0';

	latter = strdup (split + 1);

	*former_result = former;
	*latter_result = latter;
	return 0;
}

/**
 * Split a string by the last occurence of the given character.
 * @param str The string to split.
 * @param sep The separator character.
 * @param former_result The first part (before the separator).
 * @param latter_result The last part (after the separator).
 * @return True on error, otherwise false.
 */
static int strrchrsplit (const char *str, const char sep, char **former_result, char **latter_result)
{
	char *split;
	char *former, *latter;

	split = strrchr (str, sep);
	if (!split) {
		return 1;
	}

	former = malloc (split - str + 1);
	if (!former) {
		return 1;
	}

	strncpy (former, str, split - str);
	former[split - str] = '\0';

	latter = strdup (split + 1);

	*former_result = former;
	*latter_result = latter;
	return 0;
}

/**
 * Split a string by the given occurence of the given character.
 * @param str The string to split.
 * @param pos The position to search from.
 * @param sep The separator character.
 * @param former_result The first part (before the separator).
 * @param latter_result The last part (after the separator).
 * @return True on error, otherwise false.
 */
static int strpchrsplit (const char *str, const char *pos, const char sep, char **former_result, char **latter_result)
{
	char *split;
	char *former, *latter;

	split = strchr (pos, sep);
	if (!split) {
		return 1;
	}

	former = malloc (split - str + 1);
	if (!former) {
		return 1;
	}

	strncpy (former, str, split - str);
	former[split - str] = '\0';

	latter = strdup (split + 1);

	*former_result = former;
	*latter_result = latter;
	return 0;
}
