/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

/** @file
 * Functions for working with lists/arrays of strings.
 * All such lists are assumed to be NULL-terminated.
 */

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include "xmmsc/xmmsc_strlist.h"

/**
 * Convert a list of variable arguments into a list of strings.
 * Note: Assumes that the arguments provided are all strings.
 * @param first The first string. Cannot be NULL.
 * @param ap List of variable arguments.
 * @return A newly allocated list of strings.
 */
char **
xmms_valist_to_strlist (char *first, va_list ap)
{
	if (first == NULL)
		abort ();
	
	char *cur = first;
	char **ret = NULL;
	int i = 0, size = sizeof (char *);

	do {
		size += sizeof (char *);
		ret = realloc (ret, size);
		ret[i] = strdup (cur);
		cur = va_arg (ap, char *);
		i++;
	} while (cur != NULL);
	ret[i] = NULL;

	return ret;
}

/**
 * Convert a variable number of arguments into a list of strings.
 * Note: Assumes that the arguments provided are all strings.
 * @param first The first string. Cannot be NULL.
 * @param ... Variable number of strings.
 * @return A newly allocated list of strings.
 */
char **
xmms_vargs_to_strlist (char *first, ...)
{
	if (first == NULL)
		abort ();

	va_list ap;
	char **ret = NULL;

	va_start(ap, first);
	ret = xmms_valist_to_strlist (first, ap);
	va_end(ap);

	return ret;
}

/**
 * Get the number of strings in a list. Note that the real length of the
 * (char **) array will be larger by 1 element (containing the terminating NULL).
 * @param data The list of strings
 * @return Number of strings
 */
int
xmms_strlist_len (char **data)
{
	if (data == NULL)
		abort ();
	int ret = -1;
	char *cur = NULL;
	do {
		ret++;
		cur = data[ret];
	} while (cur != NULL && *cur);
	return ret;
}

/**
 * Destroy a list of strings. Equivalent to g_strfreev().
 * @param data The list to destroy.
 */
void
xmms_strlist_destroy (char **data)
{
	if (data == NULL)
		abort ();
	int i;
	for (i = 0; data[i] != NULL; i++) {
		free (data[i]);
	}
	free(data);
}

/**
 * Return a copy of a list with newstr prepended.
 * @param data The original list.
 * @param newstr The string to prepend.
 * @return A newly allocated list of strings.
 */
char **
xmms_strlist_prepend_copy (char **data, char *newstr) {
	int i = 0;
	char *cur = newstr;
	char **ret = malloc ((xmms_strlist_len (data) + 2) * sizeof (char *));
	do {
		ret[i] = strdup (cur);
		cur = data[i];
		i++;
	} while (cur != NULL);
	ret[i] = NULL;
	return ret;
}
